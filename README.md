# Nightdrive

Version **1.0.2** · Author and maintainer: **Tomasz Lipczyński (V7)** · GitHub: [**V714**](https://github.com/V714) · MIT licensed.

**Experimental performance branch after v1.0.2:** output carrier sines may differ within
a tested relative RMS error limit of -55 dB. The feedback oscillator remains exact.

A four-control feedback FM bass synthesizer for OP-XY, with an animated isometric night-drive scene.
The source targets Open-XY SDK 1.3 and later compatible 1.x releases, for stock OS 1.2.1. Audio is mono Q31 at 44.1 kHz, in 64-sample
blocks, without oversampling. This folder is the canonical source for further Nightdrive development.

See [CHANGELOG.md](CHANGELOG.md) for release changes and validation status.

## Controls

| Control | Sound                                       | Graphics                           |
| ------- | ------------------------------------------- | ---------------------------------- |
| Speed   | Continuous operator ratio, 0.25–16          | Road, streetlamp and wheel speed   |
| Tension | Modulator feedback, up to 3 radians         | Moves the entire car forward       |
| Drift   | Phase-modulation depth, up to 6 radians     | Moves the car toward the left lane |
| Glare   | Compensated overdrive up to 32×, plus Punch | Brightens the four windows         |

Tension's extra feedback boost starts above zero and follows the rational curve
`x / (16 - 15*x)`: very gentle at the bottom and steep near the top. Its full-scale
feedback remains 3 radians. Drift keeps its quadratic upper-range extension
between 70 and 100. Twenty percent clean fundamental reinforces the bass.
FM and feedback decrease at high frequencies to limit nonlinear bandwidth.
A note starts a finite 80 ms modulation transient controlled by Drift and a
25 ms pre-overdrive Punch transient controlled by Glare. Punch uses
`x + 0.15*x*(1-x)` for a modest increase at low settings, retaining zero and its
former full-scale amount. Glare's saturation mapping is unchanged. Both
transients are bypassed at zero. These are changes to the existing feedback
and Punch; there is no separate noise generator.
There is no internal amplitude decay: note length and sustain follow the device's stock ADSR. There are no alternate controls.
Four named presets are bundled in `presets/`. The SDK builder also generates an init preset
from the four control defaults.

Speed follows exponential interpolation through 0: 0.25×, 5: 0.5×, 10: 1×, 25: 2×,
50: 4×, 75: 8× and 100: 16×. Existing project values use this mapping. Visual speed remains 0.25–2×.

Note gates start and stop the road, lamp and wheels with 125 ms acceleration and braking.
The last held note starts braking; short notes between display frames also produce a brief movement.
The animation follows note activity, independently of BPM and the stock ADSR release tail.
Track changes reset activity.

The car has an opaque black backing. Wheels use seven authored vector poses per half turn; the road,
streetlamp and wheels share one speed multiplier. Reduced-motion mode freezes animation while
preserving the selected car pose and window shade. Scene dimensions are 480 × 220 pixels with the
SDK-owned 20-pixel header. The reference artwork lives in `assets/`.

## Use with Open-XY

This is a standalone machine repository. It contains no SDK, stock firmware or stock content.
The machine is at the repository root; its catalog path is `.`. Minimum supported SDK: **1.3**.
CI checks against Open-XY staging commit `6b2e4d10093584854adb86edce42c931b3bc0a7e`
(compatible with this machine's SDK 1.3 contract).

### Quick installation

For users with an existing Open-XY SDK checkout and a configured native toolchain.
Run the following commands from the SDK directory:

1. Clone Nightdrive alongside the SDK:

   ```sh
   git clone https://github.com/V714/open-xy-nightdrive.git ../open-xy-nightdrive
   ```

2. Fetch the official collection:

   ```sh
   python sdk.py catalog fetch
   ```

3. Build a personal kit containing Nightdrive:

   ```sh
   python sdk.py kit --personal \
     --collection local/catalog/collection.json \
     --machine ../open-xy-nightdrive \
     --stock-os /path/to/original/os.ldr \
     --out /path/outside/repositories/nightdrive-kit
   ```

4. Open the generated **`opxy-builder.html`** in a compatible browser, select
   Nightdrive, and follow the installation guide.

Replace the example paths with your own. The OS input must be the original stock
OP-XY 1.2.1 `os.ldr`. Keep the output directory outside every Git checkout.
No SDK modifications are required. See the SDK's
[build guide](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md)
for toolchain setup and supported build hosts.

### Preview and development

Install the tools described in the SDK's
[first-machine guide](https://github.com/etcetc000/open-xy/blob/staging/docs/first-machine.md).
Keep the SDK in a separate sibling folder. From this repository root:

```sh
git clone https://github.com/etcetc000/open-xy.git ../open-xy-sdk
git -C ../open-xy-sdk checkout 6b2e4d10093584854adb86edce42c931b3bc0a7e
python -m pip install -r requirements-dev.txt
python ../open-xy-sdk/sdk.py test --machine . --cxx clang++ --host-cxx g++
python ../open-xy-sdk/sdk.py preview --machine . --out ../nightdrive-preview-001
python ../open-xy-sdk/tools/serve_harness.py --preview ../nightdrive-preview-001
```

Open the printed local URL and enable audio. Use a fresh preview directory after source edits.
No SDK changes or copying into its source tree are required.

For native integration, use `python ../open-xy-sdk/sdk.py build --machine .` with the stock input
and external output arguments in the SDK's
[build guide](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md).
The requested registration is engine **49**, page **97**. The SDK layout allocator may assign
other free numbers; pass a previous build's `layout.json` when updating existing device projects.
The engine name and four control IDs/order are part of saved-project compatibility.

The engine uses public audio/drawing callbacks and supplies whole-number 0–100 header captions.
Keep all firmware, stock content and firmware-containing packages outside every checkout.

## Source layout

- Root C++ files: audio, drawing, state declarations and checked-in generated headers.
- `definition.json`, `machine.json`, `panel.svg.fragment`: SDK interface and authored drawing asset.
- `assets/`: the current source SVG; `generators/`: audio-table and artwork generators.
- `tests/`: host signal, range, sustain, drawing, regression and optional browser-artwork checks.
- `generated/`: reproducible table and artwork provenance, without firmware or private inputs.

## CI and submission

[Machine checks](.github/workflows/machines.yml) runs the SDK's machine checks and the engine's
own signal tests on Linux, uploads an audition preview artifact, and compiles alongside the published catalog
on Windows with the SDK's pinned Blackfin toolchain. It uses an exact SDK commit for reproducibility.
The workflow requires no stock firmware, secrets or device. Download the preview artifact, unzip it
outside this repository and serve it with the same pinned SDK's `tools/serve_harness.py`.

To propose inclusion in Open-XY, follow [SUBMITTING.md](SUBMITTING.md). Only the catalog entry goes
into Open-XY; this repository owns the sound, artwork, generators and tests.

## Checks and regeneration

Use Python 3.11+, a GNU-compatible host C++11 compiler (GCC or Clang), NumPy and Pillow. Black is needed only for Python formatting.
Install the pinned development dependencies in a virtual environment:

```sh
python -m pip install -r requirements-dev.txt
python tests/check.py --sdk-root ../open-xy-sdk --out ../nightdrive-check-001
python ../open-xy-sdk/sdk.py test --machine .
```

The host checks cover initial and extreme sound settings, all four controls, ten-second sustain, reference output level,
256 voice indices, extreme controls and pitches, rapid individual/joint automation,
and 24,000 drawing frames. They do not measure OP-XY
CPU load or establish hardware behavior. For exact before/after audio and frame comparisons, add
`--baseline /path/to/previous/nightdrive` to `tests/check.py`. Use `--baseline-sdk-root /path/to/previous/sdk` when comparing across SDK versions.
Checks remain active with Python `-O`.

Regenerate the checked-in assets with NumPy 2.3.5 and clang-format 23:

```sh
python generators/generate_tables.py
python generators/generate_artwork.py
```

Both generators find their own package root. They require no external samples, kit folders or private
files. Generated C++ is automatically formatted. Keep the reference SVG's path data unchanged unless
intentionally changing the design. Compiled and captured test results must be outside this folder.

For optional multi-frame comparison with the adapted reference SVG, start a dedicated Chrome/Chromium
instance with a fresh profile and local remote-debugging port, then run Node 22.12+:

```sh
node tests/verify_artwork.mjs --sdk-root ../open-xy-sdk \
  --out ../nightdrive-artwork-001 --cdp-url http://127.0.0.1:9222
```

The browser check additionally needs Pillow, NumPy and the host compiler. Use `--python` or `--cxx` to
select another executable. The output folder must be fresh. Cropping, grayscale quantization, the
lamp's visible starting phase and retained wheel poses are accounted for in the comparison.

Format hand-written C++ with clang-format 23 using the local `.clang-format`; Python with Black 25.1.0;
and JavaScript, JSON and this README with Prettier 3.9.9 using
`--ignore-path .prettierignore` when running from this repository root. Generated JSON uses the generators' own
stable two-space layout. Source SVG path data is excluded from automatic rewriting.

## License

Code and artwork are MIT licensed; see `LICENSE`. Preserve the Open-XY and Tomasz Lipczyński copyright
notices when redistributing. The Open-XY SDK remains a separate MIT-licensed dependency. No stock
firmware, stock artwork or third-party audio samples are included.

## CPU analysis without firmware

The feedback oscillator retains its original packed sine lookup and Q31 rounding.
Output carrier sines use 16 quadratic quarter-wave segments with arithmetic
phase folding and sign restoration. The coefficients occupy 192 bytes; each
sine needs two fixed-point multiplies. Independent output polynomials are
interleaved to expose instruction-level parallelism without changing rounding. Feedback retains its original exact sine lookup.
Regenerate coefficients with `python generators/generate_audio_sine.py`.
The 80/20 output blend uses one multiply; its rounding differs by at most one
Q31 unit from the previous two-multiply expression, before downstream processing.

Glare uses the original 33 × 129 Q15 grid with the exact former 16-bit amplitude
interpolation fraction. Its packed segment representation remains in the generated
header for reference checks and is discarded from production. Stable controls
prepare pitch coefficients and Glare rows once per block, reusing them across
transient boundaries. Audio state is held locally inside each sample range.
Voice remains 64 bytes; there is no derived per-voice coefficient cache.

Smoothing runs at the original sample rate. Automation snapshots targets once per
block and updates only coefficients whose controls move. A full-automation loop
avoids partial-update tests when all four controls move. Range, phase, feedback,
control state and output peaks are checked independently of audio approximation.

After the finite timbre transient finishes, Speed automation and automation of
three or four controls plan coefficients in two 32-sample spans before running
the feedback-dependent audio loop. Controls and coefficients still update for
every sample with the original rounding; this does not lower the control rate.
Each span uses 512 bytes of temporary coefficient storage on the stack. When
Glare stays fixed, its table rows and interpolation fraction are prepared once
per block. Sparse automation and the initial transient retain the compact loop.
Regression checks cover all 15 nonempty control subsets, including changes of
the active subset while a note is held.

`tests/analyze_blackfin.py --sdk-root <sdk> --out <fresh-dir>
--approximate-saturation` checks the Linux tool hashes and emits assembly and stack
usage with the SDK's compiler flags. It does not assemble objects or link
firmware. Static instruction counts are not device cycles. The compiler must
be built as described in the SDK's Linux toolchain guide.
`tests/profile_dsp.py` measures host component timings; `tests/paired_cpu.py`
alternates two already-built benchmark directories in two series of five runs.

`tests/approximation_check.py --sdk-root <sdk> --baseline <v1.0.2-source>
--audio-oscillator --out <fresh-dir>` requires relative RMS error at most -55 dB
for each full note, attack and sustain, bounded peaks and exact phase, feedback
and control state. Five-second holds and slow/fast Glare sweeps are also checked.
The explicit `--audio-oscillator` option allows bounded audio differences at
Glare zero; without it, Glare zero must remain bit exact for shaper-only changes.
`tests/automation_quality.py` applies the same limit separately to every one of
900 automation cases and their attack, transient and sustain regions. Its state
check excludes the two output-dependent DC-filter state values.

A/B files are exported only when the note quality gate passes. They play the
reference first and candidate second, with matched RMS and a short gap. These
metrics do not replace listening or hardware CPU profiling.

Regenerate the candidate table from the explicit exact reference:

```sh
python generators/generate_approximation.py --sdk-root <sdk> \
  --reference <exact-nightdrive> --mode dense --drive-bits 5 --amplitude-bits 7 --packed
```

`tests/measure_native_size.py --sdk-root <sdk> --machine <machine> --out
<external-private-directory>` measures owned Blackfin code/data/BSS including
the adapters and all native voice slots, without linking firmware.
Shared SDK overhead and the separate SVG asset are reported separately.
The current native engine budget is at most 90 KiB, including adapters, all
native voice slots and alignment allowance. Installed SVG, shared SDK overhead,
caller stack and preset files are separate from that budget. The size audit
enforces the native limit and also reports the complete installed SVG. `tests/broad_cpu.py --sdk-root <sdk> --build
<cpu-benchmark-directory>` adds all integer Glare values, control corners and
seeded joint patches. These diagnostics do not establish device timing.
