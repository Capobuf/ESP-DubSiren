"""Standalone review drawings from exported SVG and pad queries (no KiCad writes)."""
import json,math,copy,xml.etree.ElementTree as E
from pathlib import Path
import cairosvg
P=Path('hardware/docs/placement-38');NS='http://www.w3.org/2000/svg';E.register_namespace('',NS)
def el(parent,tag,**attrs):return E.SubElement(parent,'{'+NS+'}'+tag,{k.replace('_','-'):str(v) for k,v in attrs.items()})
def text(p,x,y,s,size=3,fill='#17232e',**kwargs):
    e=el(p,'text',x=x,y=y,fill=fill,font_size=size,font_family='Arial',**kwargs);e.text=s;return e
def rect(p,x,y,w,h,fill,**kw):return el(p,'rect',x=x,y=y,width=w,height=h,fill=fill,**kw)
def line(p,x1,y1,x2,y2,c='#576876',width=.25,**kw):return el(p,'line',x1=x1,y1=y1,x2=x2,y2=y2,stroke=c,stroke_width=width,**kw)
def circle(p,x,y,r,fill,**kw):return el(p,'circle',cx=x,cy=y,r=r,fill=fill,**kw)
def root(w,h):
    r=E.Element('{'+NS+'}svg',{'viewBox':f'0 0 {w} {h}','width':f'{w}mm','height':f'{h}mm'});rect(r,0,0,w,h,'#f5f7f8');return r
def save(r,name,width=1600):
    E.ElementTree(r).write(P/(name+'.svg'),encoding='utf-8',xml_declaration=True)
    path=P/(name+'.svg')
    path.write_text('\n'.join(line.rstrip() for line in path.read_text(encoding='utf-8').splitlines())+'\n',encoding='utf-8')
    cairosvg.svg2png(url=str(P/(name+'.svg')),write_to=str(P/(name+'.png')),output_width=width)
def native(stage,side,view):
    r=E.parse(P/f'{stage}-{side}-{view}-native.svg').getroot()
    for c in list(r):
        if '#C872AB' in c.get('style',''):r.remove(c)
    g=E.Element('{'+NS+'}g')
    for c in r:g.append(copy.deepcopy(c))
    # Preview colours only; do not change soldermask or project settings.
    for c in g.iter():
        s=c.get('style','')
        s=s.replace('#C83434','#c69a47').replace('#4D7FC4','#c69a47').replace('#AFAFAF','#566b7b').replace('#F2EDA1','#273a44').replace('#E8B2A7','#526776')
        if s:c.set('style',s)
    return g
def board(stage,side):
    b=E.Element('{'+NS+'}g');rect(b,0,-6,86,126,'#e0e6e9',rx=3,stroke='#73828a',stroke_width=.25)
    b.append(native(stage,side,'assembly'));b.append(native(stage,side,'silk'))
    p=json.loads((P/f'{stage}-pads.json').read_text())
    comp={c['reference']:c for c in json.loads((P/f'{stage}-components.json').read_text())['components']}
    if side=='F':
        # Front breakout is opaque; no claim about heights or fastener fit.
        rect(b,25.601,31.26,36.83,30.48,'#314857',rx=2,stroke='#182932',stroke_width=.25)
        rect(b,26.825,37.8,32.35,17.4,'#102c39',rx=3,stroke='#58c4d4',stroke_width=.25)
        text(b,43,45.7,'DISPLAY',2.5,fill='#d8f5fa',text_anchor='middle');text(b,43,49,'centro X=43',1.9,fill='#97ced6',text_anchor='middle')
        for a in p['U5']['pads']:
            if a['pad_type']=='np_thru_hole':circle(b,a['x'],a['y'],1.25,'#e0e6e9',stroke='#a8c6d4',stroke_width=.25)
            else:circle(b,a['x'],a['y'],.5,'#e0e6e9',stroke='#c69a47',stroke_width=.2)
        if stage=='before':
            for a in p['U6']['pads']:circle(b,a['x'],a['y'],1.4,'none',stroke='#d3483f',stroke_width=.35)
            text(b,1.5,68,'U6: 18 fori scoperti',2.3,fill='#ac342e')
        else:
            # Ghost overlay explicitly identifies hidden holes, not visible copper.
            for a in p['U6']['pads']:
                circle(b,a['x'],a['y'],1.1,'none',stroke='#8fe4c0',stroke_width=.2,stroke_dasharray='.4 .4')
            text(b,43,65.8,'U6: 18 fori sotto U5',2.3,fill='#176d50',text_anchor='middle')
            line(b,43,63.5,43,61.8,'#176d50',.25)
        line(b,43,-5,43,30,'#5d9aab',.15,stroke_dasharray='1 1')
    else:
        for ref,color in [('U1','#d2e8e6'),('U6','#d9e5f2'),('U3','#d2e8e6')]:
            c=comp[ref]
            if ref=='U1':
                # Physical outline uses the authored Fab body, transformed with the footprint.
                a=math.radians(c['rotation']);pts=[]
                for x,y in [(-9,8.13),(-8,9.13),(9,9.13),(9,-10.07),(-9,-10.07)]:
                    pts.append((c['x']+x*math.cos(a)+y*math.sin(a),c['y']-x*math.sin(a)+y*math.cos(a)))
                el(b,'polygon',points=' '.join(f'{x},{y}' for x,y in pts),fill=color,stroke='#316f6b',stroke_width=.2)
            elif ref=='U6':
                a=math.radians(c['rotation']);pts=[]
                for x,y in [(-15.24,-10.16),(15.24,-10.16),(15.24,10.16),(-15.24,10.16)]:
                    pts.append((c['x']+x*math.cos(a)+y*math.sin(a),c['y']-x*math.sin(a)+y*math.cos(a)))
                el(b,'polygon',points=' '.join(f'{x},{y}' for x,y in pts),fill=color,stroke='#466b91',stroke_width=.2)
            else:rect(b,c['x']-1.95,c['y']-4.95,3.9,9.9,color,stroke='#316f6b',stroke_width=.2)
            # Show the breakout holes/pads on top of the explanatory body fill.
            if ref=='U6':
                for a in p[ref]['pads']:circle(b,a['x'],a['y'],a['size']['x']/2,'#f0f3f5',stroke='#c69a47',stroke_width=.2)
        if stage=='after':
            # Antenna center from Espressif Fig.10-2; cable path is a proposal.
            circle(b,6.33,52,1.25,'#b99f61',stroke='#6a562c',stroke_width=.2)
            line(b,6.33,52,-1,52,'#b07828',.55,stroke_dasharray='.8 .5')
    return b
