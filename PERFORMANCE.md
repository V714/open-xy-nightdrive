# Nightdrive 1.0.3 performance evidence

The release uses the hardware-selected DSP from
`b79fe97d486457aab6bacbe77e5b9a8c072b2815`. The subsequent `ea63027` change only
renames a standalone diagnostic assembly fixture to `.s.in`; it does not change
production audio. Release documentation does not change build-provenance inputs.

## Controlled comparison with 1.0.1

Both versions were rebuilt with SDK
`14d595b0c018238e69f9c9e30a19e0deae7d055e` and measured on OP-XY OS 1.2.1.
The old version is `c28339571117094548767fd1873556bff80ef15e`. It has one complete
capture; the selected DSP has three captures from separate boots. Means are
medians across captures; peaks retain the largest observed block, including
the combined workload. These are observed costs, not absolute worst-case bounds.

| Metric | 1.0.1 | 1.0.3 DSP | Change |
| --- | ---: | ---: | ---: |
| Midpoint mean | 4.320% | 3.536% | -18.2% |
| Midpoint peak | 5.350% | 4.242% | -20.7% |
| Heaviest sweep mean | 4.619% | 3.946% | -14.6% |
| Complete-sweep peak | 8.305% | 6.215% | -25.2% |
| Speed 0% peak | 8.305% | 5.774% | -30.5% |
| Native engine contribution | 59.28 KiB | 78.33 KiB | +19.05 KiB |
| Private voice state | 64 bytes | 64 bytes | unchanged |

The native figure includes owned code/data/BSS, adapters, every native voice slot
and alignment allowance. It excludes the 25,799-byte installed SVG, shared SDK,
caller stack and presets. It meets the 90 KiB native-engine budget; it is not a
claim that the complete installed engine plus artwork is below 90 KiB.

The issue's original 5.11% peak / 3.50% mean came from a different build context.
Do not compare its mean directly to these rebuilds or describe it as a 10% peak.

## Catalog evidence on the newer SDK

An ordinary profiling build, without the block recorder, used SDK
`0604d55` (diagnostic branch based on official staging
`2e3f7cd19ed43c2a419499c2537ca9a6c9abee06`). The production DSP is unchanged.
Firmware SHA-256:
`c6a420852d38e7b79d1231cb987a38bf7607988224201df1a2ad30cff7158254`.
All three captures contain 49 frames, complete Nightdrive sweeps and no collector
errors. Each capture follows a fresh normal restart, a saved project and stopped
playback. Installation used SDK device push with backup and readback; the
maintainer confirmed boot-partition write and restart.

| Run | Midpoint mean | Midpoint peak | Complete-sweep peak |
| --- | ---: | ---: | ---: |
| 1 | 3.495842% | 4.165713% | 6.179904% |
| 2 | 3.498894% | 4.145876% | 6.192111% |
| 3 | 3.495842% | 4.158083% | 6.173800% |

The SDK aggregate reports **3.916991%** heaviest mean (Speed 87.5%),
**3.495842%** midpoint mean and **6.192111%** retained sweep peak. The heaviest
mean setting is not the setting with the largest peak. This newer result is the
catalog record; the older same-SDK comparison above supplies the 1.0.1 comparison.

## What improved

- Reuse stable coefficients and fixed Glare row selection across a block, while
  retaining sample-by-sample smoothing when controls move.
- Keep hot render state local; use compile-time paths for individual moving
  controls and a complete full-automation attack block.
- Plan independent sustained-note coefficients in bounded 32-sample spans before
  the feedback-dependent loop, preserving the original per-sample rounding.
- Use a paired degree-five polynomial for the two output carrier sines, while
  retaining the exact full feedback lookup and packed Glare segments.
- Fold the output blend into one multiply and keep DC-filter history at half
  scale to remove redundant scaling work.

The 900-case automation gate measured a worst per-case/region relative RMS error
of -71.77 dB against exact 1.0.2, below the required -55 dB. Extended note tests
also passed. Phase, feedback and control trajectories remain exact. Audio and
output-dependent DC history have a small approximation/rounding difference;
these numerical checks do not prove perceptual identity for every possible patch.
The note range, parameter IDs/order/mapping, presets and artwork are preserved.

## Rejected experiments and remaining work

Trace-guided coefficient planning and cache experiments reduced standalone
instruction counts but did not improve the retained hardware peak. Candidate
61f41a9 measured 6.378271% across three boots at 87.93 KiB; paired-sine ablation
9150563 measured 6.424048% in one screening run at 88.12 KiB. Neither is in 1.0.3.

The traced hardware spikes were at block zero of control transitions on an old
voice already past its timbre transient, including callbacks before the next
note starts. Speed 0% is not expensive because of a divide by its 0.25x ratio.
Coefficient/control work and the feedback loop matter; cache and pipeline effects
prevent translating standalone instruction savings directly into device savings.

The stock test is one note, velocity 127, 1,000 held plus 500 release blocks,
with midpoint and discrete control sweep positions. It is not a continuously
automated LFO test. Its combined row's displayed settings are inferred by the
host; this SDK clears the selecting counters before reading them, so the actual
combined target is zero for all four controls. Preserve that fact when replaying
the transition; do not silently change the workload between comparisons.

Midpoint peak is now below the issue's 4.5% reporting threshold in our builds.
The complete sweep still exceeds 4.5% and the recommended 3.5%, so this is a
documented improvement, not a universal peak fix. No new continuous-automation
hardware peak or fix for the historical full-catalog Glare stall is claimed.

See [tests/PERFORMANCE_WORKFLOW.md](tests/PERFORMANCE_WORKFLOW.md) for reproduction
and diagnosis. Raw captures, opaque device state, firmware and instrumented
packages stay private outside checkouts.
