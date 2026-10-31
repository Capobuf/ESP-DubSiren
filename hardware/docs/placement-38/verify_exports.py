"""Read only analysis of Konnect pad/courtyard and native GenCAD exports."""
import json,math,csv,itertools,collections,hashlib
from pathlib import Path
folder=Path('hardware/docs/placement-38')
read=lambda n:json.loads((folder/n).read_text())
before,after=read('before-pads.json'),read('after-pads.json')
bc={c['reference']:c for c in read('before-components.json')['components']}
ac={c['reference']:c for c in read('after-components.json')['components']}
dist=lambda a,b:math.hypot(a['x']-b['x'],a['y']-b['y'])
pad=lambda p,r,n:next(v for v in p[r]['pads'] if v['number']==str(n))
moved=[r for r in bc if any(abs(bc[r][k]-ac[r][k])>1e-5 for k in ['x','y','rotation'])]
assert len(moved)==20
assert all(bc[r]==ac[r] for r in bc if r not in moved)
assert all(bc[r]['layer']==ac[r]['layer'] for r in bc)
unchanged_geometry=[];position_errors=[]
for r in before:
    bp={p['uuid']:p for p in before[r]['pads']};ap={p['uuid']:p for p in after[r]['pads']}
    assert bp.keys()==ap.keys()
    angle=math.radians(ac[r]['rotation']-bc[r]['rotation'])
    for uid,b in bp.items():
        a=ap[uid]
        for key in ['number','net','pad_type','shape','size','drill','layers','copper_layers']:
            assert b[key]==a[key],(r,key,b[key],a[key])
        rotation_error=(a['rotation_deg']-b['rotation_deg']-(ac[r]['rotation']-bc[r]['rotation']))%360
        assert min(rotation_error,360-rotation_error)<1e-5,(r,'rotation')
        dx,dy=b['x']-bc[r]['x'],b['y']-bc[r]['y']
        expect=(ac[r]['x']+dx*math.cos(angle)+dy*math.sin(angle),ac[r]['y']-dx*math.sin(angle)+dy*math.cos(angle))
        if math.hypot(a['x']-expect[0],a['y']-expect[1])>2e-6:position_errors.append((r,uid))
assert not position_errors
def section(name,stage):
    text=(folder/f'{stage}.cad').read_text();return text.split('$'+name+'\n')[1].split('$END'+name)[0].strip()
same={name:section(name,'before')==section(name,'after') for name in ['BOARD','PADS','PADSTACKS','SIGNALS','TRACKS','ROUTES']}
assert all(same[k] for k in ['BOARD','SIGNALS','TRACKS','ROUTES']),same
assert section('TRACKS','after')==section('ROUTES','after')==''
rows=read('after-courtyards.json');boxes={}
for row in rows:
    d=row['result'];assert d.get('available') and d.get('applicable'),row
    boxes[row['target']]=d['geometry1']['bbox'];boxes[row['other']]=d['geometry2']['bbox']
def gap(a,b):
    return math.hypot(max(a['min_x']-b['max_x'],b['min_x']-a['max_x'],0),max(a['min_y']-b['max_y'],b['min_y']-a['max_y'],0))
def overlap(a,b):
    return a['min_x']<b['max_x'] and b['min_x']<a['max_x'] and a['min_y']<b['max_y'] and b['min_y']<a['max_y']
pairs=[(a,b) for a,b in itertools.combinations(boxes,2) if overlap(boxes[a],boxes[b])]
# Pad hulls are conservative for round pads; all authored front through pads
# are checked against the moved back-side courtyard hulls.
cross=[];nearest=[]
for r in moved:
    for f in ac:
        if ac[f]['layer']!='TOP':continue
        for p in after[f]['pads']:
            if p['pad_type'] not in ['thru_hole','np_thru_hole']:continue
            angle=math.radians(p['rotation_deg']);sx,sy=p['size']['x']/2,p['size']['y']/2
            ex,ey=(sx,sx) if p['shape']=='circle' else (abs(sx*math.cos(angle))+abs(sy*math.sin(angle)),abs(sx*math.sin(angle))+abs(sy*math.cos(angle)))
            hull={'min_x':p['x']-ex,'max_x':p['x']+ex,'min_y':p['y']-ey,'max_y':p['y']+ey}
            if overlap(boxes[r],hull):cross.append([r,f,p['number'],p['uuid']])
            nearest.append((gap(boxes[r],hull),r,f,p['number']))
metrics=[]
def metric(name,r1,n1,r2,n2):
    metrics.append({'path':name,'endpoints':f'{r1}.{n1} -> {r2}.{n2}','before_mm':round(dist(pad(before,r1,n1),pad(before,r2,n2)),3),'after_mm':round(dist(pad(after,r1,n1),pad(after,r2,n2)),3)})
