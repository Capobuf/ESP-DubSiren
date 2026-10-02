"""Rebuild the V2 prototype assembly from saved KiCad exports and verified datums.

Run with FreeCAD's bundled Python. Does not modify KiCad source. Coordinates are
the board's X/Y millimetres, with Z upward. Dimensions which are conservative
occupied envelopes or adjustable prototype allowances are explicitly labelled.
"""
from pathlib import Path
import json,math,itertools
import FreeCAD as A
import Part
R=Path(__file__).resolve().parents[1]; O=R/'mechanical'; O.mkdir(exist_ok=True)
C={c['reference']:c for c in json.loads((R/'reports/pcb-placement-components.json').read_text())['components']}
P=json.loads((R/'reports/pcb-placement-pads.json').read_text())
doc=A.newDocument('ESP_DubSiren_V2_Assembly')
V=A.Vector
params=doc.addObject('App::FeaturePython','Dimensions')
values=dict(Width=125.,Depth=140.,Height=42.,Wall=3.,TopThickness=3.,BaseThickness=3.,
            BoardBottom=30.75,BoardThickness=1.6,ControlPocketCeiling=40.5,
            PotSpotfaceZ=41.5,DisplaySpacing=4.5,DACSpacing=8.,CoverBottom=44.,CoverThickness=1.,RearPanelThickness=1.5)
for n,v in values.items():params.addProperty('App::PropertyLength',n,'Assembly dimensions');setattr(params,n,v)
params.addProperty('App::PropertyString','RebuildInstructions');params.RebuildInstructions='Enclosure dimensions and machining primitives are editable through native FreeCAD expressions/booleans. Component placements remain verified fixed datums. After any edit, rebuild/revalidate assembly, collisions and templates; change the Python parameters to reproduce the whole assembly.'
Z=values['BoardBottom'];T=Z+values['BoardThickness'];objects={};groups={};notes={};tools=[];cuts={'top':[],'rear':[],'base':[],'overlay':[]}
def solid(name,s,group,source='',color=(.65,.65,.68)):
    o=doc.addObject('PartDesign::Feature',name);o.Shape=s
    o.addProperty('App::PropertyString','Evidence');o.Evidence=source
    o.addProperty('App::PropertyString','AssemblyGroup');o.AssemblyGroup=group
    objects[name]=o;groups[name]=group;notes[name]=source
    if o.ViewObject:o.ViewObject.ShapeColor=color
    return o
def bx(w,d,h,x,y,z):return Part.makeBox(w,d,h,V(x,y,z))
def cy(r,h,x,y,z,axis=(0,0,1)):return Part.makeCylinder(r,h,V(x,y,z),V(*axis))
def ring(ro,ri,h,x,y,z,axis=(0,0,1)):return cy(ro,h,x,y,z,axis).cut(cy(ri,h,x,y,z,axis))
def union(ss):return Part.makeCompound(ss) if len(ss)>1 else ss[0]
def transform(s,diag,origin):
    m=A.Matrix();m.A11,m.A22,m.A33=diag;m.A14,m.A24,m.A34=origin
    return s.transformGeometry(m)
def cut_feature(name,shape,panel,kind,info):
    bb=shape.BoundBox
    cylinders=[f.Surface for f in shape.Faces if isinstance(f.Surface,Part.Cylinder)]
    if len(cylinders)==1 and len(shape.Faces)==3:
        o=doc.addObject('Part::Cylinder',name);o.Radius=cylinders[0].Radius
        axis=cylinders[0].Axis
        if abs(axis.z)>.9:
            o.Height=bb.ZLength;o.Placement.Base=V((bb.XMin+bb.XMax)/2,(bb.YMin+bb.YMax)/2,bb.ZMin)
        else:
            assert abs(axis.y)>.9
            o.Height=bb.YLength;o.Placement=A.Placement(V((bb.XMin+bb.XMax)/2,bb.YMin,(bb.ZMin+bb.ZMax)/2),A.Rotation(V(0,0,1),V(0,1,0)))
    else:
        assert len(shape.Faces)==6,name
        o=doc.addObject('Part::Box',name);o.Length=bb.XLength;o.Width=bb.YLength;o.Height=bb.ZLength;o.Placement.Base=V(bb.XMin,bb.YMin,bb.ZMin)
    o.addProperty('App::PropertyString','Evidence');o.Evidence=info
    o.addProperty('App::PropertyString','AssemblyGroup');o.AssemblyGroup='CUT_TOOL'
    objects[name]=o;groups[name]='CUT_TOOL';notes[name]=info;tools.append(o);doc.recompute()
    assert abs(o.Shape.Volume-shape.Volume)<1e-5,name
    cuts[panel].append(dict(name=name,kind=kind,shape_bounds=list(shape.BoundBox.__getattribute__(a) for a in ['XMin','YMin','ZMin','XMax','YMax','ZMax']),info=info))
    return o
def top_circle(name,x,y,r,z=38,h=6,kind='through'):
    return cut_feature(name,cy(r,h,x,y,z),'top',kind,f'diameter {2*r:g} mm; Z {z:g}..{z+h:g}')
