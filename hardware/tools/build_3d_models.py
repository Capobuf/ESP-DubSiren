"""Build the project 3D models in KiCad footprint coordinates.

Run with FreeCAD's Python (no GUI):
    "C:/Program Files/FreeCAD 1.1/bin/python.exe" -I hardware/tools/build_3d_models.py <vendor_dir> <out_dir>

Frame of every output: X = footprint X, Y = -footprint Y (KiCad 3D),
Z = 0 on the board surface, positive away from the board. KiCad can then use
offset 0 / rotation 0 / scale 1. Sources and dimensions: hardware/cad/3dmodels/README.md.
"""
import os
import sys

sys.path.append(r"C:/Program Files/FreeCAD 1.1/bin")
sys.path.append(r"C:/Program Files/FreeCAD 1.1/lib")
import FreeCAD  # noqa: E402
import Import  # noqa: E402
import Part  # noqa: E402
from FreeCAD import Matrix, Placement, Rotation, Vector  # noqa: E402

VENDOR, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
BIG = 200.0


def read(name):
    s = Part.Shape()
    s.read(os.path.join(VENDOR, name))
    return s


def rotated(shape, *rots):
    s = shape.copy()
    for axis, deg in rots:
        s = s.transformGeometry(Placement(Vector(), Rotation(axis, deg)).toMatrix())
    return s


def moved(shape, x=0.0, y=0.0, z=0.0):
    s = shape.copy()
    s.translate(Vector(x, y, z))
    return s


def save(shape, name):
    path = os.path.join(OUT, name)
    shape.exportStep(path)
    b = shape.BoundBox
    print("%-38s x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f" % (name, b.XMin, b.XMax, b.YMin, b.YMax, b.ZMin, b.ZMax))


X, Z = Vector(1, 0, 0), Vector(0, 0, 1)

# Bourns PTV111-3220A (pin style 3, 20 mm): same outline as PTV111-3420A (detent is internal).
# Vendor frame: shaft +Y, pins at Z=+11; body face Y=0, four 0.5 x 1.3 standoff feet down to
# Y=-1.3 (seat plane).  Rx(+90): (x,y,z) -> (x,-z,y); then feet to Z=0.
save(moved(rotated(read("ptv111-3220a.stp"), (X, 90)), z=1.3), "Bourns_PTV111-3420A.step")

# Bourns PTV111-1415A (pin style 1, 15 mm) lengthened to the 20 mm PTV111-1420A:
# cut inside the plain D6 shaft (Y=13, between bushing and flat) and insert 5 mm.
s = read("ptv111-1x15a.stp")
lower = s.common(Part.makeBox(BIG, 13 + BIG, BIG, Vector(-BIG / 2, -BIG, -BIG / 2)))
upper = s.common(Part.makeBox(BIG, BIG, BIG, Vector(-BIG / 2, 13, -BIG / 2)))
upper.translate(Vector(0, 5, 0))
mid = Part.Face(Part.Wire(s.slice(Vector(0, 1, 0), 13)[0])).extrude(Vector(0, 5, 0))
s = lower.fuse([mid, upper]).removeSplitter()
# Vendor frame: pins +Z at Y=2.0, legs at Y=-1.3, body face Z=9.4, four 0.5 x 1.0 standoff
# feet to Z=10.0 (seat plane). Rx(180): (x,y,z) -> (x,-y,-z); then legs to Y=0, feet to Z=0.
save(moved(rotated(s, (X, 180)), y=-1.3, z=10.0), "Bourns_PTV111-1420A.step")

# Bourns PEC11R-4x20F-Sxxxx (file header PEC11-4220F-S0024): shaft +Y,
# standoff plane Y=-5.5, A/C/B at Z=-7.5, switch at Z=+7.05.
# Rx(+90) then Rz(180): (x,y,z) -> (-x, z, y); seat to Z=0.
save(moved(rotated(read("pec11r-4x20f-sxxxx.stp"), (X, 90), (Z, 180)), z=5.5), "Bourns_PEC11R-4120F-S0018.step")

