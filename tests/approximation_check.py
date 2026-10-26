"""Gate experimental post-oscillator approximations and export approved A/B audio."""

from pathlib import Path
import argparse
import hashlib
import json
import math
import subprocess
import wave

import numpy as np
from check import MACHINE, compile_test


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--error-limit-db", type=float, default=-55)
    parser.add_argument("--cxx", default="/usr/bin/c++")
    args = parser.parse_args()
    sdk, baseline, out = args.sdk_root.resolve(), args.baseline.resolve(), args.out.resolve()
    if out.is_relative_to(MACHINE) or out.is_relative_to(baseline):
        parser.error("Keep outputs outside source folders")
    out.mkdir(parents=True, exist_ok=False)
    raw = out / "raw"
    raw.mkdir()
    binaries = {}
    for tag, source in (("before", baseline), ("after", MACHINE)):
        binaries[tag] = out / tag
        compile_test(MACHINE / "tests/helpers/quality_render.cpp", binaries[tag], sdk, args.cxx, source)
    definition = json.loads((MACHINE / "definition.json").read_text())
    peak = definition["resources"]["outputPeakQ31"] / 2147483648

    def render(tag, values, hz, blocks, name, mode=0):
        path = raw / f"{name}-{tag}.q31"
        subprocess.run([str(binaries[tag]), *map(str, values), "0", "0", "0", "0",
                        str(round(hz * (1 << 32) / 44100)), str(blocks), str(path), str(mode)], check=True)
        return np.fromfile(path, dtype="<i4").astype(float) / 2147483648

    def rms(x):
        return float(np.sqrt(np.mean(x * x)))

    def error_db(a, b):
        level, error = rms(a), rms(b - a)
        if level < 1e-12:
            return -300.0 if error < 1e-12 else 300.0
        return max(-300.0, 20 * math.log10(max(error / level, 1e-15)))

    report = {"sources": {tag: {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in sorted(root.iterdir()) if p.suffix in (".h", ".cpp", ".json")}
                           for tag, root in (("before", baseline), ("after", MACHINE))},
              "relative_error_limit_db": args.error_limit_db, "cases": [], "failures": [],
              "device_cycles_measured": False, "hardware_tested": False}
    glare_values = (0, .01, .05, .1, .25, .5, .75, .9, 1, .031, .123, .401, .613, .913)
    patches = ((.25, .25, .35), (1, 1, 1), (.25, .25, 0), (.25, 0, .35))
    for patch in patches:
        for glare in glare_values:
            for hz in (55, 110, 220, 440, 880, 1760):
                name = str(len(report["cases"]))
                values = [*patch, glare]
                before = render("before", values, hz, 128, name)
                after = render("after", values, hz, 128, name)
                entry = {"parameters": values, "hz": hz,
                         "error_db": error_db(before, after),
                         "attack_error_db": error_db(before[:1103], after[:1103]),
                         "sustain_error_db": error_db(before[3528:], after[3528:]),
                         "rms_before": rms(before), "rms_after": rms(after),
                         "peak_after": float(abs(after).max()),
                         "dc_delta": float((after - before).mean())}
                frequencies = np.fft.rfftfreq(len(before), 1 / 44100)
                window = np.hanning(len(before))
                spectra = [abs(np.fft.rfft(signal * window)) ** 2 for signal in (before, after)]
                entry["spectrum_band_db_changes"] = {}
                for low, high in ((20, 200), (200, 2000), (2000, 10000), (10000, 22050)):
                    mask = (frequencies >= low) & (frequencies < high)
                    energies = [float(s[mask].sum()) for s in spectra]
                    entry["spectrum_band_db_changes"][f"{low}-{high}"] = 10 * math.log10(
                        max(energies[1], 1e-20) / max(energies[0], 1e-20))
                if max(entry[k] for k in ("error_db", "attack_error_db", "sustain_error_db")) > args.error_limit_db:
                    report["failures"].append({"case": name, "reason": "relative error above gate"})
                if entry["peak_after"] > peak + 1e-9:
                    report["failures"].append({"case": name, "reason": "declared output peak exceeded"})
                if glare == 0 and not np.array_equal(before, after):
                    report["failures"].append({"case": name, "reason": "Glare zero changed"})
                report["cases"].append(entry)

    # Long holds and Glare sweeps exercise the same helper against both engines.
    report["extended_cases"] = []
    for mode in (0, 1, 2):
        for hz in (55, 440, 1760):
            values = [1, 1, 1, .401]
            name = f"extended-{mode}-{hz}"
            signals = {tag: render(tag, values, hz, 3446, name, mode) for tag in binaries}
            before, after = signals["before"], signals["after"]
            err = error_db(before, after)
            report["extended_cases"].append({"mode": mode, "hz": hz, "error_db": err})
            if err > args.error_limit_db or float(abs(after).max()) > peak + 1e-9:
                report["failures"].append({"case": name, "reason": "extended signal gate"})

    # Feedback, oscillators and controls must stay exact even when the output shaper changes.
    streams = {}
    for tag, source in (("before", baseline), ("after", MACHINE)):
        binary, path = out / (tag + "-state"), out / (tag + "-state.q31")
        compile_test(MACHINE / "tests/helpers/exact_render.cpp", binary, sdk, args.cxx, source)
        subprocess.run([str(binary), str(path)], check=True)
        words = np.fromfile(path, dtype="<i4")
        if words.size % 80:
            raise RuntimeError("State corpus format changed")
        streams[tag] = words.reshape(-1, 80)
    if not np.array_equal(streams["before"][:, 64:78], streams["after"][:, 64:78]):
        report["failures"].append({"reason": "oscillator, feedback or controls changed"})
    a = streams["before"][:, :64].astype(float) / 2147483648
    b = streams["after"][:, :64].astype(float) / 2147483648
    report["corpus_error_db"] = error_db(a, b)
    report["state_frames"] = len(a)
    if report["corpus_error_db"] > args.error_limit_db:
        report["failures"].append({"reason": "full-corpus relative error above gate"})
    report["passed"] = not report["failures"]
    report["worst_case_error_db"] = max(max(c[k] for k in ("error_db", "attack_error_db", "sustain_error_db"))
                                        for c in report["cases"])
    if report["passed"]:
        audition = out / "auditions"
        audition.mkdir()
        all_pairs = []
        report["auditions"] = []
        for position in (0, 1, 5, 10, 25, 40.1, 50, 75, 90, 100):
            signals = {}
            for tag in binaries:
                notes = []
                for hz in (55, 110, 220, 440, 880, 1760):
                    signal = render(tag, [.25, .25, .35, position / 100], hz, 345, f"audition-{position}-{hz}")
                    fade = np.linspace(0, 1, 441)
                    signal[:441] *= fade
                    signal[-441:] *= fade[::-1]
                    notes.append(signal)
                signals[tag] = np.concatenate(notes)
            target = min(rms(signal) for signal in signals.values())
            report["auditions"].append({"glare": position, "gains": {
                tag: target / rms(signal) if rms(signal) else 1 for tag, signal in signals.items()}})
            for tag, signal in signals.items():
                signals[tag] = signal * (target / rms(signal) if rms(signal) else 1)
            pair = np.concatenate([signals["before"], np.zeros(11025), signals["after"], np.zeros(22050)])
            with wave.open(str(audition / f"glare-{str(position).zfill(3)}-AB.wav"), "wb") as stream:
                stream.setnchannels(1)
                stream.setsampwidth(2)
                stream.setframerate(44100)
                stream.writeframes(np.rint(np.clip(pair, -1, 1) * 32767).astype("<i2").tobytes())
            all_pairs.append(pair)
        for mode, label in ((1, "slow"), (2, "fast")):
            signals = {tag: render(tag, [1, 1, 1, 0], 110, 3446,
                                   "sweep-" + label, mode) for tag in binaries}
            target = min(rms(signal) for signal in signals.values())
            report["auditions"].append({"glare_sweep": label, "gains": {
                tag: target / rms(signal) if rms(signal) else 1 for tag, signal in signals.items()}})
            for tag, signal in signals.items():
                signal *= target / rms(signal) if rms(signal) else 1
                fade = np.linspace(0, 1, 441)
                signal[:441] *= fade
                signal[-441:] *= fade[::-1]
            pair = np.concatenate([signals["before"], np.zeros(11025), signals["after"]])
            with wave.open(str(audition / ("glare-sweep-" + label + "-AB.wav")), "wb") as stream:
                stream.setparams((1, 2, 44100, 0, "NONE", "not compressed"))
                stream.writeframes(np.rint(np.clip(pair, -1, 1) * 32767).astype("<i2").tobytes())
        with wave.open(str(audition / "all-AB.wav"), "wb") as stream:
            stream.setnchannels(1)
            stream.setsampwidth(2)
            stream.setframerate(44100)
            stream.writeframes(np.rint(np.clip(np.concatenate(all_pairs), -1, 1) * 32767).astype("<i2").tobytes())
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: report[k] for k in ("passed", "worst_case_error_db", "corpus_error_db")}), flush=True)
    if not report["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