def rear_circle(name,x,z,r):return cut_feature(name,cy(r,6,x,-1,z,(0,1,0)),'rear','through',f'diameter {2*r:g} mm')
def rect_cut(name,w,d,x,y,z,h,panel='top',kind='through'):
    return cut_feature(name,bx(w,d,h,x,y,z),panel,kind,f'{w:g} x {d:g} mm')
def screw(name,x,y,underhead,length,d=2,head_d=3.8,head_h=2,group='HARDWARE'):
    return solid(name,cy(d/2,length,x,y,underhead-length).fuse(cy(head_d/2,head_h,x,y,underhead)),group,'ISO cylindrical screw envelope; selected nominal length, unthreaded envelope',(0.72,.73,.76))
def washer(name,x,y,z,d=2,outer=5,h=.5,group='HARDWARE'):
    return solid(name,ring(outer/2,(d+.2)/2,h,x,y,z),group,'Nominal washer envelope')
def nut(name,x,y,z,d=2,af=4,h=1.6,group='HARDWARE'):
    pts=[V(x+af/math.sqrt(3)*math.cos(math.pi/6+i*math.pi/3),y+af/math.sqrt(3)*math.sin(math.pi/6+i*math.pi/3),z) for i in range(6)]
    s=Part.Face(Part.makePolygon(pts+[pts[0]])).extrude(V(0,0,h)).cut(cy(d/2,h,x,y,z))
    return solid(name,s,group,'Nominal hex nut envelope')
def board_module(name,w,d,thick,x,y,z,mounts,group):
    r=2.54
    s=bx(w-2*r,d,thick,x-w/2+r,y-d/2,z).fuse(bx(w,d-2*r,thick,x-w/2,y-d/2+r,z))
    for dx in [-1,1]:
        for dy in [-1,1]:s=s.fuse(cy(r,thick,x+dx*(w/2-r),y+dy*(d/2-r),z))
    s=s.removeSplitter()
    for a,b in mounts:s=s.cut(cy(1.25,thick+2,a,b,z-1))
    ref='U5' if group=='DISPLAY' else 'U6'
    for q in P[ref]['pads']:
        if q['number']:s=s.cut(cy(.5,thick+2,q['x'],q['y'],z-1))
    return solid(name,s,group,'Official Eagle outline bounding envelope and exact mounting-hole centers',(0.06,.31,.28))

# Imported KiCad STEP contains the substrate; nominal finished PCB is 1.6 mm.
src=Part.Shape();src.read(str(O/'pcb-v2.step'))
assert len(src.Solids)==1,'Export PCB only; assembly components are placed below from their datums'
bb=src.optimalBoundingBox();pcb=transform(src,(1,-1,1.6/bb.ZLength),(0,0,Z-bb.ZMin*1.6/bb.ZLength))
solid('Motherboard',pcb,'PCB','KiCad STEP exact XY/drills/slots; exported substrate Z normalized to specified finished 1.600 mm',(0.05,.35,.24))
mounts=[(C[f'H{i}']['x'],C[f'H{i}']['y']) for i in range(1,5)]
base=bx(125,140,3,0,0,0);body=bx(125,140,39,0,0,3).cut(bx(119,134,36,3,3,3))
body_cuts=[];base_cuts=[]
for i,(x,y) in enumerate(mounts,1):
    # Boss plus a removable 1 mm support shim provides prototype Z adjustment.
    base=base.fuse(ring(4,1.25,Z-4,x,y,3))
    solid(f'MainboardSupportShim{i}',ring(4,1.6,1,x,y,Z-1),'MOUNT', 'Custom insulating spacer, nominal 1 mm; select actual thickness during assembly')
    washer(f'MainboardWasher{i}',x,y,T,d=3,outer=7,group='MOUNT')
    screw(f'MainboardScrew{i}',x,y,T+.5,8,d=3,head_d=5.5,head_h=3,group='MOUNT')
    # Blind tapped pilot accommodates M3 screw without touching lower equipment.
    base=base.cut(cy(1.25,10,x,y,Z-10))
closure=[(5,5),(120,5),(5,135),(120,135)]
for i,(x,y) in enumerate(closure,1):
    boss=cy(3.5,10,x,y,3).cut(cy(1.25,10,x,y,3));body=body.fuse(boss)
    hole=cy(1.6,4,x,y,-.5);base_cuts.append(hole)
    cb=cy(3,2,x,y,0);base_cuts.append(cb)
    cut_feature(f'BaseM3{i}',hole,'base','through','M3 clearance diameter3.2')
    cut_feature(f'BaseCounterbore{i}',cb,'base','counterbore','diameter6 depth2 from underside')
    # Bottom-facing screw, head in the base recess, shaft toward +Z.
    s=cy(1.5,8,x,y,2).fuse(cy(2.75,2,x,y,0))
    solid(f'ClosureScrew{i}',s,'CLOSURE','M3 x8, low-profile head <=2mm in counterbore')

