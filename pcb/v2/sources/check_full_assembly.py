"""Numerical solid-intersection check of the saved assembly; no CAD mutations."""
from pathlib import Path
import json,itertools
import FreeCAD as A
R=Path(__file__).resolve().parents[1]
d=A.openDocument(str(R/'mechanical/assembly-v2.FCStd'))
objs=[o for o in d.Objects if hasattr(o,'AssemblyGroup') and o.AssemblyGroup not in ['CUT_TOOL','RESERVE','CAD_CONSTRUCTION']]
bounds={o.Name:o.Shape.optimalBoundingBox() for o in objs}
hits=[];tested=0
for a,b in itertools.combinations(objs,2):
    if a.AssemblyGroup==b.AssemblyGroup:continue
    aa,bb=bounds[a.Name],bounds[b.Name]
    if any(min(getattr(aa,c+'Max'),getattr(bb,c+'Max'))-max(getattr(aa,c+'Min'),getattr(bb,c+'Min'))<1e-5 for c in 'XYZ'):continue
    tested+=1;v=a.Shape.common(b.Shape).Volume
    if v>1e-5:hits.append(dict(a=a.Name,b=b.Name,volume_mm3=round(v,6),groups=[a.AssemblyGroup,b.AssemblyGroup]))
out=dict(fitted_shapes=len(objs),broadphase_pairs=tested,positive_intersections=hits,
         note='Same assembly-group solid overlaps are intended component/hardware interfaces and separately dimensioned. Thread engagement and clearance envelopes require adjudication, not blanket exclusion.')
(R/'reports/assembly-collision-raw.json').write_text(json.dumps(out,indent=2))
print(json.dumps(out,indent=2))