for name,r1,n1,r2,n2 in [('USB DM U4-MCU','U4',6,'U1',13),('USB DP U4-MCU','U4',4,'U1',14),('USB DM U4-series','U4',6,'R2',1),('USB DP U4-series','U4',4,'R3',1),('USB DM series-MCU','R2',2,'U1',13),('USB DP series-MCU','R3',2,'U1',14),('I2S BCLK','U1',9,'U6',5),('I2S LRCK','U1',10,'U6',3),('I2S DIN','U1',11,'U6',4),('OUTL AUDIO_MONO','U6',6,'J2','T'),('ADC MUX','U3',3,'U1',4),('ADC cap-MCU','C9',1,'U1',4),('VDD 100n U1','C6',1,'U1',2),('VDD bulk U1','C5',1,'U1',2),('VDD MUX','C8',1,'U3',16),('5V source-DAC','U2',2,'U6',1)]:
    metric(name,r1,n1,r2,n2)
metric('ADC output-testpoint','U3',3,'TP4',1)
def signal_mst(p):
    nets=collections.defaultdict(list);lengths={}
    for v in p.values():
        for q in v['pads']:
            n=q['net']
            if not n or n in ['GND','+5V','+3V3'] or n.startswith('unconnected-'):continue
            nets[n].append(q)
    for n,points in nets.items():
        used=points[:1];pending=points[1:];length=0
        while pending:
            d,j=min((dist(a,b),j) for a in used for j,b in enumerate(pending))
            length+=d;used.append(pending.pop(j))
        lengths[n]=length
    return lengths
bm,am=signal_mst(before),signal_mst(after)
mst={'method':'Euclidean MST per net, excludes GND, +5V, +3V3 and unconnected nets; lower bound, not routing prediction','before_mm':sum(bm.values()),'after_mm':sum(am.values()),'per_net':[{'net':n,'before_mm':bm[n],'after_mm':am[n],'delta_mm':am[n]-bm[n]} for n in sorted(bm)]}
(folder/'signal-mst-comparison.json').write_text(json.dumps(mst,indent=2))
holes5=[p for p in after['U5']['pads'] if p['pad_type']=='np_thru_hole']
holes6=[p for p in after['U6']['pads'] if p['pad_type']=='np_thru_hole']
holes_gap=min(dist(a,b) for a in holes5 for b in holes6)
original_candidate=[dict(p,x=p['x'],y=p['y']-2.5) for p in holes6]
old_gap=min(dist(a,b) for a in holes5 for b in original_candidate)
m3=[]
for r,v in after.items():
    if r.startswith('H'):continue
    for p in v['pads']:
        for h in ['H1','H2','H3','H4']:
            c=ac[h];a=math.radians(p['rotation_deg']);sx,sy=p['size']['x']/2,p['size']['y']/2
            ex,ey=(sx,sx) if p['shape']=='circle' else (abs(sx*math.cos(a))+abs(sy*math.sin(a)),abs(sx*math.sin(a))+abs(sy*math.cos(a)))
            if abs(p['x']-c['x'])<4+ex and abs(p['y']-c['y'])<4+ey:m3.append([r,p['number'],h])
report={'source':'Konnect saved_file pad/courtyard queries and native KiCad GenCAD/DRC exports','footprints':len(ac),'numbered_pads':sum(bool(p['number']) for v in after.values() for p in v['pads']),'moved':moved,'pad_geometry_net_identity_preserved':True,'native_gencad_sections_identical':same,'track_routes_empty':True,'same_side_courtyard_overlaps':pairs,'front_through_pad_to_moved_back_courtyard_overlaps':cross,'nearest_front_pad_hull_to_moved_back_courtyard_mm':sorted(nearest)[:10],'m3_intrusions':m3,'u5_u6_hole_center_min_mm':holes_gap,'u5_u6_candidate_44_hole_center_min_mm':old_gap,'equal_4mm_hardware_nominal_edge_gap_mm':holes_gap-4,'metrics_straight_lines_not_routed_lengths':metrics,'courtyard_bboxes':boxes,'drc':{s:dict(collections.Counter(v['severity']+':'+v['rule'] for v in read(s+'-drc.json')['violations'])) for s in ['before','after']},'pcb_sha256':hashlib.sha256(Path('hardware/kicad/ESP-DubSiren.kicad_pcb').read_bytes()).hexdigest()}
report['unique_numbered_reference_pad_pairs']=len({(r,v['number']) for r,k in after.items() for v in k['pads'] if v['number']})
report['signal_mst_comparison']={k:v for k,v in mst.items() if k!='per_net'}
(folder/'verification.json').write_text(json.dumps(report,indent=2))
with (folder/'positions.csv').open('w',newline='') as f:
    w=csv.writer(f);w.writerow(['Reference','BeforeX_mm','BeforeY_mm','BeforeRotation_deg','X_mm','Y_mm','Rotation_deg','Side','Moved'])
    for r in sorted(ac): w.writerow([r,*[round(bc[r][k],6) for k in ['x','y','rotation']],*[round(ac[r][k],6) for k in ['x','y','rotation']],ac[r]['layer'],r in moved])
print(json.dumps({k:v for k,v in report.items() if k!='courtyard_bboxes'},indent=2))
