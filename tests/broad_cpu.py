"""Extend an SDK host benchmark with all integer Glare values and joint patches."""

from pathlib import Path
import argparse
import hashlib
import json
import subprocess

EXTRA = r'''
    for (int step = 0; step <= 100; ++step) {
        for (int k = 0; k < 8; ++k) out[count][k] = defaults[k];
        out[count][3] = opxy::i32(2147483647LL * step / 100);
        ++count;
    }
    for (unsigned mask = 0; mask < 16; ++mask) {
        for (int k = 0; k < 8; ++k) out[count][k] = k < 4 && (mask & (1u << k)) ? 2147483647 : 0;
        ++count;
    }
    unsigned random = 17;
    for (int patch = 0; patch < 32; ++patch) {
        for (int k = 0; k < 8; ++k) {
            random = random * 1664525u + 1013904223u;
            out[count][k] = k < 4 ? opxy::i32(random >> 1) : 0;
        }
        ++count;
    }
    return count;
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--build", type=Path, action="append", required=True)
    args = parser.parse_args()
    for root in args.build:
        root = root.resolve()
        report = json.loads((root / "report.json").read_text())
        machine = Path(report["machine"])
        for name, digest in report["sources"].items():
            if hashlib.sha256((machine / name).read_bytes()).hexdigest() != digest:
                raise RuntimeError("Source changed since benchmark build: " + name)
        source = (root / "cpu-nightdrive/bench.cpp").read_text()
        if source.count("return count;") != 1 or "all[setting_count][8]" not in source:
            raise RuntimeError("SDK template changed")
        source = source.replace("return count;", EXTRA, 1).replace("all[setting_count][8]", "all[512][8]")
        unit = root / "broad.cpp"
        unit.write_text(source)
        subprocess.run(["/usr/bin/c++", "-std=c++11", "-O2",
                        "-I" + str(args.sdk_root.resolve() / "sdk/include"), "-I" + str(machine),
                        str(unit), "-o", str(root / "broad")], check=True)
        print("Built broad steady benchmark:", root, flush=True)


if __name__ == "__main__":
    main()
