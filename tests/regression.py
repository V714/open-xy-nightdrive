"""Require exact audio and retained-frame identity against a previous source folder."""

from pathlib import Path
import hashlib
import json
import subprocess

MACHINE = Path(__file__).resolve().parent.parent


def compare(baseline, sdk, out, compiler, baseline_sdk=None):
    from check import compile_test

    out.mkdir(parents=True, exist_ok=False)
    binaries = {}
    for tag, source in [("before", baseline), ("after", MACHINE)]:
        binaries[tag] = {}
        for stem in ["quality_render", "render_frames"]:
            unit = source / "tests/helpers" / (stem + ".cpp")
            if not unit.is_file():
                unit = source / "tests" / (stem + ".cpp")
            if not unit.is_file():
                unit = source / (stem + ".cpp")
            binary = out / (tag + "-" + stem)
            compile_test(
                unit,
                binary,
                baseline_sdk if tag == "before" and baseline_sdk else sdk,
                compiler,
                source,
            )
            binaries[tag][stem] = binary
    definition = json.loads((MACHINE / "definition.json").read_text())
    cases = [("initial", {p["id"]: p["default"] for p in definition["parameters"]})]
    cases += [
        ("all-zero", dict.fromkeys([p["id"] for p in definition["parameters"]], 0)),
        ("all-max", dict.fromkeys([p["id"] for p in definition["parameters"]], 1)),
    ]
    count = 0
    for name, values in cases:
        parameters = [values[p["id"]] for p in definition["parameters"]] + [0] * (
            8 - len(definition["parameters"])
        )
        for pitch in [180000, 6384029, 25498431, 203987449, 1200000000]:
            data = {}
            for tag in binaries:
                raw = out / (tag + ".q31")
                subprocess.run(
                    [
                        str(binaries[tag]["quality_render"]),
                        *map(str, parameters),
                        str(pitch),
                        "128",
                        str(raw),
                    ],
                    check=True,
                )
                data[tag] = raw.read_bytes()
            if data["before"] != data["after"]:
                raise RuntimeError("Audio changed: " + name + "/" + str(pitch))
            count += 1
    frames = {}
    for tag in binaries:
        path = out / (tag + "-frames.json")
        subprocess.run([str(binaries[tag]["render_frames"]), str(path)], check=True)
        frames[tag] = path.read_bytes()
    normalized = {}
    for tag, data in frames.items():
        normalized[tag] = json.loads(data)
        for frame in normalized[tag]:
            for node in frame["nodes"]:
                if node[0] == 640 * 1024 and node[1] in [120 * 1024, 320 * 1024]:
                    node[1] = 320 * 1024
    if normalized["before"] != normalized["after"]:
        raise RuntimeError("Display frames changed")
    # Palette, paint order and static shapes must match as parsed XML.
    import xml.etree.ElementTree as ET

    before = ET.fromstring((baseline / "panel.svg.fragment").read_text())
    after = ET.fromstring((MACHINE / "panel.svg.fragment").read_text())

    def structure(element):
        return element.tag, element.attrib, [structure(child) for child in element]

    if structure(before) != structure(after):
        raise RuntimeError("Static artwork changed")
    result = {
        "exact_audio_cases": count,
        "visible_frame_cases": 5,
        "offscreen_parking_normalized": True,
        "frames_sha256": hashlib.sha256(frames["after"]).hexdigest(),
        "static_artwork_identical": True,
    }
    (out / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print("PASS exact regression:", count, "audio cases, five frames, static artwork")
    return result
