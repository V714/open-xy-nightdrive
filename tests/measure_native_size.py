"""Measure the SDK-owned engine contribution from isolated Blackfin objects.

Objects must stay outside every checkout. No linking, stock OS or firmware image.
"""
from pathlib import Path
import argparse
import hashlib
import json
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--machine", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    sdk, out = args.sdk_root.resolve(), args.out.resolve()
    sys.path.insert(0, str(sdk))
    from tooling import adapters
    from tooling.machines import machine_at
    from tooling.native import compiler_paths, compile_cxx, includes, object_memory
    from tooling.pipeline import tool_paths
    from tooling.formats import elf_sections
    from tooling.release import outside_checkouts

    if not outside_checkouts(out):
        parser.error("Keep native objects outside every checkout")
    machine = machine_at(args.machine.resolve())
    if machine.kind != "synth" or machine.sources:
        parser.error("This size audit expects a single-translation-unit synth")
    cxx = compiler_paths(sdk / "local/toolchain")["cxx"]
    tools = tool_paths(sdk / "local/toolchain/bin")
    out.mkdir(parents=True, exist_ok=False)
    adapters.generate_display(out, machine=machine)
    sources = {"voice": adapters.generate_voice(out, machine),
               "display": out / "display_binding.cpp",
               "screen": adapters.generate_screen(out, machine),
               "registration": adapters.generate_registration(out, machine)}
    report = {"machine": str(machine.root), "units": {}, "initialized_bytes": 0,
              "state_bytes": 0, "alignment_allowance": 0, "firmware_built": False,
              "metric": "SDK engine-owned code/data/BSS, including native adapters and all voice slots; excludes shared SDK and SVG",
              "sources": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted(machine.root.iterdir()) if p.suffix in (".h", ".cpp", ".json")}}
    for name, source in sources.items():
        obj = out / (name + ".o")
        compile_cxx(cxx, tools["as"], source, obj, includes(machine))
        data = obj.read_bytes()
        initialized, state = object_memory(data)
        allowance = sum(max(0, h[8] - 1) for _, h in elf_sections(data) if h[2] & 2 and h[5])
        report["units"][name] = {"initialized_bytes": initialized, "state_bytes": state,
                                  "alignment_allowance": allowance}
        report["initialized_bytes"] += initialized
        report["state_bytes"] += state
        report["alignment_allowance"] += allowance
    report["allocated_bytes"] = report["initialized_bytes"] + report["state_bytes"]
    report["conservative_bytes"] = report["allocated_bytes"] + report["alignment_allowance"]
    report["within_70_kib"] = report["conservative_bytes"] <= 70 * 1024
    report["svg_source_bytes_separate"] = (machine.root / "panel.svg.fragment").stat().st_size
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: report[k] for k in ("allocated_bytes", "conservative_bytes", "within_70_kib")}), flush=True)


if __name__ == "__main__":
    main()
