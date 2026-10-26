// Exact audio and persistent-state corpus, run against both source versions.
#include "engine.cpp"
#include <cstdio>
#include <cstdlib>
using namespace nightdrive;
static FILE *file;
static unsigned blocks;
static void write_block(Voice &v, Shared &s, opxy::i32 *p, opxy::u32 *pitch, unsigned owner = 1) {
    opxy::i32 output[64];
    render(v, s, owner, output, p, pitch);
    // Explicit members exclude padding and any future derived caches.
    const opxy::u32 phase[] = {v.carrier, v.modulator, v.ready, v.transientAge};
    const opxy::i32 state[] = {v.feedback[0], v.feedback[1], v.prev, v.dc};
    if (std::fwrite(output, sizeof(output), 1, file) != 1 ||
        std::fwrite(phase, sizeof(phase), 1, file) != 1 ||
        std::fwrite(v.controls, sizeof(v.controls), 1, file) != 1 ||
        std::fwrite(state, sizeof(state), 1, file) != 1)
        std::exit(3);
    ++blocks;
}
int main(int argc, char **argv) {
    if (argc != 2 || !(file = std::fopen(argv[1], "wb")))
        return 2;
    const opxy::u32 pitches[] = {180000, 6384029, 25498431, 203987449, 540000000, 1200000000};
    const opxy::i32 defaults[] = {536870912, 536870912, 751619276, 536870912};
    opxy::u32 random = 17;
    unsigned cases = 0;
    for (auto base : pitches) {
        // Defaults, corners, individual nine-position sweeps and random patches.
        for (unsigned setting = 0; setting < 85; ++setting) {
            Voice v{};
            Shared s{};
            initialize(v, s, 1);
            opxy::i32 p[8] = {};
            opxy::u32 pitch[64];
            for (unsigned k = 0; k < 4; ++k) {
                p[k] = defaults[k];
                if (setting >= 1 && setting <= 16)
                    p[k] = ((setting - 1) & (1u << k)) ? 2147483647 : 0;
                else if (setting >= 17 && setting < 53 && (setting - 17) / 9 == k)
                    p[k] = opxy::i32((2147483647ull * ((setting - 17) % 9)) / 8);
                else if (setting >= 53) {
                    random = random * 1664525u + 1013904223u;
                    p[k] = opxy::i32(random >> 1);
                }
            }
            for (auto &x : pitch)
                x = base;
            for (unsigned b = 0; b < 128; ++b)
                write_block(v, s, p, pitch);
            // Retrigger the same voice after it has accumulated state.
            start(v, s, 1);
            for (unsigned b = 0; b < 64; ++b)
                write_block(v, s, p, pitch);
            ++cases;
        }
        Voice v{};
        Shared s{};
        initialize(v, s, 1);
        opxy::i32 p[8] = {};
        opxy::u32 pitch[64];
        for (unsigned b = 0; b < 1024; ++b) {
            if (b % 19 == 0)
                for (unsigned k = 0; k < 4; ++k) {
                    random = random * 1664525u + 1013904223u;
                    p[k] = b % 38 ? opxy::i32(random >> 1) : 2147483647 - p[k];
                }
            if (b % 127 == 0)
                start(v, s, 1);
            for (unsigned i = 0; i < 64; ++i)
                pitch[i] = base + (b % 3 ? i * 320u : 0u);
            write_block(v, s, p, pitch);
        }
        ++cases;
        start(v, s, 1);
        for (unsigned k = 0; k < 4; ++k)
            p[k] = defaults[k];
        for (auto &x : pitch)
            x = base;
        for (unsigned b = 0; b < 6891; ++b)
            write_block(v, s, p, pitch);
        ++cases;
    }
    // Alternate voices and cache keys, crossing zero, smoothing and pitch modes.
    Voice voices[3]{};
    Shared shared{};
    for (unsigned v = 0; v < 3; ++v)
        initialize(voices[v], shared, v + 1);
    for (unsigned b = 0; b < 2048; ++b) {
        for (unsigned owner = 0; owner < 3; ++owner) {
            Voice &v = voices[owner];
            if (b % 71 == owner)
                start(v, shared, owner + 1);
            opxy::i32 p[8] = {};
            opxy::u32 pitch[64];
            for (unsigned k = 0; k < 4; ++k)
                p[k] = (b / 128 + k + owner) % 3 == 0   ? 0
                       : (b / 128 + k + owner) % 3 == 1 ? 2147483647
                                                        : defaults[k];
            for (unsigned i = 0; i < 64; ++i)
                pitch[i] = pitches[(b / 64 + owner) % 6] + (b % 2 ? i * 320 : 0);
            write_block(v, shared, p, pitch, owner + 1);
        }
    }
    cases += 3;
    // Frequencies around both bandwidth boundaries, at three oscillator ratios.
    const opxy::i32 speeds[] = {536870912, 1610612735, 2147483647};
    const opxy::u32 limits[] = {0x06000000u, 0x30000000u};
    const int offsets[] = {-32, -1, 0, 1, 32};
    const opxy::i32 glares[] = {0, 2147483647};
    for (auto speed : speeds) {
        const double ratio = dsp::operator_ratio(speed) / 67108864.;
        for (auto limit : limits) {
            const auto center = opxy::u32(limit / ratio);
            for (auto offset : offsets) {
                for (auto glare : glares) {
                    Voice v{};
                    Shared s{};
                    initialize(v, s, 1);
                    opxy::i32 p[8] = {speed, 536870912, 751619276, glare};
                    opxy::u32 pitch[64];
                    for (auto &x : pitch)
                        x = opxy::u32(center + offset);
                    for (unsigned b = 0; b < 128; ++b)
                        write_block(v, s, p, pitch);
                    ++cases;
                }
            }
            Voice v{};
            Shared s{};
            initialize(v, s, 1);
            opxy::i32 p[8] = {speed, 536870912, 751619276, 536870912};
            opxy::u32 pitch[64];
            for (unsigned b = 0; b < 128; ++b) {
                for (unsigned i = 0; i < 64; ++i)
                    pitch[i] = opxy::u32(center + offsets[(i + b) % 5]);
                write_block(v, s, p, pitch);
            }
            ++cases;
        }
    }
    if (std::fclose(file))
        return 3;
    std::printf("%u cases; %u blocks; %u exact audio samples plus persistent state\n", cases,
                blocks, blocks * 64);
}