# Front potentiometers. XY envelope deliberately includes housing and lead sweep.
for i in range(1,7):
    c=C[f'RV{i}'];x,y=c['x'],c['y'];g=f'POT{i}'
    solid(g+'Body',bx(14.4,17.7,7.5,x-7.2,y-6.7,T),g,'Conservative PTV111-3 envelope: verified maximum stake width, shaft-to-top6.7, PCB lead row11, axial7.5')
    solid(g+'Bushing',cy(4.5,5,x,y,T+7.5),g,'Bourns M9x0.75, length5')
    shaft=cy(3,20,x,y,T+7.5).cut(bx(8,2,20,x-4,y+1.5,T+7.5))
    solid(g+'Shaft',shaft,g,'6mm shaft /4.5mm D-flat, L20')
    solid(g+'ReactionCollar',ring(6,4.6,.65,x,y,T+7.5),g,'Custom measured insulating collar; nominal0.65; do not tighten against unsupported PCB')
    washer(g+'Washer',x,y,41.5,d=9,outer=14,h=.5,group=g)
    nut(g+'Nut',x,y,42,d=9,af=11,h=2,group=g)
    # Actual external knob and quarter-inch bore; 9mm nominal insertion is adjustable.
    kz=T+27.5-9
    knob=cy(19.84375/2,11.90625,x,y,kz).cut(cy(6.35/2,10.31875,x,y,kz))
    solid(g+'Davies1100',knob,g,'Davies catalog envelope; 6.35 bore, chosen9mm insertion; set screw and concentricity require physical check',(.13,.13,.15))
    body_cuts.append(top_circle(g+'Hole',x,y,4.65).Shape)
    body_cuts.append(top_circle(g+'Spotface',x,y,7.5,41.5,2,'spotface').Shape)
    body_cuts.append(rect_cut(g+'UndersidePocket',16.4,19.7,x-8.2,y-7.7,38,2.5,kind='underside_pocket').Shape)
    cuts['overlay'].append(dict(kind='knob',center=[x,y],diameter=19.84375,label=f'POT{i}'))

# Encoder and specified CLIFF knob.
x,y=C['SW7']['x'],C['SW7']['y'];g='ENCODER'
solid('EncoderBody',bx(12.5,13.4,6.5,x-6.25,y-6.7,T),g,'PEC11R-4 push version, body6.5 axial')
solid('EncoderBushing',cy(3.5,7,x,y,T+6.5),g,'M7x0.75, LB7')
shaft=cy(3,20,x,y,T+6.5).cut(bx(8,2,10,x-4,y+1.5,T+16.5))
solid('EncoderShaft',shaft,g,'6mm/4.5flat, L20 F10, switch travel max0.8')
solid('EncoderCollar',ring(5.5,3.6,1.65,x,y,T+6.5),g,'Measured insulating reaction collar nominal1.65')
washer('EncoderWasher',x,y,42,d=7,outer=12,group=g);nut('EncoderNut',x,y,42.5,d=7,af=10,h=2,group=g)
kz=T+26.5-12.5
knob=cy(7,17.8,x,y,kz).cut(cy(3,12.5,x,y,kz))
solid('CliffCL170805B',knob,g,'K85M envelope; nominal12.5 insertion, D-flat retention is a prototype check',(.12,.12,.14))
solid('EncoderTravelEnvelope',cy(7,18.6,x,y,kz-.8),'RESERVE','0.8mm maximum push travel; clearance envelope')
body_cuts.append(top_circle('EncoderHole',x,y,3.65).Shape)
body_cuts.append(rect_cut('EncoderUndersidePocket',15,16,x-7.5,y-8,38,2.5,kind='underside_pocket').Shape)
cuts['overlay'].append(dict(kind='knob',center=[x,y],diameter=14,label='ENCODER'))

# CHERRY native-footprint centers; PCB fixation means no plate-latch assumption.
for i in range(1,5):
    c=C[f'SW{i}'];x,y=c['x']-2.54,c['y']+5.08;g=f'KEY{i}'
    solid(g+'MXBody',bx(15.63,15.63,11.6,x-7.815,y-7.815,T),g,'CHERRY FFF equivalence; legacy body11.6, current15.63 square')
    cross=bx(4.1,1.2,3.6,x-2.05,y-.6,T+11.6).fuse(bx(1.2,4.1,3.6,x-.6,y-2.05,T+11.6))
    solid(g+'Stem',cross,g,'Illustrative cross inside verified3.6mm actuator-height envelope')
    # Keycaps are explicitly purchase-independent keepout envelopes, not fake MPNs.
    cap=bx(19.05,19.05,8,x-9.525,y-9.525,T+11.6).cut(bx(16.5,16.5,6.5,x-8.25,y-8.25,T+11.6))
    solid(g+'KeycapEnvelope',cap,g,'Unselected realistic1u allowance19.05 square x8 high; fit/travel must be checked',(.2,.2,.22))
    solid(g+'TravelEnvelope',bx(19.05,19.05,12,x-9.525,y-9.525,T+7.6),'RESERVE','Full4mm travel swept outer keycap allowance')
    body_cuts.append(rect_cut(g+'Opening',20.05,20.05,x-10.025,y-10.025,38,6).Shape)
    cuts['overlay'].append(dict(kind='keycap',center=[x,y],width=19.05,height=19.05,label=f'BTN{i}'))

