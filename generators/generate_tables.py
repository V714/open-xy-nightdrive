"""Nightdrive FM tables: offline only, no runtime allocation or transcendental calls."""

from pathlib import Path
import json
import hashlib
import numpy as np
from cpp_format import write_header

HERE = Path(__file__).resolve().parent.parent
Q = 2147483647


def main():
    text = "#pragma once\n#include <opxy/types.hpp>\nnamespace nightdrive {\n"

    def table(name, values, ctype="opxy::i32"):
        nonlocal text
        text += (
            "static const "
            + ctype
            + " "
            + name
            + "["
            + str(len(values))
            + "]={"
            + ",".join(str(int(x)) for x in np.rint(values))
            + "};\n"
        )

    x = np.arange(257) / 256
    table("audio_sine_lut", np.sin(np.arange(2048) / 2048 * 2 * np.pi) * 32760, "opxy::i16")
    table(
        "ratio_lut",
        2 ** np.interp(np.arange(101), [0, 5, 10, 25, 50, 75, 100], [-2, -1, 0, 1, 2, 3, 4])
        * (1 << 26),
    )
    ramp = np.maximum(0, (x - 0.7) / 0.3) ** 2
    table("feedback_boost_lut", 2.15 * ramp / (2 * np.pi) * (1 << 32))
    table("modulation_boost_lut", 2 * ramp / (2 * np.pi) * (1 << 31))
    table("tanh_lut", 0.5 * np.tanh(np.arange(1025) / 128) * Q)
    table("drive_comp_lut", (1 / (1 + 0.08 * x)) * Q)
    write_header(HERE / "tables.h", text + "}\n")
    (HERE / "generated").mkdir(exist_ok=True)
    (HERE / "generated/audio_manifest.json").write_text(
        json.dumps(
            dict(
                sample_rate=44100,
                oversampling=1,
                stock_adsr_amplitude=True,
                internal_decay=False,
                max_overdrive_gain=32,
                feedback_max_radians=3,
                ratio_min=0.25,
                ratio_max=16,
                punch_ms=25,
                modulation_transient_ms=80,
                modulation_max_radians=6,
                boost_starts_at=0.7,
                tables_sha256=hashlib.sha256((HERE / "tables.h").read_bytes()).hexdigest(),
            ),
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
