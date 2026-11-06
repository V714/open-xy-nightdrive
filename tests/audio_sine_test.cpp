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
    opxy::u32 random = 17;
    for (unsigned i = 0; i < 2000000; ++i) {
        random = random * 1664525u + 1013904223u;
        const auto value = dsp::sine_audio(random);
        const opxy::i64 error = opxy::i64(value) - dsp::sine(random);
        require(error >= -450000 && error <= 450000, "phase quantization bound");
        require(dsp::sine_audio(random + 0x80000000u) == -value, "half-cycle symmetry");
    }
    std::puts("PASS audio sine: nodes, wrap, symmetry and 2 million phase-error bounds");
}
