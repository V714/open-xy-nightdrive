"""Initial-sound, control-response and held-tone checks; no hardware timing claims."""

from pathlib import Path
import json
import subprocess

import numpy as np

MACHINE = Path(__file__).resolve().parent.parent


def verify(sdk, out, compiler):
    from check import compile_test

    out.mkdir(parents=True, exist_ok=False)
    binary = out / "render"
    compile_test(MACHINE / "tests/helpers/quality_render.cpp", binary, sdk, compiler)
    definition = json.loads((MACHINE / "definition.json").read_text())
    ids = [parameter["id"] for parameter in definition["parameters"]]
    if ids != ["speed", "tension", "drift", "glare"]:
        raise RuntimeError("Nightdrive requires exactly four controls")
    defaults = {parameter["id"]: parameter["default"] for parameter in definition["parameters"]}
    peak = definition["resources"]["outputPeakQ31"] / 2147483648

    def render(values, tag, blocks=690):
        parameters = [values[key] for key in ids] + [0] * (8 - len(ids))
        raw = out / (tag + ".q31")
        subprocess.run(
            [str(binary), *map(str, parameters), "6384029", str(blocks), str(raw)], check=True
        )
        return np.fromfile(raw, dtype="<i4").astype(float) / 2147483648

    report = {"patches": {}, "controls": {}, "sample_rate": 44100, "oversampling": 1}
    if definition["presets"]:
        raise RuntimeError("Nightdrive release must not distribute named presets")
    patches = {"initial": defaults}
    patches.update({key + "-maximum": {**defaults, key: 1} for key in ids})
    for name, values in patches.items():
        signal = render(values, name.replace(" ", "_"))
        rms = float(np.sqrt(np.mean(signal * signal)))
        limited = float(np.mean(abs(signal) >= peak - 1e-8))
        if rms < 0.00005 or limited > 0.02:
            raise RuntimeError("Silent or excessively limited patch: " + name)
        report["patches"][name] = {
            "rms": rms,
            "dc": float(signal.mean()),
            "peak": float(abs(signal).max()),
            "limited_fraction": limited,
        }
    for control in ids:
        low = render({**defaults, control: 0}, control + "-low", 345)
        high = render({**defaults, control: 1}, control + "-high", 345)
        difference = float(np.sqrt(np.mean((low - high) ** 2)))
        if difference < 1e-7:
            raise RuntimeError("Inactive control: " + control)
        report["controls"][control] = {"difference_rms": difference}
    held = render(defaults, "held-ten-seconds", 6891)
    early = float(np.sqrt(np.mean(held[44100:88200] ** 2)))
    late = float(np.sqrt(np.mean(held[-44100:] ** 2)))
    if not 0.85 < late / early < 1.15:
        raise RuntimeError("Unexpected internal amplitude/modulation decay")
    report["held_tone_rms_ratio"] = late / early
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("PASS Nightdrive: initial and extreme settings, four effective controls, sustained tone")
    return report
