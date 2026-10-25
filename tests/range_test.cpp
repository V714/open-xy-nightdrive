#include "engine.cpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace nightdrive;
static void require(bool ok, const char *why) {
    if (!ok) {
        std::fprintf(stderr, "FAIL Nightdrive ranges: %s\n", why);
        std::exit(1);
    }
}
int main() {
    const unsigned points[] = {0, 5, 10, 25, 50, 75, 100};
    const double ratios[] = {.25, .5, 1, 2, 4, 8, 16};
    for (unsigned j = 0; j < 7; ++j) {
        int control = int(points[j] * 2147483647.0 / 100);
        require(std::fabs(dsp::operator_ratio(control) / 67108864.0 - ratios[j]) < .001,
                "ratio anchor differs");
    }
    int previousRatio = -1;
    for (unsigned k = 0; k <= 10000; ++k) {
        int ratio = dsp::operator_ratio(int(k * 2147483647.0 / 10000));
        require(ratio >= previousRatio, "ratio discontinuity");
        previousRatio = ratio;
    }
    int previousFb = -1, previousMod = -1;
    for (unsigned k = 0; k <= 1000; ++k) {
        int c = int(k * 2147483647.0 / 1000), fb = dsp::feedback_amount(c),
            mod = dsp::modulation_amount(c);
        require(fb >= previousFb && mod >= previousMod, "range not monotonic");
        previousFb = fb;
        previousMod = mod;
        if (k <= 690) {
            require(fb == dsp::multiply(c, 581030184), "low feedback range changed");
            require(mod == dsp::multiply(c, 1367130551), "low modulation range changed");
        }
    }
    double fb = dsp::feedback_amount(2147483647) / 4294967296. * (2 * 3.141592653589793),
           mod = dsp::modulation_amount(2147483647) / 2147483648. * (2 * 3.141592653589793);
    require(std::fabs(fb - 3.0) < .000001 && std::fabs(mod - 6) < .000001, "wrong maximum radians");
    std::puts(
        "PASS Nightdrive ranges: monotonic, lower 69% identical, feedback 3rad, modulation 6rad");
}
