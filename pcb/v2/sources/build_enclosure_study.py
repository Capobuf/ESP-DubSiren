"""Parametric replacement enclosure STUDY, pending complete component Z stacks.

This is not a machining release. No unverified switch or display cutouts are
made. All output is derived from this model's explicit dimensions and centers.
Run with FreeCAD's bundled Python; no KiCad source is read or modified.
"""
from pathlib import Path
import json
import FreeCAD as App
import Part

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'mechanical'
OUT.mkdir(exist_ok=True)
doc=App.newDocument('V2EnclosureStudy')
params=doc.addObject('App::FeaturePython','Parameters')
for name,value in [('Width',125),('Depth',140),('Height',42),('Wall',3),
                   ('TopThickness',2),('BaseThickness',3),('BoardThickness',1.6),
                   ('BoardLowerZ',29),('BoardWidth',113),('BoardDepth',128)]:
    params.addProperty('App::PropertyLength',name,'Study dimensions')
    setattr(params,name,value)
params.addProperty('App::PropertyString','Status')
params.Status='STUDY ONLY: no final Z stack, component fit, routing or cutouts'

def box(name,l,w,h,x,y,z):
    b=doc.addObject('Part::Box',name)
    b.Length,b.Width,b.Height=l,w,h
    b.Placement.Base=App.Vector(x,y,z)
    return b

outer=box('BodyOuter',125,140,39,0,0,3)
for prop,expr in [('Length','Parameters.Width'),('Width','Parameters.Depth'),
                  ('Height','Parameters.Height - Parameters.BaseThickness'),
                  ('Placement.Base.z','Parameters.BaseThickness')]:
    outer.setExpression(prop,expr)
inner=box('BodyCavity',119,134,37,3,3,3)
for prop,expr in [('Length','Parameters.Width - 2 * Parameters.Wall'),
                  ('Width','Parameters.Depth - 2 * Parameters.Wall'),
                  ('Height','Parameters.Height - Parameters.BaseThickness - Parameters.TopThickness'),
                  ('Placement.Base.x','Parameters.Wall'),('Placement.Base.y','Parameters.Wall'),
                  ('Placement.Base.z','Parameters.BaseThickness')]:
    inner.setExpression(prop,expr)
body=doc.addObject('Part::Cut','Body')
body.Base,body.Tool=outer,inner
base=box('Base',125,140,3,0,0,0)
for prop,expr in [('Length','Parameters.Width'),('Width','Parameters.Depth'),('Height','Parameters.BaseThickness')]:
    base.setExpression(prop,expr)
pcb=box('TrialBoardEnvelope',113,128,1.6,6,6,29)
pcb.setExpression('Length','Parameters.BoardWidth')
pcb.setExpression('Width','Parameters.BoardDepth')
pcb.setExpression('Height','Parameters.BoardThickness')
pcb.setExpression('Placement.Base.z','Parameters.BoardLowerZ')

# Standard M3 clearance on explicit study centers. These are intended supports;
# PCB holes are not yet applied to KiCad, and the study is not fit-accepted.
mounts=[(12,12),(113,12),(12,128),(113,128)]
bosses=[]
for i,(x,y) in enumerate(mounts,1):
    boss=doc.addObject('Part::Cylinder','Boss%d'%i)
    boss.Radius,boss.Height=4,26
    boss.Placement.Base=App.Vector(x,y,3)
    boss.setExpression('Height','Parameters.BoardLowerZ - Parameters.BaseThickness')
    hole=doc.addObject('Part::Cylinder','M3Pilot%d'%i)
    hole.Radius,hole.Height=1.25,10
    hole.Placement.Base=App.Vector(x,y,19)
    hole.setExpression('Placement.Base.z','Parameters.BoardLowerZ - 10 mm')
    bored=doc.addObject('Part::Cut','BoredBoss%d'%i)
    bored.Base,bored.Tool=boss,hole
    bosses.append(bored)
base_supported=doc.addObject('Part::MultiFuse','BaseWithTrialSupports')
base_supported.Shapes=[base]+bosses
doc.recompute()
assert body.Shape.isValid() and base_supported.Shape.isValid()
assert body.Shape.common(base_supported.Shape).Volume < 1e-6
doc.saveAs(str(OUT/'enclosure-study-v2.FCStd'))
result={
    'status':params.Status,'outer_mm':[125,140,42],
    'trial_board_mm':[113,128,1.6], 'trial_board_lower_z_mm':29,
    'mount_centers_mm':mounts,'boss_diameter_mm':8,'pilot_diameter_mm':2.5,
    'pot_centers_mm':[[x,y] for y in [26,94] for x in [27.5,62.5,97.5]],
    'display_board_center_mm':[62.5,60], 'encoder_center_mm':[97.5,60],
    'keycap_center_candidates_mm':[[x,122] for x in [27.5,50.8,74.2,97.5]],
    'checks':{'body_valid':True,'base_valid':True,'body_base_intersection_mm3':0},
    'unperformed':['final PCB mounting holes','rear opening locations','display Z stack',
                   'MX2A drill acceptance','keycap travel','all component collisions',
                   'lid fasteners','machining templates'],
}
(ROOT/'reports/enclosure-study-v2.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result,indent=2))
