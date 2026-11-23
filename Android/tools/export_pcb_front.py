"""Export the front-side geometry of the KiCad board for the Android render.

Reads hardware/kicad/ESP-DubSiren.kicad_pcb and writes
app/assets/pcb_front.json (Kotlin and Flutter apps) with, in board millimetres:
  outline  closed Edge.Cuts loops (board contour and milled slots)
  pour     F.Cu filled zone polygons
  tracks   F.Cu segments [width, x1, y1, x2, y2]
  vias     [x, y, diameter] (tented on this board)
  pads     front pads [kind, shape, x, y, w, h, rot, drill, drill_h]
           kind: "pth" plated, "npth" bare hole, "smd" front copper only
  silk     F.SilkS polylines [width, x1, y1, ...] and texts

Run after every PCB change:  python Android/tools/export_pcb_front.py
"""

import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PCB = ROOT / "hardware" / "kicad" / "ESP-DubSiren.kicad_pcb"
OUT = ROOT / "app" / "assets" / "pcb_front.json"


# ---- s-expression parsing -------------------------------------------------

def tokenize(text):
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in "()":
            yield c
            i += 1
        elif c.isspace():
            i += 1
        elif c == '"':
            j = i + 1
            out = []
            while text[j] != '"':
                if text[j] == "\\":
                    j += 1
                out.append(text[j])
                j += 1
            yield ("str", "".join(out))
            i = j + 1
        else:
            j = i
            while j < n and not text[j].isspace() and text[j] not in "()":
                j += 1
            yield text[i:j]
            i = j


def parse(text):
    stack = [[]]
    for token in tokenize(text):
        if token == "(":
            stack.append([])
        elif token == ")":
            done = stack.pop()
            stack[-1].append(done)
        else:
            stack[-1].append(token[1] if isinstance(token, tuple) else token)
    return stack[0][0]


def children(node, name):
    return [c for c in node[1:] if isinstance(c, list) and c and c[0] == name]


def child(node, name):
    found = children(node, name)
    return found[0] if found else None


def nums(node, count=None):
    values = [float(v) for v in node[1:] if not isinstance(v, list)]
    return values if count is None else values[:count]


def layers_of(node):
    single = child(node, "layer")
    if single:
        return [single[1]]
    many = child(node, "layers")
    return list(many[1:]) if many else []


# ---- geometry helpers -----------------------------------------------------

def rotate(x, y, degrees):
    # KiCad: Y down, positive angle counter-clockwise on screen.
    a = math.radians(degrees)
    return x * math.cos(a) + y * math.sin(a), -x * math.sin(a) + y * math.cos(a)


def arc_points(start, mid, end, step_deg=6.0):
    (x1, y1), (x2, y2), (x3, y3) = start, mid, end
    d = 2 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2))
    if abs(d) < 1e-12:
        return [start, end]
    ux = ((x1 * x1 + y1 * y1) * (y2 - y3) + (x2 * x2 + y2 * y2) * (y3 - y1) + (x3 * x3 + y3 * y3) * (y1 - y2)) / d
    uy = ((x1 * x1 + y1 * y1) * (x3 - x2) + (x2 * x2 + y2 * y2) * (x1 - x3) + (x3 * x3 + y3 * y3) * (x2 - x1)) / d
    r = math.hypot(x1 - ux, y1 - uy)
    a1 = math.atan2(y1 - uy, x1 - ux)
    a2 = math.atan2(y2 - uy, x2 - ux)
    a3 = math.atan2(y3 - uy, x3 - ux)

    def ccw(a, b):
        return (b - a) % (2 * math.pi)

    if ccw(a1, a2) <= ccw(a1, a3):
        sweep = ccw(a1, a3)
    else:
        sweep = -ccw(a3, a1)
    steps = max(2, int(abs(math.degrees(sweep)) / step_deg))
    return [(ux + r * math.cos(a1 + sweep * k / steps), uy + r * math.sin(a1 + sweep * k / steps))
            for k in range(steps + 1)]


