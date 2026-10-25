"""Adapt the supplied SVG to the retained OP-XY renderer. No browser-only animation."""

from pathlib import Path
import xml.etree.ElementTree as ET
import re
import math
import json
import hashlib
from cpp_format import write_header

HERE = Path(__file__).resolve().parent.parent
SOURCE = HERE / "assets/nightdrive-reference.svg"


def fmt(v):
    s = f"{v:.4f}".rstrip("0").rstrip(".")
    return s if s not in ("", "-0") else "0"


def gray(color):
    if color == "none":
        return color
    if len(color) == 4:
        color = "#" + "".join(c * 2 for c in color[1:])
    shade = max(0, min(15, round(sum(int(color[i : i + 2], 16) for i in [1, 3, 5]) / 3 / 17)))
    return "#" + f"{shade*17:02x}" * 3


def mul(a, b):
    return [
        a[0] * b[0] + a[2] * b[1],
        a[1] * b[0] + a[3] * b[1],
        a[0] * b[2] + a[2] * b[3],
        a[1] * b[2] + a[3] * b[3],
        a[0] * b[4] + a[2] * b[5] + a[4],
        a[1] * b[4] + a[3] * b[5] + a[5],
    ]


def transform(text):
    a = [1, 0, 0, 1, 0, 0]
    for name, args in re.findall(r"(\w+)\(([^)]+)\)", text):
        p = list(map(float, re.findall(r"[-+]?(?:\d*\.)?\d+", args)))
        if name == "translate":
            b = [1, 0, 0, 1, p[0], p[1] if len(p) > 1 else 0]
        elif name == "scale":
            b = [p[0], 0, 0, p[1] if len(p) > 1 else p[0], 0, 0]
        elif name == "rotate":
            c = math.cos(math.radians(p[0]))
            s = math.sin(math.radians(p[0]))
            b = [c, s, -s, c, 0, 0]
        else:
            raise ValueError(name)
        a = mul(a, b)
    return a


def point(a, p):
    x, y = p
    return (a[0] * x + a[2] * y + a[4], a[1] * x + a[3] * y + a[5])


def path_points(text):
    tokens = re.findall(r"[MLHVQZ]|[-+]?(?:\d*\.)?\d+", text)
    i = 0
    command = ""
    paths = []
    current = []
    p = (0, 0)
    while i < len(tokens):
        if tokens[i].isalpha():
            command = tokens[i]
            i += 1
        if command == "Z":
            if current:
                current.append(current[0])
                paths.append(current)
                current = []
            command = ""
            continue
        count = {"M": 2, "L": 2, "H": 1, "V": 1, "Q": 4}[command]
        v = list(map(float, tokens[i : i + count]))
        i += count
        if command == "M":
            if current:
                paths.append(current)
            p = tuple(v)
            current = [p]
            command = "L"
        elif command == "L":
            p = tuple(v)
            current.append(p)
        elif command == "H":
            p = (v[0], p[1])
            current.append(p)
        elif command == "V":
            p = (p[0], v[0])
            current.append(p)
        elif command == "Q":
            begin = p
            for j in range(1, 25):
                t = j / 24
                current.append(
                    (
                        (1 - t) ** 2 * begin[0] + 2 * (1 - t) * t * v[0] + t * t * v[2],
                        (1 - t) ** 2 * begin[1] + 2 * (1 - t) * t * v[1] + t * t * v[3],
                    )
                )
            p = (v[2], v[3])
    if current:
        paths.append(current)
    return paths


def clipline(a, b):
    x, y = a
    dx = b[0] - x
    dy = b[1] - y
    t0 = 0
    t1 = 1
    for p, q in [(-dx, x), (dx, 480 - x), (-dy, y - 20), (dy, 220 - y)]:
        if not p:
            if q < 0:
                return None
        elif p < 0:
            t0 = max(t0, q / p)
        else:
            t1 = min(t1, q / p)
    if t0 > t1:
        return None
    return [(x + t0 * dx, y + t0 * dy), (x + t1 * dx, y + t1 * dy)]


