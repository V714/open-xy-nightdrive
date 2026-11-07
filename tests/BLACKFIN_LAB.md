# Offline Blackfin instruction replay

This is a standalone DSP diagnostic, not an OP-XY CPU measurement. It compiles
with the SDK's BF707 flags and executes the resulting instructions in GNU's
classic Blackfin simulator with a small, explicit numerical extension. It does
not model the BF707 pipeline, instruction/data caches, stock voice adapter,
filters, release envelope, interrupts or graphics. Do not convert these counts
to hardware CPU percentages or a catalog CPU record.

`prepare_blackfin_sim.py` copies the supplied GNU source into a fresh directory
outside checkouts. It leaves the SDK and original toolchain unchanged, adds the
SDK's full signed Q31 multiply semantics, and builds the simulator. Unsupported
Blackfin+ instructions still fail. Its test-only counter ABI makes a `CYCLES`
read return executed instruction packets and place data-read/write totals in R1/R2.
DSP code does not read this register. Counter/call overhead is included and is
small and fixed. One parallel instruction bundle counts as one interpreter step. The generated patch and source hashes are saved beside the
simulator. A separate compatibility edit makes old Linux signal-stack metadata
build on current glibc; replay uses libgloss semihosting, not Linux signals.

The assembly startup and linker script in `helpers/blackfin_lab*` are exclusively
for this computer diagnostic. They are not part of the machine or its firmware.

From the SDK checkout, with `<machine>` pointing to this repository and
`<private>` pointing outside every checkout:

```sh
python <machine>/tests/prepare_blackfin_sim.py --sdk-root . \
  --source local/blackfin-plus-gnu/binutils-gdb --out <private>/isa-tool-001
python <machine>/tests/blackfin_replay.py --sdk-root . --machine <machine> \
  --simulator <private>/isa-tool-001/build/sim/bfin/run --out <private>/before-001
python <machine>/tests/blackfin_replay.py --sdk-root . --machine <candidate> \
  --simulator <private>/isa-tool-001/build/sim/bfin/run \
  --baseline <private>/before-001 --out <private>/after-001
```

The replay runs 11,520 blocks: 18 scenarios, notes 24/60/96/108, 160 blocks per
case. Scenarios cover midpoint/default/extreme settings, Speed zero, dry/full and
off-grid Glare, each individual control automated, all controls automated,
ramps, moving pitch, simultaneous pitch/control motion and frequent retriggers.
Each output record includes all 64 audio samples and the complete voice state.
Every native capture must match its own portable C++ reference bit for bit.
Comparisons require exact oscillator phase, feedback and control trajectories;
audio RMS and changed filter history are reported separately. Full sound
acceptance still uses `approximation_check.py` and `automation_quality.py`.

Block 17 crosses Punch's 1103-sample boundary, and block 55 crosses the
3528-sample modulation transient boundary. Reports include these blocks and
the peak block index, rather than timing only an average. A modeled instruction
peak can identify a costly source path; it cannot identify which block caused
the stock hardware timing maximum without a corresponding hardware trace.
