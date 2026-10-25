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
static inline void render(Voice &voice, Shared &, u32, opxy::Output output,
                          opxy::Parameters parameters, opxy::Pitch pitch) {
    if (!voice.ready) {
        for (unsigned k = 0; k < 4; ++k)
            voice.controls[k] = parameters[k] < 0 ? 0 : parameters[k];
        voice.ready = 1;
    }
    i32 *controls = voice.controls;
    bool changing = !settled(controls, parameters);
    i32 ratio = operator_ratio(controls[0]);
    i32 feedbackAmount = feedback_amount(controls[1]),
        modulationAmount = modulation_amount(controls[2]);
    for (unsigned i = 0; i < 64; ++i) {
        if (changing) {
            tick_controls(controls, parameters);
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
            signal += multiply(signal, multiply(controls[3], envelope)) / 2;
        }
        signal = saturate(signal, controls[3]);
        if (voice.transientAge < 3528)
            ++voice.transientAge;
        output[i] = clamp(dc_block(signal, voice.prev, voice.dc) / 4, -402653184, 402653184);
        voice.carrier += increment;
        voice.modulator += modInc;
    }
}
} // namespace nightdrive
