#pragma once
#include "core.h"
#include "tables.h"
#include "approx_lut.h"
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
    const u32 packed = sine_packed_lut[j];
    const i32 a = i32(opxy::i16(packed & 65535u)) * 32768;
    const i32 delta = i32(opxy::i16(packed >> 16)) * 32768;
    return a + multiply(delta, i32((p & 0x1fffffu) << 10));
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
// Tension grows gently from zero; Drift keeps its upper-range quadratic boost.
[[gnu::always_inline]] static inline i32 feedback_amount(i32 control) {
    if (control == 2147483647)
        return feedback_maximum;
    return multiply(control, 581030184) + lut(feedback_boost_lut, control);
}
static inline i32 modulation_amount(i32 control) {
    return multiply(control, 1367130551) + lut(modulation_boost_lut, control);
}
static inline i32 punch_amount(i32 control) {
    if (control == 2147483647)
        return 2147483647;
    return lut(punch_lut, control);
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
static inline i32 saturate(i32 x, i32 drive, i32 squared, i32 compensation) {
    if (!drive)
        return x;
    i32 a = x < 0 ? -x : x;
    u32 pos = (u32(a) >> 5) + (u32(multiply(a, squared)) >> 5) * 31u;
    if (pos >= 0x20000000u)
        pos = 0x1fffffffu;
    unsigned j = pos >> 19;
    i32 y = tanh_lut[j] + multiply(tanh_delta_lut[j], i32((pos & 0x7ffffu) << 12));
    y = multiply(y, compensation);
    return mix(x, x < 0 ? -y : y, drive);
}
[[gnu::always_inline]] static inline i32 saturate(i32 x, i32 drive, i32 squared, i32 compensation,
                                                  i32 clipped) {
    if (!drive)
        return x;
    i32 a = x < 0 ? -x : x;
    u32 pos = (u32(a) >> 5) + (u32(multiply(a, squared)) >> 5) * 31u;
    if (pos >= 0x20000000u)
        return mix(x, x < 0 ? -clipped : clipped, drive);
    unsigned j = pos >> 19;
    i32 y = tanh_lut[j] + multiply(tanh_delta_lut[j], i32((pos & 0x7ffffu) << 12));
    y = multiply(y, compensation);
    return mix(x, x < 0 ? -y : y, drive);
}
static inline i32 saturate(i32 x, i32 drive) {
    if (!drive)
        return x;
    return saturate(x, drive, multiply(drive, drive), lut(drive_comp_lut, drive));
}
// Experimental bilinear output-shaper LUT: no change to oscillator or feedback state.
[[gnu::always_inline]] static inline i32 shaper_row(unsigned row, u32 amplitude) {
    const unsigned shift = 31 - shaper_amplitude_bits;
    const unsigned column = amplitude >> shift;
    if (shaper_scale == 65536) {
        const i32 a = shaper_lut[row][column];
        const opxy::i16 delta = opxy::i16(i32(shaper_lut[row][column + 1]) - a);
        const opxy::u16 fraction = opxy::u16(amplitude >> (shift - 16));
        // Integer Q15 interpolation: one 16x16 multiply instead of a Q31 multiply.
        // Truncating the fraction adds at most |delta| Q31 words of error.
        return a * 65536 + i32(delta) * i32(fraction);
    }
    const i32 fraction = i32((amplitude & ((1u << shift) - 1)) << shaper_amplitude_bits);
    return mix(shaper_lut[row][column], shaper_lut[row][column + 1], fraction);
}
[[gnu::always_inline]] static inline i32 saturate_approx(i32 x, i32 drive, i32, i32, i32) {
    if (!drive)
        return x;
    const u32 amplitude = u32(x < 0 ? -x : x);
    const unsigned shift = 31 - shaper_drive_bits;
    const unsigned row = u32(drive) >> shift;
    const u32 fraction = (u32(drive) & ((1u << shift) - 1)) << shaper_drive_bits;
    i32 value;
    if (fraction == 0)
        value = shaper_row(row, amplitude);
    else if (fraction >= 0x80000000u - (2u << shaper_drive_bits))
        value = shaper_row(row + 1, amplitude);
    else
        value = mix(shaper_row(row, amplitude), shaper_row(row + 1, amplitude), i32(fraction));
    return x < 0 ? -value : value;
}
static inline i32 saturate_approx(i32 x, i32 drive) {
    return saturate_approx(x, drive, 0, 0, 0);
}
static inline i32 dc_block(i32 x, i32 &previous, i32 &state) {
    i32 half = x / 2 - previous / 2 + multiply(state, 2145980416) / 2;
    previous = x;
    state = clamp(half, -805306368, 805306368) * 2;
    return state;
}
} // namespace dsp
} // namespace nightdrive