# Display carrier, exact holes; height split is bounded conservatively on each face.
x,y=C['U5']['x'],C['U5']['y'];dm=[(q['x'],q['y']) for q in P['U5']['pads'] if q['pad_type']=='np_thru_hole']
dz=T+values['DisplaySpacing'];board_module('DisplayPCB',36.83,30.48,1.6,x,y,dz,dm,'DISPLAY')
under=bx(37.3,30.5,3.8,x-18.65,y-15.25,dz-3.8)
for a,b in dm:under=under.cut(cy(2.7,4,a,b,dz-3.9))
under=under.cut(bx(29,3.5,4,x-14.5,y+11.585,dz-3.9))
solid('DisplayUndersideBound',under,'DISPLAY','Conservative whole-module5.4 minusPCB1.6 underside bound; mounting hardware keepouts removed')
solid('DisplayGlassBound',bx(36.3,19.4,3.8,x-18.216,y-9.7,dz+1.6),'DISPLAY','Official Eagle physical TFT/FPC outline bound, <=3.8 abovePCB; total5.4 is not double-counted as a claimed real model')
active_center=(x-1.016,y)
solid('DisplayActiveArea',bx(32.35,17.4,.02,active_center[0]-16.175,y-8.7,dz+5.4),'RESERVE','Eagle tDocu rounded visible area bound; offset -1.016 X',(0.05,.6,.8))
for i,(a,b) in enumerate(dm,1):
    solid(f'DisplaySpacer{i}',ring(1.75,1.1,4.5,a,b,T),'DISPLAY_MOUNT','Custom insulating M2 spacer OD3.5 ID2.2 length4.5; clears header insulator')
    washer(f'DisplayTopWasher{i}',a,b,dz+1.6,group='DISPLAY_MOUNT');screw(f'DisplayScrew{i}',a,b,dz+2.1,12,group='DISPLAY_MOUNT')
    washer(f'DisplayBottomWasher{i}',a,b,Z-.5,outer=4,group='DISPLAY_MOUNT');nut(f'DisplayNut{i}',a,b,Z-2.1,group='DISPLAY_MOUNT')
opening=rect_cut('DisplayPanelOpening',38.3,31.5,x-19.15,y-15.75,38,6);body_cuts.append(opening.Shape)
bezel=bx(48,41,3,x-24,y-20.5,42)
bezel=bezel.cut(bx(38.3,21.4,5,x-19.15,y-10.7,41))
bezel=bezel.cut(bx(41.4,34.4,2,x-20.7,y-17.2,44))
bezel_mounts=[(x+dx,y+dy) for dx in [-22,22] for dy in [-18.5,18.5]]
for i,(a,b) in enumerate(bezel_mounts,1):
    hole=cy(1.1,10,a,b,38);bezel=bezel.cut(hole);body_cuts.append(top_circle(f'BezelM2Hole{i}',a,b,1.1).Shape)
    screw(f'BezelScrew{i}',a,b,45,10,group='BEZEL_MOUNT');washer(f'BezelWasher{i}',a,b,38.5,group='BEZEL_MOUNT');nut(f'BezelNut{i}',a,b,36.9,group='BEZEL_MOUNT')
solid('DisplayBezel',bezel,'BEZEL','Raised3mm retainer; 1mm clear cover pocket atZ44; mounting below panel')
solid('DisplayClearCover',bx(41,34,1,x-20.5,y-17,44),'BEZEL','1mm polycarbonate; retain at edge, no hard glass preload',(.65,.84,.9))
for side in [-1,1]:
    solid('DisplayFoam'+('Upper' if side<0 else 'Lower'),bx(30,.5,1.75,active_center[0]-15,y+side*9.15-.25,42.25),'DISPLAY_RETAINER','Soft closed-cell foam strips outside active area; nominal compressed1.75mm (2mm free,12.5%); select actual foam/shims from measured glass-to-cover gap; no hard preload',(.25,.25,.28))
if objects['DisplayClearCover'].ViewObject:objects['DisplayClearCover'].ViewObject.Transparency=65
cuts['overlay'].append(dict(kind='display_active',center=list(active_center),width=32.35,height=17.4,corner_radius=3))
cuts['overlay'].append(dict(kind='bezel',center=[x,y],width=48,height=41))

