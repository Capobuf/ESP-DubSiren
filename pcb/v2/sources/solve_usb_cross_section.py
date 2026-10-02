"""2D electrostatic finite-volume differential impedance estimate.

Paired conductors at +/-1 V, opposite copper plane and distant boundaries at
zero. Solve Laplace equation with local dielectric permittivity, repeat in air,
and derive quasi-TEM impedance from differential capacitances. SI units in flux.
No KiCad source is modified. This is an engineering estimate, not fab acceptance.
"""
import argparse,json,time
from pathlib import Path
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.linalg import spsolve

EPS0=8.8541878128e-12
C0=299792458.0
def capacitance(w,g,step,air=False):
    # JLCPCB nominal two-layer 1.6 mm, finished outer copper 1.6 mil;
    # 0.6 mil mask above copper. Derived dielectric spacing = 1.48824 mm.
    h=1.48824;t=.04064;mask=.01524;er=4.5
    x=np.arange(-4,4+step/2,step);y=np.arange(0,4+step/2,step)
    xx,yy=np.meshgrid(x,y);ny,nx=xx.shape
    e=np.ones_like(xx)
    if not air:
        e[yy<h]=er
        e[(yy>=h)&(yy<=h+.03048)]=3.8
        over_trace=((xx>=-g/2-w)&(xx<=-g/2))|((xx>=g/2)&(xx<=g/2+w))
        e[over_trace&(yy>=h)&(yy<=h+t+mask)]=3.8
    left=(xx>=-g/2-w)&(xx<=-g/2)&(yy>=h)&(yy<=h+t)
    right=(xx>=g/2)&(xx<=g/2+w)&(yy>=h)&(yy<=h+t)
    fixed=left|right
    fixed[[0,-1],:]=True;fixed[:,[0,-1]]=True
    voltage=np.zeros_like(xx);voltage[left]=1;voltage[right]=-1
    ids=np.full(xx.shape,-1,int);ids[~fixed]=np.arange(np.sum(~fixed));n=np.sum(~fixed)
    diag=np.zeros(n);rhs=np.zeros(n);rows=[];cols=[];vals=[]
    for dy,dx in [(0,1),(0,-1),(1,0),(-1,0)]:
        ya=slice(1,ny-1);xa=slice(1,nx-1)
        yb=slice(1+dy,ny-1+dy);xb=slice(1+dx,nx-1+dx)
        a=ids[ya,xa];b=ids[yb,xb];ea=e[ya,xa];eb=e[yb,xb]
        conduct=2*ea*eb/(ea+eb);valid=a>=0
        conductor_neighbor=(left|right)[yb,xb]
        conduct[conductor_neighbor]=ea[conductor_neighbor]
        diag[a[valid]]+=conduct[valid]
        unknown=valid&(b>=0)
        rows.extend(a[unknown]);cols.extend(b[unknown]);vals.extend(-conduct[unknown])
        known=valid&(b<0)
        rhs[a[known]]+=conduct[known]*voltage[yb,xb][known]
    rows.extend(range(n));cols.extend(range(n));vals.extend(diag)
    mat=coo_matrix((vals,(rows,cols)),shape=(n,n)).tocsr()
    voltage[~fixed]=spsolve(mat,rhs)
    q=0.
    for dy,dx in [(0,1),(0,-1),(1,0),(-1,0)]:
        a=left[1:-1,1:-1];b=left[1+dy:ny-1+dy,1+dx:nx-1+dx]
        border=a&~b
        eb=e[1+dy:ny-1+dy,1+dx:nx-1+dx]
        vb=voltage[1+dy:ny-1+dy,1+dx:nx-1+dx]
        q+=np.sum(eb[border]*(1-vb[border]))*EPS0
    return q/2 # Vdiff = 2 V; capacitance per unit length F/m

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--step',type=float,default=.025)
    p.add_argument('--widths',type=float,nargs='+',default=[.2,.3,.4,.5]);p.add_argument('--gap',type=float,default=.15)
    a=p.parse_args();out=[]
    for w in a.widths:
        ca=capacitance(w,a.gap,a.step,True);cd=capacitance(w,a.gap,a.step,False)
        row=dict(width_mm=w,gap_mm=a.gap,grid_mm=a.step,C_air_F_per_m=ca,C_dielectric_F_per_m=cd,Zdiff_ohm=1/(C0*np.sqrt(ca*cd)))
        out.append(row);print(json.dumps(row),flush=True)
    Path(__file__).resolve().parents[1].joinpath('reports','usb-cross-section-'+str(a.step)+'.json').write_text(json.dumps(out,indent=2)+'\n')