def circle_points(cx, cy, r, steps=48):
    return [(cx + r * math.cos(2 * math.pi * k / steps), cy + r * math.sin(2 * math.pi * k / steps))
            for k in range(steps + 1)]


def shape_polyline(node, transform):
    """fp_/gr_ line, arc, circle, rect, poly -> list of points (board coords)."""
    kind = node[0].split("_", 1)[1]
    if kind == "line":
        pts = [tuple(nums(child(node, "start"))), tuple(nums(child(node, "end")))]
    elif kind == "arc":
        pts = arc_points(tuple(nums(child(node, "start"))), tuple(nums(child(node, "mid"))),
                         tuple(nums(child(node, "end"))))
    elif kind == "circle":
        cx, cy = nums(child(node, "center"))
        ex, ey = nums(child(node, "end"))
        pts = circle_points(cx, cy, math.hypot(ex - cx, ey - cy))
    elif kind == "rect":
        x1, y1 = nums(child(node, "start"))
        x2, y2 = nums(child(node, "end"))
        pts = [(x1, y1), (x2, y1), (x2, y2), (x1, y2), (x1, y1)]
    elif kind == "poly":
        pts = [tuple(nums(p)) for p in children(child(node, "pts"), "xy")]
        pts.append(pts[0])
    else:
        return None
    return [transform(x, y) for x, y in pts]


def stroke_width(node):
    stroke = child(node, "stroke")
    if stroke and child(stroke, "width"):
        return float(child(stroke, "width")[1])
    if child(node, "width"):
        return float(child(node, "width")[1])
    return 0.12


def chain_loops(segments, tolerance=0.01):
    """Join open polylines into closed loops by matching endpoints."""
    pending = [list(s) for s in segments]
    loops = []
    while pending:
        loop = pending.pop(0)
        changed = True
        while changed and math.dist(loop[0], loop[-1]) > tolerance:
            changed = False
            for i, seg in enumerate(pending):
                if math.dist(loop[-1], seg[0]) <= tolerance:
                    loop.extend(seg[1:])
                elif math.dist(loop[-1], seg[-1]) <= tolerance:
                    loop.extend(reversed(seg[:-1]))
                else:
                    continue
                pending.pop(i)
                changed = True
                break
        loops.append(loop)
    return loops


def flat(points, digits=3):
    return [round(v, digits) for p in points for v in p]


# ---- export ---------------------------------------------------------------

def main():
    board = parse(PCB.read_text(encoding="utf-8"))
    edges, pour, tracks, vias, pads, silk, texts = [], [], [], [], [], [], []

    def identity(x, y):
        return x, y

    for node in board[1:]:
        if not isinstance(node, list):
            continue
        tag = node[0]
        if tag == "segment" and child(node, "layer")[1] == "F.Cu":
            x1, y1 = nums(child(node, "start"))
            x2, y2 = nums(child(node, "end"))
            tracks.append([float(child(node, "width")[1]), x1, y1, x2, y2])
        elif tag == "arc" and child(node, "layer")[1] == "F.Cu":
            pts = arc_points(tuple(nums(child(node, "start"))), tuple(nums(child(node, "mid"))),
                             tuple(nums(child(node, "end"))))
            w = float(child(node, "width")[1])
            for a, b in zip(pts, pts[1:]):
                tracks.append([w, a[0], a[1], b[0], b[1]])
        elif tag == "via":
            x, y = nums(child(node, "at"), 2)
            vias.append([x, y, float(child(node, "size")[1])])
        elif tag == "zone":
            for filled in children(node, "filled_polygon"):
                if child(filled, "layer")[1] == "F.Cu":
                    pts = [tuple(nums(p)) for p in children(child(filled, "pts"), "xy")]
                    pour.append(flat(pts))
        elif tag.startswith("gr_") and tag != "gr_text":
            layers = layers_of(node)
            pts = shape_polyline(node, identity)
            if pts is None:
                continue
            if "Edge.Cuts" in layers:
                edges.append(pts)
            elif "F.SilkS" in layers:
                silk.append([round(stroke_width(node), 3)] + flat(pts))
        elif tag == "gr_text" and "F.SilkS" in layers_of(node):
            texts.append(text_entry(node, identity, 0.0))
        elif tag == "footprint":
            export_footprint(node, edges, pads, silk, texts)

    # Board contour first (largest bounding box), then the milled slots.
    def extent(loop):
        xs, ys = [p[0] for p in loop], [p[1] for p in loop]
        return (max(xs) - min(xs)) * (max(ys) - min(ys))

    outline = [flat(loop) for loop in sorted(chain_loops(edges), key=extent, reverse=True)]
    data = {
        "source": str(PCB.relative_to(ROOT)).replace("\\", "/"),
        "outline": outline,
        "pour": pour,
        "tracks": [[round(v, 3) for v in t] for t in tracks],
        "vias": [[round(v, 3) for v in v_] for v_ in vias],
        "pads": pads,
        "silk": silk,
        "texts": texts,
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(data, separators=(",", ":")), encoding="utf-8")
    print(f"{OUT.relative_to(ROOT)}: {len(outline)} outline loops, {len(pour)} pour polygons, "
          f"{len(tracks)} tracks, {len(vias)} vias, {len(pads)} pads, {len(silk)} silk, {len(texts)} texts")