# Rear controls, USB and original manufacturer RCA model.
x,y=C['RV7']['x'],C['RV7']['y'];axis=Z-10;g='MASTER'
solid('MasterBody',bx(12,10.1,16.7,x-6,y-7.5,Z-16.7),g,'Conservative PTV111-1 occupied envelope; includes rear stake projection2.6')
solid('MasterBushing',cy(4.5,5,x,y-7.5,axis,(0,-1,0)),g,'M9x0.75 axial5')
solid('MasterShaft',cy(3,20,x,y-7.5,axis,(0,-1,0)).cut(bx(8,20,2,x-4,y-27.5,axis+1.5)),g,'6mm shaft /4.5mm D-flat coupling per Bourns')
solid('MasterCollar',ring(6,4.6,.5,x,2,axis,(0,-1,0)),g,'Measured reaction collar nominal0.5')
solid('MasterWasher',ring(7,4.55,.5,x,0,axis,(0,-1,0)),g,'M9 washer OD14 ID9.1 thickness0.5')
solid('MasterNut',ring(11/math.sqrt(3),4.5,2,x,-.5,axis,(0,-1,0)),g,'Conservative M9 nut circumscribed envelope,11AF x2')
knob=cy(7.25,15.1,x,-9.8,axis,(0,-1,0)).cut(cy(3,8.2,x,-9.8,axis,(0,-1,0)))
solid('SameSkySAK023',knob,g,'Exact14mm shell plus1.1boss, maxdiameter14.5; nominal8.2mm insertion',(.13,.13,.15))
body_cuts.append(rear_circle('MasterHole',x,axis,4.65).Shape)
relief=bx(20,2,22,x-10,1.5,axis-8);body_cuts.append(relief)
cut_feature('MasterInsideRelief',relief,'rear','inside_pocket','Rear wall1.5mm locally;20x22 pocket')
x=45.;uz=Z-1.7
solid('USB4085',bx(8.94,9.17,3.4,x-4.47,1.39,Z-3.4),'USB','GCT RevB: body8.94 x9.17 x3.4, mating datum from verified footprint')
u=bx(12,6,6,x-6,-1,uz-3);body_cuts.append(u);cut_feature('USBOpening',u,'rear','through','12 x6 rectangular opening; connector centerZ29.05')
solid('USBPlugAllowance',bx(14,28,8,x-7,-28,uz-4),'RESERVE','Unselected plug overmould allowance14x8,28mm rear access; actual plug prototype check')
rca=Part.Shape();rca.read(str(R/'3dmodels/SameSky_RCJ-2123_aligned.step'))
rca=transform(rca,(-1,-1,-1),(C['J2']['x'],C['J2']['y'],Z))
solid('RCJ2123',rca,'RCA','Original Same Sky STEP, aligned to all footprint lead/mounting datums',(.2,.2,.22))
for i,rx in enumerate([85.5,99.5],1):
    body_cuts.append(rear_circle(f'RCAOpening{i}',rx,Z-8,5.6).Shape)
    solid(f'RCAPlugAllowance{i}',cy(6.5,30,rx,-1.3,Z-8,(0,-1,0)),'RESERVE','Unselected plug allowancediameter13;1mm between adjacent allowances')

# DAC, opposite face with two through bolts and two base locating supports.
x,y=C['U6']['x'],C['U6']['y'];dm=[(q['x'],q['y']) for q in P['U6']['pads'] if q['pad_type']=='np_thru_hole'];du=Z-8;db=du-1.6
board_module('DACPCB',30.48,20.32,1.6,x,y,db,dm,'DAC')
dac=bx(32.5,20.32,4.6,x-16.25,y-10.16,db-4.6)
for a,b in dm:dac=dac.cut(cy(2.7,5,a,b,db-4.7))
for q in P['U6']['pads']:
    if q['number']:dac=dac.cut(cy(.8,5,q['x'],q['y'],db-4.7))
solid('DACComponentBound',dac,'DAC','Conservative6250 whole-module6.2 minusPCB1.6 outward envelope; jack overhang included')
for i,(a,b) in enumerate(dm,1):
    if b<y:
        solid(f'DACSpacer{i}',ring(2,1.1,8,a,b,du),'DAC_MOUNT','Custom M2 spacer OD4 ID2.2 length8')
        washer(f'DACTopWasher{i}',a,b,T,group='DAC_MOUNT');screw(f'DACScrew{i}',a,b,T+.5,16,group='DAC_MOUNT')
        washer(f'DACBottomWasher{i}',a,b,db-.5,group='DAC_MOUNT');nut(f'DACNut{i}',a,b,db-2.1,group='DAC_MOUNT')
    else:
        base=base.fuse(cy(2,db-3,a,b,3)).fuse(cy(1,2,a,b,db))
        notes[f'DACBaseSupport{i}']='Two OD4 base pillars withdiameter2 locatingpegs support unused lower holes; no front screwhead'

# Long soldered header pins; standard, documented Samtec parts.
for ref,mpn,n,plane,direction,post,tail in [('U5','TSW-111-07-T-S',11,T,1,5.84,2.54),('U6','TSW-108-14-T-S',8,Z,-1,8.13,2.79)]:
    pins=[q for q in P[ref]['pads'] if q['number'] and int(q['number'])<=n]
    for i,q in enumerate(pins,1):
        a,b=q['x'],q['y'];lo=min(plane-tail*direction,plane+(2.54+post)*direction)
        solid(f'{ref}HeaderPin{i}',bx(.64,.64,tail+2.54+post,a-.32,b-.32,lo),ref+'_HEADER',mpn+';0.64square,2.54pitch, tin; solder both boards')
    a=min(q['x'] for q in pins)-1.27;b=pins[0]['y']-1.24
    solid(ref+'HeaderInsulator',bx(n*2.54,2.48,2.54,a,b,min(plane,plane+2.54*direction)),ref+'_HEADER',mpn+' 2.54mm insulator')

# ESP and large power/service parts. Small discretes use package occupied bounds.
c=C['U1'];solid('ESP32Module',bx(18,19.2,3.2,c['x']-9,c['y']-9.6,Z-3.2),'MCU','Espressif WROOM-1U v1.8 Fig10-2,18x19.2x3.2')
solid('AntennaAccess',bx(12,12,9.55,36,50.14,18),'RESERVE','U.FL center(42,56.14) from Fig10-2; mating/service allowance; exact cable MPN not selected')
solid('AntennaCableRoute',bx(8,64,6,38,62,18),'RESERVE','Pigtail routing corridor toward front wall; bend allowance>=5mm, validate selected cable')
for ref,w,d,h in [('U2',3,3,2),('U3',3.9,9.9,1.75),('U4',3,3,1.45),('U7',3,3,1.45),('F1',5.44,4.45,1.1),('D1',3.8,2.8,1.1),('C1',2,1.25,1.45),('C3',2,1.25,1.45)]:
    c=C[ref];solid(ref+'Package',bx(w,d,h,c['x']-w/2,c['y']-d/2,Z-h),'ELECTRONICS','Manufacturer/package maximum occupied-envelope allocation; see electrical source sheets')
