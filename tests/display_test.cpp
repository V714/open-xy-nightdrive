#include "display.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
using namespace nightdrive;
static void require(bool ok, const char *why) {
    if (!ok) {
        std::fprintf(stderr, "FAIL nightdrive display: %s\n", why);
        std::exit(1);
    }
}
static uint32_t geometry(const Frame &f) {
    uint32_t h = 2166136261u;
    for (const auto &p : f.paths)
        for (const auto &q : p.points) {
            h = (h ^ uint32_t(q.x)) * 16777619u;
            h = (h ^ uint32_t(q.y)) * 16777619u;
        }
    for (const auto &n : f.nodes) {
        h = (h ^ uint32_t(n.x)) * 16777619u;
        h = (h ^ uint32_t(n.y)) * 16777619u;
        h = (h ^ uint32_t(n.scale)) * 16777619u;
        h = (h ^ n.shade) * 16777619u;
    }
    return h;
}
int main() {
    DisplayState s{};
    Frame f{};
    opxy::draw::Input in{};
    in.reduced_motion = 1;
    display_reset(s);
    display(s, in, f);
    require(opxy::draw::valid(f), "initial frame");
    for (unsigned k = 1; k < 4; ++k) {
        auto a = geometry(f);
        in.slots[k] = 2147483647;
        display(s, in, f);
        require(a != geometry(f), "inactive pose/window control");
        in.slots[k] = 0;
        display(s, in, f);
    }
    auto a = geometry(f);
    in.slots[4] = 2147483647;
    display(s, in, f);
    require(a == geometry(f), "unused slot changes the scene");
    in.slots[1] = 2147483647;
    display(s, in, f);
    require(f.nodes[car_nodes[0]].x == 29860 && f.nodes[car_nodes[0]].y == -21604,
            "forward road basis");
    in.slots[1] = 0;
    in.slots[2] = 2147483647;
    display(s, in, f);
    require(f.nodes[car_nodes[0]].x < 0 && f.nodes[car_nodes[0]].y < 0, "leftward lane basis");
    in.slots[3] = 2147483647;
    display(s, in, f);
    for (unsigned n : window_nodes)
        require(f.nodes[n].shade == 15 && f.nodes[n].paint == opxy::draw::fill_and_stroke,
                "window white endpoint");
    unsigned dynamicPaint = 0;
    for (const auto &n : f.nodes)
        dynamicPaint += n.paint != opxy::draw::keep;
    require(dynamicPaint == 4, "paint changed more than four windows");
    a = geometry(f);
    in.elapsed = 102;
    for (unsigned n = 0; n < 100; ++n)
        display(s, in, f);
    require(a == geometry(f), "reduced motion animates");
    DisplayState slow{}, fast{};
    Frame sf{}, ff{};
    opxy::draw::Input si{}, fi{};
    si.elapsed = fi.elapsed = 34;
    si.held_notes = fi.held_notes = 1;
    fi.slots[0] = 2147483647;
    display_reset(slow);
    display_reset(fast);
    display(slow, si, sf);
    display(fast, fi, ff);
    require(fast.road == slow.road * 8u &&
                fast.lamp - 0x80000000u == (slow.lamp - 0x80000000u) * 8u &&
                fast.wheel == slow.wheel * 8u,
            "shared 0.25x..2x speed mapping");
    require(sf.nodes[lamp_node].x != opxy::draw::hidden_x * 1024, "lamp is missing at startup");
    bool sawLampMove = false, sawWheelChange = false;
    auto lampX = sf.nodes[lamp_node].x;
    unsigned oldPose = 99;
    for (unsigned step = 0; step < 120; ++step) {
        display(slow, si, sf);
        unsigned visible = 0, pose = 99;
        for (unsigned j = 0; j < 7; ++j)
            if (sf.nodes[wheel_sprite_nodes[0][j]].x != opxy::draw::hidden_x * 1024) {
                ++visible;
                pose = j;
            }
        require(visible == 1, "wheel must select exactly one authored pose");
        if (oldPose != 99 && pose != oldPose)
            sawWheelChange = true;
        oldPose = pose;
        if (sf.nodes[lamp_node].x != opxy::draw::hidden_x * 1024 && sf.nodes[lamp_node].x != lampX)
            sawLampMove = true;
    }
    require(sawLampMove && sawWheelChange, "lamp/wheel animation did not move");
    // Motion stops at the same phase and never restarts without note activity.
    si.held_notes = 0;
    for (unsigned j = 0; j < 4; ++j)
        display(slow, si, sf);
    require(slow.motion == 0, "release did not stop within 125ms");
    unsigned stopped = slow.road;
    for (unsigned j = 0; j < 30; ++j)
        display(slow, si, sf);
    require(slow.road == stopped, "idle scene is moving");
    si.elapsed = 32;
    si.held_notes = 2;
    display(slow, si, sf);
    require(slow.motion == 256, "first quarter of attack ramp");
    si.held_notes = 1;
    display(slow, si, sf);
    require(slow.motion == 512, "releasing one chord note slowed the car");
    si.held_notes = 0;
    display(slow, si, sf);
    require(slow.motion == 256, "release must start from current speed");
    si.held_notes = 1;
    display(slow, si, sf);
    require(slow.motion == 512, "retrigger during braking is discontinuous");
    si.held_notes = 0;
    for (unsigned j = 0; j < 4; ++j)
        display(slow, si, sf);
    ++si.note_serial;
    display(slow, si, sf);
    require(slow.motion > 0, "missed short note between drawing frames");
    for (unsigned j = 0; j < 12; ++j)
        display(slow, si, sf);
    require(slow.motion == 0, "short-note motion did not stop");
    si.held_notes = 1;
    display(slow, si, sf);
    ++si.track;
    si.held_notes = 0;
    display(slow, si, sf);
    require(slow.motion == 0 && slow.road == 0, "track switch retained old activity");
    // Every clipped lamp edge must lie on an original contour segment: no bridges.
    DisplayState lampState{};
    opxy::draw::Input lampInput{};
    lampInput.reduced_motion = 1;
    display(lampState, lampInput, sf);
    for (unsigned phase = 0; phase < 10000; ++phase) {
        lampState.lamp = opxy::u32(phase * 4294967296.0 / 10000);
        lampState.previous[0] ^= 1;
        display(lampState, lampInput, sf);
        if (phase >= 8700)
            require(sf.nodes[lamp_node].x != opxy::draw::hidden_x * 1024 &&
                        sf.nodes[0].x == opxy::draw::hidden_x * 1024 &&
                        sf.nodes[1].x == opxy::draw::hidden_x * 1024,
                    "bottom exit must retain the original open lamp sprite");
        for (unsigned path = 0; path < 2; ++path) {
            if (sf.nodes[path].x == opxy::draw::hidden_x * 1024)
                continue;
            for (unsigned j = 1; j < 64; ++j) {
                auto a = sf.paths[path].points[j - 1], b = sf.paths[path].points[j];
                if (a.x == b.x && a.y == b.y)
                    continue;
                bool onContour = false;
                for (unsigned k = 1; k < sizeof(lamp_shape) / sizeof(lamp_shape[0]); ++k) {
                    auto p = lamp_shape[k - 1], q = lamp_shape[k];
                    long long dx = q.x - p.x, dy = q.y - p.y;
                    long long tolerance = 120 * (std::llabs(dx) + std::llabs(dy) + 1);
                    if (std::llabs((a.x - p.x) * dy - (a.y - p.y) * dx) <= tolerance &&
                        std::llabs((b.x - p.x) * dy - (b.y - p.y) * dx) <= tolerance)
                        onContour = true;
                }
                require(onContour, "clipping introduced a spurious lamp segment");
            }
        }
    }
    const unsigned captionValues[] = {0, 25, 50, 100};
    for (unsigned value : captionValues) {
        for (unsigned slot = 0; slot < 4; ++slot)
            in.slots[slot] = int(value * 2147483647.0 / 100);
        display(s, in, f);
        char expected[8] = {};
        std::snprintf(expected, sizeof(expected), "%u", value);
        for (unsigned slot = 0; slot < 4; ++slot)
            require(std::strcmp(f.header[slot], expected) == 0, "integer caption");
    }
    uint32_t random = 123;
    for (unsigned k = 0; k < 24000; ++k) {
        in.elapsed = k % 7 == 0 ? -1 : k % 7 == 1 ? 2147483647 : 17;
        in.track = k % 8;
        in.note_serial = k / 13;
        in.reduced_motion = k % 3 == 0;
        for (unsigned j = 0; j < 8; ++j) {
            random = random * 1664525u + 1013904223u;
            in.slots[j] = k < 256 ? ((k >> j) & 1 ? 2147483647 : 0) : int(random >> 1);
        }
        display(s, in, f);
        require(opxy::draw::valid(f), "invalid random/extreme frame");
    }
    std::puts("PASS nightdrive display: pose, four windows only, unused slots invariant, "
              "synchronized road/lamp/wheels, reduced motion, 24000 frames");
}
