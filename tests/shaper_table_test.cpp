#include "dsp.h"
#include <cstdio>
using namespace nightdrive;
int main() {
    // Packed interpolation requires nonnegative Q15 values and signed 16-bit slopes.
    for (unsigned row = 0; row <= (1u << shaper_drive_bits); ++row) {
        if (shaper_lut[row][0] != 0)
            return 1;
        for (unsigned col = 0; col < (1u << shaper_amplitude_bits); ++col) {
            const int a = shaper_lut[row][col], b = shaper_lut[row][col + 1];
            if (a > 32767 || b > 32767 || b < a || b - a > 32767)
                return 2;
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
                if (positive < 0 || negative != -positive || (!drive && positive != x))
                    return 3;
            }
        }
    }
    std::puts(
        "PASS packed shaper: slope bounds, grid boundaries, odd symmetry and exact zero Glare");
}
