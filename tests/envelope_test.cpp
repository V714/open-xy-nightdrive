#include "engine.cpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace nightdrive;
static void require(bool ok, const char *why) {
    if (!ok) {
        std::fprintf(stderr, "FAIL nightdrive ADSR: %s\n", why);
        std::exit(1);
    }
}
int main() {
    Voice v{};
    Shared s{};
    opxy::i32 p[8] = {0, 0, 0, 0, 0, 0, 0, 0}, out[64];
    opxy::u32 pitch[64];
    for (auto &x : pitch)
        x = 6384029;
    start(v, s, 1);
    double sum = 0, late = 0;
    for (unsigned b = 0; b < 6891; ++b) {
        render(v, s, 1, out, p, pitch);
        if (b < 690 || b > 6200)
            for (auto x : out) {
                double y = x / 2147483648.;
                if (b < 690)
                    sum += y * y;
                else
                    late += y * y;
            }
    }
    double rms = std::sqrt(late / (690 * 64));
    require(rms > .085 && rms < .091, "clean sine differs from reference SDK -18dB peak level");
    require(late / sum > .98 && late / sum < 1.02, "held note decays without stock ADSR");
    // A note starts fresh finite timbre transients, even after an earlier sustain.
    require(v.transientAge == 3528, "transient must finish and stay finished");
    Voice attackVoice{};
    start(attackVoice, s, 2);
    require(attackVoice.transientAge == 0, "retrigger did not reset transient");
    for (unsigned n = 0; n < 56; ++n)
        render(attackVoice, s, 2, out, p, pitch);
    require(attackVoice.transientAge == 3528, "transient timing differs from 80ms");
    auto phase = v.carrier;
    for (unsigned k = 0; k < 4; ++k)
        p[k] = 2147483647;
    for (unsigned n = 0; n < 256; ++n) {
        render(v, s, 1, out, p, pitch);
        phase += 6384029u * 64u;
        require(v.carrier == phase, "parameter change reset carrier");
    }
    for (unsigned k = 0; k < 4; ++k)
        p[k] = 0;
    for (unsigned n = 0; n < 256; ++n)
        render(v, s, 1, out, p, pitch);
    for (unsigned k = 0; k < 4; ++k)
        require(v.controls[k] == 0, "control did not settle");
    require(dsp::saturate(12345678, 0) == 12345678, "zero drive changes signal");
    std::puts("PASS nightdrive: ten-second sustain, stock-reference clean level, phase continuity, "
              "exact zero and drive bypass");
}
