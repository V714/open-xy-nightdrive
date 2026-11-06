"""Gate audio approximation under all 900 automation cases, per case and transient region."""

from pathlib import Path
import argparse
import hashlib
import json
import math
import subprocess
import numpy as np
from check import MACHINE, compile_test


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--error-limit-db", type=float, default=-55)
    args = parser.parse_args()
    sdk, baseline, out = args.sdk_root.resolve(), args.baseline.resolve(), args.out.resolve()
    if out.is_relative_to(MACHINE) or out.is_relative_to(baseline):
        parser.error("Keep captures outside source folders")
    out.mkdir(parents=True, exist_ok=False)
    captures = {}
    for tag, source in [("before", baseline), ("after", MACHINE)]:
        binary, path = out / tag, out / (tag + ".q31")
        compile_test(MACHINE / "tests/automation_test.cpp", binary, sdk, args.cxx, source)
        subprocess.run([str(binary), str(path)], check=True)
        captures[tag] = np.memmap(path, dtype="<i4", mode="r")
    expected = 900 * 768 * 80
    if any(v.size != expected for v in captures.values()):
        raise RuntimeError("Automation capture format/case count changed")
    before, after = [captures[tag].reshape(900, 768, 80) for tag in ["before", "after"]]
    peak_limit = json.loads((MACHINE / "definition.json").read_text())["resources"]["outputPeakQ31"]
    report = {
        "relative_error_limit_db": args.error_limit_db,
        "cases": [],
        "failures": [],
        "hardware_tested": False,
        "device_cycles_measured": False,
        "sources": {
            tag: {
                p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(source.iterdir())
                if p.suffix in (".h", ".cpp", ".json")
            }
            for tag, source in [("before", baseline), ("after", MACHINE)]
        },
    }
    for index in range(900):
        a, b = before[index], after[index]
        if not np.array_equal(a[:, 64:78], b[:, 64:78]):
            report["failures"].append(
                {"case": index, "reason": "oscillator, feedback or controls changed"}
            )
        ref, candidate = a[:, :64].astype(np.float64), b[:, :64].astype(np.float64)
        if np.abs(candidate).max() > peak_limit:
            report["failures"].append({"case": index, "reason": "declared output peak exceeded"})
        age = a[:, 67].astype(np.int64)
        previous = np.concatenate([[0], age[:-1]])
        previous = np.where(age < previous, 0, previous)
        ages = previous[:, None] + np.arange(64)[None, :]
        entry = {"case": index}
        for region, mask in [
            ("whole", np.ones_like(ages, dtype=bool)),
            ("attack", ages < 1103),
            ("transient", ages < 3528),
            ("sustain", ages >= 3528),
        ]:
            r, error = ref[mask], (candidate - ref)[mask]
            if not len(r):
                continue
            power, error_power = float(np.dot(r, r)), float(np.dot(error, error))
            db = -300.0 if error_power == 0 else 10 * math.log10(error_power / max(power, 1e-30))
            entry[region + "_error_db"] = db
            if db > args.error_limit_db:
                report["failures"].append(
                    {
                        "case": index,
                        "region": region,
                        "error_db": db,
                        "reason": "relative RMS error above limit",
                    }
                )
        report["cases"].append(entry)
    report["worst_error_db"] = max(
        v for case in report["cases"] for k, v in case.items() if k.endswith("_error_db")
    )
    report["passed"] = not report["failures"]
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: report[k] for k in ["passed", "worst_error_db"]}), flush=True)
    if not report["passed"]:
        raise SystemExit("FAIL automation audio approximation gate")


if __name__ == "__main__":
    main()
