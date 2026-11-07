"""Build a private, numerically extended GNU ISA diagnostic; never a BF707 timing model."""

from pathlib import Path
import argparse
import difflib
import hashlib
import json
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.sdk_root.resolve()))
    from tooling.release import outside_checkouts

    out, source = args.out.resolve(), args.source.resolve()
    if not outside_checkouts(out):
        parser.error('Build the simulator outside every checkout')
    out.mkdir(exist_ok=False)
    copied = out / 'gnu-source'
    shutil.copytree(source, copied)
    path = copied / 'sim/bfin/bfin-sim.c'
    original = path.read_text()
    text = original
    start = text.index('decode_dsp32mult_0 (SIM_CPU')
    position = text.index('  if (w1 == 0 && w0 == 0)', start)
    extension = '''  /* Offline numerical BF707 extension. No BF707 timing/cache model. */
  if (op1 && w0 && MM && !P && op0 == 0 && mmod == M32_T)
    {
      bs64 product = (bs64)(bs32)DREG (src0) * (bs64)(bs32)DREG (src1);
      bs64 scale = (bs64)1 << 31;
      bs64 value = product / scale;
      if (product < 0 && product % scale) --value;
      if (value > 2147483647LL) value = 2147483647LL;
      STORE (DREG (dst), (bu32)value);
      TRACE_INSN (cpu, "local BF707 Q31 multiply");
      return;
    }

'''
    text = text[:position] + extension + text[position:]
    needle = '  value = *whichreg;\n\n  if (whichreg == &CYCLESREG)'
    if text.count(needle) != 1:
        raise RuntimeError('Simulator register decoder changed')
    text = text.replace(needle, '''  /* Test-only counter ABI: CYCLES returns instructions and sets R1/R2
     to data read/write totals. DSP code never reads this register. */
  if (whichreg == &CYCLESREG)
    {
      DREG (1) = PROFILE_CORE_COUNT (CPU_PROFILE_DATA (cpu))[read_map];
      DREG (2) = PROFILE_CORE_COUNT (CPU_PROFILE_DATA (cpu))[write_map];
      return (bu32)PROFILE_TOTAL_INSN_COUNT (CPU_PROFILE_DATA (cpu));
    }
  value = *whichreg;

  if (whichreg == &CYCLESREG)''', 1)
    path.write_text(text)
    # Old Linux syscall metadata assumes SIGSTKSZ is constant. This ELF replay
    # uses libgloss semihosting, never the Linux signal-stack interface.
    signals = copied / 'sim/bfin/linux-targ-map.h'
    signals.write_text(signals.read_text().replace(
        '{ "SIGSTKSZ", SIGSTKSZ, TARGET_LINUX_SIGSTKSZ }',
        '{ "SIGSTKSZ", 8192, TARGET_LINUX_SIGSTKSZ }'))
    (out / 'diagnostic.patch').write_text(''.join(difflib.unified_diff(
        original.splitlines(True), text.splitlines(True),
        fromfile='sim/bfin/bfin-sim.c', tofile='sim/bfin/bfin-sim.c')))
    build = out / 'build'
    build.mkdir()
    flags = ('-O2 -std=gnu11 -fcommon -D_GNU_SOURCE '
             '-Wno-error=implicit-function-declaration '
             '-Wno-error=incompatible-pointer-types')
    command = [str(copied / 'configure'), '--target=bfin-elf', '--disable-gdb',
               '--disable-binutils', '--disable-gas', '--disable-ld',
               '--disable-gprof', '--disable-nls', '--disable-werror',
               '--enable-sim', '--enable-sim-profile', '--enable-sim-trace',
               '--without-guile', 'CFLAGS=' + flags]
    with (out / 'configure.log').open('w') as log:
        subprocess.run(command, cwd=build, stdout=log, stderr=log, check=True)
    with (out / 'make.log').open('w') as log:
        subprocess.run(['make', '-j4', 'CFLAGS=' + flags, 'all-sim'], cwd=build,
                       stdout=log, stderr=log, check=True)
    metadata = {
        'counter_contract': 'nightdrive-isa-lab-v1',
        'source_bfin_sim_sha256': hashlib.sha256(original.encode()).hexdigest(),
        'patched_bfin_sim_sha256': hashlib.sha256(text.encode()).hexdigest(),
        'bf707_timing_model': False,
        'device_operations': False,
        'simulator': str(build / 'sim/bfin/run'),
    }
    (out / 'manifest.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(metadata['simulator'])


if __name__ == '__main__':
    main()
