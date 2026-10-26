#include "engine.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace nightdrive;
static void require(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL response: %s\n", message);
        std::exit(1);
    }
}
int main() {
    int lastFeedback = -1, lastPunch = -1;
    for (unsigned n = 0; n <= 10000; ++n) {
        const int c = int(n * 2147483647.0 / 10000);
        const double x = c / 2147483647.0;
        const int feedback = dsp::feedback_amount(c), punch = dsp::punch_amount(c);
        require(feedback >= lastFeedback && punch >= lastPunch, "nonmonotonic curve");
        require(punch >= c && punch <= 2147483647, "Punch range");
        require(std::fabs(punch / 2147483647.0 - (x + .15 * x * (1 - x))) < 6e-7,
                "Punch differs from the mild polynomial curve");
        const double boost =
            (feedback - dsp::multiply(c, 581030184)) / 4294967296. * (2 * 3.141592653589793);
        // Bound the piecewise-linear 257-point rational LUT's interpolation error.
        require(std::fabs(boost - 2.15 * x / (16 - 15 * x)) < .0021,
                "feedback differs from rational curve");
        lastFeedback = feedback;
        lastPunch = punch;
    }
    require(dsp::punch_amount(0) == 0 && dsp::punch_amount(2147483647) == 2147483647,
            "Punch endpoints changed");
    require(dsp::feedback_amount(0) == 0 && dsp::feedback_amount(2147483647) == 2050695520,
            "exact feedback endpoints changed");
    const int onePercent = 21474836;
    require(dsp::feedback_amount(onePercent) > dsp::multiply(onePercent, 581030184),
            "no boost at one percent");
    // Tiny values and the uppermost Q31 words must remain monotonic and bounded.
    for (int c = 0; c < 32; ++c) {
        require(dsp::punch_amount(c + 1) >= dsp::punch_amount(c), "low-end jump");
        require(dsp::feedback_amount(2147483647 - c) >= dsp::feedback_amount(2147483646 - c),
                "upper-end jump");
    }
    std::puts(
        "PASS response: early rational feedback, mild Punch, monotonicity and exact endpoints");
}
