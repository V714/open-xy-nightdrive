"""Alternate already-built SDK benchmark binaries; never builds firmware."""

from pathlib import Path
import argparse
import hashlib
import json
import statistics
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--case", action="append")
    args = parser.parse_args()
    before, after, out = args.before.resolve(), args.after.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    paths = {"before": before, "after": after}
    names = ["standard"] + sorted(p.name for p in before.iterdir()
                                   if p.is_file() and not p.suffix and p.stat().st_mode & 0o111
                                   and (after / p.name).is_file())
    if args.case:
        if any(name not in names for name in args.case):
            parser.error("Unknown or missing benchmark case")
        names = args.case
    report = {"builds": {tag: json.loads((root / "report.json").read_text())
                         for tag, root in paths.items()}, "series": [], "binaries": {},
              "device_cycles_measured": False, "normalization_instructions": 590}
    for tag, root in paths.items():
        report["binaries"][tag] = {}
        for name in names:
            path = root / ("cpu-nightdrive/bench" if name == "standard" else name)
            report["binaries"][tag][name] = hashlib.sha256(path.read_bytes()).hexdigest()
    for series in range(2):
        results = {name: {"before": [], "after": []} for name in names}
        for run in range(5):
            order = ("before", "after") if (run + series) % 2 == 0 else ("after", "before")
            for name in names:
                for tag in order:
                    root = paths[tag]
                    reference = root / "cpu-reference/reference"
                    binary = root / ("cpu-nightdrive/bench" if name == "standard" else name)
                    ref1 = float(subprocess.check_output([str(reference)], text=True))
                    own = float(subprocess.check_output([str(binary)], text=True))
                    ref2 = float(subprocess.check_output([str(reference)], text=True))
                    results[name][tag].append(own / ((ref1 + ref2) / 2) * 590)
        summary = {}
        for name, values in results.items():
            summary[name] = {tag: statistics.median(runs) for tag, runs in values.items()}
            summary[name]["change_percent"] = (summary[name]["after"] / summary[name]["before"] - 1) * 100
        report["series"].append({"runs": results, "medians": summary})
        print("Series", series + 1, json.dumps(summary), flush=True)
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
