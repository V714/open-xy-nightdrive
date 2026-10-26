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
    std::puts("PASS exact precomputed differences: all 2048 sine and 1024 tanh segments");
}
