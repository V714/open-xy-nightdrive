"""Host component timings, in nanoseconds per 64 calls, not device cycles."""

from pathlib import Path
import argparse
import json
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--machine", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--prepared-saturation", action="store_true")
    parser.add_argument("--approximate-saturation", action="store_true")
    args = parser.parse_args()
    out, machine = args.out.resolve(), args.machine.resolve()
    if out.is_relative_to(machine):
        parser.error("Keep generated results outside the source folder")
    out.mkdir(parents=True, exist_ok=False)
    source = (Path(__file__).parent / "helpers/profile_dsp.cpp").read_text()
    if args.prepared_saturation or args.approximate_saturation:
        source = source.replace("const auto compensation = dsp::lut(drive_comp_lut, drive);",
                                "const auto compensation = dsp::lut(drive_comp_lut, drive);\n"
                                "const auto clipped = dsp::multiply(tanh_clipped, compensation);")
        source = source.replace("drive, squared, compensation);", "drive, squared, compensation, clipped);")
        if "compensation, clipped);" not in source:
            raise RuntimeError("Profiler template changed")
    if args.approximate_saturation:
        source = source.replace("dsp::saturate(", "dsp::saturate_approx(")
    unit, binary = out / "profile.cpp", out / "profile"
    unit.write_text(source)
    subprocess.run(["/usr/bin/c++", "-std=c++11", "-O2",
                    "-I" + str(args.sdk_root.resolve() / "sdk/include"),
                    "-I" + str(machine), str(unit), "-o", str(binary)], check=True)
    result = subprocess.check_output([str(binary)], text=True)
    print(result, end="")
    report = {line.split()[0]: float(line.split()[1]) for line in result.splitlines()}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
