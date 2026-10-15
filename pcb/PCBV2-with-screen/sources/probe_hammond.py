"""Read manufacturer solids and measure the usable cavity; never edit KiCad."""
import json
from pathlib import Path
import FreeCAD as A
import Part

ROOT = Path(__file__).resolve().parents[1]
source = Part.read(str(ROOT.parent / '3dmodels/Hammond_29830PSLA.stp'))
V = A.Vector
m = A.Matrix()
# Portrait enclosure, rear at Y=0, removable lid at the bottom; rigid rotation.
m.A11, m.A12, m.A13, m.A14 = 0, 0, -1, 47.75
m.A21, m.A22, m.A23, m.A24 = 1, 0, 0, 60.4
m.A31, m.A32, m.A33, m.A34 = 0, -1, 0, 3.8

def bounds(s):
    b = s.optimalBoundingBox()
    return [getattr(b, k) for k in ('XMin','YMin','ZMin','XMax','YMax','ZMax')]

out = []
for i, s in enumerate(source.Solids):
    s.transformShape(m)
    out.append(dict(index=i, bounds=bounds(s), valid=s.isValid(), volume=s.Volume))
    if i == 0:
        body = s
        out[-1]['horizontal_faces'] = [dict(z=f.CenterOfMass.z, area=f.Area,
            bounds=bounds(f)) for f in s.Faces if f.optimalBoundingBox().ZLength < 1e-5]
        out[-1]['slices'] = {}
        for z in (3, 10, 23.1, 24.7, 31.8, 32.2, 32.7):
            section = s.section(Part.makePlane(200,200,V(-20,-20,z)))
            out[-1]['slices'][str(z)] = [bounds(e) for e in section.Edges]
path = ROOT / 'reports/hammond-geometry-probe.json'
path.write_text(json.dumps(out,indent=2))
print(json.dumps([{k:v for k,v in r.items() if k != 'slices'} for r in out],indent=2))
