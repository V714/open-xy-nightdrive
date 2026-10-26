"""Check unchanged endpoints and export level-matched response A/B auditions."""

from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import wave

import numpy as np

from check import MACHINE, compile_test


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default="/usr/bin/c++")
    args = parser.parse_args()
    sdk, baseline, out = args.sdk_root.resolve(), args.baseline.resolve(), args.out.resolve()
    if out.is_relative_to(MACHINE) or out.is_relative_to(baseline):
        parser.error("Keep outputs outside both machine folders")
    out.mkdir(parents=True, exist_ok=False)
    raw = out / "raw"
    raw.mkdir()
    binaries = {}
    for tag, source in (("before", baseline), ("after", MACHINE)):
        binary = out / tag
        compile_test(MACHINE / "tests/helpers/quality_render.cpp", binary, sdk, args.cxx, source)
        binaries[tag] = binary

    def render(tag, values, pitch, blocks, name):
        path = raw / (name + "-" + tag + ".q31")
        subprocess.run([str(binaries[tag]), *map(str, values), "0", "0", "0", "0",
                        str(pitch), str(blocks), str(path)], check=True)
        return np.fromfile(path, dtype="<i4")

    endpoint_cases = 0
    for tension in (0, 1):
        for glare in (0, 1):
            for drift in (0, .35, 1):
                for pitch in (180000, 6384029, 42852281, 203987449, 1200000000):
                    name = f"endpoint-{endpoint_cases}"
                    values = [.25, tension, drift, glare]
                    before = render("before", values, pitch, 128, name)
                    after = render("after", values, pitch, 128, name)
                    if not np.array_equal(before, after):
                        raise RuntimeError("Endpoint audio changed: " + name)
                    endpoint_cases += 1

    unchanged = {}
    for name in ("core.h", "definition.json", "machine.json", "display.cpp", "display.h",
                 "panel.svg.fragment"):
        before, after = (baseline / name).read_bytes(), (MACHINE / name).read_bytes()
        if before != after:
            raise RuntimeError("Unexpected interface/artwork change: " + name)
        unchanged[name] = hashlib.sha256(after).hexdigest()
    frames = []
    for tag, source in (("before", baseline), ("after", MACHINE)):
        binary, path = out / (tag + "-frames"), out / (tag + "-frames.json")
        compile_test(MACHINE / "tests/helpers/render_frames.cpp", binary, sdk, args.cxx, source)
        subprocess.run([str(binary), str(path)], check=True)
        frames.append(path.read_bytes())
    if frames[0] != frames[1]:
        raise RuntimeError("Display frames changed")

    def rms(signal):
        return float(np.sqrt(np.mean(signal * signal)))

    def wav(path, signal):
        with wave.open(str(path), "wb") as stream:
            stream.setnchannels(1)
            stream.setsampwidth(2)
            stream.setframerate(44100)
            stream.writeframes(np.rint(np.clip(signal, -1, 1) * 32767).astype("<i2").tobytes())

    report = {"exact_endpoint_cases": endpoint_cases, "unchanged": unchanged,
              "frames_sha256": hashlib.sha256(frames[0]).hexdigest(), "auditions": [],
              "hardware_tested": False, "device_cycles_measured": False}
    positions = (0, 1, 5, 10, 25, 50, 75, 90, 100)
    for control, index in (("tension", 1), ("glare", 3)):
        montage = []
        for position in positions:
            # Isolate feedback with Glare=0, and Punch with Tension=0.
            values = [.25, .25 if control == "tension" else 0, .35,
                      .25 if control == "glare" else 0]
            values[index] = position / 100
            sounds, stats = {}, {}
            for tag in binaries:
                notes = []
                for hz in (55, 110, 220):
                    pitch = round(hz * (1 << 32) / 44100)
                    samples = render(tag, values, pitch, 345, f"{control}-{position}-{hz}")
                    signal = samples.astype(float) / 2147483648
                    # Identical presentation fades; keep raw Q31 available for analysis.
                    fade = np.linspace(0, 1, 441)
                    signal[:441] *= fade
                    signal[-441:] *= fade[::-1]
                    notes.append(signal)
                sounds[tag] = np.concatenate(notes)
                stats[tag] = {"rms": rms(sounds[tag]), "peak": float(abs(sounds[tag]).max())}
            target = min(stats[tag]["rms"] for tag in binaries)
            matched = {}
            for tag in binaries:
                gain = target / stats[tag]["rms"] if stats[tag]["rms"] else 1
                matched[tag] = sounds[tag] * gain
                stats[tag]["presentation_gain"] = gain
                wav(out / f"{control}-{position:03}-{tag}.wav", matched[tag])
            pair = np.concatenate([matched["before"], np.zeros(11025), matched["after"],
                                   np.zeros(22050)])
            wav(out / f"{control}-{position:03}-AB.wav", pair)
            montage.append(pair)
            report["auditions"].append({"control": control, "position": position,
                                        "parameters": values, "levels": stats})
        wav(out / (control + "-all-AB.wav"), np.concatenate(montage))
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    (out / "README.txt").write_text(
        "A/B: before, 250 ms silence, after, 500 ms silence. Each side: 55/110/220 Hz.\n"
        "Positions in the all-AB files: 0, 1, 5, 10, 25, 50, 75, 90, 100.\n"
        "Tension auditions hold Glare=0; Glare auditions hold Tension=0.\n"
        "Each pair is RMS-matched by attenuation only, with identical 10 ms fades.\n"
        "Raw Q31 and presentation gains are preserved. Stock ADSR is not emulated.\n")
    print(f"PASS {endpoint_cases} exact endpoint cases, identical artwork/frames; 18 A/B pairs: {out}")


if __name__ == "__main__":
    main()
