"""Compress the original discrete sine table using exact quarter-wave symmetry."""

from pathlib import Path
import hashlib
import json
import re
from cpp_format import write_header

ROOT = Path(__file__).resolve().parent.parent


def main():
    text = (ROOT / "tables.h").read_text().split("audio_sine_lut[2048]", 1)[1]
    values = [int(v) for v in re.findall(r"-?\d+", text.split("{", 1)[1].split("};", 1)[0])]
    if len(values) != 2048:
        raise RuntimeError("Original sine table size changed")
    quarter = values[:513]
    for position, value in enumerate(values):
        quadrant, index = position >> 9, position & 511
        if quadrant & 1:
            index = 512 - index
        expected = -quarter[index] if quadrant & 2 else quarter[index]
        if value != expected:
            raise RuntimeError("Original discrete table is not exactly symmetric")
    deltas = [quarter[i + 1] - quarter[i] for i in range(512)] + [0]
    if min(deltas) < 0 or max(deltas) > 32767:
        raise RuntimeError("Quarter-wave differences do not fit the packed format")
    packed = [(a & 65535) | (delta << 16) for a, delta in zip(quarter, deltas)]
    source = "#pragma once\n#include <opxy/types.hpp>\nnamespace nightdrive {\n"
    source += (
        "static const opxy::u32 feedback_sine_quarter[513] = {"
        + ",".join(map(str, packed))
        + "};\n}\n"
    )
    header = ROOT / "feedback_sine_quarter.h"
    write_header(header, source)
    manifest = {
        "exact": True,
        "full_cycle_nodes": 2048,
        "quarter_entries": 513,
        "runtime_table_bytes": 2052,
        "reference_packed_table_bytes": 8192,
        "negative_interpolation": "polarity is applied before Q31 truncation",
        "target_tables_sha256": hashlib.sha256((ROOT / "tables.h").read_bytes()).hexdigest(),
        "header_sha256": hashlib.sha256(header.read_bytes()).hexdigest(),
    }
    (ROOT / "generated/feedback_sine.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
