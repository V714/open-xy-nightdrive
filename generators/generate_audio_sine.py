"""Fit a bounded audio-only sine polynomial; feedback stays on the exact lookup."""

from pathlib import Path
import hashlib
import json
import re
import numpy as np
from cpp_format import write_header

ROOT = Path(__file__).resolve().parent.parent


def main():
    text = (ROOT / "tables.h").read_text().split("audio_sine_lut[2048]", 1)[1]
    values = np.array(
        [int(v) for v in re.findall(r"-?\d+", text.split("{", 1)[1].split("};", 1)[0])]
    )
    x = np.linspace(0, 1, 65537)
    target = np.interp(x * 512, np.arange(513), values[:513]) / 65536
    amplitude = float(values[512] / 65536)
    fit = np.linalg.lstsq(
        np.stack([x - x**5, x**3 - x**5], axis=1), target - amplitude * x**5, rcond=None
    )[0]
    coefficients = [float(fit[0]), float(fit[1]), amplitude - float(fit.sum())]
    fixed = [int(round(v * 2147483648)) for v in coefficients]
    header = ROOT / "audio_sine_coefficients.h"
    source = "#pragma once\n#include <opxy/types.hpp>\nnamespace nightdrive {\n"
    source += "".join(
        "static const opxy::i32 audio_sine_" + name + " = " + str(value) + ";\n"
        for name, value in zip("abc", fixed)
    )
    write_header(header, source + "}\n")
    manifest = {
        "experimental": True,
        "kind": "branch-free odd degree-five quarter-wave polynomial, output carriers only",
        "coefficients_q31": fixed,
        "fit_points": 65537,
        "fit_constraint": "original quarter-wave endpoint amplitude",
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
