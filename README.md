# Nightdrive

Version **1.0.0** · Author and maintainer: **Tomasz Lipczyński (V7)** · GitHub: [**V714**](https://github.com/V714) · MIT licensed.

A four-control feedback FM bass synthesizer for OP-XY, with an animated isometric night-drive scene.
The source targets Open-XY SDK 1.1 and later compatible 1.x releases, for stock OS 1.2.0. Audio is mono Q31 at 44.1 kHz, in 64-sample
blocks, without oversampling. This folder is the canonical source for further Nightdrive development.

## Controls

| Control | Sound                                       | Graphics                           |
| ------- | ------------------------------------------- | ---------------------------------- |
| Speed   | Continuous operator ratio, 0.25–16          | Road, streetlamp and wheel speed   |
| Tension | Modulator feedback, up to 3 radians         | Moves the entire car forward       |
| Drift   | Phase-modulation depth, up to 6 radians     | Moves the car toward the left lane |
| Glare   | Compensated overdrive up to 32×, plus Punch | Brightens the four windows         |

Tension and Drift extend their upper ranges mainly between 70 and 100. The lower ranges retain their
original response. Twenty percent clean fundamental reinforces the bass. FM and feedback decrease
at high frequencies to limit nonlinear bandwidth. A note starts a finite 80 ms modulation transient controlled by Drift and a 25 ms
pre-overdrive Punch transient controlled by Glare. At zero, each corresponding transient is bypassed.
There is no internal amplitude decay: note length and sustain follow the device's stock ADSR. There are no alternate controls.
No named presets are distributed. The four control defaults provide the initial sound.
The SDK builder may generate its standard init preset from these defaults.

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
The machine is at the repository root; its catalog path is `.`. Minimum supported SDK: **1.1**.
CI checks against Open-XY staging commit `fdf48e583ebb19fb5cd191687229bf77107f1aed`
(SDK 1.2, compatible with this machine's SDK 1.1 contract).

Install the tools described in the SDK's
[first-machine guide](https://github.com/etcetc000/open-xy/blob/staging/docs/first-machine.md).
Keep the SDK in a separate sibling folder. From this repository root:

```sh
git clone https://github.com/etcetc000/open-xy.git ../open-xy-sdk
git -C ../open-xy-sdk checkout fdf48e583ebb19fb5cd191687229bf77107f1aed
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
256 voice indices, extreme controls and pitches, and 24,000 drawing frames. They do not measure OP-XY
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