for ref in ['SW5','SW6']:
    c=C[ref];solid(ref+'ServiceSwitch',bx(3.5,6,4.3,c['x']-1.75,c['y']-3,Z-4.3),'SERVICE', 'C&K PTS636SM43SMTR,6x3.5x4.3, rotated90')
    hole=cy(2,4,c['x'],c['y'],-.5);base_cuts.append(hole);cut_feature(ref+'AccessHole',hole,'base','through','diameter4 service probe access')
    solid(ref+'ToolAccess',cy(1.5,Z-7.3,c['x'],c['y'],3),'RESERVE','3mm insulated probe allowance from removable base to button')

# Assembly clearance allocations, not invented component CAD dimensions.
for ref,ps in P.items():
    joints=[]
    for q in ps['pads']:
        if q['pad_type']!='thru_hole' or not q['number']:continue
        if ref=='U6' and int(q['number'])>8:continue # unpopulated configuration header
        # Bound hand-solder tails to <=1.5mm beyond the opposite board face.
        zz=Z-1.5 if C[ref]['layer']=='F.Cu' else T
        joints.append(cy(max(q['size'].values())/2,1.5,q['x'],q['y'],zz))
    if joints:solid(ref+'SolderAllowance',union(joints),'RESERVE','Assembly allocation: trim exposed leads and solder height <=1.5mm beyond board, radial envelope bounded by copper pad')
for ref in ['SW1','SW2','SW3','SW4']:
    posts=[]
    for q in P[ref]['pads']:
        d=q.get('drill',{}).get('size',{}).get('x',0)
        if d:posts.append(cy(d/2,1.7,q['x'],q['y'],Z-1.7))
    solid(ref+'LowerFeatureAllowance',union(posts),'RESERVE','CHERRY nominal3.30mm belowbody minus1.60PCB =1.70mm belowboard; verified five-hole XY envelopes, conservative clearance only; do not trim plastic')

# Removable top permits axial engagement of rear connectors with the front
# shafts unobstructed, followed by vertical lowering of the top cover.
for i,(a,b) in enumerate(closure,1):
    body_cuts.append(top_circle(f'TopCoverM3Hole{i}',a,b,1.6,z=39,h=5).Shape)
    washer(f'TopCoverWasher{i}',a,b,42,d=3,outer=7,group='TOP_MOUNT')
    screw(f'TopCoverScrew{i}',a,b,42.5,8,d=3,head_d=5.5,head_h=3,group='TOP_MOUNT')
for i,a in enumerate([5,120],1):
    body_cuts.append(rear_circle(f'RearPanelM2Hole{i}',a,22,1.1).Shape)
    solid(f'RearPanelScrew{i}',cy(1,8,a,0,22,(0,1,0)).fuse(cy(1.9,2,a,-2,22,(0,1,0))),'REAR_MOUNT','M2x8 screw, rear facing, diameter3.8 x2 head; body boss tapped M2')

for s in body_cuts:body=body.cut(s)
for s in base_cuts:base=base.cut(s)
# Native editable CSG enclosure. Machining features are the very same objects
# used by the projection exporter, so dimensions are never transcribed twice.
construction=[]
def native_box(name,w,d,h,x=0,y=0,z=0,expressions=None):
    o=doc.addObject('Part::Box',name);o.Length=w;o.Width=d;o.Height=h;o.Placement.Base=V(x,y,z)
    for key,val in (expressions or {}).items():o.setExpression(key,val)
    construction.append(o);return o
def native_cylinder(name,r,h,x,y,z,expressions=None):
    o=doc.addObject('Part::Cylinder',name);o.Radius=r;o.Height=h;o.Placement.Base=V(x,y,z)
    for key,val in (expressions or {}).items():o.setExpression(key,val)
    construction.append(o);return o
def native_cut(name,a,b):
    o=doc.addObject('Part::Cut',name);o.Base=a;o.Tool=b;construction.append(o);return o
def native_fuse(name,items):
    o=doc.addObject('Part::MultiFuse',name);o.Shapes=items;construction.append(o);return o
outer=native_box('HousingOuter',125,140,39,z=3,expressions={'Length':'Dimensions.Width','Width':'Dimensions.Depth','Height':'Dimensions.Height - Dimensions.BaseThickness','Placement.Base.z':'Dimensions.BaseThickness'})
inner=native_box('HousingCavity',119,134,36,3,3,3,{'Length':'Dimensions.Width - 2 * Dimensions.Wall','Width':'Dimensions.Depth - 2 * Dimensions.Wall','Height':'Dimensions.Height - Dimensions.BaseThickness - Dimensions.TopThickness','Placement.Base.x':'Dimensions.Wall','Placement.Base.y':'Dimensions.Wall','Placement.Base.z':'Dimensions.BaseThickness'})
shell=native_cut('HollowHousing',outer,inner);body_parts=[shell]
for i,(a,b) in enumerate(closure,1):
    od=native_cylinder(f'ClosureBoss{i}',3.5,10,a,b,3)
    pilot=native_cylinder(f'ClosurePilot{i}',1.25,10,a,b,3)
    body_parts.append(native_cut(f'TappedClosureBoss{i}',od,pilot))
