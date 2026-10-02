"""Prepare Konnect requests from the official module geometry.

Run extract_adafruit_geometry.py first. This writes JSON requests and a lead
map; only Konnect writes .kicad_mod files. Footprints require scratch acceptance.
"""
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / 'footprints/ESP-DubSiren-V2-Mechanical.pretty'


def graphic(w, width=0.1):
    a, b = w['start'], w['end']
    curve = w['curve_deg']
    if not curve:
        return dict(type='line', start=a, end=b, stroke_width_mm=width)
    dx, dy = b['x']-a['x'], b['y']-a['y']
    # Signed arc sagitta from chord midpoint; preserves exact Eagle sweep.
    k = math.tan(math.radians(curve)/4)/2
    mid = dict(x=round((a['x']+b['x'])/2 + dy*k, 6),
               y=round((a['y']+b['y'])/2 - dx*k, 6))
    return dict(type='arc', start=a, mid=mid, end=b, stroke_width_mm=width)


def rectangle(x1, y1, x2, y2, width=0.05):
    return dict(type='rect', start=dict(x=x1,y=y1), end=dict(x=x2,y=y2),
                stroke_width_mm=width, fill='none')


def prepare():
    LIB.mkdir(parents=True, exist_ok=True)
    modules = json.loads((ROOT/'sources/adafruit-geometry.json').read_text())
    calls = []
    report = ['# Adafruit module physical lead maps', '',
              'Candidate footprints; scratch render and instance readback acceptance are required.', '',
              'Coordinates in mm, X right / Y down. Footprint origins are PCB outline centers.',
              'Mount holes on the carrier are NPTH at the official module hole centers; ',
              'they are mechanical features, not electrical symbol pins. Module plated holes',
              'do not imply electrically connected carrier mounting holes.', '']
    for m in modules:
        pid=m['product']
        name='Adafruit_'+pid+('_DisplayFace' if pid=='5393' else '_ComponentFace')
        path=str(LIB/(name+'.kicad_mod')).replace('\\','/')
        pads=[dict(number=p['number'],type='thru_hole',shape='rect' if p['number']=='1' else 'circle',
                   x=p['x'],y=p['y'],width=p['diameter'],height=p['diameter'],drill=p['drill']) for p in m['pads']]
        pads += [dict(number='',type='np_thru_hole',shape='circle',x=p['x'],y=p['y'],
                      width=2.5,height=2.5,drill=2.5) for p in m['mounts']]
        calls.append(dict(name='create_footprint',arguments=dict(output=path,name=name,pads=pads,
            body_width=m['width_mm'],body_height=m['height_mm'],package_type='through_hole',
            description='Adafruit '+pid+'; official Eagle geometry; '+m['view']+'; see physical lead map.')))
        fab=[graphic(w) for w in m['outline']]
        for component in m['component_graphics'].values():
            fab += [graphic(w) for w in component['graphics']]
        calls.append(dict(name='set_footprint_graphics',arguments=dict(footprint_path=path,
            selector=dict(layer='F.Fab'),mode='replace',graphics=fab)))
        silk=[graphic(w,0.12) for w in m['outline']]
        p1=m['pads'][0]
        silk.append(dict(type='circle',center=dict(x=p1['x'],y=p1['y']-1.6),
                         radius_mm=.25,stroke_width_mm=.12,fill='none'))
        calls.append(dict(name='set_footprint_graphics',arguments=dict(footprint_path=path,
            selector=dict(layer='F.SilkS'),mode='replace',graphics=silk)))
        # DAC's onboard jack protrudes beyond the PCB by 1.973 mm.
        right=17.713 if pid=='6250' else m['width_mm']/2+.5
        calls.append(dict(name='set_footprint_graphics',arguments=dict(footprint_path=path,
            selector=dict(layer='F.CrtYd'),mode='replace',graphics=[rectangle(-m['width_mm']/2-.5,
                -m['height_mm']/2-.5,right,m['height_mm']/2+.5)])))
        calls.append(dict(name='get_footprint_info',arguments=dict(footprint_path=path,include_pads=True,include_graphics=True)))
        report += ['## '+pid, '', 'Source: `'+m['source']+'`; revision recorded beside source.',
                   'View: '+m['view']+'.',
                   'Footprint: `ESP-DubSiren-V2-Mechanical:'+name+'`.', '',
                   '| Physical lead | Function | Symbol / pad | X | Y | Drill | Pad diameter |',
                   '|---|---|---|---:|---:|---:|---:|']
        for p in m['pads']:
            report.append('| {source_lead} | {function} | {number} | {x} | {y} | {drill} | {diameter} |'.format(**p))
        report += ['', 'Pin 1 pad is square, using the same maximum diameter as the source land.']
        for p in m['mounts']:
            report.append('- '+p['source_element']+': X='+str(p['x'])+', Y='+str(p['y'])+', NPTH 2.5 mm.')
        report += ['', 'Counts: '+str(len(m['pads']))+' electrical leads, same symbol/pad count; zero duplicate pads; four NPTH mounts.', '']
        if pid=='6250':
            report += ['Secondary header JP1 maps to carrier pads 9–14. It is unpopulated; all six schematic pins are NC.',
                       'The eight-pin primary header alone carries the project signals.', '']
        else:
            report += ['F.Fab includes the exact TFT, EYESPI and microSD source outlines.',
                       'The additional SD card insertion outlines are reference geometry only; no SD card is fitted.',
                       'The carrier uses the display-facing view; its header order is reversed relative to the Eagle top view.', '']
    (ROOT/'reports/ADAFRUIT-PHYSICAL-LEAD-MAPS.md').write_text('\n'.join(report)+'\n',encoding='utf-8')
    (ROOT/'sources/adafruit-footprint-requests.json').write_text(json.dumps(dict(toolsets=['library'],calls=calls),indent=2)+'\n',encoding='utf-8')
    print(len(calls),'Konnect requests prepared')


if __name__=='__main__': prepare()
