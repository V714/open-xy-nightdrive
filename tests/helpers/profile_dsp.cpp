// Host-only component timings; these are not measured Blackfin cycles.
#include "engine.cpp"
#include <chrono>
#include <cstdio>
using namespace nightdrive;
template <class Function> static double measure(Function function) {
    volatile opxy::i32 sink = 0;
    double best = 1e30;
    for (unsigned run = 0; run < 5; ++run) {
        const auto begin = std::chrono::steady_clock::now();
        for (unsigned block = 0; block < 20000; ++block)
            sink = function(block);
        const double elapsed =
            std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin)
                .count();
        if (elapsed < best)
            best = elapsed;
    }
    return best / 20000;
}
int main() {
    opxy::i32 samples[64];
    opxy::u32 random = 17;
    for (unsigned i = 0; i < 64; ++i) {
        random = random * 1664525u + 1013904223u;
        samples[i] = opxy::i32(random >> 1) - 1073741824;
    }
    std::printf("sine_ns_per_64_calls %.3f\n", measure([&](unsigned b) {
                    opxy::i32 sum = 0;
                    for (unsigned i = 0; i < 64; ++i)
                        sum ^= dsp::sine(opxy::u32(samples[i]) + b * 65537u);
                    return sum;
                }));
    const opxy::i32 drives[] = {536870912, 2147483647};
    for (opxy::i32 drive : drives) {
        const auto squared = dsp::multiply(drive, drive);
        const auto compensation = dsp::lut(drive_comp_lut, drive);
        std::printf("saturate_drive_%d_ns_per_64_calls %.3f\n", drive, measure([&](unsigned b) {
                        opxy::i32 sum = 0;
                        for (unsigned i = 0; i < 64; ++i)
                            sum ^=
                                dsp::saturate(samples[(i + b) & 63], drive, squared, compensation);
                        return sum;
                    }));
    }
    std::printf(
        "control_coefficients_ns_per_64_calls %.3f\n", measure([&](unsigned b) {
            opxy::i32 sum = 0;
            for (unsigned i = 0; i < 64; ++i) {
                const auto c = opxy::i32((opxy::u32(samples[(i + b) & 63]) >> 1) & 0x7fffffffu);
                sum ^= dsp::operator_ratio(c) ^ dsp::feedback_amount(c) ^ dsp::modulation_amount(c);
            }
            return sum;
        }));
}