def label_back(r,x,y,stage):
    # Labels are outside the mirrored board geometry, and therefore readable.
    comp=json.loads((P/f'{stage}-components.json').read_text())['components']
    for c in comp:
        if c['reference'] in ['U1','U3','U6']:
            text(r,x+86-c['x'],y+c['y'],c['reference'],2.8,text_anchor='middle')
for side,label in [('F','FRONTE'),('B','RETRO')]:
    r=root(202,172);text(r,7,8,f'#38 / {label} - confronto 2D',4.2)
    for i,stage in enumerate(['before','after']):
        x=8+i*99;y=40
        text(r,x,17,'PRIMA / base #36 preservata' if i==0 else 'DOPO / proposta da verificare',3.1)
        g=el(r,'g',transform=f'translate({x} {y})')
        if side=='B':g=el(g,'g',transform='translate(86 0) scale(-1 1)')
        g.append(board(stage,side))
        if side=='B':label_back(r,x,y,stage)
    text(r,7,166,'Verde tratteggiato: fori U6 nascosti sotto U5. Gli altri fori restano nel plot.' if side=='F' else 'Retro specchiato. Contorni e pad da export KiCad; riempimenti e sigle aggiunti per lettura.',2.25)
    text(r,7,170,'Proiezione XY: non certifica altezze, saldature, isolamento, ferramenta o accesso con utensili.',2.25)
    save(r,f'{side}-comparison')
# Analytical ratsnest: minimum spanning trees from the actual exported pad/net pairs.
r=root(112,165);text(r,5,7,'#38 / reti effettive, senza routing',3.5)
g=el(r,'g',transform='translate(12 36)');g.append(board('after','B'))
selected={'/USB_DM_CONN':'#df7831','/USB_DP_CONN':'#df7831','/USB_DM_MCU':'#df7831','/USB_DP_MCU':'#df7831','/I2S_BCLK':'#8b4caf','/I2S_LRCK':'#8b4caf','/I2S_DIN':'#8b4caf','/AUDIO_MONO':'#cc4450','/ADC_MUX':'#187b6a'}
p=json.loads((P/'after-pads.json').read_text());lengths={}
for net,col in selected.items():
    pads=[a for v in p.values() for a in v['pads'] if a['net']==net];used=[pads[0]];pending=pads[1:];lengths[net]=0
    while pending:
        _,i,j=min((math.hypot(a['x']-b['x'],a['y']-b['y']),i,j) for i,a in enumerate(used) for j,b in enumerate(pending))
        a,b=used[i],pending.pop(j);line(g,a['x'],a['y'],b['x'],b['y'],col,.23,stroke_dasharray='.8 .6');lengths[net]+=math.hypot(a['x']-b['x'],a['y']-b['y']);used.append(b)
