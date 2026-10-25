#pragma once
#include "core.h"
#include "tables.h"
#include <opxy/q31.hpp>
namespace nightdrive {
namespace dsp {
using opxy::i32;
using opxy::u32;
using opxy::q31::multiply;
static inline i32 clamp(i32 x, i32 lo, i32 hi) {
    return x < lo ? lo : x > hi ? hi : x;
}
static inline i32 mix(i32 a, i32 b, i32 t) {
    return a + multiply(b - a, t);
}
static inline i32 sine(u32 p) {
    unsigned j = p >> 21;
    i32 a = i32(audio_sine_lut[j]) * 32768, b = i32(audio_sine_lut[(j + 1) & 2047]) * 32768;
    return mix(a, b, i32((p & 0x1fffffu) << 10));
}
static inline i32 lut(const i32 *table, i32 control) {
    unsigned j = u32(control) >> 23;
    return mix(table[j], table[j + 1], i32((u32(control) & 0x7fffffu) << 8));
}
// Ratio uses Q26 so 16x fits without signed overflow.
static inline i32 operator_ratio(i32 control) {
    if (control == 2147483647)
        return ratio_lut[100];
    // Q24 table position = control * 100/128, using the SDK multiply.
    // This avoids division and preserves sub-percent resolution while smoothing.
    unsigned position = u32(multiply(control, 1677721600));
    unsigned index = position >> 24;
    return mix(ratio_lut[index], ratio_lut[index + 1], i32((position & 0xffffffu) << 7));
}
// Extra range grows quadratically over the final 30% of each control.
static inline i32 feedback_amount(i32 control) {
    return multiply(control, 581030184) + lut(feedback_boost_lut, control);
}
static inline i32 modulation_amount(i32 control) {
    return multiply(control, 1367130551) + lut(modulation_boost_lut, control);
}
static inline bool settled(const i32 *c, const i32 *p) {
    for (unsigned k = 0; k < 4; ++k)
        if (c[k] != p[k])
            return false;
    return true;
}
static inline void tick_controls(i32 *c, const i32 *p) {
    for (unsigned k = 0; k < 4; ++k) {
        i32 target = p[k] < 0 ? 0 : p[k], d = target - c[k];
        c[k] += (d < 512 && d > -512) ? d : d / 512;
    }
}
static inline i32 saturate(i32 x, i32 drive) {
    if (!drive)
        return x;
    i32 a = x < 0 ? -x : x;
    u32 pos = (u32(a) >> 5) + (u32(multiply(a, multiply(drive, drive))) >> 5) * 31u;
    if (pos >= 0x20000000u)
        pos = 0x1fffffffu;
    unsigned j = pos >> 19;
    i32 y = mix(tanh_lut[j], tanh_lut[j + 1], i32((pos & 0x7ffffu) << 12));
    y = multiply(y, lut(drive_comp_lut, drive));
    return mix(x, x < 0 ? -y : y, drive);
}
static inline i32 dc_block(i32 x, i32 &previous, i32 &state) {
    i32 half = x / 2 - previous / 2 + multiply(state, 2145980416) / 2;
    previous = x;
    state = clamp(half, -805306368, 805306368) * 2;
    return state;
}
} // namespace dsp
} // namespace nightdrive
