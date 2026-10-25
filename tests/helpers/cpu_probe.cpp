#include "engine.cpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
using namespace nightdrive;
int main(int argc, char **argv) {
    if (argc != 10)
        return 2;
    Voice v{};
    Shared shared{};
    opxy::i32 p[8], out[64];
    opxy::u32 pitch[64];
    for (unsigned j = 0; j < 8; ++j)
        p[j] = int(std::atof(argv[j + 1]) * 2147483647.0);
    for (auto &x : pitch)
        x = 6384029;
    unsigned blocks = unsigned(std::strtoul(argv[9], nullptr, 10));
    volatile unsigned checksum = 0;
    auto begin = std::chrono::steady_clock::now();
    for (unsigned b = 0; b < blocks; ++b) {
        if (b % 344u == 0)
            start(v, shared, 1);
        render(v, shared, 1, out, p, pitch);
        checksum += unsigned(out[b & 63]);
    }
    auto end = std::chrono::steady_clock::now();
    std::printf("%.9f %u\n", std::chrono::duration<double>(end - begin).count(), checksum);
}
