#include "dsp.h"
#include <cstdio>
#include <cstdlib>
using namespace nightdrive;
static void require(bool ok) {
    if (!ok) {
        std::fputs("FAIL exact clipped saturation\n", stderr);
        std::exit(1);
    }
}
int main() {
    const auto expected = dsp::mix(tanh_lut[1023], tanh_lut[1024], opxy::i32(0x7ffffu << 12));
    require(expected == tanh_clipped);
    const int signs[] = {1, -1};
    opxy::u32 random = 17;
    unsigned checked = 0;
    for (unsigned d = 0; d <= 128; ++d) {
        const auto drive = opxy::i32((2147483647ull * d) / 128);
        const auto squared = dsp::multiply(drive, drive);
        const auto compensation = dsp::lut(drive_comp_lut, drive);
        const auto clipped = dsp::multiply(tanh_clipped, compensation);
        for (unsigned i = 0; i < 8192; ++i) {
            random = random * 1664525u + 1013904223u;
            const auto magnitude = opxy::i32(random >> 1);
            const auto sample = i & 1 ? -magnitude : magnitude;
            require(dsp::saturate(sample, drive, squared, compensation) ==
                    dsp::saturate(sample, drive, squared, compensation, clipped));
            ++checked;
        }
        // Find each drive's actual integer clamp threshold and check adjacent inputs.
        const auto position = [&](int x) {
            return (opxy::u32(x) >> 5) + (opxy::u32(dsp::multiply(x, squared)) >> 5) * 31u;
        };
        if (position(2147483647) >= 0x20000000u) {
            int lo = 0, hi = 2147483647;
            while (lo < hi) {
                const int middle = lo + (hi - lo) / 2;
                if (position(middle) >= 0x20000000u)
                    hi = middle;
                else
                    lo = middle + 1;
            }
            for (int delta = -2; delta <= 2; ++delta) {
                const auto wide = opxy::i64(lo) + delta;
                if (wide < 0 || wide > 2147483647)
                    continue;
                for (int sign : signs) {
                    const int x = int(wide) * sign;
                    require(dsp::saturate(x, drive, squared, compensation) ==
                            dsp::saturate(x, drive, squared, compensation, clipped));
                }
            }
        }
    }
    std::printf("PASS exact saturation: %u random samples and signed threshold neighborhoods\n",
                checked);
}
