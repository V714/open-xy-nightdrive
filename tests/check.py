"""Run Nightdrive's host checks with an explicit SDK and output directory."""

from pathlib import Path
import argparse
import json
import subprocess

MACHINE = Path(__file__).resolve().parent.parent


def compile_test(source, output, sdk, compiler, source_root=MACHINE):
    command = [
        compiler,
        "-std=c++11",
        "-O2",
        "-DNDEBUG",
        "-fsanitize=undefined",
        "-fno-sanitize-recover=all",
        "-Wall",
        "-Wextra",
        "-I" + str(sdk / "sdk/include"),
        "-I" + str(source_root),
        str(source),
        "-o",
        str(output),
    ]
    subprocess.run(command, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument(
        "--baseline",
        type=Path,
        help="Optional previous machine folder for exact audio/frame comparisons",
    )
    parser.add_argument(
        "--baseline-sdk-root", type=Path, help="SDK used to build the baseline, if different"
    )
    args = parser.parse_args()
    sdk = args.sdk_root.resolve()
    out = args.out.resolve()
    if not (sdk / "sdk/include/opxy/audio.hpp").is_file():
        parser.error("--sdk-root must point to an Open-XY SDK checkout")
    if out.is_relative_to(MACHINE):
        parser.error("Keep generated checks outside the machine folder")
    out.mkdir(parents=True, exist_ok=False)
    report = {"hardware_tested": False, "device_cycles_measured": False, "checks": {}}
    for name in ["range_test", "envelope_test", "display_test", "sound_test", "response_test", "saturation_test", "table_differences_test", "shaper_table_test"]:
        binary = out / name
        compile_test(MACHINE / "tests" / (name + ".cpp"), binary, sdk, args.cxx)
        command = [str(binary)]
        if name == "sound_test":
            command.append(str(out / "audition.q31"))
        result = subprocess.run(command, check=True, text=True, capture_output=True)
        print(result.stdout, end="")
        report["checks"][name] = result.stdout.strip()
    from verify_sound import verify

    report["sound"] = verify(sdk, out / "quality", args.cxx)
    if args.baseline:
        from regression import compare

        report["regression"] = compare(
            args.baseline.resolve(),
            sdk,
            out / "regression",
            args.cxx,
            args.baseline_sdk_root.resolve() if args.baseline_sdk_root else None,
        )
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("PASS Nightdrive host checks:", out)


if __name__ == "__main__":
    main()
