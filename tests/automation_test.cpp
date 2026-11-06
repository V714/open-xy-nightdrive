#include "engine.cpp"
#include <cstdio>
#include <cstdlib>
using namespace nightdrive;

static void require(bool ok, const char *why) {
    if (!ok) {
        std::fprintf(stderr, "FAIL Nightdrive automation: %s\n", why);
        std::exit(1);
    }
}

static FILE *capture;
static void record(const Voice &voice, const opxy::i32 *output) {
    if (!capture)
        return;
    const opxy::u32 phase[] = {voice.carrier, voice.modulator, voice.ready, voice.transientAge};
    const opxy::i32 state[] = {voice.feedback[0], voice.feedback[1], voice.prev, voice.dc};
    require(std::fwrite(output, sizeof(*output) * 64, 1, capture) == 1 &&
                std::fwrite(phase, sizeof(phase), 1, capture) == 1 &&
                std::fwrite(voice.controls, sizeof(voice.controls), 1, capture) == 1 &&
                std::fwrite(state, sizeof(state), 1, capture) == 1,
            "could not write comparison capture");
}

int main(int argc, char **argv) {
    require(argc <= 2, "expected at most one capture path");
    if (argc == 2) {
        capture = std::fopen(argv[1], "wb");
        require(capture != nullptr, "could not open comparison capture");
    }
    const opxy::u32 bases[] = {180000, 6384029, 25498431, 203987449, 540000000, 1200000000};
    const opxy::i32 defaults[] = {536870912, 536870912, 751619276, 536870912};
    opxy::u32 random = 17;
    unsigned cases = 0;
    unsigned long long energy = 0;
    // Every nonempty subset, including transitions between sparse and dense automation.
    for (unsigned mask = 1; mask < 16; ++mask)
        for (unsigned pattern = 0; pattern < 5; ++pattern)
            for (auto base : bases)
                for (unsigned movingPitch = 0; movingPitch < 2; ++movingPitch) {
                    Voice voice{};
                    Shared shared{};
                    initialize(voice, shared, 1);
                    opxy::i32 parameters[8] = {}, output[64];
                    opxy::u32 pitch[64];
                    for (unsigned b = 0; b < 512; ++b) {
                        if (b % 127 == 126)
                            start(voice, shared, 1);
                        opxy::i32 previous[4];
                        const unsigned activeMask = pattern == 4 ? (b / 16 + mask) % 15 + 1 : mask;
                        for (unsigned k = 0; k < 4; ++k) {
                            previous[k] = voice.controls[k];
                            parameters[k] = defaults[k];
                            if (!(activeMask & (1u << k)))
                                continue;
                            random = random * 1664525u + 1013904223u;
                            if (pattern == 0)
                                parameters[k] = (b & 1) ? 2147483647 : 0;
                            else if (pattern == 1)
                                parameters[k] = opxy::i32(random >> 1);
                            else if (pattern == 2)
                                parameters[k] = opxy::i32(2147483647LL * (b % 65) / 64);
                            else if (pattern == 3)
                                parameters[k] = (b & 8) ? 2147483647 : 1879048191;
                            else
                                parameters[k] = (b & 8) ? 2147483647 : 0;
                        }
                        opxy::u32 expectedCarrier = voice.carrier;
                        for (unsigned i = 0; i < 64; ++i) {
                            pitch[i] = base + (movingPitch ? i * 320u : 0u);
                            expectedCarrier += pitch[i];
                        }
                        render(voice, shared, 1, output, parameters, pitch);
                        record(voice, output);
                        require(voice.carrier == expectedCarrier, "carrier phase discontinuity");
                        require(voice.transientAge <= 3528, "transient exceeded finite duration");
                        for (unsigned k = 0; k < 4; ++k) {
                            const auto lo =
                                previous[k] < parameters[k] ? previous[k] : parameters[k];
                            const auto hi =
                                previous[k] > parameters[k] ? previous[k] : parameters[k];
                            require(voice.controls[k] >= lo && voice.controls[k] <= hi,
                                    "smoothed control overshot its target");
                        }
                        for (unsigned k = 4; k < 8; ++k)
                            require(voice.controls[k] == 0, "unused control changed");
                        for (auto x : output) {
                            require(x >= -402653184 && x <= 402653184, "output peak exceeded");
                            energy += x < 0 ? -opxy::i64(x) : x;
                        }
                    }
                    // Automation must eventually return to the settled rendering path.
                    for (unsigned b = 0; b < 256; ++b) {
                        render(voice, shared, 1, output, parameters, pitch);
                        record(voice, output);
                    }
                    require(dsp::settled(voice.controls, parameters), "controls failed to settle");
                    ++cases;
                }
    require(energy > 1000000, "automation corpus is silent");
    if (capture)
        require(std::fclose(capture) == 0, "could not close comparison capture");
    std::printf("PASS Nightdrive automation: %u cases, all 15 control subsets, block-rate steps, "
                "random targets, ramps, 87.5/100%% transitions, pitch motion and retriggers\n",
                cases);
}
