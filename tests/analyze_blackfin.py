"""Hash-checked Blackfin assembly only: no objects, linking, firmware or device."""

from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import sys

WRAPPER = '''#include "engine.cpp"
extern "C" void nightdrive_analysis_render(nightdrive::Voice* voice, nightdrive::Shared* shared,
    opxy::i32* output, const opxy::i32* parameters, const opxy::u32* pitch) {
    nightdrive::render(*voice, *shared, 1, output, parameters, pitch);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--machine", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--prepared-saturation", action="store_true")
    parser.add_argument("--approximate-saturation", action="store_true")
    args = parser.parse_args()
    sdk, machine, out = args.sdk_root.resolve(), args.machine.resolve(), args.out.resolve()
    if out.is_relative_to(machine):
        parser.error("Keep generated assembly outside the machine folder")
    sys.path.insert(0, str(sdk))
    from tooling.native import CXX_FLAGS, compiler_paths
    from tooling.pipeline import tool_paths

    compiler = (args.compiler or sdk / "local/toolchain").resolve()
    cxx = compiler_paths(compiler)["cxx"]
    tool_paths(compiler / "bin")
    out.mkdir(parents=True, exist_ok=False)
    unit, assembly = out / "render.cpp", out / "render.s"
    unit.write_text(WRAPPER)
    command = [str(cxx), *CXX_FLAGS, "-I" + str(sdk / "sdk/include"),
               "-I" + str(sdk / "sdk/platform/blackfin/freestanding"),
               "-I" + str(machine), "-fstack-usage", "-S", str(unit), "-o", str(assembly)]
    subprocess.run(command, check=True, capture_output=True, text=True)
    text = assembly.read_text()
    lines = [line.strip() for line in text.splitlines()
             if line.startswith("\t") and not line.lstrip().startswith((".", "//", "/*"))]
    report = {"machine": str(machine), "command": command,
              "sources": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted(machine.iterdir()) if p.suffix in (".h", ".cpp", ".json")},
              "assembly_sha256": hashlib.sha256(assembly.read_bytes()).hexdigest(),
              "static_instruction_lines": len(lines),
              "calls": [line for line in lines if re.search(r"\bCALL\b", line, re.I)],
              "q31_multiply_sites": sum("(T)" in line for line in lines),
              "branch_sites": sum(bool(re.search(r"\b(IF|JUMP)\b", line, re.I)) for line in lines),
              "stack_usage": [p.read_text() for p in out.glob("*.su")],
              "static_counts_are_not_dynamic_cycles": True, "firmware_built": False,
              "device_cycles_measured": False}
    helpers = out / "helpers.cpp"
    call = "x, drive, squared, compensation" + (", clipped" if args.prepared_saturation or args.approximate_saturation else "")
    function = "saturate_approx" if args.approximate_saturation else "saturate"
    report["helper_function"] = function
    helpers.write_text('#include "dsp.h"\n'
        'extern "C" opxy::i32 analysis_sine(opxy::u32 phase) { return nightdrive::dsp::sine(phase); }\n'
        'extern "C" opxy::i32 analysis_saturate(opxy::i32 x, opxy::i32 drive, opxy::i32 squared, '
        'opxy::i32 compensation, opxy::i32 clipped) { return nightdrive::dsp::' + function + '(' + call + '); }\n')
    helper_command = command.copy()
    helper_command[helper_command.index(str(unit))] = str(helpers)
    helper_command[-1] = str(out / "helpers.s")
    subprocess.run(helper_command, check=True, capture_output=True, text=True)
    report["helpers_assembly_sha256"] = hashlib.sha256((out / "helpers.s").read_bytes()).hexdigest()
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: report[k] for k in ("static_instruction_lines", "q31_multiply_sites", "calls")}, indent=2))


if __name__ == "__main__":
    main()