# Adafruit 6250 without the PJ-332A jack (removed in assembly, see hardware/README.md).
# Vendor frame: board 30.48 x 20.32 from origin, bottom face Z=0; centre = footprint origin.
doc = FreeCAD.newDocument("dac")
Import.insert(os.path.join(VENDOR, "Adafruit_6250_PCM510x_I2S_DAC.step"), doc.Name)
keep = []
for o in doc.Objects:
    if o.TypeId == "Part::Feature" and o.Shape.Solids and not o.Label.startswith("PJ-332A"):
        sh = o.Shape.copy()
        sh.Placement = o.getGlobalPlacement()  # include parent App::Part placements
        keep.append(sh)
save(moved(Part.makeCompound(keep), -15.24, -10.16), "Adafruit_6250_PCM510x_no_jack.step")

# CHERRY MX2A-11NW, simplified from the CHERRY datasheets (MX2A Black p.3, MX legacy layout):
# 15.6 x 15.6 top housing, 11.6 mm above PCB, 14.0 x 14.0 lower housing for the 5.0 mm
# plate-to-PCB distance, 3.6 mm stem, 3.3 mm pins (18.5 mm total).
cx, cy = -2.54, -5.08  # switch centre in KiCad 3D frame (footprint centre post at 2.54/5.08 rows)
parts = [
    Part.makeBox(14.0, 14.0, 5.0, Vector(cx - 7.0, cy - 7.0, 0)),
    Part.makeBox(15.6, 15.6, 6.6, Vector(cx - 7.8, cy - 7.8, 5.0)),
    Part.makeBox(4.0, 1.2, 3.6, Vector(cx - 2.0, cy - 0.6, 11.6)),
    Part.makeBox(1.2, 4.0, 3.6, Vector(cx - 0.6, cy - 2.0, 11.6)),
    Part.makeCylinder(1.9, 3.3, Vector(cx, cy, -3.3)),          # centre post, hole D4.0
    Part.makeCylinder(0.8, 3.3, Vector(cx - 5.08, cy, -3.3)),   # fixation pins, holes D1.7
    Part.makeCylinder(0.8, 3.3, Vector(cx + 5.08, cy, -3.3)),
    Part.makeCylinder(0.5, 3.3, Vector(0, 0, -3.3)),            # contact pin 1, hole D1.5
    Part.makeCylinder(0.5, 3.3, Vector(-6.35, -2.54, -3.3)),    # contact pin 2
]
save(parts[0].fuse(parts[1:]).removeSplitter(), "CHERRY_MX2A-11NW_simplified.step")

# C&K PTS636 SM43 SMTR LFS (datasheet p.2, SMT G leads, H=4.3): body 6.0 x 3.5 x 3.3,
# actuator 3.0 x 1.5 up to 4.3, leads 1.0 wide over 8.7 mm.
parts = [
    Part.makeBox(6.0, 3.5, 3.3, Vector(-3.0, -1.75, 0)),
    Part.makeBox(3.0, 1.5, 1.0, Vector(-1.5, -0.75, 3.3)),
    Part.makeBox(1.35, 1.0, 0.3, Vector(-4.35, -0.5, 0)),
    Part.makeBox(1.35, 1.0, 0.3, Vector(3.0, -0.5, 0)),
]
save(parts[0].fuse(parts[1:]).removeSplitter(), "CK_PTS636SM43SMTR_simplified.step")

# Bel 0ZCG0110CF2B: maximum body 4.73 x 3.41 x 1.30 (footprint descr, Bel datasheet p.3).
save(Part.makeBox(4.73, 3.41, 1.30, Vector(-2.365, -1.705, 0)), "Bel_0ZCG0110CF2B_envelope.step")

# MPS MPM3612GLQ-33: LGA 3 x 3 x 2 mm package (datasheet p.1).
save(Part.makeBox(3.0, 3.0, 2.0, Vector(-1.5, -1.5, 0)), "MPS_MPM3612GLQ-33_envelope.step")
