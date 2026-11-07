"""Replay BF707-compiled DSP in the explicitly patched offline ISA simulator.

This is NOT a BF707 cache/pipeline model or a hardware CPU measurement.
See BLACKFIN_LAB.md for the required diagnostic-counter simulator contract.
"""
from pathlib import Path
import argparse, subprocess, sys, json, hashlib
import numpy as np

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--sdk-root', type=Path, required=True)
    p.add_argument('--machine', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--simulator', type=Path, required=True)
    p.add_argument('--baseline', type=Path)
    a = p.parse_args()
    sdk = a.sdk_root.resolve()
    machine = a.machine.resolve()
    out = a.out.resolve()
    lab = Path(__file__).resolve().parent / 'helpers'
    sys.path.insert(0, str(sdk))
    from tooling.native import CXX_FLAGS, compiler_paths, compile_cxx
    from tooling.pipeline import tool_paths
    from tooling.release import outside_checkouts
    if not outside_checkouts(out):
        p.error('Keep native objects and captures outside every checkout')
    out.mkdir(exist_ok=False)
    tools = tool_paths(sdk / 'local/toolchain/bin')
    cxx = compiler_paths(sdk / 'local/toolchain')['cxx']
    subprocess.run([str(tools['as']), '-mcpu=bf707', str(lab / 'blackfin_lab_runtime.S'), '-o', str(out / 'runtime.o')], check=True)
    for source in ('blackfin_replay.cpp', 'blackfin_native_runtime.cpp'):
        compile_cxx(cxx, tools['as'], lab / source, out / (source + '.o'),
                    [sdk / 'sdk/include', sdk / 'sdk/platform/blackfin/freestanding', machine])
    subprocess.run([str(tools['ld']), '-T', str(lab / 'blackfin_lab.ld'), str(out / 'runtime.o'), str(out / 'blackfin_replay.cpp.o'), str(out / 'blackfin_native_runtime.cpp.o'), '-o', str(out / 'replay.elf')], check=True)
    subprocess.run(['g++', '-O2', '-std=c++11', '-I' + str(sdk / 'sdk/include'), '-I' + str(machine), str(lab / 'blackfin_replay.cpp'), str(lab / 'blackfin_host_runtime.cpp'), '-o', str(out / 'replay-host')], check=True)
    with (out / 'host.bin').open('wb') as f:
        subprocess.run([str(out / 'replay-host')], stdout=f, check=True)
    with (out / 'native.bin').open('wb') as f, (out / 'native.log').open('w') as log:
        subprocess.run([str(a.simulator.resolve()), '--profile-core', '--profile-file', str(out / 'profile.txt'), str(out / 'replay.elf')], stdout=f, stderr=log, check=True)
    native = np.fromfile(out / 'native.bin', dtype='<u4').reshape(-1, 89)
    host = np.fromfile(out / 'host.bin', dtype='<u4').reshape(-1, 89)
    if native.shape != (11520, 89) or not np.array_equal(native[:, 9:], host[:, 9:]):
        raise RuntimeError('Native ISA audio/state differs from portable reference')
    if native[:, 6:9].min() == 0:
        raise RuntimeError('Counters inactive')
    report = dict(metric='Executed standalone ISA instruction packets (parallel bundle counts once) and data accesses; includes fixed counter/call overhead; no BF707 pipeline/cache, stock processing or hardware CPU percentages', device_cycles_measured=False, compiler_flags=CXX_FLAGS, simulator_sha256=hashlib.sha256(a.simulator.resolve().read_bytes()).hexdigest(), replay_source_sha256=hashlib.sha256((lab / 'blackfin_replay.cpp').read_bytes()).hexdigest(), native_host_exact=True, blocks=11520, machine=str(machine), sources={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(machine.iterdir()) if p.suffix in ('.h', '.cpp', '.json')}, cases=[])
    for scenario in range(18):
        for note in (24, 60, 96, 108):
            x = native[(native[:, 0] == scenario) & (native[:, 1] == note)]
            j = int(x[:, 6].argmax())
            report['cases'].append(dict(scenario=scenario, note=note, peak_block=int(x[j, 2]), peak_instructions=int(x[j, 6]), peak_reads=int(x[j, 7]), peak_writes=int(x[j, 8]), first_instructions=int(x[0, 6]), punch_boundary_instructions=int(x[17, 6]), transient_boundary_instructions=int(x[55, 6]), sustain_median_instructions=float(np.median(x[80:, 6]))))
    if a.baseline:
        baseline = np.fromfile(a.baseline.resolve() / 'native.bin', dtype='<u4').reshape(-1, 89)
        if baseline.shape != native.shape or not np.array_equal(baseline[:, :6], native[:, :6]):
            raise RuntimeError('Baseline workloads differ')
        report['phase_feedback_controls_exact'] = bool(np.array_equal(baseline[:, 9:23], native[:, 9:23]))
        report['audio_state_bit_exact'] = bool(np.array_equal(baseline[:, 9:], native[:, 9:]))
        report['worst_rms_error_db'] = -300.0
        for row in report['cases']:
            mask = (native[:, 0] == row['scenario']) & (native[:, 1] == row['note'])
            before = baseline[mask]
            after = native[mask]
            reference = before[:, 25:].view('<i4').astype(np.float64)
            error = after[:, 25:].view('<i4').astype(np.float64) - reference
            row['rms_error_db'] = -300.0 if not np.any(error) else float(10 * np.log10(np.sum(error ** 2) / max(np.sum(reference ** 2), 1e-30)))
            row['peak_instruction_change_percent'] = float(100 * (after[:, 6].max() / before[:, 6].max() - 1))
            report['worst_rms_error_db'] = max(report['worst_rms_error_db'], row['rms_error_db'])
        if not report['phase_feedback_controls_exact']:
            raise RuntimeError('Phase, feedback or control trajectory changed')
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(dict(native_host_exact=True, blocks=11520, max_instructions=int(native[:, 6].max()))), flush=True)
    for row in report['cases']:
        if row['note'] == 60:
            print(row, flush=True)
if __name__ == '__main__':
    main()
