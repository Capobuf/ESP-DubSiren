"""Exact 1:1 projections of the assembly's native machining-tool geometry.

DXF uses millimetres with plan Y negated (rear vertical coordinate is Z-42). SVG and PDF
are A4 sheets in millimetres. Do not fit printed PDFs to paper.
"""
from pathlib import Path
import json,math,html
import ezdxf
from reportlab.pdfgen import canvas
from reportlab.lib.units import mm
import fitz
R=Path(__file__).resolve().parents[1];O=R/'mechanical';D=json.loads((R/'reports/assembly-cad-geometry.json').read_text())
COLORS={'OUTLINE':'#202830','CUT':'#111111','POCKET':'#2875b3','REFERENCE':'#87929c','ACTIVE':'#17865b','ALLOWANCE':'#965baa','DIMENSION':'#45515b'}
W=D['parameters']['Width'];H=D['parameters']['Depth'];Z=D['parameters']['Height']
records=[]

def generate(panel,stem):
    rear=panel=='rear';overlay=panel=='overlay';height=Z if rear else H
    rows=D['cuts']['top' if overlay else panel];items=[];seen=set()
    def add(kind,layer,**kw):
        key=(kind,layer,json.dumps(kw,sort_keys=True));
        if key not in seen:seen.add(key);items.append(dict(kind=kind,layer=layer,**kw))
    def line(a,b,l='CUT'):
        a=[round(x,6) for x in a];b=[round(x,6) for x in b]
        if math.dist(a,b)>1e-5:
            if tuple(a)>tuple(b):a,b=b,a
            add('line',l,a=a,b=b)
    def rect(x,y,w,h,l):
        q=[(x,y),(x+w,y),(x+w,y+h),(x,y+h),(x,y)]
        for a,b in zip(q,q[1:]):line(a,b,l)
    def circle(x,y,r,l='CUT'):add('circle',l,center=[round(x,6),round(y,6)],radius=round(r,6))
    def cross(x,y):line((x-1.3,y),(x+1.3,y),'REFERENCE');line((x,y-1.3),(x,y+1.3),'REFERENCE')
    def project(p):return (p[0],Z-p[2]) if rear else (p[0],p[1])
    rect(0,0,W,height,'OUTLINE')
    if rear:
        b=D['objects']['EnclosureRearPanel']['bounds'];rect(b[0],Z-b[5],b[3]-b[0],b[5]-b[2],'ALLOWANCE')
    for row in rows:
        layer='CUT' if row['kind']=='through' else 'POCKET'
        if overlay and layer=='POCKET':continue
        for e in row['analytic_edges']:
            if e['kind']=='circle':
                axis=e['axis'][1 if rear else 2]
                if abs(axis)>.99:
                    a,b=project(e['center']);circle(a,b,e['radius'],layer)
            else:
                a,b=map(project,e['points']);line(a,b,layer)
        bb=row['shape_bounds'];center=[(bb[i]+bb[i+3])/2 for i in range(3)]
        cross(*project(center))
    # Mount and module references come from the CAD hardware envelopes, not
    # a second coordinate list. They are NOT extra through-panel drill holes.
    if not rear:
        for name,q in D['objects'].items():
            if name.startswith('MainboardSupportShim') or name.startswith('DisplaySpacer'):
                b=q['bounds'];x=(b[0]+b[3])/2;y=(b[1]+b[4])/2
                circle(x,y,(b[3]-b[0])/2,'REFERENCE');cross(x,y)
    if overlay:
        rect(5,5,W-10,H-10,'ALLOWANCE')
        for q in D['cuts']['overlay']:
            x,y=q['center']
            if q['kind']=='knob':circle(x,y,q['diameter']/2,'ALLOWANCE');cross(x,y)
            elif q['kind']=='display_active':
                # Rounded corners are exported as exact DXF/SVG/PDF arcs below.
                add('rounded_rect','ACTIVE',x=x-q['width']/2,y=y-q['height']/2,w=q['width'],h=q['height'],r=q['corner_radius'])
            else:rect(x-q['width']/2,y-q['height']/2,q['width'],q['height'],'ALLOWANCE')
    # Dimension extensions and scale check live outside the part perimeter.
    line((0,-5),(W,-5),'DIMENSION');line((0,-7),(0,-2),'DIMENSION');line((W,-7),(W,-2),'DIMENSION')
    line((-5,0),(-5,height),'DIMENSION');line((-7,0),(-2,0),'DIMENSION');line((-7,height),(-2,height),'DIMENSION')
    annotations=[(W/2,-7.5,f'{W:g} mm'),(-15,height/2,f'{height:g} mm')]
    for x,y in [(0,0),(W,0),(0,height),(W,height)]:cross(x,y)
    scale_y=height+12;line((0,scale_y),(100,scale_y),'DIMENSION')
    for x in range(0,101,10):line((x,scale_y-1),(x,scale_y+1),'DIMENSION')
    annotations.append((50,scale_y+4,'100 mm PRINT SCALE CHECK'))
    page=canvas.Canvas(str(O/(stem+'.pdf')),pagesize=(210*mm,297*mm));page.setTitle('ESP-DubSiren V2 '+panel+' template 1:1')
    page.setFont('Helvetica-Bold',13);page.drawString(20*mm,277*mm,'ESP-DubSiren V2 | '+panel.upper())
    page.setFont('Helvetica',9);page.drawString(20*mm,270*mm,'Prototype machining / geometry reference - scale 1:1, dimensions mm')
    page.setFont('Helvetica',8);page.drawString(20*mm,264*mm,'Print at 100% / Actual size. Disable Fit / Shrink. Verify the 100 mm scale.')
    ox,oy=35,50
    svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="210mm" height="297mm" viewBox="0 0 210 297">', '<rect width="210" height="297" fill="white"/>',f'<text x="20" y="20" font-family="sans-serif" font-size="5">ESP-DubSiren V2 | {panel.upper()}</text>', '<text x="20" y="27" font-family="sans-serif" font-size="3">1:1 millimetres - print at 100%, verify the 100 mm scale.</text>',f'<g transform="translate({ox},{oy})" fill="none">']
    cad=ezdxf.new('R2010');cad.units=4;cad.header['$INSUNITS']=4;cad.header['$MEASUREMENT']=1;msp=cad.modelspace()
    for i,l in enumerate(COLORS):cad.layers.new(l,dxfattribs={'color':[7,7,5,8,3,6,8][i]})
    def pdfpt(p):return ((ox+p[0])*mm,(297-oy-p[1])*mm)
    for q in items:
        col=COLORS[q['layer']];rgb=tuple(int(col[i:i+2],16)/255 for i in [1,3,5]);page.setStrokeColorRGB(*rgb)
        width=.25 if q['layer']=='OUTLINE' else .15;page.setLineWidth(width*mm)
        dashed=q['layer'] in ['POCKET','REFERENCE','ALLOWANCE'];page.setDash(1.4*mm,.8*mm) if dashed else page.setDash()
        style=f'stroke="{col}" stroke-width="{width}"'+(' stroke-dasharray="1.4 .8"' if dashed else '')
        layer={'layer':q['layer']}
        if q['kind']=='line':
            a,b=q['a'],q['b'];page.line(*pdfpt(a),*pdfpt(b));svg.append(f'<line x1="{a[0]}" y1="{a[1]}" x2="{b[0]}" y2="{b[1]}" {style}/>');msp.add_line((a[0],-a[1]),(b[0],-b[1]),dxfattribs=layer)
        elif q['kind']=='circle':
            a=q['center'];r=q['radius'];page.circle(*pdfpt(a),r*mm);svg.append(f'<circle cx="{a[0]}" cy="{a[1]}" r="{r}" {style}/>');msp.add_circle((a[0],-a[1]),r,dxfattribs=layer)
        else:
            x,y,w,h,r=[q[k] for k in ['x','y','w','h','r']];page.roundRect((ox+x)*mm,(297-oy-y-h)*mm,w*mm,h*mm,r*mm);svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" {style}/>')
            for a,b in [((x+r,y),(x+w-r,y)),((x+w,y+r),(x+w,y+h-r)),((x+w-r,y+h),(x+r,y+h)),((x,y+h-r),(x,y+r))]:msp.add_line((a[0],-a[1]),(b[0],-b[1]),dxfattribs=layer)
            for a,b,aa,bb in [(x+r,-y-r,90,180),(x+w-r,-y-r,0,90),(x+w-r,-y-h+r,270,360),(x+r,-y-h+r,180,270)]:msp.add_arc((a,b),r,aa,bb,dxfattribs=layer)
    page.setDash();page.setFillColorRGB(.15,.2,.25);page.setFont('Helvetica',7)
    for x,y,t in annotations:
        page.drawCentredString(*pdfpt((x,y)),t);svg.append(f'<text x="{x}" y="{y}" fill="#45515b" stroke="none" text-anchor="middle" font-size="2.4" font-family="sans-serif">{html.escape(t)}</text>');msp.add_text(t,dxfattribs={'layer':'DIMENSION','height':2.4}).set_placement((x,-y))
    svg.append('</g>')
    footer=max(oy+height+26,130)
    notes=['BLACK: through cut. BLUE DASH: pocket/spotface/counterbore.', 'GREY: internal PCB/module mounting reference; do not drill through the lid.', 'Rear view: outside looking into enclosure, Z upward.' if rear else 'Top view: rear edge at Y=0, X right, Y downward; origin is upper left.', 'DXF is in mm; top/base DXF Y is negative. Refer to MECHANICAL-V2.md for depths.', 'Generated from assembly-v2.FCStd native machining tools; no separate coordinate table.']
    if overlay:notes.insert(0,'PURPLE: knob/keycap/bezel allowance and 5 mm graphic safe margin. GREEN: visible TFT.')
    if rear:notes.insert(0,'PURPLE: removable 125 x 36 x 1.5 mm aluminium rear panel (Z=3..39).')
    for i,t in enumerate(notes):
        page.setFont('Helvetica',8);page.drawString(20*mm,(297-footer-i*5)*mm,t);svg.append(f'<text x="20" y="{footer+i*5}" fill="#45515b" font-family="sans-serif" font-size="2.5">{html.escape(t)}</text>')
    svg.append('</svg>');(O/(stem+'.svg')).write_text('\n'.join(svg));cad.saveas(O/(stem+'.dxf'));page.save()
    check=ezdxf.readfile(O/(stem+'.dxf'));assert check.units==4
    pdf=fitz.open(O/(stem+'.pdf'));assert abs(pdf[0].rect.width-210*mm)<.01
    pdf[0].get_pixmap(matrix=fitz.Matrix(1.5,1.5)).save(str(R/'reports'/(stem+'-preview.png')))
    assert any(q['kind']=='line' and q['a']==[0.0,scale_y] and q['b']==[100.0,scale_y] for q in items)
    records.append(dict(panel=panel,stem=stem,entity_count=len(items),units='mm',print_scale=1,reference_width_mm=W,reference_height_mm=height,scale_check_mm=100,source='assembly-cad-geometry.json analytic native CAD edges'))

for panel,stem in [('top','panel-top-v2'),('rear','panel-rear-v2'),('base','panel-base-v2'),('overlay','panel-overlay-v2')]:generate(panel,stem)
(R/'reports/panel-template-validation.json').write_text(json.dumps(records,indent=2));print(json.dumps(records,indent=2))
