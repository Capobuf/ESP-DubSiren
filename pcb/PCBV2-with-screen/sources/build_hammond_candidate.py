"""Reuse V2 component solids in the original Hammond; candidate, not fit approval."""
import json
from pathlib import Path
import FreeCAD as A
import FreeCADGui as G
import Part

R = Path(__file__).resolve().parents[1]
OLD = R.parent / 'v2'
V = A.Vector
C = {c['reference']: c for c in json.loads((OLD/'reports/pcb-placement-components.json').read_text())['components']}
plan = {f'RV{i+1}': (x,y) for i,(x,y) in enumerate(( (x,y) for y in (22,83.6) for x in (22,47.75,73.5)))}
# In this reused CAD frame, +Y points toward the button row and +Z outward.
# Seen from outside with buttons down, screen-right is CAD -X.
plan.update({'SW7':(16,53.8),'U5':(47.75,53.8),'U6':(47.75,80),
             'RV7':(18,9.5),'J1':(39-(45-C['J1']['x']), C['J1']['y']),'J2':(69,12),'U1':(73,55)})
plan.update({f'SW{i+1}':(x+2.54,107.7-5.08) for i,x in enumerate((18,38,58,78))})
Z,T = 23.1,24.7
old = A.openDocument(str(OLD/'mechanical/assembly-v2.FCStd'))
existing = A.listDocuments().get('HammondPlacementCandidate')
if existing:
    A.closeDocument(existing.Name)
doc = A.newDocument('HammondPlacementCandidate')
objects, moving, cuts = {}, {}, []

def box(w,d,h,x,y,z):
    return Part.makeBox(w,d,h,V(x,y,z))

def cylinder(r,h,x,y,z,axis=(0,0,1)):
    return Part.makeCylinder(r,h,V(x,y,z),V(*axis))

def add(name,s,group,evidence='',color=(0.65,0.65,0.68)):
    o=doc.addObject('PartDesign::Feature',name)
    o.Shape=s
    o.addProperty('App::PropertyString','AssemblyGroup');o.AssemblyGroup=group
    o.addProperty('App::PropertyString','Evidence');o.Evidence=evidence
    o.ViewObject.ShapeColor=color
    objects[name]=o
    return o

def copy(name,ref,group=None,dz=None):
    src=old.getObject(name)
    s=src.Shape.copy()
    x,y=plan[ref];c=C[ref]
    s.translate(V(x-c['x'],y-c['y'],Z-30.75 if dz is None else dz))
    o=add(name,s,group or src.AssemblyGroup,getattr(src,'Evidence',''),src.ViewObject.ShapeColor)
    return o

source=Part.read(str(R.parent/'3dmodels/Hammond_29830PSLA.stp'))
m=A.Matrix()
m.A11,m.A12,m.A13,m.A14=0,0,-1,47.75
m.A21,m.A22,m.A23,m.A24=1,0,0,60.4
m.A31,m.A32,m.A33,m.A34=0,-1,0,3.8
shells=[]
for i,s in enumerate(source.Solids):
    s.transformShape(m);shells.append(s)
    o=add(('OriginalBody','OriginalBottomCover')[i] if i<2 else f'OriginalScrew{i-1}',s,'HAMMOND_ORIGINAL','Manufacturer STEP, rigid portrait transform')
    if i==0:o.ViewObject.Visibility=False

for i in range(1,7):
    ref=f'RV{i}';x,y=plan[ref]
    for suffix in ('Body','Bushing','Shaft'):
        o=copy(f'POT{i}{suffix}',ref);moving[o.Name]=o
    copy(f'POT{i}Davies1100',ref)
    copy(f'POT{i}Washer',ref,dz=33.8-41.5)
    copy(f'POT{i}Nut',ref,dz=33.8-41.5)
    # Seat the bushing on a 0.5 mm reaction ring at the pocket ceiling.
    add(f'POT{i}ReactionRing',cylinder(6,.5,x,y,32.2).cut(cylinder(4.6,.5,x,y,32.2)),f'POT{i}','Nominal 0.5 mm reaction ring; tolerance and load validation pending')
    cuts += [cylinder(4.65,8,x,y,29),box(16.4,19.7,2.7,x-8.2,y-7.7,30)]

for suffix in ('Body','Bushing','Shaft'):
    o=copy('Encoder'+suffix,'SW7');moving[o.Name]=o
copy('CliffCL170805B','SW7')
copy('EncoderTravelEnvelope','SW7')
for suffix in ('Washer','Nut'):
    copy('Encoder'+suffix,'SW7',dz=33.8-42)
x,y=plan['SW7']
cuts.append(cylinder(3.65,8,x,y,29))
add('EncoderReactionRing',cylinder(5.5,.6,x,y,31.2).cut(cylinder(3.6,.6,x,y,31.2)),'ENCODER','Nominal 0.6 mm ring to the unpocketed inner roof')

for i in range(1,5):
    ref=f'SW{i}';cx,cy=plan[ref][0]-2.54,plan[ref][1]+5.08
    for suffix in ('MXBody','Stem'):
        o=copy(f'KEY{i}{suffix}',ref);moving[o.Name]=o
    for suffix in ('KeycapEnvelope','TravelEnvelope'):
        copy(f'KEY{i}{suffix}',ref)
    cuts.append(box(20.05,20.05,10,cx-10.025,cy-10.025,28))