def text_entry(node, transform, footprint_rotation):
    at = nums(child(node, "at"))
    x, y = transform(at[0], at[1])
    rotation = at[2] if len(at) > 2 else 0.0
    effects = child(node, "effects")
    font = child(effects, "font") if effects else None
    size = nums(child(font, "size"))[1] if font and child(font, "size") else 1.0
    justify = child(effects, "justify") if effects else None
    align = "left" if justify and "left" in justify else "right" if justify and "right" in justify else "center"
    return [round(x, 3), round(y, 3), size, rotation, align, node[2] if node[0] == "property" else node[1]]


def export_footprint(node, edges, pads, silk, texts):
    fx, fy, *rest = nums(child(node, "at"))
    frot = rest[0] if rest else 0.0
    front = child(node, "layer")[1] == "F.Cu"

    def transform(x, y):
        rx, ry = rotate(x, y, frot)
        return fx + rx, fy + ry

    for item in node[1:]:
        if not isinstance(item, list):
            continue
        tag = item[0]
        layers = layers_of(item)
        if tag.startswith("fp_") and tag != "fp_text":
            pts = shape_polyline(item, transform)
            if pts is None:
                continue
            if "Edge.Cuts" in layers:
                edges.append(pts)
            elif "F.SilkS" in layers:
                silk.append([round(stroke_width(item), 3)] + flat(pts))
        elif tag in ("fp_text", "property") and "F.SilkS" in layers:
            if child(item, "hide") or (child(item, "effects") and "hide" in child(item, "effects")):
                continue
            entry = text_entry(item, transform, frot)
            if entry[5] == "${REFERENCE}":
                ref = child(node, "property")
                entry[5] = next(p[2] for p in children(node, "property") if p[1] == "Reference")
            texts.append(entry)
        elif tag == "pad":
            kind = item[2]
            shape = item[3]
            if kind == "np_thru_hole":
                pad_kind = "npth"
            elif kind == "thru_hole":
                pad_kind = "pth"
            elif front and any(l in ("F.Cu",) for l in layers):
                pad_kind = "smd"
            else:
                continue
            at = nums(child(item, "at"))
            x, y = transform(at[0], at[1])
            rot = at[2] if len(at) > 2 else frot
            w, h = nums(child(item, "size"), 2)
            drill = child(item, "drill")
            drill_w = drill_h = 0.0
            if drill:
                values = [float(v) for v in drill[1:] if not isinstance(v, list) and v != "oval"]
                drill_w = values[0] if values else 0.0
                drill_h = values[1] if len(values) > 1 else drill_w
            pads.append([pad_kind, shape, round(x, 3), round(y, 3), w, h, rot, drill_w, drill_h])


if __name__ == "__main__":
    main()
