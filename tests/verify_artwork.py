"""Compare five rendered C++ frames with the cropped, grayscale SVG reference."""

from pathlib import Path
import argparse
import sys
import json
import copy
import subprocess
import xml.etree.ElementTree as ET

MACHINE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(MACHINE / "generators"))
from generate_artwork import gray, fmt
from check import compile_test

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("action", choices=["prepare", "compare"])
parser.add_argument("--out", required=True, type=Path)
parser.add_argument("--sdk-root", required=True, type=Path)
parser.add_argument("--cxx", default="g++")
args = parser.parse_args()
out = args.out.resolve()
if out.is_relative_to(MACHINE):
    parser.error("Keep captures outside machine sources")
HERE = MACHINE
SVG = "http://www.w3.org/2000/svg"
ET.register_namespace("", SVG)
if args.action == "prepare":
    out.mkdir(parents=True, exist_ok=False)
    binary = out / "render-frames"
    compile_test(
        HERE / "tests/helpers/render_frames.cpp", binary, args.sdk_root.resolve(), args.cxx
    )
    subprocess.run([str(binary), str(out / "frames.json")], check=True)
    frames = json.loads((out / "frames.json").read_text())
    original = ET.parse(HERE / "assets/nightdrive-reference.svg").getroot()
    scene = next(e for e in original.iter() if e.get("clip-path"))
    zoom = list(scene)[0]
    for k, frame in enumerate(frames):
        ref = ET.Element("svg", xmlns=SVG, width="480", height="220", viewBox="0 0 480 220")
        defs = ET.SubElement(ref, "defs")
        clip = ET.SubElement(defs, "clipPath", id="scene")
        ET.SubElement(clip, "rect", x="0", y="20", width="480", height="200")
        ET.SubElement(ref, "rect", width="480", height="220", fill="#000000")
        g = ET.SubElement(ref, "g", {"clip-path": "url(#scene)"})
        shift = ET.SubElement(g, "g", transform="translate(0 -20)")
        shift.append(copy.deepcopy(zoom))
        for e in ref.iter():
            for key in ["fill", "stroke"]:
                if key in e.attrib:
                    e.set(key, gray(e.get(key)))
            if e.get("id") == "car-windows":
                e.set("fill", "#555555")
            if e.get("id") == "streetlamp-anim":
                e.set("begin", "-0.9695625s")
        (out / f"reference-{k}.svg").write_text(ET.tostring(ref, encoding="unicode"))
        actual = ET.Element("svg", xmlns=SVG, width="480", height="220", viewBox="0 0 480 220")
        ET.SubElement(actual, "rect", width="480", height="220", fill="#000000")
        panel = ET.fromstring((HERE / "panel.svg.fragment").read_text())
        actual.append(panel)
        for e in panel.iter():
            id = e.get("id", "")
            if id.startswith("nightdrive_node_"):
                n = int(id.rsplit("_", 1)[1])
                x, y, scale, shade, paint = frame["nodes"][n]
                e.set(
                    "transform",
                    f"matrix({fmt(scale/1024)} 0 0 {fmt(scale/1024)} {fmt(x/1024)} {fmt(y/1024)})",
                )
                if n < 4:
                    e.set(
                        "d",
                        "M"
                        + " L".join(
                            fmt(px / 1024) + " " + fmt(py / 1024) for px, py in frame["paths"][n]
                        )
                        + " Z",
                    )
                if paint:
                    color = "#" + f"{shade*17:02x}" * 3
                    e.set("stroke", color)
                    e.set("stroke-width", ".001")
                    if paint == 2 and e.get("fill") != "none":
                        e.set("fill", color)
        (out / f"cpp-{k}.svg").write_text(ET.tostring(actual, encoding="unicode"))
else:
    from PIL import Image, ImageChops
    import numpy as np

    rows = []
    for k, frame in enumerate(json.loads((out / "frames.json").read_text())):
        a = np.array(Image.open(out / f"reference-{k}.png").convert("RGB"), dtype=float)
        b = np.array(Image.open(out / f"cpp-{k}.png").convert("RGB"), dtype=float)
        delta = np.max(abs(a - b), axis=2)
        row = dict(
            time=frame["time"],
            changed_pixels=int(np.sum(delta > 30)),
            fraction_over_30=float(np.mean(delta > 30)),
            mean_absolute_channel_error=float(np.mean(abs(a - b))),
        )
        Image.fromarray(np.uint8(np.clip(abs(a - b) * 3, 0, 255))).save(out / f"difference-{k}.png")
        if row["fraction_over_30"] > 0.005 or row["mean_absolute_channel_error"] > 1:
            raise SystemExit(
                "Reference mismatch exceeds 0.5% image area or mean error 1: " + str(row)
            )
        rows.append(row)
    report = dict(
        reference="supplied SVG with same top crop and grayscale quantization",
        pixel_identical=False,
        tolerance="0.5% of scene pixels differ by more than 30 levels; includes rasterization and retained-path adaptations",
        frames=rows,
    )
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
