#include "dsp.h"
#include <cstdio>
#include <cstdlib>
using namespace nightdrive;
static void require(bool value, const char *reason) {
    if (!value) {
        std::fprintf(stderr, "FAIL audio sine: %s\n", reason);
        std::exit(1);
    }
}
int main() {
    require(dsp::sine_audio(0) == 0 && dsp::sine_audio(0x80000000u) == 0, "zero crossings");
    require(dsp::sine_audio(0x40000000u) == 32760 * 32768 &&
                dsp::sine_audio(0xc0000000u) == -32760 * 32768,
            "quarter-wave peaks");
    for (unsigned i = 0; i <= 512; ++i)
        require(audio_sine_quarter_lut[i * 4] == audio_sine_lut[i], "original aligned nodes");
    for (unsigned position = 0; position < 8192; ++position) {
        unsigned index = position & 2047u;
        if (position & 2048u)
            index = 2048u - index;
        const auto quarter =
            position & 4096u ? -audio_sine_quarter_lut[index] : audio_sine_quarter_lut[index];
        require(audio_sine_full_lut[position] == quarter, "full and quarter tables differ");
        require(dsp::sine_audio(opxy::u32(position) << 19) == quarter * 32768,
                "full-cycle phase indexing");
    }
    opxy::u32 random = 17;
    for (unsigned i = 0; i < 2000000; ++i) {
        random = random * 1664525u + 1013904223u;
        const auto value = dsp::sine_audio(random);
        const opxy::i64 error = opxy::i64(value) - dsp::sine(random);
        require(error >= -450000 && error <= 450000, "phase quantization bound");
        require(dsp::sine_audio(random + 0x80000000u) == -value, "half-cycle symmetry");
    }
    for (unsigned i = 0; i < 100000; ++i) {
        random = random * 1664525u + 1013904223u;
        const auto a = dsp::sine(random);
        random = random * 1664525u + 1013904223u;
        const auto b = dsp::sine_audio(random);
        const auto previous = a - dsp::multiply(a, 429496729) + dsp::multiply(b, 429496729);
        const auto candidate = dsp::mix(a, b, 429496729);
        require(candidate == previous || candidate == previous - 1, "one-LSB blend rounding bound");
    }
    std::puts("PASS audio sine: nodes, wrap, symmetry, 2 million phases and blend rounding");
}
