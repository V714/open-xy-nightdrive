#pragma once
#include "tables.h"
namespace nightdrive {
// Engine-owned integer captions work on both SDK 1.0 and 1.1.
static inline void percentage_caption(char *text, opxy::u32 value) {
    unsigned percent = (((value >> 16) * 100u) + 16384u) >> 15;
    unsigned position = 0;
    if (percent == 100)
        text[position++] = '1';
    if (percent >= 10)
        text[position++] = char('0' + (percent / 10) % 10);
    text[position++] = char('0' + percent % 10);
    text[position] = 0;
}
static inline void display_reset(DisplayState &s) {
    s.road = s.lamp = s.wheel = s.ready = s.track = s.key = s.clock = 0;
    s.noteSerial = s.motion = s.pulse = 0;
    for (unsigned j = 0; j < 4; ++j)
        s.previous[j] = 0;
}
// Bounded Q10 ratio for clipping. No 64-bit multiply or division helpers.
static inline int fraction(int numerator, int denominator) {
    if (numerator <= 0)
        return 0;
    if (numerator >= denominator)
        return 1024;
    int q = 0;
    for (unsigned j = 0; j < 10; ++j) {
        numerator *= 2;
        q *= 2;
        if (numerator >= denominator) {
            numerator -= denominator;
            ++q;
        }
    }
    return q;
}
static inline bool clip(ArtPoint &a, ArtPoint &b) {
    for (unsigned boundary = 0; boundary < 4; ++boundary) {
        bool axis = boundary >= 2;
        int edge = boundary == 0   ? 0
                   : boundary == 1 ? 480 * 1024
                   : boundary == 2 ? 20 * 1024
                                   : 220 * 1024;
        int av = axis ? a.y : a.x, bv = axis ? b.y : b.x;
        bool lower = (boundary & 1) == 0;
        bool outsideA = lower ? av < edge : av > edge, outsideB = lower ? bv < edge : bv > edge;
        if (outsideA && outsideB)
            return false;
        if (outsideA != outsideB) {
            int numerator = edge - av, denominator = bv - av;
            if (denominator < 0) {
                denominator = -denominator;
                numerator = -numerator;
            }
            int t = fraction(numerator, denominator);
            ArtPoint p = {a.x + (b.x - a.x) * t / 1024, a.y + (b.y - a.y) * t / 1024};
            if (axis)
                p.y = edge;
            else
                p.x = edge;
            if (outsideA)
                a = p;
            else
                b = p;
        }
    }
    return true;
}
static inline void contour(Frame &f, unsigned id, const ArtPoint *points, unsigned count,
                           ArtPoint origin) {
    using namespace opxy::draw;
    if (count < 2) {
        hide(f, id);
        return;
    }
    for (unsigned j = 0; j < 64; ++j) {
        unsigned index = j < count ? j : j < 2 * count ? 2 * count - 1 - j : 0;
        if (index >= count)
            index = 0;
        f.paths[id].points[j] = {points[index].x - origin.x, points[index].y - origin.y};
    }
    place(f, id, origin.x, origin.y, 1024, 0, keep);
}
} // namespace nightdrive
