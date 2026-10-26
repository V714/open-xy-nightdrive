#include "engine.cpp"
#include <cstdio>
#include <cstdlib>
using namespace nightdrive;
int main(int argc, char **argv) {
    if (argc != 12 && argc != 13)
        return 2;
    Voice v{};
    Shared s{};
    initialize(v, s, 1);
    start(v, s, 1);
    opxy::i32 p[8], out[64];
    opxy::u32 pitch[64];
    for (unsigned j = 0; j < 8; ++j)
        p[j] = opxy::i32(std::atof(argv[j + 1]) * 2147483647.0);
    for (auto &x : pitch)
        x = opxy::u32(std::strtoul(argv[9], nullptr, 10));
    const unsigned mode = argc == 13 ? unsigned(std::strtoul(argv[12], nullptr, 10)) : 0;
    unsigned blocks = unsigned(std::strtoul(argv[10], nullptr, 10));
    FILE *f = std::fopen(argv[11], "wb");
    if (!f)
        return 3;
    for (unsigned b = 0; b < blocks; ++b) {
        if (mode) {
            const unsigned period = mode == 1 ? 2048 : 128, step = b % period;
            const unsigned triangle = step <= period / 2 ? step : period - step;
            p[3] = opxy::i32(opxy::i64(triangle) * 2147483647 / (period / 2));
        }
        render(v, s, 1, out, p, pitch);
        if (std::fwrite(out, sizeof(out), 1, f) != 1)
            return 4;
    }
    return std::fclose(f) == 0 ? 0 : 5;
}
