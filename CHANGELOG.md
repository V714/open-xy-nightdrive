# Changelog

## 1.0.3

Release the hardware-selected b79fe97 DSP, retaining the previous controls,
parameter IDs, presets, artwork, note range and sample-rate control smoothing.
Prepare stable coefficients once per block, retain render state locally, use
paired polynomial output sines, pack exact feedback/Glare lookups, specialize
single-control automation and the full-automation attack path, and remove
redundant output-scale divisions. Feedback, phase and control trajectories stay
exact; output audio has a small bounded approximation and rounding difference.

In a same-SDK OP-XY comparison against 1.0.1, midpoint mean fell from 4.320% to
3.536%, midpoint peak from 5.350% to 4.242%, and full-sweep peak from 8.305% to
6.215%. The old version has one capture; the selected DSP has three independent
boots, retaining the largest observed peak. A later three-boot run on the newer
SDK measured 3.917% heaviest mean, 4.166% midpoint peak and 6.192% full-sweep
peak. These are distinct build contexts, documented in [PERFORMANCE.md](PERFORMANCE.md).

Native engine contribution is 78.32 KiB on the newer SDK (code/data/BSS,
adapters, all voice slots and alignment), below the 90 KiB limit. SVG, shared
SDK overhead, caller stack and presets are separate. Private voice state stays
64 bytes. Validation covers 900 automation cases, extended note tests,
ASan/UBSan, SDK machine tests and a qualified native package. The worst measured
automation-region relative RMS error against the exact 1.0.2 reference is
-71.77 dB, within the -55 dB gate; 1.0.2 itself matches 1.0.1 audio exactly.

Later trace-guided variants are excluded because hardware did not show a better
peak despite lower standalone instruction counts. This release improves the
reported midpoint workload, but does not meet the 3.5% recommendation across
the full sweep. Continuous four-control hardware automation has no new measured
peak. No claim is made that the original full-catalog Glare stall is fixed.

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