roughbody=native_fuse('HousingWithBosses',body_parts)
bodytool=doc.addObject('Part::Compound','HousingMachiningTools');bodytool.Links=[objects[row['name']] for panel in ['top','rear'] for row in cuts[panel]];construction.append(bodytool)
machined=native_cut('MachinedHousing',roughbody,bodytool)
top_slab=native_box('TopSplitSlab',125,140,3,z=39,expressions={'Length':'Dimensions.Width','Width':'Dimensions.Depth','Height':'Dimensions.TopThickness','Placement.Base.z':'Dimensions.Height - Dimensions.TopThickness'})
topobj=doc.addObject('Part::Common','EnclosureTop');topobj.Base=machined;topobj.Tool=top_slab
frame=native_cut('OpenHousingFrame',machined,top_slab)
rear_slab=native_box('RearPanelSplit',125,1.5,36,z=3,expressions={'Length':'Dimensions.Width','Width':'Dimensions.RearPanelThickness','Height':'Dimensions.Height - Dimensions.TopThickness - Dimensions.BaseThickness','Placement.Base.z':'Dimensions.BaseThickness'})
rearobj=doc.addObject('Part::Common','EnclosureRearPanel');rearobj.Base=frame;rearobj.Tool=rear_slab
frame=native_cut('FrameRearPocket',frame,rear_slab)
rear_window=native_box('RearAssemblyOpening',109,1.5,36,8,1.5,3,{'Length':'Dimensions.Width - 16 mm','Width':'Dimensions.Wall - Dimensions.RearPanelThickness','Height':'Dimensions.Height - Dimensions.TopThickness - Dimensions.BaseThickness','Placement.Base.y':'Dimensions.RearPanelThickness','Placement.Base.z':'Dimensions.BaseThickness'})
frame=native_cut('FrameWithRearOpening',frame,rear_window);upper_parts=[frame]
for i,a in enumerate([5,120],1):
    boss=native_box(f'RearFasteningBoss{i}',8,6.5,8,0 if i==1 else 117,1.5,18)
    pilot=native_cylinder(f'RearFasteningPilot{i}',.8,6.5,a,1.5,22)
    pilot.Placement.Rotation=A.Rotation(V(0,0,1),V(0,1,0))
    upper_parts.append(native_cut(f'TappedRearBoss{i}',boss,pilot))
for i,(a,b) in enumerate(closure,1):
    od=native_cylinder(f'TopBoss{i}',3.5,5,a,b,34,{'Placement.Base.z':'Dimensions.Height - Dimensions.TopThickness - 5 mm'})
    pilot=native_cylinder(f'TopPilot{i}',1.25,5,a,b,34,{'Placement.Base.z':'Dimensions.Height - Dimensions.TopThickness - 5 mm'})
    upper_parts.append(native_cut(f'TappedTopBoss{i}',od,pilot))
bodyobj=native_fuse('HousingWithUpperBosses',upper_parts)
# Open-rear insertion at the final PCB elevation avoids moving the frame's
# bosses vertically through the board. Reliefs extend the entry width to114mm.
for i,a in enumerate([5.5,117],1):
    entry=native_box(f'PCBEntryRelief{i}',2.5,1.5,2.5,a,1.5,30.5)
    previous=bodyobj;previous.Label='Frame before PCB entry relief '+str(i)
    bodyobj=native_cut('EnclosureBody' if i==2 else f'EntryRelievedFrame{i}',previous,entry)
bodyobj.Label='EnclosureBody'
base_parts=[native_box('BasePlate',125,140,3,expressions={'Length':'Dimensions.Width','Width':'Dimensions.Depth','Height':'Dimensions.BaseThickness'})]
for i,(a,b) in enumerate(mounts,1):
    od=native_cylinder(f'BoardBoss{i}',4,Z-4,a,b,3,{'Height':'Dimensions.BoardBottom - Dimensions.BaseThickness - 1 mm','Placement.Base.z':'Dimensions.BaseThickness'})
    pilot=native_cylinder(f'BoardPilot{i}',1.25,Z-4,a,b,3,{'Height':'Dimensions.BoardBottom - Dimensions.BaseThickness - 1 mm','Placement.Base.z':'Dimensions.BaseThickness'})
    base_parts.append(native_cut(f'TappedBoardBoss{i}',od,pilot))
for i,(a,b) in enumerate([(q['x'],q['y']) for q in P['U6']['pads'] if q['pad_type']=='np_thru_hole' and q['y']>C['U6']['y']],1):
    base_parts.append(native_cylinder(f'DACBasePillar{i}',2,db-3,a,b,3,{'Height':'Dimensions.BoardBottom - Dimensions.DACSpacing - 1.6 mm - Dimensions.BaseThickness','Placement.Base.z':'Dimensions.BaseThickness'}))
    base_parts.append(native_cylinder(f'DACLocatingPeg{i}',1,2,a,b,db,{'Placement.Base.z':'Dimensions.BoardBottom - Dimensions.DACSpacing - 1.6 mm'}))
