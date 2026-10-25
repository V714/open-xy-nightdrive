#include "display.cpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace nightdrive;
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    FILE *file = std::fopen(argv[1], "w");
    if (!file)
        return 3;
    const double times[] = {0,   .045, .11, .75, 1.6, .78, .80, .82, .84,
                            .86, .88,  .90, .92, .94, .96, .98, 1.0, 1.02};
    const unsigned frameCount = sizeof(times) / sizeof(times[0]);
    std::fputs("[", file);
    for (unsigned k = 0; k < frameCount; ++k) {
        DisplayState s{};
        Frame f{};
        opxy::draw::Input in{};
        in.held_notes = 1;
        in.slots[0] = int((3. / 7) * 2147483647.);
        in.slots[3] = int((5. / 15) * 2147483647.);
        display_reset(s);
        display(s, in, f);
        double animationTime = 0;
        unsigned target = unsigned(times[k] * 1024 + .5), elapsed = 0;
        while (elapsed < target) {
            unsigned step = target - elapsed;
            if (step > 34)
                step = 34;
            in.elapsed = int(step);
            unsigned beforeMotion = s.motion;
            unsigned beforeClock = s.clock;
            display(s, in, f);
            animationTime +=
                ((beforeClock + step * (beforeMotion + s.motion) / 2) / 1024u) / 1024.0;
            elapsed += step;
        }
        // Flush a partial display tick without advancing time or animation.
        if (s.clock) {
            s.previous[0] ^= 1;
            in.elapsed = 0;
            display(s, in, f);
        }
        if (!opxy::draw::valid(f))
            return 4;
        std::fprintf(file, "%s{\"time\":%.6f,\"animation_time\":%.6f,\"nodes\":[", k ? "," : "",
                     double(target) / 1024, animationTime);
        for (unsigned j = 0; j < art_nodes; ++j) {
            const auto &n = f.nodes[j];
            std::fprintf(file, "%s[%d,%d,%d,%u,%u]", j ? "," : "", n.x, n.y, n.scale, n.shade,
                         unsigned(n.paint));
        }
        std::fputs("],\"paths\":[", file);
        for (unsigned p = 0; p < 4; ++p) {
            std::fputs(p ? ",[" : "[", file);
            for (unsigned j = 0; j < 64; ++j)
                std::fprintf(file, "%s[%d,%d]", j ? "," : "", f.paths[p].points[j].x,
                             f.paths[p].points[j].y);
            std::fputs("]", file);
        }
        std::fputs("]}", file);
    }
    std::fputs("]\n", file);
    return std::fclose(file) == 0 ? 0 : 5;
}
