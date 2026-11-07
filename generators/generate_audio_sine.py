"""Fit small audio-only quadratic segments; feedback stays on the exact lookup."""

from pathlib import Path
import hashlib
import json
import re
import numpy as np
from cpp_format import write_header

ROOT = Path(__file__).resolve().parent.parent
SEGMENT_BITS = 4


def main():
    text = (ROOT / "tables.h").read_text().split("audio_sine_lut[2048]", 1)[1]
    values = np.array(
        [int(v) for v in re.findall(r"-?\d+", text.split("{", 1)[1].split("};", 1)[0])]
    )
    count, segments = 1 << SEGMENT_BITS, []
    u = np.linspace(0, 1, 4097)
    basis = u * (u - 1)
    for i in range(count):
        target = np.interp((i + u) * 512 / count, np.arange(513), values[:513]) / 65536
        start, end = float(values[i * 512 // count] / 65536), float(
            values[(i + 1) * 512 // count] / 65536
        )
        curve = float(np.dot(basis, target - (start + (end - start) * u)) / np.dot(basis, basis))
        base, quadratic = int(round(start * 2147483648)), int(round(curve * 2147483648))
        linear = int(round(end * 2147483648)) - base - quadratic
        segments.append([base, linear, quadratic])
    header = ROOT / "audio_sine_coefficients.h"
    source = "#pragma once\n#include <opxy/types.hpp>\nnamespace nightdrive {\n"
    source += "static const unsigned audio_sine_segment_bits = " + str(SEGMENT_BITS) + ";\n"
    source += "struct AudioSineSegment { opxy::i32 base, linear, quadratic; };\n"
    source += "static const AudioSineSegment audio_sine_segments[" + str(count) + "] = {"
    source += ",".join("{" + ",".join(map(str, row)) + "}" for row in segments) + "};\n}\n"
    write_header(header, source)
    manifest = {
        "experimental": True,
        "kind": "branch-free quadratic quarter-wave segments, output carriers only",
        "segment_bits": SEGMENT_BITS,
        "segments_q31": segments,
        "runtime_table_bytes": count * 12,
        "fit_points_per_segment": 4097,
        "fit_constraint": "original endpoints of each segment",
        "target_tables_sha256": hashlib.sha256((ROOT / "tables.h").read_bytes()).hexdigest(),
        "feedback_oscillator": "original exact packed lookup",
        "phase_and_control_state": "exact",
        "relative_rms_error_limit_db": -55,
        "header_sha256": hashlib.sha256(header.read_bytes()).hexdigest(),
    }
    (ROOT / "generated/audio_sine_approximation.json").write_text(
        json.dumps(manifest, indent=2) + "\n"
    )


if __name__ == "__main__":
    main()
