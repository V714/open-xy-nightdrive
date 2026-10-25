#include "engine.cpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace nightdrive;
static void require(bool b, const char *text) {
    if (!b) {
        std::fprintf(stderr, "FAIL: %s\n", text);
        std::exit(1);
    }
}
static const int defaults[8] = {1073741823, 536870912, 751619276, 536870912, 0, 0, 0, 0};
int main(int argc, char **argv) {
    static Voice voices[256]{};
    static Shared shared{};
    opxy::i32 output[64], parameters[8] = {};
    opxy::u32 pitch[64];
    for (unsigned j = 0; j < 256; ++j)
        initialize(voices[j], shared, j + 1);
    for (unsigned j = 0; j < 256; ++j) {
        start(voices[j], shared, j + 1);
        for (auto &p : pitch)
            p = 6384029;
        render(voices[j], shared, j + 1, output, parameters, pitch);
        destroy(voices[j], shared, j + 1);
    }
    unsigned random = 17;
    long long total = 0;
    long long energy = 0;
    int peak = 0;
    for (unsigned mode = 0; mode < 82; ++mode) {
        for (unsigned k = 0; k < 8; ++k) {
            random = random * 1664525u + 1013904223u;
            parameters[k] = mode == 0   ? 0
                            : mode == 1 ? 2147483647
                            : mode < 18 ? 1073741823
                                        : int(random >> 1);
        }
        if (mode >= 2 && mode < 18)
            parameters[(mode - 2) / 2] = (mode & 1) ? 2147483647 : 0;
        for (unsigned note = 0; note < 6; ++note) {
            const unsigned incs[6] = {180000, 6384029, 25498431, 203987449, 540000000, 1200000000};
            start(voices[255], shared, 256);
            for (auto &p : pitch)
                p = incs[note];
            for (unsigned block = 0; block < 128; ++block) {
                if (block == 48) {
                    for (unsigned k = 0; k < 8; ++k)
                        parameters[k] = 2147483647 - parameters[k];
                }
                render(voices[255], shared, 256, output, parameters, pitch);
                for (auto x : output) {
                    require(x >= -402653184 && x <= 402653184, "peak");
                    total += x;
                    energy += std::abs(x) / 1024;
                    if (std::abs(x) > peak)
                        peak = std::abs(x);
                }
            }
        }
    }
    require(energy > 1000000, "silence");
    // Constant high drive/resonance long run and sample-varying pitch, no timing benchmark.
    for (auto &p : parameters)
        p = 2147483647;
    start(voices[127], shared, 128);
    for (unsigned b = 0; b < 4096; ++b) {
        for (unsigned j = 0; j < 64; ++j)
            pitch[j] = 6384029 + (b % 2048) * 9000 + j * 320;
        render(voices[127], shared, 128, output, parameters, pitch);
    }
    if (argc > 1) {
        FILE *f = std::fopen(argv[1], "wb");
        require(f, "open audio output");
        const unsigned notes[] = {6384029, 7589725, 8518815, 10130914};
        for (unsigned n = 0; n < 8; ++n) {
            for (unsigned j = 0; j < 8; ++j)
                parameters[j] = defaults[j];
            start(voices[0], shared, 1);
            for (auto &p : pitch)
                p = notes[n % 4];
            for (unsigned b = 0; b < 517; ++b) {
                render(voices[0], shared, 1, output, parameters, pitch);
                std::fwrite(output, sizeof(output), 1, f);
            }
        }
        std::fclose(f);
    }
    std::printf("PASS nightdrive: 256 owners, 82 settings, 6 pitches, modulation, long-run; peak "
                "%d, checksum %lld\n",
                peak, total);
}
