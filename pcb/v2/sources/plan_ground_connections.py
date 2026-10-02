"""Plan ground connections from saved readbacks, including filled zone polygons.

No KiCad source writes. Geometry is checked again by native DRC after MCP apply.
"""
import json,math
from shapely.geometry import Point,LineString,Polygon
from shapely.ops import unary_union
import plan_remaining_routes as p
D=p.D
for v in json.loads((D/'remaining-route-plan.json').read_text()):
    if v['kind']=='via':p.addvia(v)
drills=[]
for d in p.P.values():
    for pad in d['pads']:
        q=pad.get('drill')
        if q and q.get('size',{}).get('x',0)>0:
            drills.append(Point(pad['x'],pad['y']).buffer(max(q['size'].values())/2))
for f in ['usb-route-plan.json','power-route-plan.json','power-route-correction-plan.json','remaining-route-plan.json']:
    for v in json.loads((D/f).read_text()):
        if v['kind']=='via':drills.append(Point(v['x'],v['y']).buffer(v['drill']/2))
filled={0:[],1:[]}
for z in json.loads((D/'pcb-ground-filled-polygons.json').read_text()):
    filled[0 if z['layer']=='F.Cu' else 1]+=[Polygon(v['outer'],v['holes']).buffer(0) for v in z['polygons']]
planes={l:unary_union(g) for l,g in filled.items()}
obstacles={l:unary_union([o['g'] for o in p.objs if o['net']!='GND' and l in o['layers']]) for l in [0,1]}
holes=unary_union(p.holes+p.npths)
drillareas=unary_union(drills)
plan=[];newvias=[]
def safevia(x,y):
    q=Point(x,y)
    return (q.distance(obstacles[0])>=.3+.185 and q.distance(obstacles[1])>=.3+.185
            and q.distance(holes)>=.3+.5 and q.distance(drillareas)>=.15+.285
            and all(q.distance(a)>=.6 for a in newvias))
def via(x,y):
    newvias.append(Point(x,y));plan.append(dict(kind='via',net_name='GND',x=x,y=y,pad_size=.6,drill=.3))
def trace(a,b,l,width=.3):
    plan.append(dict(kind='trace',net_name='GND',layer=['F.Cu','B.Cu'][l],width=width,x1=a[0],y1=a[1],x2=b[0],y2=b[1]))
# Direct ground connections at critical SMD pads and the thermally starved headers.
targets=[('C6','2'),('C8','2'),('C5','2'),('C7','2'),('C9','2'),('U1','1'),('U1','40'),('U1','41'),('U3','4'),('U3','6'),('U3','8'),('U4','2'),('U7','2'),('U5','3'),('J1','SH'),('RV1','1'),('RV2','1')]
results=[]
for ref,num in targets:
    ps=[q for q in p.P[ref]['pads'] if q['number']==num]
    for pad in ps:
        a=(pad['x'],pad['y']);found=None
        for r in [1.,1.2,1.5,1.8,2.,2.5,3.]:
            for dx,dy in [(-1,0),(1,0),(0,-1),(0,1)]:
                x,y=round(a[0]+dx*r,5),round(a[1]+dy*r,5)
                line=LineString([a,(x,y)]).buffer(.15)
                if (safevia(x,y) and planes[0].contains(Point(x,y).buffer(.32))
                    and line.distance(obstacles[1])>=.185 and line.distance(holes)>=.5):
                    found=(x,y);break
            if found:break
        if found:
            via(*found);trace(a,found,1);results.append(dict(reference=ref,pad=num,via=found))
        else:results.append(dict(reference=ref,pad=num,status='no_safe_cardinal_connection'))
# Stitch each retained filled region, then a sparse 15 mm grid for return paths.
for l,gs in filled.items():
    for g in gs:
        region=g.intersection(planes[1-l]).buffer(-.5)
        if region.is_empty:continue
        q=region.representative_point();candidates=[(q.x,q.y)]
        x1,y1,x2,y2=region.bounds
        candidates += [(x1+ix*.7,y1+iy*.7) for ix in range(int((x2-x1)/.7)+1) for iy in range(int((y2-y1)/.7)+1)]
        for x,y in candidates:
            x,y=round(x,4),round(y,4)
            if region.contains(Point(x,y)) and safevia(x,y):via(x,y);break
for y in range(10,131,15):
    for x in range(10,116,15):
        if all(planes[l].contains(Point(x,y).buffer(.4)) for l in [0,1]) and safevia(x,y):via(x,y)
(D/'ground-connections-plan.json').write_text(json.dumps(plan,indent=2))
(D/'ground-connections-plan-summary.json').write_text(json.dumps(results,indent=2))
print(json.dumps(dict(operations=len(plan),vias=len(newvias),pad_connections=results),indent=2))
