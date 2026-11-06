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
struct StableCoefficients {
    PreparedShaper shaper;
    PitchFactors pitch;
    i32 ratio, feedback, modulation, punch;
};
template <bool ConstantPitch, bool ModTransient, bool Punch, bool ZeroModulation>
static inline void render_range(Voice &voice, opxy::Output output, opxy::Pitch pitch,
                                unsigned begin, unsigned end, const StableCoefficients &prepared) {
    const i32 drift = voice.controls[2];
    u32 carrier = voice.carrier, modulator = voice.modulator, age = voice.transientAge;
    i32 feedback0 = voice.feedback[0], feedback1 = voice.feedback[1];
    i32 previous = voice.prev, dc = voice.dc;
    PitchFactors factors = prepared.pitch;
    for (unsigned i = begin; i < end; ++i) {
        const u32 increment = pitch[i];
        if (!ConstantPitch)
            factors =
                pitch_factors(increment, prepared.ratio, prepared.feedback, prepared.modulation);
        // Keep both Q31 rounding stages identical to the original render.
        i32 fboffset = multiply(feedback0 + feedback1, factors.feedback);
        i32 modulatorSignal = sine(modulator + u32(fboffset));
        feedback1 = feedback0;
        feedback0 = modulatorSignal;

        i32 phaseModulation =
            ZeroModulation ? 0 : multiply(modulatorSignal * 2, factors.modulation);
        // Linear finite transients leave steady sustain entirely to stock ADSR.
        u32 transientPhase = 0;
        if (ModTransient && !ZeroModulation) {
            i32 envelope = i32(3528 - age) * 608697;
            transientPhase = u32(multiply(phaseModulation, multiply(drift, envelope)));
        }
        const i32 fundamental = sine_audio(carrier);
        const u32 phaseOffset = u32(phaseModulation) * 2u + transientPhase;
        i32 signal = fundamental;
        if (!ZeroModulation) {
            const i32 modulatedSignal = sine_audio(carrier + phaseOffset);
            signal = mix(modulatedSignal, fundamental, 429496729);
        }
        if (Punch) {
            i32 envelope = i32(1103 - age) * 1946948;
            signal += multiply(signal, multiply(prepared.punch, envelope)) / 2;
        }
        signal = apply_shaper(signal, prepared.shaper);
        if (ModTransient)
            ++age;
        output[i] = clamp(dc_block(signal, previous, dc) / 4, -402653184, 402653184);
        carrier += increment;
        modulator += factors.modulator;
    }
    voice.carrier = carrier;
    voice.modulator = modulator;
    voice.transientAge = age;
    voice.feedback[0] = feedback0;
    voice.feedback[1] = feedback1;
    voice.prev = previous;
    voice.dc = dc;
}
// Split only at the original sample boundaries; state updates keep their order.
template <bool ConstantPitch, bool ZeroModulation>
static inline void render_block(Voice &voice, opxy::Output output, opxy::Pitch pitch) {
    const i32 ratio = operator_ratio(voice.controls[0]);
    const i32 feedback = feedback_amount(voice.controls[1]);
    const i32 modulation = modulation_amount(voice.controls[2]);
    const StableCoefficients prepared = {prepare_shaper(voice.controls[3]),
                                         pitch_factors(pitch[0], ratio, feedback, modulation),
                                         ratio,
                                         feedback,
                                         modulation,
                                         voice.transientAge < 1103 ? punch_amount(voice.controls[3])
                                                                   : 0};
    unsigned begin = 0;
    if (voice.transientAge < 1103) {
        const unsigned remaining = 1103 - voice.transientAge;
        const unsigned end = remaining < 64 ? remaining : 64;
        render_range<ConstantPitch, true, true, ZeroModulation>(voice, output, pitch, begin, end,
                                                                prepared);
        begin = end;
    }
    if (begin < 64 && voice.transientAge < 3528) {
        const unsigned remaining = 3528 - voice.transientAge;
        const unsigned end = remaining < 64 - begin ? begin + remaining : 64;
        render_range<ConstantPitch, true, false, ZeroModulation>(voice, output, pitch, begin, end,
                                                                 prepared);
        begin = end;
    }
    if (begin < 64)
        render_range<ConstantPitch, false, false, ZeroModulation>(voice, output, pitch, begin, 64,
                                                                  prepared);
}
// After the finite attack, plan independent coefficients in bounded spans before the FM loop.
template <bool AllChanging>
[[gnu::noinline]] static void
render_planned_sustain(Voice &voice, Shared &, u32, opxy::Output output,
                       opxy::Parameters parameters, opxy::Pitch pitch) {
    i32 *controls = voice.controls;
    i32 speed = controls[0], tension = controls[1], drift = controls[2], glare = controls[3];
    const i32 speedTarget = parameters[0] < 0 ? 0 : parameters[0];
    const i32 tensionTarget = parameters[1] < 0 ? 0 : parameters[1];
    const i32 driftTarget = parameters[2] < 0 ? 0 : parameters[2];
    const i32 glareTarget = parameters[3] < 0 ? 0 : parameters[3];
    const bool glareChanging = AllChanging || glare != glareTarget;
    const PreparedShaper heldShaper = prepare_shaper(glareChanging ? 0 : glare);
    const bool speedChanging = AllChanging || speed != parameters[0];
    const bool tensionChanging = AllChanging || tension != parameters[1];
    const bool driftChanging = AllChanging || drift != parameters[2];
    bool fixedPitch = !speedChanging;
    if (fixedPitch)
        for (unsigned i = 1; i < 64; ++i)
            if (pitch[i] != pitch[0]) {
                fixedPitch = false;
                break;
            }
    u32 modInc = 0;
    i32 bandwidth = 0;
    u32 carrier = voice.carrier, modulator = voice.modulator;
    i32 feedback0 = voice.feedback[0], feedback1 = voice.feedback[1];
    i32 previous = voice.prev, dc = voice.dc;
    i32 ratio = operator_ratio(speed);
    i32 feedbackAmount = feedback_amount(tension), modulationAmount = modulation_amount(drift);
    struct Coefficients {
        u32 modulator;
        i32 feedback, modulation, glare;
    };
    const unsigned span = 32;
    for (unsigned begin = 0; begin < 64; begin += span) {
        Coefficients frames[span];
        for (unsigned j = 0; j < span; ++j) {
            const unsigned i = begin + j;
            if (speedChanging)
                speed = smooth_control(speed, speedTarget);
            if (tensionChanging)
                tension = smooth_control(tension, tensionTarget);
            if (driftChanging)
                drift = smooth_control(drift, driftTarget);
            if (glareChanging)
                glare = smooth_control(glare, glareTarget);
            if (speedChanging)
                ratio = operator_ratio(speed);
            if (tensionChanging)
                feedbackAmount = feedback_amount(tension);
            if (driftChanging)
                modulationAmount = modulation_amount(drift);
            const u32 increment = pitch[i];
            if (!fixedPitch || i == 0) {
                const u32 scaledIncrement = u32(multiply(i32(increment >> 1), ratio));
                modInc = scaledIncrement >= 0x02000000u ? 0x7fffffffu : scaledIncrement << 6;
                const u32 highest = increment > modInc ? increment : modInc;
                bandwidth = highest <= 0x06000000u ? 2147483647
                            : highest >= 0x30000000u
                                ? 0
                                : i32((0x30000000u - highest - 1u) * 3u) +
                                      multiply(i32(0x30000000u - highest - 1u), 102261126);
            }
            Coefficients &frame = frames[j];
            frame.modulator = modInc;
            frame.feedback = multiply(feedbackAmount, bandwidth);
            frame.modulation = multiply(modulationAmount, bandwidth);
            frame.glare = glare;
        }
        for (unsigned j = 0; j < span; ++j) {
            const unsigned i = begin + j;
            const Coefficients &frame = frames[j];
            const i32 fboffset = multiply(feedback0 + feedback1, frame.feedback);
            const i32 modulatorSignal = sine(modulator + u32(fboffset));
            feedback1 = feedback0;
            feedback0 = modulatorSignal;
            const i32 phaseModulation = multiply(modulatorSignal * 2, frame.modulation);
            const i32 fundamental = sine_audio(carrier);
            const i32 modulatedSignal = sine_audio(carrier + u32(phaseModulation) * 2u);
            i32 signal = mix(modulatedSignal, fundamental, 429496729);
            signal = glareChanging ? saturate_approx(signal, frame.glare)
                                   : apply_shaper(signal, heldShaper);
            output[i] = clamp(dc_block(signal, previous, dc) / 4, -402653184, 402653184);
            carrier += pitch[i];
            modulator += frame.modulator;
        }
    }
    controls[0] = speed;
    controls[1] = tension;
    controls[2] = drift;
    controls[3] = glare;
    voice.carrier = carrier;
    voice.modulator = modulator;
    voice.feedback[0] = feedback0;
    voice.feedback[1] = feedback1;
    voice.prev = previous;
    voice.dc = dc;
}
// Select full automation once per block; partial automation updates only moving coefficients.
template <bool AllChanging>
[[gnu::noinline]] static void render_dynamic_impl(Voice &voice, Shared &, u32, opxy::Output output,
                                                  opxy::Parameters parameters, opxy::Pitch pitch) {
    if (!voice.ready) {
        for (unsigned k = 0; k < 4; ++k)
            voice.controls[k] = parameters[k] < 0 ? 0 : parameters[k];
        voice.ready = 1;
    }
    i32 *controls = voice.controls;
    i32 speed = controls[0], tension = controls[1], drift = controls[2], glare = controls[3];
    const i32 speedTarget = parameters[0] < 0 ? 0 : parameters[0];
    const i32 tensionTarget = parameters[1] < 0 ? 0 : parameters[1];
    const i32 driftTarget = parameters[2] < 0 ? 0 : parameters[2];
    const i32 glareTarget = parameters[3] < 0 ? 0 : parameters[3];
    const bool glareChanging = AllChanging || glare != glareTarget;
    const bool speedChanging = AllChanging || speed != parameters[0];
    const bool tensionChanging = AllChanging || tension != parameters[1];
    const bool driftChanging = AllChanging || drift != parameters[2];
    bool fixedPitch = !speedChanging;
    if (fixedPitch)
        for (unsigned i = 1; i < 64; ++i)
            if (pitch[i] != pitch[0]) {
                fixedPitch = false;
                break;
            }
    u32 modInc = 0;
    i32 bandwidth = 0;
    u32 carrier = voice.carrier, modulator = voice.modulator, age = voice.transientAge;
    i32 feedback0 = voice.feedback[0], feedback1 = voice.feedback[1];
    i32 previous = voice.prev, dc = voice.dc;
    i32 previousGlare = glare;
    i32 punchAmount = age < 1103 ? punch_amount(previousGlare) : 0;
    i32 ratio = operator_ratio(speed);
    i32 feedbackAmount = feedback_amount(tension), modulationAmount = modulation_amount(drift);
    for (unsigned i = 0; i < 64; ++i) {
        if (speedChanging)
            speed = smooth_control(speed, speedTarget);
        if (tensionChanging)
            tension = smooth_control(tension, tensionTarget);
        if (driftChanging)
            drift = smooth_control(drift, driftTarget);
        if (glareChanging)
            glare = smooth_control(glare, glareTarget);
        if (speedChanging)
            ratio = operator_ratio(speed);
        if (tensionChanging)
            feedbackAmount = feedback_amount(tension);
        if (driftChanging)
            modulationAmount = modulation_amount(drift);
        if (age < 1103 && glare != previousGlare) {
            previousGlare = glare;
            punchAmount = punch_amount(glare);
        }
        const u32 increment = pitch[i];
        if (!fixedPitch || i == 0) {
            const u32 scaledIncrement = u32(multiply(i32(increment >> 1), ratio));
            modInc = scaledIncrement >= 0x02000000u ? 0x7fffffffu : scaledIncrement << 6;
            const u32 highest = increment > modInc ? increment : modInc;
            bandwidth = highest <= 0x06000000u ? 2147483647
                        : highest >= 0x30000000u
                            ? 0
                            : i32((0x30000000u - highest - 1u) * 3u) +
                                  multiply(i32(0x30000000u - highest - 1u), 102261126);
        }
        // Smoothly reduce the nonlinear bandwidth at high notes; phases never reset.
        i32 fboffset = multiply(feedback0 + feedback1, multiply(feedbackAmount, bandwidth));
        i32 modulatorSignal = sine(modulator + u32(fboffset));
        feedback1 = feedback0;
        feedback0 = modulatorSignal;

        i32 phaseModulation = multiply(modulatorSignal * 2, multiply(modulationAmount, bandwidth));
        // Linear finite transients leave steady sustain entirely to stock ADSR.
        u32 transientPhase = 0;
        if (age < 3528) {
            i32 envelope = i32(3528 - age) * 608697;
            transientPhase = u32(multiply(phaseModulation, multiply(drift, envelope)));
        }
        i32 fundamental = sine_audio(carrier),
            modulatedSignal = sine_audio(carrier + u32(phaseModulation) * 2u + transientPhase);
        i32 signal = mix(modulatedSignal, fundamental, 429496729);
        if (age < 1103) {
            i32 envelope = i32(1103 - age) * 1946948;
            signal += multiply(signal, multiply(punchAmount, envelope)) / 2;
        }
        signal = saturate_approx(signal, glare);
        if (age < 3528)
            ++age;
        output[i] = clamp(dc_block(signal, previous, dc) / 4, -402653184, 402653184);
        carrier += increment;
        modulator += modInc;
    }
    controls[0] = speed;
    controls[1] = tension;
    controls[2] = drift;
    controls[3] = glare;
    voice.carrier = carrier;
    voice.modulator = modulator;
    voice.transientAge = age;
    voice.feedback[0] = feedback0;
    voice.feedback[1] = feedback1;
    voice.prev = previous;
    voice.dc = dc;
}
static inline void render_dynamic(Voice &voice, Shared &shared, u32 owner, opxy::Output output,
                                  opxy::Parameters parameters, opxy::Pitch pitch) {
    const bool speedChanging = voice.controls[0] != parameters[0];
    const bool otherThreeChanging = voice.controls[1] != parameters[1] &&
                                    voice.controls[2] != parameters[2] &&
                                    voice.controls[3] != parameters[3];
    const bool allChanging = speedChanging && otherThreeChanging;
    if (voice.transientAge >= 3528 && (speedChanging || otherThreeChanging)) {
        if (allChanging)
            render_planned_sustain<true>(voice, shared, owner, output, parameters, pitch);
        else
            render_planned_sustain<false>(voice, shared, owner, output, parameters, pitch);
    } else if (allChanging)
        render_dynamic_impl<true>(voice, shared, owner, output, parameters, pitch);
    else
        render_dynamic_impl<false>(voice, shared, owner, output, parameters, pitch);
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
