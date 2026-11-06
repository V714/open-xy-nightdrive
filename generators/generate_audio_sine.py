"""Generate an audio-only nearest-phase quarter-wave sine table; feedback stays exact."""

from pathlib import Path
import hashlib
import json
import numpy as np
from cpp_format import write_header

ROOT = Path(__file__).resolve().parent.parent
PHASE_BITS = 13


def main():
    count = 1 << (PHASE_BITS - 2)
    values = np.rint(np.sin(np.arange(count + 1) / count * np.pi / 2) * 32760).astype(np.int64)
    text = "#pragma once\n#include <opxy/types.hpp>\nnamespace nightdrive {\n"
    text += "static const opxy::i16 audio_sine_quarter_lut[" + str(count + 1) + "] = {"
    text += ",".join(map(str, values)) + "};\n}\n"
    header = ROOT / "audio_sine_lut.h"
    write_header(header, text)
    manifest = {
        "experimental": True,
        "kind": "nearest phase, quarter-wave symmetry, output carriers only",
        "phase_bits": PHASE_BITS,
        "quarter_nodes": count + 1,
        "table_bytes": (count + 1) * 2,
        "peak_q15": 32760,
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