for src in old.Objects:
    group=getattr(src,'AssemblyGroup','')
    if group in ('DISPLAY','DISPLAY_MOUNT','U5_HEADER','DAC','DAC_MOUNT','U6_HEADER'):
        ref='U5' if group.startswith('DISPLAY') or group=='U5_HEADER' else 'U6'
        o=copy(src.Name,ref);moving[o.Name]=o
copy('DisplayActiveArea','U5')
x,y=plan['U5'];cuts.append(box(38.3,31.5,10,x-19.15,y-15.75,28))

for name in ('MasterBody','MasterBushing','MasterShaft','USB4085','RCJ2123','ESP32Module'):
    ref='RV7' if name.startswith('Master') else {'USB4085':'J1','RCJ2123':'J2','ESP32Module':'U1'}[name]
    o=copy(name,ref);moving[name]=o
for name in ('MasterWasher','MasterNut','SameSkySAK023'):
    copy(name,'RV7')
x,y=plan['RV7'];cuts += [cylinder(4.65,8,x,-2,Z-10,(0,1,0)),box(20,3,22,x-10,1.5,Z-18)]
x,y=plan['J1'];usb_center=x+(45-C['J1']['x']);cuts.append(box(12,8,6,usb_center-6,-2,Z-4.7))
x,y=plan['J2']
for dx in (-7,7):cuts.append(cylinder(5.6,8,x+dx,-2,Z-8,(0,1,0)))

for src in old.Objects:
    if src.Name.endswith(('SolderAllowance','LowerFeatureAllowance')):
        ref=src.Name.replace('SolderAllowance','').replace('LowerFeatureAllowance','')
        if ref in plan:
            o=copy(src.Name,ref);moving[o.Name]=o

# Keep the four original board holes in the candidate; their new coordinates
# must subsequently be transferred to KiCad with the rest of the placement.
holes=[(10,40),(85.5,40),(10,95),(85.5,95)]
outline=[(10,4),(85.5,4),(85.5,10),(91.5,10),(91.5,110.8),(85.5,110.8),
         (85.5,116.8),(10,116.8),(10,110.8),(4,110.8),(4,10),(10,10)]
pcb=Part.Face(Part.makePolygon([V(x,y,Z) for x,y in outline+[outline[0]]])).extrude(V(0,0,1.6))
for i,(x,y) in enumerate(holes,1):
    pcb=pcb.cut(cylinder(1.6,3,x,y,Z-.5));plan[f'H{i}']=(x,y)
o=add('PCBEnvelope_NOT_KICAD_EXPORT',pcb,'PCB','Candidate outline only; not a KiCad export',(0.05,.35,.24));moving[o.Name]=o
body=shells[0]
for s in cuts:body=body.cut(s)
machined=add('HammondMachinedCandidate',body,'ENCLOSURE','Original Hammond with openings, six 0.9 mm internal pot pockets, and local master relief')
machined.ViewObject.Transparency=70
cutobj=add('MachiningVolumes',Part.makeCompound(cuts),'CUT_TOOL');cutobj.ViewObject.Visibility=False
doc.recompute()

violations=[]
for name,o in objects.items():
    if o.AssemblyGroup in ('HAMMOND_ORIGINAL','ENCLOSURE','CUT_TOOL'):continue
    for ename,s in [('body',body),('bottom',shells[1])]:
        if o.Shape.BoundBox.intersect(s.BoundBox):
            common=o.Shape.common(s);v=common.Volume
            if v>1e-5:violations.append(dict(item=name,enclosure=ename,volume_mm3=v))

result=dict(status='CANDIDATE_NOT_VALIDATED',pcb_bottom_z=Z,pcb_top_z=T,
    coordinate_frame='CAD +Y toward buttons, +Z out of control face; user front view has RCA up, buttons down, screen-right along CAD -X. These are CAD anchors, not final KiCad placements.',
    original_enclosure_mm=[95.5,120.8,33.8],pot_pocket_depth_mm=.9,
    remaining_roof_at_pots_mm=1.1,mechanical_strength_verified=False,
    outline=outline,placements={ref:dict(x=p[0],y=p[1],rotation=C[ref]['rotation'],layer=C[ref]['layer']) for ref,p in plan.items()},
    enclosure_intersections=violations,unperformed=['insertion path','complete electronic placement','tolerances and pocket strength','real KiCad board export','complete component pair collision audit'])
(R/'reports/hammond-candidate.json').write_text(json.dumps(result,indent=2))
doc.saveAs(str(R/'mechanical/hammond-placement-candidate.FCStd'))
A.closeDocument(old.Name)
G.activeDocument().activeView().viewAxonometric();G.activeDocument().activeView().fitAll()
G.activeDocument().activeView().saveImage(str(R/'reports/hammond-candidate.png'),1600,1200,'Current')
view=G.activeDocument().activeView()
view.viewTop();view.setCameraOrientation(A.Rotation(V(0,0,1),180).Q);view.fitAll()
view.saveImage(str(R/'reports/hammond-front-rca-up-buttons-down.png'),1200,1500,'Current')
print(json.dumps(dict(status=result['status'],enclosure_intersections=violations),indent=2))