roughbase=native_fuse('BaseWithSupports',base_parts)
basetool=doc.addObject('Part::Compound','BaseMachiningTools');basetool.Links=[objects[row['name']] for row in cuts['base']];construction.append(basetool)
baseobj=native_cut('EnclosureBase',roughbase,basetool)
for o,group,evidence,color in [(bodyobj,'ENCLOSURE','Parametric open-top/rear housing frame; PCB slides through114mm-wide relief atZ30.5..33 before base installation',(.7,.71,.73)),(baseobj,'BASE','Parametric3mm base with four adjustable motherboard bosses and two DAC supports',(.55,.56,.58)),(topobj,'TOP','Removable3mm top permits separate axial engagement of rear connectors and control shafts',(.7,.71,.73)),(rearobj,'REAR_PANEL','Removable125x36x1.5mm aluminium rear panel; axial installation over MASTER shaft and connectors',(.62,.63,.65))]:
    if o in construction:construction.remove(o)
    key='EnclosureBody' if group=='ENCLOSURE' else o.Name
    objects[key]=o;groups[key]=group;notes[key]=evidence
    o.addProperty('App::PropertyString','Evidence');o.Evidence=evidence;o.addProperty('App::PropertyString','AssemblyGroup');o.AssemblyGroup=group
    if o.ViewObject:o.ViewObject.ShapeColor=color
doc.recompute()
expected_frame=body.cut(bx(125,140,3,0,0,39))
expected_rear=expected_frame.common(bx(125,1.5,36,0,0,3))
expected_frame=expected_frame.cut(bx(125,1.5,36,0,0,3)).cut(bx(109,1.5,36,8,1.5,3))
for i,a in enumerate([5,120],1):expected_frame=expected_frame.fuse(bx(8,6.5,8,0 if i==1 else 117,1.5,18).cut(cy(.8,6.5,a,1.5,22,(0,1,0))))
for a,b in closure:expected_frame=expected_frame.fuse(ring(3.5,1.25,5,a,b,34))
for a in [5.5,117]:expected_frame=expected_frame.cut(bx(2.5,1.5,2.5,a,1.5,30.5))
assert abs(bodyobj.Shape.Volume-expected_frame.Volume)<1e-4,(bodyobj.Shape.Volume,expected_frame.Volume)
assert abs(topobj.Shape.Volume-body.common(bx(125,140,3,0,0,39)).Volume)<1e-4
assert abs(rearobj.Shape.Volume-expected_rear.Volume)<1e-4
assert abs(baseobj.Shape.Volume-base.Volume)<1e-4,(baseobj.Shape.Volume,base.Volume)
invalid=[n for n,o in objects.items() if not o.Shape.isValid()]
assert not invalid,invalid
for o in tools:
    if o.ViewObject:o.ViewObject.Visibility=False
for o in construction:
    o.addProperty('App::PropertyString','AssemblyGroup');o.AssemblyGroup='CAD_CONSTRUCTION'
    if o.ViewObject:o.ViewObject.Visibility=False
for n,o in objects.items():
    if groups[n]=='RESERVE' and o.ViewObject:o.ViewObject.Visibility=False
doc.saveAs(str(O/'assembly-v2.FCStd'))
fitted=[o for n,o in objects.items() if groups[n] not in ['CUT_TOOL','RESERVE']]
Part.export(fitted,str(O/'assembly-v2.step'))
Part.export([objects['EnclosureBody']],str(O/'enclosure-body-v2.step'))
Part.export([objects['EnclosureBase']],str(O/'enclosure-base-v2.step'))
Part.export([objects['EnclosureTop']],str(O/'enclosure-top-v2.step'))
Part.export([objects['EnclosureRearPanel']],str(O/'enclosure-rear-panel-v2.step'))
Part.export([objects['DisplayBezel']],str(O/'panel-bezel-v2.step'))
# Exact machining tool projections are extracted from saved CAD object shapes.
for panel,rows in cuts.items():
    if panel=='overlay':continue
    for row in rows:
        s=objects[row['name']].Shape
        row['cad_volume_mm3']=s.Volume
        row['edges']=[dict(points=[[v.x,v.y,v.z] for v in e.discretize(Deflection=.02)]) for e in s.Edges]
        row['analytic_edges']=[]
        for edge in s.Edges:
            curve=edge.Curve
            if isinstance(curve,Part.Circle):
                row['analytic_edges'].append(dict(kind='circle',center=list(curve.Center),axis=list(curve.Axis),radius=curve.Radius))
            else:row['analytic_edges'].append(dict(kind='line',points=[list(edge.Vertexes[0].Point),list(edge.Vertexes[-1].Point)]))
(R/'reports/assembly-cad-geometry.json').write_text(json.dumps(dict(parameters=values,cuts=cuts,objects={n:dict(group=groups[n],evidence=notes[n],bounds=[getattr(o.Shape.BoundBox,a) for a in ['XMin','YMin','ZMin','XMax','YMax','ZMax']],volume=o.Shape.Volume) for n,o in objects.items()}),indent=2))
print(json.dumps(dict(objects=len(objects),valid_shapes=len(objects),step=str(O/'assembly-v2.step'),status='Built; numerical collision review required'),indent=2))