def clipped_polygon(poly):
    for axis, edge, greater in [(0, 0, True), (0, 480, False), (1, 20, True), (1, 220, False)]:
        out = []
        for a, b in zip(poly, poly[1:] + poly[:1]):
            inside = lambda p: p[axis] >= edge if greater else p[axis] <= edge
            ia, ib = inside(a), inside(b)
            if ia:
                out.append(a)
            if ia != ib:
                t = (edge - a[axis]) / (b[axis] - a[axis])
                out.append((a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1])))
        poly = out
        if not poly:
            break
    return poly


def main():
    root = ET.parse(SOURCE).getroot()
    byid = {e.get("id"): e for e in root.iter() if e.get("id")}
    items = []
    car_nodes = []
    windows = []
    mapping = {}
    wheel_data = []
    wheel_sprites = []
    base = mul(
        [1, 0, 0, 1, 0, -20], transform("translate(240 132) scale(0.75) translate(-240 -132)")
    )

    def emit(paths, style, kind="static", source="", node=None):
        if node is None:
            node = len([x for x in items if x["id"] >= 4]) + 4
        # Extra mutable nodes use indices 0..3 and do not consume ordinary bindings.
        fill = gray(style.get("fill", "#000000"))
        stroke = gray(style.get("stroke", "none"))
        width = float(style.get("stroke-width", "1")) * 0.75
        if kind == "window":
            fill = "#000000"
            stroke = "none"
        if kind == "outline":
            fill = "none"
        pieces = []
        for p in paths:
            if kind == "static" and fill != "none":
                p = clipped_polygon(p)
                pieces.append(
                    "M" + " L".join(fmt(x) + " " + fmt(y) for x, y in p) + " Z" if p else ""
                )
            elif kind == "static":
                for a, b in zip(p, p[1:]):
                    clipped = clipline(a, b)
                    if clipped:
                        pieces.append("M" + " L".join(fmt(x) + " " + fmt(y) for x, y in clipped))
            else:
                pieces.append("M" + " L".join(fmt(x) + " " + fmt(y) for x, y in p))
        item = dict(
            id=node,
            d=" ".join(pieces) or "M0 0 L0 0",
            fill=fill,
            stroke=stroke,
            width=width,
            kind=kind,
        )
        items.append(item)
        if kind in ["car", "window", "outline"]:
            car_nodes.append(node)
        if kind == "window":
            windows.append(node)
        mapping.setdefault(source, []).append(node)
        return node

    def walk(e, a, style, in_windows=False):
        tag = e.tag.split("}")[-1]
        style = {
            **style,
            **{k: v for k, v in e.attrib.items() if k in ["fill", "stroke", "stroke-width"]},
        }
        a = mul(a, transform(e.get("transform", "")))
        in_windows = in_windows or e.get("id") == "car-windows"
        if tag == "g":
            # A rotating spoke group supplies the authored wheel sprites and legacy path slots.
            if any(c.tag.endswith("animateTransform") and c.get("type") == "rotate" for c in e):
                path = next(c for c in e if c.tag.endswith("path"))
                raw = path_points(path.get("d"))
                idx = 2 + len(wheel_data)
                wheel_data.append((a, raw))
                poses = []
                for pose in range(7):
                    rotation = transform("rotate(" + str(pose * 180 / 7) + ")")
                    rotated = [[point(mul(a, rotation), p) for p in path] for path in raw]
                    poses.append(emit(rotated, style, "car", "wheel-spokes-sprites"))
                wheel_sprites.append(poses)
                items.append(
                    dict(
                        id=idx,
                        d="",
                        fill="none",
                        stroke=gray(style["stroke"]),
                        width=float(style["stroke-width"]) * 0.75,
                        kind="spokes",
                    )
                )
                mapping.setdefault("wheel-spokes", []).append(idx)
                return
            for c in e:
                walk(c, a, style, in_windows)
            return
        if tag not in ["polygon", "path", "ellipse", "circle"]:
            return
        if tag == "polygon":
            nums = list(map(float, re.findall(r"[-+]?(?:\d*\.)?\d+", e.get("points"))))
            p = list(zip(nums[::2], nums[1::2]))
            paths = [p + [p[0]]]
        elif tag == "path":
            paths = path_points(e.get("d"))
        else:
            rx = float(e.get("rx", e.get("r", "0")))
            ry = float(e.get("ry", e.get("r", "0")))
            cx = float(e.get("cx", "0"))
            cy = float(e.get("cy", "0"))
            paths = [
                [
                    (
                        cx + rx * math.cos(j * 2 * math.pi / 48),
                        cy + ry * math.sin(j * 2 * math.pi / 48),
                    )
                    for j in range(49)
                ]
            ]
        paths = [[point(a, p) for p in path] for path in paths]
        source = e.get("id", "static-car")
        if in_windows and "fill" not in e.attrib:
            emit(paths, style, "window", "car-windows")
            emit(paths, style, "outline", "car-window-outlines")
        else:
            emit(paths, style, "car", source)

    # Road and its shoulders, in source paint order.
    scene = next(e for e in root.iter() if e.get("clip-path"))
    zoom = list(scene)[0]
    for e in list(zoom)[:2]:
        leaves = list(e) if e.tag.endswith("g") else [e]
        for leaf in leaves:
            style = {**e.attrib, **leaf.attrib}
            paths = [[point(base, p) for p in path] for path in path_points(leaf.get("d"))]
            for path in paths:
                if style.get("fill") == "none":
                    a, b = path[0], path[-1]
                    delta = (b[0] - a[0], b[1] - a[1])
                    path[:] = [
                        (a[0] - 2 * delta[0], a[1] - 2 * delta[1]),
                        (b[0] + 2 * delta[0], b[1] + 2 * delta[1]),
                    ]
                else:
                    # Continue the two road sides below the viewport before clipping the trapezoid.
                    a, b, c, d = path[:4]
                    path[0] = (3 * a[0] - 2 * b[0], 3 * a[1] - 2 * b[1])
                    path[3] = (3 * d[0] - 2 * c[0], 3 * d[1] - 2 * c[1])
                    path[-1] = path[0]
            emit(paths, style, "static", "road")
    # Center dashes, source length 21 / period 53. Full extent safely outside the canvas.
    dash_nodes = []
    ux = 436.5 / math.hypot(436.5, 315.5)
    uy = -315.5 / math.hypot(436.5, 315.5)
    for j in range(13):
        dash_nodes.append(
            emit(
                [[(0, 0), (21 * 0.75 * ux, 21 * 0.75 * uy)]],
                dict(fill="none", stroke="#ededed", **{"stroke-width": "1.65"}),
                "dash",
                "road-markings",
            )
        )
    # Lamp is behind the car. Two paths permit split contours when the top clips the arch.
    for idx in [0, 1]:
        items.append(
            dict(id=idx, d="", fill="none", stroke="#ffffff", width=2.15 * 0.75, kind="lamp")
        )
    lamp_local = [
        [(x * 0.75, y * 0.75 + 40) for x, y in path]
        for path in path_points(byid["streetlamp"].get("d"))
    ]
    lamp_node = emit(
        lamp_local,
        dict(fill="none", stroke="#ffffff", **{"stroke-width": "2.15"}),
        "lampfull",
        "passing-lamp-full",
    )
    body_mask = [
        (161, 182),
        (177.8, 172),
        (186.5, 155.8),
        (208.2, 140),
        (231.8, 133),
        (267, 148),
        (267, 160),
        (199, 210),
        (161, 195),
        (161, 182),
    ]
    emit(
        [[point(base, p) for p in body_mask]],
        dict(fill="#000000", stroke="none", **{"stroke-width": "0"}),
        "car",
        "car-opaque-underlay",
    )
    walk(byid["static-car"], base, dict(fill="#000000", stroke="none", **{"stroke-width": "1"}))
    total = max(i["id"] for i in items) + 1
    if total > 64:
        raise RuntimeError("Node budget exceeded: " + str(total))
    # Non-degenerate seed topology keeps native path geometry/bounds initialized.
    seed = [
        (12 * math.cos(j * 2 * math.pi / 64), 12 * math.sin(j * 2 * math.pi / 64))
        for j in range(64)
    ]
    mutable = "M" + " L".join(fmt(x) + " " + fmt(y) for x, y in seed) + " Z"
    svg = ['<g id="nightdrive_panel">']
    for item in items:
        attrs = {
            "id": "nightdrive_node_" + str(item["id"]),
            "d": mutable if item["id"] < 4 else item["d"],
            "fill": item["fill"],
            "stroke": item["stroke"],
            "stroke-width": fmt(item["width"]),
            "stroke-linejoin": "round",
        }
        svg.append("<path " + " ".join(k + '="' + v + '"' for k, v in attrs.items()) + "/>")
    for j in range(16):
        color = "#" + f"{j*17:02x}" * 3
        svg.append(
            f'<circle id="nightdrive_palette_{j}" cx="640" cy="0" r="1" fill="{color}" stroke="{color}" stroke-width="0.001"/>'
        )
    svg.append("</g>")
    (HERE / "panel.svg.fragment").write_text("\n".join(svg) + "\n")
    # Lamp relative to a centered pivot: all mutable coordinates remain within +/-64.
    lamp = path_points(byid["streetlamp"].get("d"))[0]
    lamp = [(round(x * 0.75 * 1024), round((y * 0.75 + 40) * 1024)) for x, y in lamp]
    header = [
        "#pragma once",
        "namespace nightdrive {",
        "struct ArtPoint {int x,y;};",
        f"static const unsigned art_nodes={total};",
        f"static const unsigned lamp_node={lamp_node};",
        "static const unsigned wheel_sprite_nodes[2][7]={"
        + ",".join("{" + ",".join(map(str, p)) + "}" for p in wheel_sprites)
        + "};",
    ]
    for name, values in [
        ("car_nodes", car_nodes),
        ("window_nodes", windows),
        ("dash_nodes", dash_nodes),
    ]:
        header.append("static const unsigned " + name + "[]={" + ",".join(map(str, values)) + "};")
    header.append(
        "static const ArtPoint lamp_shape[]={"
        + ",".join("{" + str(x) + "," + str(y) + "}" for x, y in lamp)
        + "};"
    )
    # Lane-center spacing comes from the reference road at the car centroid, not a guessed screen distance.
    center = (218, 173)

    def intersection(a, b):
        vx, vy = b[0] - a[0], b[1] - a[1]
        return ((a[0] - center[0]) * vy - (a[1] - center[1]) * vx) / (0.586 * vy - 0.810 * vx)

    spacing = abs(intersection((-44, 272), (406, -36)) - intersection((95, 313), (518, -10))) / 2
    header.append(
        "static const int lane_shift_q10=" + str(round(spacing * 0.75 * 0.75 * 1024)) + ";"
    )
    header.append("}")
    write_header(HERE / "artwork.h", "\n".join(header) + "\n")
    mapping.update(
        {
            "car-position": car_nodes + [2, 3],
            "drive-motion": dash_nodes + [0, 1, 2, 3],
            "road-markings-anim": dash_nodes,
            "streetlamp-anim": [0, 1],
            "passing-lamp": [0, 1],
        }
    )
    (HERE / "generated/artwork_map.json").write_text(
        json.dumps(
            dict(
                source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                canvas=[480, 220],
                crop_top=20,
                scene_scale=0.75,
                nodes=total,
                windows=windows,
                car_nodes=car_nodes,
                mapping=mapping,
                lane_shift_screen_px=spacing * 0.75 * 0.75,
            ),
            indent=2,
        )
        + "\n"
    )
    d = json.loads((HERE / "definition.json").read_text())
    d["resources"]["display"].update(nodes=total, stateBytes=56)
    d["resources"]["voiceBytes"] = 64
    (HERE / "definition.json").write_text(json.dumps(d, indent=2) + "\n")
    print(
        "Nightdrive artwork:",
        total,
        "nodes,",
        len(windows),
        "window fills,",
        len(car_nodes),
        "car parts",
    )


if __name__ == "__main__":
    main()
