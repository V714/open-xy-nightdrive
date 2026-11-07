# Performance analysis and optimization workflow

Keep engine changes here and shared profiling/adapter changes in an isolated SDK
branch. Preserve known-good commits and packages. Use one candidate per branch;
record its source revision, SDK revision, compiler hashes, flags and native size.
Compare against an unchanged reference on the same SDK. The selected release DSP
is b79fe97; 99df9d4, 61f41a9 and 9150563 remain experimental branches. The last
two have no demonstrated hardware peak improvement.

## Sound, state and memory constraints

Use a compatible sibling SDK and fresh output folders. Firmware, captured state,
executable diagnostic binaries and device captures belong outside every checkout,
represented below as `<private>`. Keep stock inputs and opaque device data out
of commits, documentation and PR attachments.

```sh
python <sdk>/sdk.py test --machine <machine>
python <machine>/tests/check.py --sdk-root <sdk> --out <private>/host-001
python <machine>/tests/approximation_check.py --sdk-root <sdk> \
  --baseline <exact-reference> --audio-oscillator --out <private>/sound-001
python <machine>/tests/automation_quality.py --sdk-root <sdk> \
  --baseline <exact-reference> --out <private>/automation-001
python <machine>/tests/measure_native_size.py --sdk-root <sdk> \
  --machine <machine> --out <private>/size-001
```

For audio changes require <= -55 dB relative RMS error separately for every case
and tested attack/transient/sustain region, bounded peaks and exact phase,
feedback and control trajectories. DC-filter history can differ only as part of
an explicitly accepted output change. Run ASan/UBSan too. Check extreme notes,
controls, holds, retriggers, moving pitch and all 15 nonempty automation masks.
Header captions are integers, but smoothing and LFOs produce intermediate values:
do not quantize those away without sound evidence. Numerical gates supplement
listening; they do not guarantee perceptual identity for every patch.

Native contribution must be <= 90 KiB including adapters, all voice slots and
alignment. Report SVG, shared SDK, presets and stack separately. Inspect emitted
assembly and supplemental stack bounds, including register-based SP adjustments;
the SDK's simple 240-byte result missed those in this investigation. Do not
change SDK memory guards to admit an oversized engine.

## Rank candidates on the computer

Use [BLACKFIN_LAB.md](BLACKFIN_LAB.md) for reproducible GNU instruction replay.
The `.s.in` startup fixture is exclusively a standalone test, not machine assembly.
Match every native replay against its own portable C++ reference before trusting
the model. Compare all 11,520 blocks, peak indices, packet counts, loads/stores
and state, including samples 1103 (Punch end) and 3528 (transient end).

The model counts instruction packets and data accesses. It does not model BF707
pipeline stalls, caches, stock envelope/filter/mixing or interrupts. Never convert
packet counts to OP-XY CPU percentages. Host timing is only a ranking aid too.
Fewer multiplies or divisions do not guarantee a faster emitted loop.

Prioritize coefficient work, redundant lookups, spills and feedback dependencies.
Specialize common masks only when size and broad tests justify it. Preserve
sample-rate smoothing and Q31 rounding. Plan independent work before feedback
where useful. Test packed tables against compact ones: more table bytes can be
faster on hardware, and a smaller instruction count can lose through scheduling
or cache effects. Do not ship all experiments just because each looks plausible.

## Locate a hardware spike before guessing

Start with normal uninstrumented sweeps. For unexplained peaks use a separate
SDK block-trace diagnostic, following that SDK's profiling guide. Observe old
and fading voices and callbacks before note-on, and preserve stock counters
across report export. On the diagnostic SDK branch used here:

```sh
python sdk.py build --machine <machine> --profiling on --sweep on \
  --block-trace nightdrive --out <private>/trace-build-001 [stock/tool arguments]
python sdk.py device push <private>/trace-build-001/candidate
# Maintainer writes OS to boot partition, waits for success and restarts.
python sdk.py profile benchmark --package <private>/trace-build-001 \
  --diagnostic-trace --installed --greet --out <private>/trace-run-001
# Maintainer enters MTP without saving the diagnostic project state.
python sdk.py device pull /logs/open-xy-block-trace.bin \
  --out <private>/trace-001.bin
python sdk.py profile trace --package <private>/trace-build-001 \
  --trace-file <private>/trace-001.bin --out <private>/trace-readout-001
```

Check supported options in the chosen SDK. The recorder is an isolated SDK
diagnostic, not an engine dependency. Firmware installation needs user
authorization: use SDK transfers with backup/readback, never custom MTP scripts
or raw boot writes. MTP readback does not prove the boot partition was written.
If writing hangs, confirm physical state and known-good recovery rather than
assuming the candidate runs.

Check CRC, source identity, completion, coverage and overflow. A peak is located
only when valid per-block deltas reproduce the stock raw maximum. A partial
capture cannot show the peak was absent. Read actual targets, pitch buffers,
control mask, voice age and fade status. Private snapshots require the matching
source/layout; do not guess their ABI. The trace decoder/replay belongs to the
diagnostic SDK and experimental engine branches, not the v1.0.3 release.

The useful result here was an old voice at age 3528 and a block-zero control
transition, not costly division by the 0.25x ratio. Do not skip/reset that voice
or omit its first block to improve the score. Future changes need broad tests,
a replay gain on that actual transition, then ordinary hardware confirmation.

## Accept only measured hardware gains

Build an ordinary profiling package without the block recorder. Qualify it with
all required SDK native checks and independent reconstruction. Record source,
SDK, firmware hash, layout and toolchain; never bypass source/hash, reservation,
memory or reconstruction guards. After authorized installation, confirm successful
boot write/restart, saved project and stopped playback. Collect once per fresh
normal boot:

```sh
python sdk.py profile benchmark --package <private>/candidate-build-001 \
  --installed --greet --out <private>/candidate-run-001
python sdk.py profile report --package <private>/candidate-build-001 \
  --machine nightdrive --capture <private>/candidate-run-001/capture.json \
  --out <private>/candidate-report-001
```

Add `--capture` arguments for repeats. Require complete captures, no protocol
errors, adequate activity and unclipped peaks. CPU percent is
`100 * encoded_maximum / 65535`, using stock's 580,352-cycle block budget. The
whole-voice metric includes the oscillator: never add it again. Means use medians
across boots; release peaks retain the largest observed block over every captured
workload/boot, not the median of maxima. Report midpoint and sweep separately.
Three independent boots help distinguish small gains from run variation.

The discrete stock sweep is not live four-parameter automation. Record actual
combined targets: this SDK's counter-clear ordering selected zeros despite
host-inferred labels. Keep that workload unchanged across comparisons. Traced
firmware has observer overhead and cannot publish catalog CPU. After diagnostics,
restart normally without saving modified test state before resuming music.

Reject model improvements that worsen hardware peaks. Here 61f41a9 reached
6.378% versus the selected 6.192%; 9150563 reached 6.424% in one screening run.
Both are excluded despite passing audio checks.

## Freeze and publish

Commit the selected source, generate `sdk.py profile report` against a clean
matching tree, and tag that revision. SDK provenance excludes Markdown, so a
documentation-only release commit can reuse complete captures when the official
reporter verifies every included file. Never rewrite captures/manifests, manually
change the CPU commit or invent missing peaks.

Copy the complete generated `cpu` member into only Nightdrive's catalog entry,
with the tag and full commit. Push the engine release before the fork's catalog
branch so CI can fetch it. Keep local notes and PR drafts outside the catalog
commit, which must change only `catalog/machines.json`. Disclose the unmet 3.5%
sweep recommendation; distinguish midpoint improvements from a universal fix.
