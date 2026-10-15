"""Document exact collisions for candidate extraction motions; not exhaustive."""
import json
from pathlib import Path
import FreeCAD as A
import Part

R=Path(__file__).resolve().parents[1]
doc=A.getDocument('HammondPlacementCandidate')
body=doc.getObject('HammondMachinedCandidate').Shape
names=[f'POT{i}{s}' for i in range(1,7) for s in ('Body','Bushing','Shaft')]
names+=['MasterBody','MasterBushing','MasterShaft','USB4085','RCJ2123','PCBEnvelope_NOT_KICAD_EXPORT']
shapes={n:doc.getObject(n).Shape for n in names}

def check(angle,dy,dz):
    hits=[]
    for n,shape in shapes.items():
        s=shape.copy()
        s.rotate(A.Vector(0,0,13.1),A.Vector(1,0,0),angle)
        s.translate(A.Vector(0,dy,dz))
        if not s.BoundBox.intersect(body.BoundBox):continue
        v=s.common(body).Volume
        if v>1e-4:hits.append(dict(item=n,volume_mm3=round(v,6)))
    return dict(angle_deg=angle,translation_y_mm=dy,translation_z_mm=dz,collisions=hits)

poses=[(0,0,0),(0,0,-.2),(0,0,-1),(0,0,-5),(0,1,0),(0,3,0),
       (-2,0,0),(-5,0,0),(-10,0,0),(-5,-1,0),(-5,1,0),(5,1,-1)]
results=[check(*p) for p in poses]
out=dict(status='NO_VALIDATED_INSERTION_PATH',method='Exact solid intersections at listed poses, bottom cover and external knobs/nuts removed',
         limitation='Finite diagnostic samples do not prove global impossibility of every six-axis path',
         poses=results)
(R/'reports/insertion-diagnostic.json').write_text(json.dumps(out,indent=2))
print(json.dumps([dict(**{k:v for k,v in r.items() if k!='collisions'},collisions=[h['item'] for h in r['collisions']]) for r in results],indent=2))
