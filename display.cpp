// SPDX-FileCopyrightText: 2026 Tomasz Lipczyński (V7, GitHub: V714)
// SPDX-License-Identifier: MIT
#include "display.h"
#include "visual.h"
#include <opxy/q31.hpp>
namespace nightdrive {
static inline void display(DisplayState &s, const opxy::draw::Input &in, Frame &f) {
    using namespace opxy::draw;
    int c[4];
    bool changed = !s.ready || s.track != in.track;
    for (unsigned j = 0; j < 4; ++j) {
        c[j] = int(unsigned(in.slots[j]) >> 16);
        if (s.previous[j] != unsigned(in.slots[j]))
            changed = true;
    }
    unsigned elapsed = unsigned(clamp(in.elapsed, 0, 102));
    bool reset = !s.ready || s.track != in.track;
    if (reset) {
        s.motion = s.pulse = s.clock = 0;
        s.noteSerial = in.note_serial;
    }
    if (in.note_serial != s.noteSerial)
        s.pulse = 128;
    s.noteSerial = in.note_serial;
    bool held = in.held_notes <= 128 && in.held_notes != 0;
    unsigned oldMotion = s.motion;
    unsigned delta = elapsed * 8u;
    if (held || s.pulse)
        s.motion = s.motion + delta > 1024 ? 1024 : s.motion + delta;
    else
        s.motion = s.motion > delta ? s.motion - delta : 0;
    s.pulse = s.pulse > elapsed ? s.pulse - elapsed : 0;
    // Integrate velocity each input frame, including frames below drawing cadence.
    if (!in.reduced_motion)
        s.clock += elapsed * (oldMotion + s.motion) / 2;
    if (s.ready && !changed && in.reduced_motion) {
        s.clock = 0;
        return;
    }
    if (s.ready && !changed && s.clock < 34 * 1024 && oldMotion == s.motion)
        return;
    if (!s.ready || s.track != in.track) {
        s.road = s.wheel = 0;
        s.lamp = 0x80000000u;
    }
    if (!in.reduced_motion) {
        unsigned speed = 256u + (unsigned(c[0]) * 1792u + 16383u) / 32767u,
                 ticks = (s.clock / 1024u) * speed;
        s.road += ticks * 28494u;
        s.lamp += ticks * 2112u;
        s.wheel += ticks * 18618u;
    }
    s.clock %= 1024u;
    s.ready = 1;
    s.track = in.track;
    if (++s.key == 0)
        s.key = 1;
    for (unsigned j = 0; j < 4; ++j)
        s.previous[j] = unsigned(in.slots[j]);
    for (unsigned j = 0; j < art_nodes; ++j)
        place(f, j, 0, 0, 1024, 0, keep);
    for (unsigned p = 0; p < 4; ++p) {
        f.paths[p].key = s.key;
        for (unsigned j = 0; j < 64; ++j)
            f.paths[p].points[j] = {0, 0};
        hide(f, p);
    }
    for (unsigned position = 0; position < 4; ++position) {
        char caption[8] = {};
        percentage_caption(caption, unsigned(in.slots[position]));
        header(f, position, caption);
    }
    // Translate every body and wheel part in the reference road basis.
    int dx = c[1] * 29860 / 32767 - c[2] * (lane_shift_q10 * 600 / 1024) / 32767;
    int dy = -c[1] * 21604 / 32767 - c[2] * (lane_shift_q10 * 829 / 1024) / 32767;
    for (unsigned node : car_nodes)
        place(f, node, dx, dy, 1024, 0, keep);
    unsigned shade = (unsigned(c[3]) * 15u + 16383u) / 32767u;
    for (unsigned node : window_nodes)
        place(f, node, dx, dy, 1024, shade, fill_and_stroke);
    // Markings move toward the lower-left; one 53-unit period hides wrapping.
    int phaseOffset = opxy::q31::multiply(int(s.road >> 1), 40704);
    for (unsigned j = 0; j < 13; ++j) {
        int distance = (int(j) - 1) * 40704 - phaseOffset;
        ArtPoint a = {81024 + distance * 830 / 1024, 237952 - distance * 600 / 1024};
        ArtPoint b = {a.x + 13074, a.y - 9449};
        if (!clip(a, b)) {
            hide(f, dash_nodes[j]);
            continue;
        }
        int scale = fraction(b.y - a.y < 0 ? a.y - b.y : b.y - a.y, 9449);
        place(f, dash_nodes[j], a.x, a.y, scale ? scale : 1, 0, keep);
    }
    // The lamp has one contour, occasionally split by viewport clipping into two.

    int lx = 414720 - opxy::q31::multiply(int(s.lamp >> 1), 453120);
    int ly = -42783 + opxy::q31::multiply(int(s.lamp >> 1), 310134);
    ArtPoint origin = {lx, ly - 40 * 1024};
    ArtPoint parts[2][32];
    unsigned counts[2] = {0, 0}, part = 0;
    bool connected = false;
    for (unsigned j = 1; j < sizeof(lamp_shape) / sizeof(lamp_shape[0]); ++j) {
        ArtPoint a = {origin.x + lamp_shape[j - 1].x, origin.y + lamp_shape[j - 1].y},
                 b = {origin.x + lamp_shape[j].x, origin.y + lamp_shape[j].y};
        if (!clip(a, b)) {
            connected = false;
            continue;
        }
        if (a.x == b.x && a.y == b.y)
            continue;
        if (connected && counts[part] &&
            (parts[part][counts[part] - 1].x != a.x || parts[part][counts[part] - 1].y != a.y))
            connected = false;
        if (!connected && counts[part]) {
            if (part == 1)
                break;
            ++part;
        }
        if (!connected && counts[part] < 32)
            parts[part][counts[part]++] = a;
        if (counts[part] < 32)
            parts[part][counts[part]++] = b;
        connected = true;
    }
    for (unsigned p = 0; p < 2; ++p)
        contour(f, p, parts[p], counts[p], origin);
    // Full lamp uses an ordinary retained path; mutable outlines are needed only at clipping edges.
    // The screen clips the lower/side edges naturally. Keep the ordinary open
    // lamp there instead of switching to a closed mutable contour at the exit.
    // Mutable contours are needed only to protect the stock header above y=20.
    int minY = origin.y + lamp_shape[0].y;
    for (const auto &point : lamp_shape)
        if (origin.y + point.y < minY)
            minY = origin.y + point.y;
    if (minY >= 20 * 1024) {
        hide(f, 0);
        hide(f, 1);
        place(f, lamp_node, origin.x, origin.y, 1024, 0, keep);
    } else
        hide(f, lamp_node);
    // Seven authored vector poses per half turn preserve the original disconnected spokes.
    unsigned pose = (((s.wheel & 0x7fffffffu) >> 16) * 7u) >> 15;
    for (unsigned wheel = 0; wheel < 2; ++wheel)
        for (unsigned j = 0; j < 7; ++j)
            if (j != pose)
                hide(f, wheel_sprite_nodes[wheel][j]);
}
} // namespace nightdrive
