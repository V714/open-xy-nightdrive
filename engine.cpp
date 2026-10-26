// SPDX-FileCopyrightText: 2026 Tomasz Lipczyński (V7, GitHub: V714)
// SPDX-License-Identifier: MIT
#include "core.h"
#include "dsp.h"
namespace nightdrive {
using namespace dsp;
static inline void start(Voice &voice, Shared &, u32) {
    voice.carrier = voice.modulator = voice.ready = voice.transientAge = 0;
    voice.feedback[0] = voice.feedback[1] = voice.prev = voice.dc = 0;
    for (unsigned j = 0; j < 8; ++j)
        voice.controls[j] = 0;
}
static inline void initialize(Voice &voice, Shared &s, u32 owner) {
    start(voice, s, owner);
}
static inline void destroy(Voice &, Shared &, u32) {
}
// Specializing the stable path keeps control work outside the sample loop.
struct PitchFactors {
    u32 modulator;
    i32 feedback, modulation;
};
[[gnu::always_inline]] static inline PitchFactors pitch_factors(u32 increment, i32 ratio,
                                                                i32 feedback, i32 modulation) {
    const u32 scaled = u32(multiply(i32(increment >> 1), ratio));
    const u32 modulator = scaled >= 0x02000000u ? 0x7fffffffu : scaled << 6;
    const u32 highest = increment > modulator ? increment : modulator;
    const i32 bandwidth = highest <= 0x06000000u ? 2147483647
                          : highest >= 0x30000000u
                              ? 0
                              : i32((0x30000000u - highest - 1u) * 3u) +
                                    multiply(i32(0x30000000u - highest - 1u), 102261126);
    return {modulator, multiply(feedback, bandwidth), multiply(modulation, bandwidth)};
}
// Below this input increment even the maximum 16x ratio cannot reach the cutoff.
static inline bool bandwidth_disabled(u32 increment, i32 speed) {
    if (increment < 0x03000000u)
        return false;
    const u32 scaled = u32(multiply(i32(increment >> 1), operator_ratio(speed)));
    const u32 modulator = scaled >= 0x02000000u ? 0x7fffffffu : scaled << 6;
    return increment >= 0x30000000u || modulator >= 0x30000000u;
}
template <bool ConstantPitch, bool ModTransient, bool Punch, bool ZeroModulation>
static inline void render_range(Voice &voice, opxy::Output output, opxy::Pitch pitch,
                                unsigned begin, unsigned end) {
    i32 *controls = voice.controls;
    const i32 punchAmount = Punch ? punch_amount(controls[3]) : 0;
    i32 driveSquared = multiply(controls[3], controls[3]);
    i32 driveCompensation = lut(drive_comp_lut, controls[3]);
    const i32 clippedCompensated = multiply(tanh_clipped, driveCompensation);
    i32 ratio = operator_ratio(controls[0]);
    i32 feedbackAmount = feedback_amount(controls[1]),
        modulationAmount = modulation_amount(controls[2]);
    PitchFactors factors = pitch_factors(pitch[begin], ratio, feedbackAmount, modulationAmount);
    for (unsigned i = begin; i < end; ++i) {
        const u32 increment = pitch[i];
        if (!ConstantPitch)
            factors = pitch_factors(increment, ratio, feedbackAmount, modulationAmount);
        // Keep both Q31 rounding stages identical to the original render.
        i32 fboffset = multiply(voice.feedback[0] + voice.feedback[1], factors.feedback);
        i32 modulatorSignal = sine(voice.modulator + u32(fboffset));
        voice.feedback[1] = voice.feedback[0];
        voice.feedback[0] = modulatorSignal;

        i32 phaseModulation =
            ZeroModulation ? 0 : multiply(modulatorSignal * 2, factors.modulation);
        // Linear finite transients leave steady sustain entirely to stock ADSR.
        u32 transientPhase = 0;
        if (ModTransient && !ZeroModulation) {
            i32 envelope = i32(3528 - voice.transientAge) * 608697;
            transientPhase = u32(multiply(phaseModulation, multiply(controls[2], envelope)));
        }
        const i32 fundamental = sine(voice.carrier);
        const u32 phaseOffset = u32(phaseModulation) * 2u + transientPhase;
        i32 signal = fundamental;
        if (!ZeroModulation) {
            const i32 modulatedSignal = sine(voice.carrier + phaseOffset);
            signal = modulatedSignal - multiply(modulatedSignal, 429496729) +
                     multiply(fundamental, 429496729);
        }
        if (Punch) {
            i32 envelope = i32(1103 - voice.transientAge) * 1946948;
            signal += multiply(signal, multiply(punchAmount, envelope)) / 2;
        }
        signal = saturate_approx(signal, controls[3], driveSquared, driveCompensation,
                                 clippedCompensated);
        if (ModTransient)
            ++voice.transientAge;
        output[i] = clamp(dc_block(signal, voice.prev, voice.dc) / 4, -402653184, 402653184);
        voice.carrier += increment;
        voice.modulator += factors.modulator;
    }
}
// Split only at the original sample boundaries; state updates keep their order.
template <bool ConstantPitch, bool ZeroModulation>
static inline void render_block(Voice &voice, opxy::Output output, opxy::Pitch pitch) {
    unsigned begin = 0;
    if (voice.transientAge < 1103) {
        const unsigned remaining = 1103 - voice.transientAge;
        const unsigned end = remaining < 64 ? remaining : 64;
        render_range<ConstantPitch, true, true, ZeroModulation>(voice, output, pitch, begin, end);
        begin = end;
    }
    if (begin < 64 && voice.transientAge < 3528) {
        const unsigned remaining = 3528 - voice.transientAge;
        const unsigned end = remaining < 64 - begin ? begin + remaining : 64;
        render_range<ConstantPitch, true, false, ZeroModulation>(voice, output, pitch, begin, end);
        begin = end;
    }
    if (begin < 64)
        render_range<ConstantPitch, false, false, ZeroModulation>(voice, output, pitch, begin, 64);
}
// Retain the original compact loop during smoothing; specializing it regresses host cost.
static inline void render_dynamic(Voice &voice, Shared &, u32, opxy::Output output,
                                  opxy::Parameters parameters, opxy::Pitch pitch) {
    if (!voice.ready) {
        for (unsigned k = 0; k < 4; ++k)
            voice.controls[k] = parameters[k] < 0 ? 0 : parameters[k];
        voice.ready = 1;
    }
    i32 *controls = voice.controls;
    bool changing = !settled(controls, parameters);
    i32 previousGlare = controls[3];
    i32 punchAmount = voice.transientAge < 1103 ? punch_amount(previousGlare) : 0;
    i32 ratio = operator_ratio(controls[0]);
    i32 feedbackAmount = feedback_amount(controls[1]),
        modulationAmount = modulation_amount(controls[2]);
    for (unsigned i = 0; i < 64; ++i) {
        if (changing) {
            tick_controls(controls, parameters);
            if (voice.transientAge < 1103 && controls[3] != previousGlare) {
                previousGlare = controls[3];
                punchAmount = punch_amount(previousGlare);
            }
            ratio = operator_ratio(controls[0]);
            feedbackAmount = feedback_amount(controls[1]);
            modulationAmount = modulation_amount(controls[2]);
        }
        u32 increment = pitch[i], scaledIncrement = u32(multiply(i32(increment >> 1), ratio));
        u32 modInc = scaledIncrement >= 0x02000000u ? 0x7fffffffu : scaledIncrement << 6;
        u32 highest = increment > modInc ? increment : modInc;
        i32 bandwidth = highest <= 0x06000000u ? 2147483647
                        : highest >= 0x30000000u
                            ? 0
                            : i32((0x30000000u - highest - 1u) * 3u) +
                                  multiply(i32(0x30000000u - highest - 1u), 102261126);
        // Smoothly reduce the nonlinear bandwidth at high notes; phases never reset.
        i32 fboffset =
            multiply(voice.feedback[0] + voice.feedback[1], multiply(feedbackAmount, bandwidth));
        i32 modulatorSignal = sine(voice.modulator + u32(fboffset));
        voice.feedback[1] = voice.feedback[0];
        voice.feedback[0] = modulatorSignal;

        i32 phaseModulation = multiply(modulatorSignal * 2, multiply(modulationAmount, bandwidth));
        // Linear finite transients leave steady sustain entirely to stock ADSR.
        u32 transientPhase = 0;
        if (voice.transientAge < 3528) {
            i32 envelope = i32(3528 - voice.transientAge) * 608697;
            transientPhase = u32(multiply(phaseModulation, multiply(controls[2], envelope)));
        }
        i32 fundamental = sine(voice.carrier),
            modulatedSignal = sine(voice.carrier + u32(phaseModulation) * 2u + transientPhase);
        i32 signal = modulatedSignal - multiply(modulatedSignal, 429496729) +
                     multiply(fundamental, 429496729);
        if (voice.transientAge < 1103) {
            i32 envelope = i32(1103 - voice.transientAge) * 1946948;
            signal += multiply(signal, multiply(punchAmount, envelope)) / 2;
        }
        signal = saturate_approx(signal, controls[3]);
        if (voice.transientAge < 3528)
            ++voice.transientAge;
        output[i] = clamp(dc_block(signal, voice.prev, voice.dc) / 4, -402653184, 402653184);
        voice.carrier += increment;
        voice.modulator += modInc;
    }
}
static inline void render(Voice &voice, Shared &shared, u32 owner, opxy::Output output,
                          opxy::Parameters parameters, opxy::Pitch pitch) {
    if (!voice.ready) {
        for (unsigned k = 0; k < 4; ++k)
            voice.controls[k] = parameters[k] < 0 ? 0 : parameters[k];
        voice.ready = 1;
    }
    if (settled(voice.controls, parameters)) {
        bool constantPitch = true;
        for (unsigned i = 1; i < 64; ++i)
            if (pitch[i] != pitch[0]) {
                constantPitch = false;
                break;
            }
        if (voice.controls[2] == 0 ||
            (constantPitch && bandwidth_disabled(pitch[0], voice.controls[0]))) {
            if (constantPitch)
                render_block<true, true>(voice, output, pitch);
            else
                render_block<false, true>(voice, output, pitch);
        } else {
            if (constantPitch)
                render_block<true, false>(voice, output, pitch);
            else
                render_block<false, false>(voice, output, pitch);
        }
    } else {
        render_dynamic(voice, shared, owner, output, parameters, pitch);
    }
}
} // namespace nightdrive