text(r,5,160,'MST ricostruito dai pad/net salvati; segmenti indicativi, non piste.',2.2)
text(r,5,164,'Arancio USB / viola I2S / rosso MONO / verde ADC. Vista da sopra.',2.2)
save(r,'ratsnest',1100)
(P/'ratsnest-lengths.json').write_text(json.dumps(lengths,indent=2))
# U5 underside projection: primary source Eagle outlines, transformed to KiCad top view.
r=root(138,108);text(r,5,8,'#38 / U5-U6: sovrapposizione da verificare',3.8)
g=el(r,'g',transform='translate(-20 -42) scale(2.1)')
rect(g,25.601,31.26,36.83,30.48,'#dce8ed',rx=1)
rect(g,28.76,36.34,30.48,20.32,'none',stroke='#3e7298',stroke_width=.22)
# Metal SD body only; card positions shown separately by the Eagle package are excluded.
rect(g,27.225,38.723,15.1,14,'#d4b881',opacity='.65',stroke='#976824',stroke_width=.2)
rect(g,56.88,40,4.4,13,'#bda4cc',opacity='.7',stroke='#825c98',stroke_width=.2)
for a in p['U5']['pads']:
    if a['pad_type']=='np_thru_hole':circle(g,a['x'],a['y'],2,'none',stroke='#258377',stroke_width=.22);circle(g,a['x'],a['y'],1.25,'#f5f7f8')
for a in p['U6']['pads']:
    circle(g,a['x'],a['y'],1.25 if not a['number'] else .5,'#f5f7f8',stroke='#c74446',stroke_width=.22)
    if not a['number']:circle(g,a['x'],a['y'],2,'none',stroke='#c74446',stroke_width=.22,stroke_dasharray='.4 .4')
text(r,5,96,'Ocra: scocca microSD. Viola: corpo EYE SPI. Rosso: fori/pad DAC.',2.6)
text(r,5,101,'Cerchi esterni: ferramenta Ø4 ipotizzata; interasse minimo 5,98 mm.',2.6)
text(r,5,106,'XY documentato da Eagle; vetro/flex, rame e altri componenti richiedono controllo Z.',2.4)
save(r,'U5-U6-overlap',1350)
# Conditional SLxG1 study; rows at different XY are represented in one section.
# Only the internal nominal distances are dimensioned. Unknown thicknesses,
# component heights and long lead ends are schematic, not a collision model.
r=root(212,165);text(r,6,9,'#38 / prova header SLxG1',4.2)
text(r,6,15,'Corto 3,00 verso portante; isolanti fra le PCB. Nessuna quota definitiva approvata.',2.7)
text(r,6,21,'Sezione composta, non in scala; estremità lunghe interrotte; spessori t5/t6 ignoti.',2.5)
rect(r,34,41,128,4,'#48768c');text(r,37,38,'U5 / PCB display, t5 da misurare',2.8)
rect(r,48,45,43,10,'none',stroke='#ad7b2a',stroke_width=.3,stroke_dasharray='1 1')
text(r,50,51,'hU5 ? microSD / flex',2.4,fill='#98661b')
rect(r,15,70,153,16,'#b6c0c7');text(r,20,79,'portante',3.0)
rect(r,34,111,128,4,'#48768c');text(r,95,120,'U6 / PCB DAC, t6 da misurare',2.8)
rect(r,67,86,8,25,'#3e474c');line(r,71,56,71,128,'#c69a47',1.1)
rect(r,127,45,8,25,'#3e474c');line(r,131,28,131,100,'#c69a47',1.1)
for x,y in [(71,125),(131,31)]:
    line(r,x-2,y-1,x+2,y+1,'#f5f7f8',1.4)
    line(r,x-2,y-1,x+2,y+1,'#66747d',.3)
text(r,78,102,'SL8 / SL6',2.7);text(r,140,63,'SL11',2.7)
line(r,22,45,22,70);line(r,19,45,25,45);line(r,19,70,25,70)
text(r,2,55,'g5',2.6);text(r,2,60,'2,50',2.6)
line(r,22,86,22,111);line(r,19,86,25,86);line(r,19,111,25,111)
text(r,2,96,'g6',2.6);text(r,2,101,'2,50',2.6)
line(r,59,56,59,70,'#a16b1b');line(r,56,56,62,56,'#a16b1b');line(r,56,70,62,70,'#a16b1b')
text(r,40,64,'1,40',2.8,fill='#98661b')
line(r,101,45,101,56,'#a16b1b');line(r,98,45,104,45,'#a16b1b');line(r,98,56,104,56,'#a16b1b')
text(r,105,52,'1,10*',2.8,fill='#98661b')
line(r,176,70,176,86);line(r,173,70,179,70);line(r,173,86,179,86)
text(r,182,78,'1,60',2.8)
text(r,140,96,'pin U5 su B.Cu: 1,40',2.5)
text(r,6,135,'* Budget nominale fra punta pin U6 e piano PCB U5: 2,50 - (3,00 - 1,60).',2.7)
text(r,6,141,'Non comprende i volumi contrapposti di componenti, giunti, viti e margini di montaggio.',2.6)
text(r,6,147,'Con le sole tolleranze header: budget fino a 0,65 mm, portante fissata a 1,60 mm.',2.6)
text(r,6,153,'I lati lunghi sono 6,10 mm: residui oltre i breakout = 6,10 - t5 / 6,10 - t6.',2.6)
text(r,6,159,'Disegno della prova SLxG1: non descrive gli header inclusi Adafruit né certifica lo stack.',2.6)
save(r,'U5-U6-section',1600)
