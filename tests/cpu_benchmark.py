"""Repeat the staging SDK's unmodified host estimator; no firmware or Wine."""

from pathlib import Path
import argparse
import hashlib
import json
import statistics
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--machine", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--cxx", default="/usr/bin/c++")
    parser.add_argument("--diagnostics", action="store_true",
                        help="also compare frequent attacks, warmed sustain and automation")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    sdk = args.sdk_root.resolve()
    sys.path.insert(0, str(sdk))
    from tooling import cpu
    from tooling.machines import machine_at

    machine = machine_at(args.machine.resolve())
    if machine.id != "nightdrive" or machine.kind != "synth":
        parser.error("This benchmark is for the Nightdrive synth")
    args.out = args.out.resolve()
    if args.out.is_relative_to(machine.root):
        parser.error("Keep generated benchmarks outside the machine folder")
    args.out.mkdir(parents=True, exist_ok=False)
    # Build using the SDK itself, then time these same binaries in fresh processes.
    cpu.block_cost(machine, args.cxx, args.out)
    cpu.reference_cost(args.cxx, args.out)
    own = args.out / ("cpu-" + machine.id) / "bench"
    reference = args.out / "cpu-reference/reference"
    diagnostic_binaries = {}
    if args.diagnostics:
        source = (own.parent / "bench.cpp").read_text()
        prefix = machine.id + "::"
        render = prefix + "render(voice, shared, 1, output, parameters, pitch);"
        start = prefix + "start(voice, shared, 1);"
        if source.count(render) != 1 or source.count("[&] { " + start + " }") != 1:
            raise RuntimeError("SDK benchmark template changed; update the diagnostic variants")
        variants = {
            "attack": source.replace(render, "if ((b & 7) == 0) " + start + "\n        " + render),
            "sustain": source.replace("setup();", "setup(all[s]);").replace(
                "[&] { " + start + " }",
                "[&](const opxy::i32* parameters) { " + start +
                " for (int warm = 0; warm < 256; ++warm) { " + render + " } }"),
            "automation": source.replace(render,
                "opxy::i32 moving[8]; for (int k = 0; k < 8; ++k) "
                "moving[k] = (b & 8) ? 2147483647 - parameters[k] : parameters[k];\n"
                "        for (int i = 0; i < 64; ++i) pitch[i] = 42852281u + (b % 17) * 9000u + i * 320u;\n"
                "        " + render.replace("output, parameters, pitch", "output, moving, pitch")),
        }
        for control, name in enumerate(("speed", "tension", "drift", "glare")):
            variants["automation-" + name] = source.replace(render,
                "opxy::i32 moving[8]; for (int k = 0; k < 8; ++k) moving[k] = parameters[k];\n"
                f"        if (b & 8) moving[{control}] = 2147483647 - moving[{control}];\n"
                "        " + render.replace("output, parameters, pitch", "output, moving, pitch"))
        variants["varying-pitch"] = source.replace(render,
            "for (int i = 0; i < 64; ++i) pitch[i] = 42852281u + (b % 17) * 9000u + i * 320u;\n"
            "        " + render)
        for control, name in ((2, "zero-modulation"), (3, "zero-glare")):
            variants[name] = source.replace(render,
                "opxy::i32 clean[8]; for (int k = 0; k < 8; ++k) clean[k] = parameters[k];\n"
                f"        clean[{control}] = 0;\n"
                "        " + render.replace("output, parameters, pitch", "output, clean, pitch"))
        for name, pitch_code in (
            ("high-note-cutoff", "for (int i = 0; i < 64; ++i) pitch[i] = 85704563u;"),
            ("bandwidth-crossings", "for (int i = 0; i < 64; ++i) pitch[i] = 50331648u + ((i & 1) ? 64u : -64u);"),
        ):
            variants[name] = source.replace(render,
                "opxy::i32 high[8]; for (int k = 0; k < 8; ++k) high[k] = parameters[k];\n"
                "        high[0] = 2147483647; " + pitch_code + "\n        " +
                render.replace("output, parameters, pitch", "output, high, pitch"))
        variants["non-grid-glare"] = source.replace(render,
            "opxy::i32 nongrid[8]; for (int k = 0; k < 8; ++k) nongrid[k] = parameters[k];\n"
            "        nongrid[3] = 861644443;\n        " +
            render.replace("output, parameters, pitch", "output, nongrid, pitch"))
        for name, text in variants.items():
            unit, binary = args.out / (name + ".cpp"), args.out / name
            unit.write_text(text)
            subprocess.run([args.cxx, "-std=c++11", "-O2", "-I" + str(sdk / "sdk/include"),
                            "-I" + str(machine.root), str(unit), "-o", str(binary)], check=True)
            diagnostic_binaries[name] = binary
    runs = []
    for _ in range(args.runs):
        ref = float(subprocess.check_output([str(reference)], text=True))
        ns = float(subprocess.check_output([str(own)], text=True))
        estimate = ns / ref * cpu.REFERENCE_INSTRUCTIONS
        runs.append(dict(host_ns_per_block=ns, reference_ns_per_block=ref,
                         estimated_instructions=estimate))
        print(f"CPU estimate: {estimate:,.0f} instructions/voice/block", flush=True)
    values = [r["estimated_instructions"] for r in runs]
    report = dict(machine=str(machine.root), sdk=str(sdk),
                  sdk_commit=subprocess.check_output(["git", "-C", str(sdk), "rev-parse", "HEAD"], text=True).strip(),
                  compiler=subprocess.check_output([args.cxx, "--version"], text=True).splitlines()[0],
                  sources={str(p.relative_to(machine.root)): hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in sorted(machine.root.glob("*")) if p.suffix in (".h", ".cpp", ".json")},
                  runs=runs, median=statistics.median(values), minimum=min(values), maximum=max(values),
                  device_cycles_measured=False, hardware_tested=False)
    report["diagnostics"] = {}
    for name, binary in diagnostic_binaries.items():
        samples = []
        for _ in range(args.runs):
            ref = float(subprocess.check_output([str(reference)], text=True))
            ns = float(subprocess.check_output([str(binary)], text=True))
            samples.append(ns / ref * cpu.REFERENCE_INSTRUCTIONS)
        report["diagnostics"][name] = dict(median=statistics.median(samples),
                                           minimum=min(samples), maximum=max(samples), runs=samples)
        print(f"Diagnostic {name}: median {statistics.median(samples):,.0f}", flush=True)
    (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Median {report['median']:,.0f}; range {min(values):,.0f}–{max(values):,.0f}")


if __name__ == "__main__":
    main()
