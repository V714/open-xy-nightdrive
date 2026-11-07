#pragma once
#include "core.h"
#include "tables.h"
#include "approx_lut.h"
#include "audio_sine_coefficients.h"
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
// Branch-free quarter-wave polynomial for audio only; feedback keeps exact lookup.
static inline i32 sine_audio(u32 phase) {
    const i32 centered = i32(phase & 0x7fffffffu) - 1073741824;
    const i32 magnitude = centered < 0 ? -centered : centered;
    const u32 doubled = (1073741824u - u32(magnitude)) << 1;
    const i32 x = i32(doubled - (doubled >> 31));
    const i32 squared = multiply(x, x);
    const i32 polynomial =
        audio_sine_a + multiply(squared, audio_sine_b + multiply(squared, audio_sine_c));
    const i32 signal = multiply(x, polynomial);
    const i32 sign = -i32(phase >> 31);
    return (signal ^ sign) - sign;
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
[[gnu::always_inline]] static inline i32 modulation_amount(i32 control) {
    const i32 linear = multiply(control, 1367130551);
    // Both neighboring table entries are exactly zero through this boundary.
    return control <= 1501560832 ? linear : linear + lut(modulation_boost_lut, control);
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
static inline i32 smooth_control(i32 current, i32 target) {
    const i32 distance = target - current;
    const i32 step = distance / 512;
    return current + (step ? step : distance);
}
static inline void tick_controls(i32 *c, const i32 *p) {
    for (unsigned k = 0; k < 4; ++k)
        c[k] = smooth_control(c[k], p[k] < 0 ? 0 : p[k]);
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
static const bool use_packed_shaper = false;
// Experimental bilinear output-shaper LUT: no change to oscillator or feedback state.
[[gnu::always_inline]] static inline i32 interpolate_segment(u32 segment, u32 amplitude) {
    const unsigned shift = 31 - shaper_amplitude_bits;
    const opxy::u16 fraction = opxy::u16(amplitude >> (shift - 16));
    return i32(segment << 16) + i32(segment >> 16) * i32(fraction);
}
[[gnu::always_inline]] static inline i32 shaper_row(unsigned row, u32 amplitude) {
    const unsigned shift = 31 - shaper_amplitude_bits;
    const unsigned column = amplitude >> shift;
    if (shaper_scale == 65536) {
        if (use_packed_shaper)
            return interpolate_segment(shaper_segments[row][column], amplitude);
        const i32 base = shaper_lut[row][column];
        const i32 delta = i32(shaper_lut[row][column + 1]) - base;
        const opxy::u16 fraction = opxy::u16(amplitude >> (shift - 16));
        return base * 65536 + delta * i32(fraction);
    }
    const i32 fraction = i32((amplitude & ((1u << shift) - 1)) << shaper_amplitude_bits);
    return mix(shaper_lut[row][column], shaper_lut[row][column + 1], fraction);
}
struct PreparedShaper {
    const u32 *segments, *next;
    unsigned row;
    i32 fraction;
};
[[gnu::always_inline]] static inline PreparedShaper prepare_shaper(i32 drive) {
    if (!drive)
        return {nullptr, nullptr, ~0u, 0};
    const unsigned shift = 31 - shaper_drive_bits;
    unsigned row = u32(drive) >> shift;
    u32 fraction = (u32(drive) & ((1u << shift) - 1)) << shaper_drive_bits;
    if (fraction >= 0x80000000u - (2u << shaper_drive_bits)) {
        ++row;
        fraction = 0;
    }
    return {use_packed_shaper && shaper_scale == 65536 ? shaper_segments[row] : nullptr,
            use_packed_shaper && shaper_scale == 65536 && fraction ? shaper_segments[row + 1]
                                                                   : nullptr,
            row, i32(fraction)};
}
[[gnu::always_inline]] static inline i32 apply_shaper(i32 x, const PreparedShaper &shaper) {
    if (shaper.row == ~0u)
        return x;
    const u32 amplitude = u32(x < 0 ? -x : x);
    const unsigned column = amplitude >> (31 - shaper_amplitude_bits);
    i32 value = use_packed_shaper && shaper_scale == 65536
                    ? interpolate_segment(shaper.segments[column], amplitude)
                    : shaper_row(shaper.row, amplitude);
    if (shaper.fraction) {
        const i32 next = use_packed_shaper && shaper_scale == 65536
                             ? interpolate_segment(shaper.next[column], amplitude)
                             : shaper_row(shaper.row + 1, amplitude);
        value = mix(value, next, shaper.fraction);
    }
    const i32 sign = -i32(u32(x) >> 31);
    return (value ^ sign) - sign;
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
    const i32 sign = -i32(u32(x) >> 31);
    return (value ^ sign) - sign;
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
