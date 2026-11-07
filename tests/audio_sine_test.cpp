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
static opxy::i32 reference_audio_sine(opxy::u32 phase) {
    const opxy::u32 half = phase & 0x7fffffffu;
    const opxy::u32 folded = half <= 0x40000000u ? half : 0x80000000u - half;
    const opxy::i32 x = folded == 0x40000000u ? 2147483647 : opxy::i32(folded << 1);
    const auto squared = dsp::multiply(x, x);
    const auto polynomial =
        audio_sine_a + dsp::multiply(squared, audio_sine_b + dsp::multiply(squared, audio_sine_c));
    const auto signal = dsp::multiply(x, polynomial);
    return phase & 0x80000000u ? -signal : signal;
}
int main() {
    require(dsp::sine_audio(0) == 0 && dsp::sine_audio(0x80000000u) == 0, "zero crossings");
    for (unsigned quadrant = 0; quadrant < 4; ++quadrant)
        for (int offset = -32; offset <= 32; ++offset) {
            const auto phase = (opxy::u32(quadrant) << 30) + opxy::u32(offset);
            require(dsp::sine_audio(phase) == reference_audio_sine(phase), "quadrant boundaries");
        }
    opxy::u32 random = 17;
    for (unsigned i = 0; i < 2000000; ++i) {
        random = random * 1664525u + 1013904223u;
        const auto value = dsp::sine_audio(random);
        opxy::i32 pairedA, pairedB;
        const auto other = random * 1664525u + 1013904223u;
        dsp::sine_audio_pair(random, other, pairedA, pairedB);
        require(pairedA == value && pairedB == dsp::sine_audio(other), "paired polynomial differs");
        require(value == reference_audio_sine(random), "branch-free polynomial differs");
        const opxy::i64 error = opxy::i64(value) - dsp::sine(random);
        require(error >= -150000 && error <= 150000, "polynomial approximation bound");
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
    std::puts("PASS audio sine: branch-free equivalence, wrap, symmetry, 2 million phases and "
              "blend rounding");
}
