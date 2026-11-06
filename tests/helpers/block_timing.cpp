// Host-only block replay diagnostic; excludes stock voice processing and device caches.
#include "engine.cpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace nightdrive;
struct Frame {
    Voice voice;
    opxy::i32 parameters[8];
    opxy::u32 pitch[64];
};
[[gnu::noinline]] static unsigned replay(const Frame &frame, unsigned block) {
    Voice voice = frame.voice;
    Shared shared{};
    opxy::i32 output[64];
    if (!block)
        start(voice, shared, 1);
    render(voice, shared, 1, output, frame.parameters, frame.pitch);
    return unsigned(output[block & 63]) + voice.carrier + voice.modulator;
}
int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    const unsigned repeats = unsigned(std::strtoul(argv[1], nullptr, 10));
    const int scenario = std::atoi(argv[2]), note = std::atoi(argv[3]);
    if (!repeats || scenario < 0 || scenario > 7 || note < 0 || note > 127)
        return 2;
    const opxy::u32 increment =
        opxy::u32(440.0 * std::pow(2.0, (note - 69) / 12.0) * 4294967296.0 / 44100.0);
    Voice voice{};
    Shared shared{};
    start(voice, shared, 1);
    std::vector<Frame> frames(1500);
    opxy::i32 output[64];
    for (unsigned b = 0; b < frames.size(); ++b) {
        Frame &frame = frames[b];
        frame.voice = voice;
        for (unsigned k = 0; k < 8; ++k)
            frame.parameters[k] = k < 4 ? 1073741823 : 0;
        if (scenario == 1) {
            const opxy::i32 defaults[] = {536870912, 536870912, 751619276, 536870912};
            std::copy(defaults, defaults + 4, frame.parameters);
        }
        if (scenario == 2)
            std::fill(frame.parameters, frame.parameters + 4, 2147483647);
        if (scenario == 3)
            frame.parameters[0] = 0;
        if (scenario == 4)
            frame.parameters[3] = 1879048191;
        if (scenario == 5)
            frame.parameters[3] = 2147483647;
        if (scenario == 6)
            frame.parameters[3] = 861644443;
        if (scenario == 7 && (b & 8))
            std::fill(frame.parameters, frame.parameters + 4, 2147483647);
        for (unsigned i = 0; i < 64; ++i)
            frame.pitch[i] = increment;
        render(voice, shared, 1, output, frame.parameters, frame.pitch);
    }
    volatile unsigned checksum = 0;
    std::vector<std::vector<double>> timings(frames.size());
    for (unsigned round = 0; round < 5; ++round) {
        for (unsigned offset = 0; offset < frames.size(); ++offset) {
            // Rotate ordering to reduce systematic warmup/frequency bias.
            const unsigned b = (offset + round * 307u) % frames.size();
            const auto begin = std::chrono::steady_clock::now();
            for (unsigned repeat = 0; repeat < repeats; ++repeat)
                checksum += replay(frames[b], b);
            const auto end = std::chrono::steady_clock::now();
            timings[b].push_back(std::chrono::duration<double, std::nano>(end - begin).count() /
                                 repeats);
        }
    }
    for (unsigned b = 0; b < frames.size(); ++b) {
        std::sort(timings[b].begin(), timings[b].end());
        std::printf("%u,%.6f\n", b, timings[b][2]);
    }
    std::fprintf(stderr, "checksum %u\n", checksum);
}
