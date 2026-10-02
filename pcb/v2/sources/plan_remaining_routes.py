"""Plan remaining signal routes from MCP readbacks; never writes KiCad files.
Octilinear A* with explicit copper, drill, routed-slot and USB-reference obstacles.
Review the JSON plan, then apply through Konnect and run independent KiCad DRC.
"""
import json,math,heapq,time,argparse
from pathlib import Path
import numpy as np
from shapely.geometry import Point,LineString,box
from shapely.affinity import rotate,translate,scale
from shapely import contains_xy
R=Path(__file__).resolve().parents[1];D=R/'reports'
STEP=.1;X0=6.;Y0=3.5;NX=1131;NY=1306;CLEAR=.15;MARGIN=.035
objs=[];holes=[];npths=[];plan=[];reserved=[]
def obj(net,geom,layers,points,tag):objs.append(dict(net=net,g=geom,layers=layers,pts=points,tag=tag))
def copper(p):
 w=p['size']['x'];h=p['size']['y'];typ=p['shape']
 if typ=='circle':g=Point(0,0).buffer(w/2,quad_segs=16)
 elif typ=='oval':
  q=min(w,h)/2;g=LineString([(-w/2+q,0),(w/2-q,0)]).buffer(q) if w>=h else LineString([(0,-h/2+q),(0,h/2-q)]).buffer(q)
 else:g=box(-w/2,-h/2,w/2,h/2)
 return translate(rotate(g,-p['rotation_deg'],origin=(0,0)),p['x'],p['y'])
P=json.loads((D/'pcb-placement-pads.json').read_text())
C={p['reference']:p for p in json.loads((D/'pcb-placement-components.json').read_text())['components']}
for ref,d in P.items():
 for p in d['pads']:
  if p['pad_type']=='np_thru_hole':npths.append(copper(p));continue
  if p['pad_type']=='smd' and p['number']=='':continue # native paste-only thermal apertures, no electrical copper
  layers=[i for i,l in enumerate(['F.Cu','B.Cu']) if l in p['layers']]
  if not layers:continue
  obj(p['net'],copper(p),layers,[(p['x'],p['y'],l) for l in layers],ref+':'+p['number'])
for ref,d in json.loads((D/'pcb-mechanical-cutouts.json').read_text()).items():
 c=C[ref]
 for side in [-1,1]:
  pts=[p for g in d['graphics'] for p in g['points'] if p['x']*side>0]
  if not pts:continue
  g=box(min(p['x'] for p in pts),min(p['y'] for p in pts),max(p['x'] for p in pts),max(p['y'] for p in pts))
  if c['layer']=='B.Cu':g=scale(g,xfact=1,yfact=-1,origin=(0,0))
  holes.append(translate(rotate(g,-c['rotation'],origin=(0,0)),c['x'],c['y']))
# MPM3612 exposed NC metal; no copper or vias in this rectangle on either layer.
holes.append(box(17.425,31.1,18.575,31.5))
def addtrack(t):
 l=0 if t['layer']=='F.Cu' else 1;a=(t['x1'],t['y1']);b=(t['x2'],t['y2'])
 obj(t.get('net',t.get('net_name')),LineString([a,b]).buffer(t['width']/2),[l],[(*a,l),(*b,l)],'track')
def addvia(v):obj(v['net_name'],Point(v['x'],v['y']).buffer(v['pad_size']/2),[0,1],[(v['x'],v['y'],0),(v['x'],v['y'],1)],'via')
for t in json.loads((D/'pcb-routed-traces.json').read_text())['traces']:addtrack(t)
for name in ['usb-route-plan.json','power-route-plan.json','power-route-correction-plan.json']:
 for v in json.loads((D/name).read_text()):
  if v['kind']=='via':addvia(v)
# Reserve continuous F.Cu ground below the main B.Cu pair. No unrelated crossing.
usbref=[]
for t in json.loads((D/'usb-route-plan.json').read_text()):
 if t['kind']=='trace' and t['width']==.6:usbref.append(LineString([(t['x1'],t['y1']),(t['x2'],t['y2'])]).buffer(1.1))
# Reserve escape paths around used MCU pads before long interconnects are planned.
# These are planning obstacles only; they never become dangling copper stubs.
for p in P['U1']['pads']:
 if p['number'] in ['9','10','11','12','7','5','6']:q=(25.8,p['y'])
 elif p['number'] in ['17','18','19','20','22']:q=(p['x'],38.4)
 elif p['number'] in [str(i) for i in range(31,40)]:q=(45.7,p['y'])
 else:continue
 reserved.append((p['net'],LineString([(p['x'],p['y']),q]).buffer(.1),[1]))
 reserved.append((p['net'],Point(*q).buffer(.3),[0,1]))
for p in P['U3']['pads']:
 if p['net']=='GND' or p['net']=='+3V3':continue
 q=(21.8 if p['x']<26 else 30.2,p['y'])
 reserved.append((p['net'],LineString([(p['x'],p['y']),q]).buffer(.1),[1]))
 reserved.append((p['net'],Point(*q).buffer(.3),[0,1]))
