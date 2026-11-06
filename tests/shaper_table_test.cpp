#include "dsp.h"
#include <cstdio>
using namespace nightdrive;
int main() {
    const unsigned fractions[] = {0u, 1u, 32767u, 65535u};
    // Packed interpolation requires nonnegative Q15 values and signed 16-bit slopes.
    for (unsigned row = 0; row <= (1u << shaper_drive_bits); ++row) {
        if (shaper_lut[row][0] != 0)
            return 1;
        for (unsigned col = 0; col < (1u << shaper_amplitude_bits); ++col) {
            const int a = shaper_lut[row][col], b = shaper_lut[row][col + 1];
            if (a > 32767 || b > 32767 || b < a || b - a > 32767)
                return 2;
            const auto segment = shaper_segments[row][col];
            if ((segment & 65535u) != unsigned(a) || (segment >> 16) != unsigned(b - a))
                return 4;
            for (unsigned fraction : fractions) {
                const unsigned shift = 31 - shaper_amplitude_bits;
                const opxy::u32 amplitude = (col << shift) | (fraction << (shift - 16));
                if (dsp::shaper_row(row, amplitude) != a * 65536 + (b - a) * int(fraction))
                    return 5;
            }
        }
    }
    // Exercise grid boundaries and both signs, including the last legal column.
    for (unsigned row = 0; row <= 32; ++row) {
        const opxy::i32 drive = opxy::i32(opxy::i64(row) * 2147483647 / 32);
        for (unsigned col = 0; col <= 128; ++col) {
            const opxy::i32 center = opxy::i32(opxy::i64(col) * 2147483647 / 128);
            for (int offset = -1; offset <= 1; ++offset) {
                const opxy::i64 candidate = opxy::i64(center) + offset;
                if (candidate < 0 || candidate > 2147483647)
                    continue;
                const opxy::i32 x = opxy::i32(candidate);
                const auto positive = dsp::saturate_approx(x, drive);
                const auto negative = dsp::saturate_approx(-x, drive);
                const auto prepared = dsp::prepare_shaper(drive);
                if (dsp::apply_shaper(x, prepared) != positive ||
                    dsp::apply_shaper(-x, prepared) != negative)
                    return 6;
                if (positive < 0 || negative != -positive || (!drive && positive != x))
                    return 3;
            }
        }
    }
    opxy::u32 random = 17;
    for (unsigned sample = 0; sample < 65536; ++sample) {
        random = random * 1664525u + 1013904223u;
        const opxy::i32 drive = opxy::i32(random >> 1);
        random = random * 1664525u + 1013904223u;
        const opxy::i32 x = opxy::i32(random >> 1);
        const auto prepared = dsp::prepare_shaper(drive);
        if (dsp::apply_shaper(x, prepared) != dsp::saturate_approx(x, drive) ||
            dsp::apply_shaper(-x, prepared) != dsp::saturate_approx(-x, drive))
            return 7;
    }
    std::puts("PASS packed shaper: exact segment bases/slopes/interpolation, prepared rows, grid "
              "boundaries, odd "
              "symmetry and exact zero Glare");
}
