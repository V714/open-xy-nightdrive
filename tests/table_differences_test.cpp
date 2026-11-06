#include "dsp.h"
#include <cstdio>
#include <cstdlib>
using namespace nightdrive;
int main() {
    for (unsigned i = 0; i < 2048; ++i)
        if (sine_delta_lut[i] != int(audio_sine_lut[(i + 1) & 2047]) - audio_sine_lut[i])
            return 1;
    for (unsigned i = 0; i < 2048; ++i)
        if (opxy::i16(sine_packed_lut[i] & 65535u) != audio_sine_lut[i] ||
            opxy::i16(sine_packed_lut[i] >> 16) != sine_delta_lut[i])
            return 3;
    for (unsigned i = 0; i < 1024; ++i)
        if (tanh_delta_lut[i] != tanh_lut[i + 1] - tanh_lut[i])
            return 2;
    // The zero-boost shortcut must retain the former result on every table boundary.
    for (unsigned row = 0; row <= 256; ++row)
        for (int offset = -2; offset <= 2; ++offset) {
            const opxy::i64 candidate = opxy::i64(row) * 8388608 + offset;
            if (candidate < 0 || candidate > 2147483647)
                continue;
            const auto control = opxy::i32(candidate);
            if (dsp::modulation_amount(control) !=
                dsp::multiply(control, 1367130551) + dsp::lut(modulation_boost_lut, control))
                return 4;
        }
    std::puts("PASS exact precomputed differences: all 2048 sine and 1024 tanh segments");
}