# Explicit, parallel encoder corridors avoid long detours and repeated via jumps.
encoder_paths={
 '/ENC_SW':[(54.5,53.3),(95,53.3),(95,53)],
 '/ENC_A':[(54.5,54.2),(86.5,54.2),(86.5,60),(89.5,63),(91.5,65),(95,65),(95,67.5)],
 '/ENC_B':[(54.5,51.5),(107,51.5),(107,62),(101.5,67.5),(100,67.5)]}
for n,pts in encoder_paths.items():
 for a,b in zip(pts,pts[1:]):
  t=dict(kind='trace',net_name=n,layer='B.Cu',width=.2,x1=a[0],y1=a[1],x2=b[0],y2=b[1]);plan.append(t);addtrack(t)
def raster(mask,g):
 x1,y1,x2,y2=g.bounds;ix1=max(0,int(math.floor((x1-X0)/STEP)));ix2=min(NX,int(math.ceil((x2-X0)/STEP))+1);iy1=max(0,int(math.floor((y1-Y0)/STEP)));iy2=min(NY,int(math.ceil((y2-Y0)/STEP))+1)
 if ix2<=ix1 or iy2<=iy1:return
 xx=X0+np.arange(ix1,ix2)[None,:]*STEP;yy=Y0+np.arange(iy1,iy2)[:,None]*STEP
 mask[iy1:iy2,ix1:ix2]|=contains_xy(g,xx,yy)
def masks(net,width):
 a=np.zeros((2,NY,NX),dtype=bool);v=np.zeros_like(a)
 for o in objs:
  if o['net']==net:
   if o['tag'] not in ['track','via'] and len(o['layers'])==1:raster(v[o['layers'][0]],o['g'].buffer(.175))
   continue
  g=o['g'].buffer(width/2+CLEAR+MARGIN);gv=o['g'].buffer(.35+CLEAR+MARGIN)
  for l in o['layers']:raster(a[l],g);raster(v[l],gv)
 for net2,g,ls in reserved:
  if net2!=net:
   for l in ls:raster(a[l],g.buffer(width/2+CLEAR+MARGIN));raster(v[l],g.buffer(.35+CLEAR+MARGIN))
 for h in npths:
  g=h.buffer(width/2+.3+MARGIN);gv=h.buffer(.35+.3+MARGIN)
  for l in [0,1]:raster(a[l],g);raster(v[l],gv)
 for h in holes:
  g=h.buffer(width/2+.5+MARGIN);gv=h.buffer(.35+.5+MARGIN)
  for l in [0,1]:raster(a[l],g);raster(v[l],gv)
 for h in usbref:
  raster(a[0],h.buffer(width/2));raster(v[0],h.buffer(.35))
 ed=int(math.ceil((.5+width/2)/STEP));ev=int(math.ceil(.85/STEP))
 a[:,:ed,:]=True;a[:,-ed:,:]=True;a[:,:,:ed]=True;a[:,:,-ed:]=True
 v[:,:ev,:]=True;v[:,-ev:,:]=True;v[:,:,:ev]=True;v[:,:,-ev:]=True
 return a,v[0]|v[1]
def components(net):
 os=[o for o in objs if o['net']==net];parent=list(range(len(os)))
 def find(i):
  while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
  return i
 for i in range(len(os)):
  for k in range(i):
   if set(os[i]['layers'])&set(os[k]['layers']) and os[i]['g'].distance(os[k]['g'])<.00001:parent[find(i)]=find(k)
 groups={}
 for i,o in enumerate(os):groups.setdefault(find(i),[]).extend(o['pts'])
 return [list(set(x)) for x in groups.values()]
