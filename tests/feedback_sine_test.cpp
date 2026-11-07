#include "dsp.h"
#include <cstdio>
#include <cstdlib>
using namespace nightdrive;
static opxy::i32 reference(opxy::u32 phase) {
    const auto packed = sine_packed_lut[phase >> 21];
    const auto a = opxy::i32(opxy::i16(packed & 65535u)) * 32768;
    const auto d = opxy::i32(opxy::i16(packed >> 16)) * 32768;
    return a + dsp::multiply(d, opxy::i32((phase & 0x1fffffu) << 10));
}
int main() {
    const opxy::u32 offsets[] = {0,  1,  2,        31,        32,        63,
                                 64, 65, 0xfffffu, 0x100000u, 0x1ffffeu, 0x1fffffu};
    for (unsigned row = 0; row < 2048; ++row)
        for (auto offset : offsets) {
            const auto phase = (opxy::u32(row) << 21) + offset;
            if (dsp::sine(phase) != reference(phase))
                return 1;
        }
    opxy::u32 random = 17;
    for (unsigned i = 0; i < 4000000; ++i) {
        random = random * 1664525u + 1013904223u;
        if (dsp::sine(random) != reference(random))
            return 2;
    }
    std::puts("PASS exact quarter feedback: every segment boundary and 4 million phases");
}
