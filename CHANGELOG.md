# Changelog

## 1.0.2

Reduce redundant coefficient calculations during parameter automation, pack exact
Glare segment bases and slopes into a single lookup, and plan sustained-note
coefficients in 32-sample spans. Sample-rate smoothing, integer rounding, oscillator
and feedback behavior, controls, artwork and bundled presets are preserved.

Compared with 1.0.1, saved host-estimator results indicate approximately 18–19%
less work for full-control or Speed automation, 25% for Tension, 36% for Drift or
Glare, and 7% without automation. These are host estimates, not measured OP-XY
CPU usage or worst-block peaks.

Owned native code/data/state and adapters occupy 72.98 KiB, or 98.17 KiB including
the complete installed SVG, versus 59.28/84.47 KiB in 1.0.1 under the same SDK.
Shared SDK overhead, caller stack and separate preset files are excluded.
Private voice state remains 64 bytes. No lower control rate or new sound
approximation is introduced by this release.

Validation: 54,476,928 regression samples and active DSP state match the original
release bit for bit. Engine tests, ASan/UBSan, SDK machine checks and independent
GNU/Python personal-builder verification pass. The maintainer confirmed listening
and parameter automation on OP-XY with the 1.0.2-rc.1 builder, whose engine sources
are unchanged in this release. Revision-bound hardware average and peak CPU
profiling remains pending; no claim is made that the previously reported
full-catalog Glare stall is fixed. Full SDK checks on the local Python 3.14 host
encounter three subcase errors in an unchanged mocked MTP link test; the SDK
itself is unchanged.

## 1.0.1

Bundle four OP-XY presets using the SDK 1.3 presets field.
