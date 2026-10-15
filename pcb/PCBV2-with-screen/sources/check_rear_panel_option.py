"""Evaluate a removable rear plate on the original Hammond, including insertion."""
import json
from pathlib import Path
import FreeCAD as A
import FreeCADGui as G
import Part

R=Path(__file__).resolve().parents[1]
V=A.Vector
source=A.getDocument('HammondPlacementCandidate')
if 'HammondRearPanelOption' in A.listDocuments():A.closeDocument('HammondRearPanelOption')
doc=A.newDocument('HammondRearPanelOption')
objects={}
for src in source.Objects:
    if not hasattr(src,'Shape'):continue
    o=doc.addObject('PartDesign::Feature',src.Name);o.Shape=src.Shape.copy()
    for prop in ('AssemblyGroup','Evidence'):
        o.addProperty('App::PropertyString',prop);setattr(o,prop,getattr(src,prop,''))
    o.ViewObject.ShapeColor=src.ViewObject.ShapeColor
    o.ViewObject.Visibility=src.ViewObject.Visibility
    o.ViewObject.Transparency=src.ViewObject.Transparency
    objects[o.Name]=o

def box(w,d,h,x,y,z):return Part.makeBox(w,d,h,V(x,y,z))
def cyl(r,h,x,y,z,axis=(0,0,1)):return Part.makeCylinder(r,h,V(x,y,z),V(*axis))
def add(name,s,group,evidence):
    o=doc.addObject('PartDesign::Feature',name);o.Shape=s
    o.addProperty('App::PropertyString','AssemblyGroup');o.AssemblyGroup=group
    o.addProperty('App::PropertyString','Evidence');o.Evidence=evidence
    objects[name]=o
    return o

# One millimetre rearward shift restores thread allowance through a 1 mm plate.
for n in ('MasterBody','MasterBushing','MasterShaft','RV7SolderAllowance'):
    s=objects[n].Shape.copy();s.translate(V(0,-1,0));objects[n].Shape=s
for n in ('MasterWasher','MasterNut','SameSkySAK023'):
    s=objects[n].Shape.copy();s.translate(V(0,-1,0));objects[n].Shape=s
window=box(76.5,8,21.8,9.5,-2,3.8)
body=objects['HammondMachinedCandidate'].Shape.cut(window)
plate=box(80.5,1,27,7.5,-1,3.8)
openings=[cyl(4.65,3,18,-2,13.1,(0,1,0)),box(12,3,6,33,-2,18.4)]
openings += [cyl(5.6,3,x,-2,15.1,(0,1,0)) for x in (62,76)]
for s in openings:plate=plate.cut(s)
for x in (12,83.5):
    hole=cyl(1.1,8,x,-2,28,(0,1,0));plate=plate.cut(hole);body=body.cut(hole)
    screw=cyl(1,8,x,-.8,28,(0,1,0)).fuse(cyl(1.9,2,x,-2.8,28,(0,1,0)))
    add(f'PlateScrew{str(x).replace(".","p")}',screw,'REAR_PLATE_MOUNT','M2x8 nominal envelope; nut and mounting-stack validation pending')
objects['HammondMachinedCandidate'].Shape=body
add('RemovableRearPlate',plate,'REAR_PLATE','Candidate 80.5 x27 x1 mm aluminium plate; two upper M2 fasteners; strength and nut retention pending')
add('RearOpeningTool',window,'CUT_TOOL','76.5 x21.8 rear opening, open to the bottom seam').ViewObject.Visibility=False

checks=[]
travel=60
def sweep_check(name,s):
    v=s.common(body).Volume
    checks.append(dict(item=name,intersection_mm3=v,valid=s.isValid()))

def swept_box(name,shape):
    b=shape.optimalBoundingBox()
    sweep_check(name,box(b.XLength,b.YLength,b.ZLength+travel,b.XMin,b.YMin,b.ZMin-travel))

for i in range(1,7):
    swept_box(f'POT{i}Body',objects[f'POT{i}Body'].Shape)
    b=objects[f'POT{i}Bushing'].Shape.optimalBoundingBox()
    top=objects[f'POT{i}Shaft'].Shape.optimalBoundingBox().ZMax
    sweep_check(f'POT{i}BushingAndShaft',cyl(4.5,top-b.ZMin+travel,(b.XMin+b.XMax)/2,(b.YMin+b.YMax)/2,b.ZMin-travel))
swept_box('EncoderBody',objects['EncoderBody'].Shape)
b=objects['EncoderBushing'].Shape.optimalBoundingBox()
top=objects['EncoderShaft'].Shape.optimalBoundingBox().ZMax
sweep_check('EncoderBushingAndShaft',cyl(3.5,top-b.ZMin+travel,(b.XMin+b.XMax)/2,(b.YMin+b.YMax)/2,b.ZMin-travel))
for n,o in objects.items():
    if o.AssemblyGroup in ('DISPLAY','DISPLAY_MOUNT','U5_HEADER','DAC','DAC_MOUNT','U6_HEADER','MCU') or n.endswith(('MXBody','Stem')):
        swept_box(n,o.Shape)
for n in ('MasterBody','MasterBushing','MasterShaft','USB4085','RCJ2123'):
    s=objects[n].Shape
    # Partition the connector at the rear wall so protruding lead tips do not
    # falsely enlarge the front socket's height in the conservative swept bound.
    for label,clip in [('rear',box(100,24,80,0,-20,-10)),('inside',box(100,140,80,0,4,-10))]:
        piece=s.common(clip)
        if not piece.isNull() and piece.Volume>1e-8:swept_box(n+'_'+label,piece)
data=json.loads((R/'reports/hammond-candidate.json').read_text())
outline=data['outline']
pcb=Part.Face(Part.makePolygon([V(x,y,23.1-travel) for x,y in outline+[outline[0]]])).extrude(V(0,0,1.6+travel))
sweep_check('PCB_outline_continuous_sweep',pcb)
for n,o in objects.items():
    if n.endswith(('SolderAllowance','LowerFeatureAllowance')):swept_box(n,o.Shape)

failures=[r for r in checks if not r['valid'] or r['intersection_mm3']>1e-5]
result=dict(status='INSERTION_ENVELOPES_CLEAR' if not failures else 'INSERTION_NOT_CLEAR',
    method='Continuous conservative swept volumes for 60 mm vertical insertion, rear plate and bottom cover removed. External knobs/nuts fitted afterwards.',
    original_enclosure_preserved=True,original_enclosure_machining=['six 0.9 mm pot pockets','top control/display apertures','rear window and two M2 plate holes'],
    master_cad_anchor=[18,8.5],rear_plate_mm=[80.5,27,1],sweeps=checks,failures=failures,
    incomplete=['complete component pair collision audit','rear plate nuts and fastening','lower DAC mounts','all small electronic packages','manufacturing tolerances and pocket strength','KiCad export correlation'])
(R/'reports/rear-panel-insertion.json').write_text(json.dumps(result,indent=2))
doc.recompute();doc.saveAs(str(R/'mechanical/hammond-rear-panel-option.FCStd'))
G.activeDocument().activeView().viewAxonometric();G.activeDocument().activeView().fitAll()
print(json.dumps(dict(status=result['status'],failures=failures,sweep_count=len(checks)),indent=2))