def grid(p):return (int(round((p[0]-X0)/STEP)),int(round((p[1]-Y0)/STEP)),p[2])
def physical(q):return (round(X0+q[0]*STEP,5),round(Y0+q[1]*STEP,5),q[2])
DIR=[(1,0,1),(0,1,1),(-1,0,1),(0,-1,1),(1,1,math.sqrt(2)),(-1,1,math.sqrt(2)),(1,-1,math.sqrt(2)),(-1,-1,math.sqrt(2))]
def astar(start,end,width,net):
 a,v=masks(net,width);s=grid(start);t=grid(end)
 if a[s[2],s[1],s[0]] or a[t[2],t[1],t[0]]:return None,'endpoint blocked'
 def index(q):return (q[2]*NY+q[1])*NX+q[0]
 def heuristic(x,y,l):
  dx=abs(t[0]-x);dy=abs(t[1]-y);return max(dx,dy)+(math.sqrt(2)-1)*min(dx,dy)+(60 if l!=t[2] else 0)
 cost=np.full(2*NY*NX,np.inf);prev=np.full(2*NY*NX,-1,dtype=np.int32);dire=np.full(2*NY*NX,-1,dtype=np.int8)
 si=index(s);ti=index(t);cost[si]=0;heap=[(heuristic(*s),0,si)];visited=0
 while heap:
  _,g,idx=heapq.heappop(heap)
  if g!=cost[idx]:continue
  if idx==ti:
   route=[]
   while idx>=0:
    x=idx%NX;y=idx//NX%NY;l=idx//(NX*NY);route.append((x,y,l));idx=int(prev[idx])
   route=route[::-1];return [start]+[physical(q) for q in route]+[end],visited
  x=idx%NX;y=idx//NX%NY;l=idx//(NX*NY);visited+=1
  if visited>1200000:return None,'expansion limit'
  for di,(dx,dy,ds) in enumerate(DIR):
   xx=x+dx;yy=y+dy
   if not(0<=xx<NX and 0<=yy<NY) or a[l,yy,xx]:continue
   if dx and dy and (a[l,y,xx] or a[l,yy,x]):continue
   ni=(l*NY+yy)*NX+xx;ng=g+ds*(1.02 if l==0 else 1)+(1.5 if dire[idx]>=0 and dire[idx]!=di else 0)
   if ng<cost[ni]:cost[ni]=ng;prev[ni]=idx;dire[ni]=di;heapq.heappush(heap,(ng+heuristic(xx,yy,l),ng,ni))
  if not v[y,x] and not a[1-l,y,x]:
   ni=((1-l)*NY+y)*NX+x;ng=g+100
   if ng<cost[ni]:cost[ni]=ng;prev[ni]=idx;dire[ni]=-1;heapq.heappush(heap,(ng+heuristic(x,y,1-l),ng,ni))
 return None,'no path'
def simplify(pts):
 out=[]
 for p in pts:
  if out and p==out[-1]:continue
  if len(out)>=2 and p[2]==out[-1][2]==out[-2][2]:
   a=out[-2];b=out[-1]
   if abs((b[0]-a[0])*(p[1]-b[1])-(b[1]-a[1])*(p[0]-b[0]))<1e-8 and (b[0]-a[0])*(p[0]-b[0])+(b[1]-a[1])*(p[1]-b[1])>0:out.pop()
  out.append(p)
 return out

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--nets',nargs='*');args=ap.parse_args()
 nets={o['net'] for o in objs if o['net'] and not o['net'].startswith('unconnected-') and o['net']!='GND' and not o['net'].startswith('/USB_D')}
 priority=['/USB_CC2','/USB_CC1','/VBUS_RAW','/VBUS_PROTECTED','+5V','+3V3','/AUDIO_MONO','/ADC_MUX','/I2S_BCLK','/I2S_LRCK','/I2S_DIN','/SPI_SCK','/SPI_MOSI','/TFT_CS','/TFT_DC']
 order=[n for n in priority if n in nets]+sorted(nets-set(priority))
 if args.nets:order=args.nets
 result=[]
 for net,local_only in [(n,True) for n in order]+[(n,False) for n in order]:
  width=.5 if net in ['+3V3','+5V','/VBUS_RAW','/VBUS_PROTECTED'] else .2
  for attempt in range(30):
   groups=components(net)
   if len(groups)<=1:break
   best=[]
   for i,g1 in enumerate(groups):
    for g2 in groups[:i]:
     cand=min((math.hypot(a[0]-b[0],a[1]-b[1])+(10 if a[2]!=b[2] else 0),a,b) for a in g1 for b in g2);best.append(cand)
   best.sort();r=None;why=''
   if local_only and best[0][0]>12:break
   for _,s,t in best[:8]:
    r,why=astar(s,t,width,net)
    if r:break
   if not r:
    result.append(dict(net=net,status='deferred' if local_only else 'failed',reason=why,remaining_components=len(groups)));print(json.dumps(result[-1]),flush=True);break
   r=simplify(r);length=0;vias=0
   for a,b in zip(r,r[1:]):
    if a[2]!=b[2]:
     v=dict(kind='via',net_name=net,x=a[0],y=a[1],pad_size=.7,drill=.3);plan.append(v);addvia(v);vias+=1
    elif a[:2]!=b[:2]:
     v=dict(kind='trace',net_name=net,layer=['F.Cu','B.Cu'][a[2]],width=width,x1=a[0],y1=a[1],x2=b[0],y2=b[1]);plan.append(v);addtrack(v);length+=math.hypot(a[0]-b[0],a[1]-b[1])
   row=dict(net=net,status='planned',from_point=s,to_point=t,length_mm=round(length,3),vias=vias,segments=len(r)-1,expanded=why);result.append(row);print(json.dumps(row),flush=True)
   (D/'remaining-route-plan.json').write_text(json.dumps(plan,indent=2));(D/'remaining-route-plan-summary.json').write_text(json.dumps(result,indent=2))
 print('DONE',len(plan),flush=True)
if __name__=='__main__':main()
