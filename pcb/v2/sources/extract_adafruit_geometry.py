"""Extract manufacturer Eagle geometry, without modifying any KiCad source.

Output coordinates are local KiCad X-right/Y-down millimetres. 5393 is viewed
from its display face (Eagle bottom); 6250 from its component face (Eagle top).
The output is an engineering input, not a declaration of footprint acceptance.
"""
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent


def transform(x, y, element):
    rot = element.get('rot', 'R0')
    # Eagle applies rotation then reflection in the board's Y axis.
    # Cross-check: KiCad 10 pcb_io_eagle.cpp orientFootprint() sets a+180,
    # then Flip(TOP_BOTTOM), after converting Eagle Y-up to KiCad Y-down.
    a = math.radians(float(rot.split('R')[1]))
    rx, ry = x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)
    if 'M' in rot:
        rx = -rx
    return float(element.get('x')) + rx, float(element.get('y')) + ry


def extract(product, width, height, display_face):
    source = next((ROOT / ('adafruit-' + product)).glob('*.brd'))
    board = ET.parse(source).getroot().find('drawing/board')
    def local(x, y):
        return {'x': round((width / 2 - x) if display_face else (x - width / 2), 6),
                'y': round(height / 2 - y, 6)}
    def package(el):
        return board.find("libraries/library[@name='%s']/packages/package[@name='%s']" %
                          (el.get('library'), el.get('package')))
    contacts = {(c.get('element'), c.get('pad')): s.get('name')
                for s in board.find('signals') for c in s.findall('contactref')}
    result = {'product': product, 'source': str(source.relative_to(ROOT)),
              'view': 'display face / Eagle bottom' if display_face else 'component face / Eagle top',
              'width_mm': width, 'height_mm': height, 'pads': [], 'mounts': [],
              'outline': [], 'component_graphics': {}}
    for w in board.find('plain').findall('wire'):
        if w.get('layer') == '20':
            result['outline'].append({'start': local(float(w.get('x1')), float(w.get('y1'))),
                                      'end': local(float(w.get('x2')), float(w.get('y2'))),
                                      'curve_deg': float(w.get('curve', 0)) * (1 if display_face else -1)})
    for el in board.find('elements'):
        pkg = package(el)
        name = el.get('name')
        if name in (['JP1'] if product == '5393' else ['JP2', 'JP1']):
            for p in pkg.findall('pad'):
                xy = local(*transform(float(p.get('x')), float(p.get('y')), el))
                number = int(p.get('name')) + (8 if product == '6250' and name == 'JP1' else 0)
                result['pads'].append(dict(number=str(number), source_lead=name + '.' + p.get('name'),
                                          function=contacts.get((name, p.get('name')), 'NC'),
                                          drill=float(p.get('drill')), diameter=float(p.get('diameter')), **xy))
        if el.get('package') == 'MOUNTINGHOLE_2.5_PLATED':
            result['mounts'].append(dict(source_element=name, drill_mm=2.5,
                                         **local(float(el.get('x')), float(el.get('y')))))
        relevant = ['DISP1', 'Q2', 'X1'] if product == '5393' else ['X3']
        if name in relevant:
            graphics = []
            for w in pkg.findall('wire'):
                if w.get('layer') not in ['21', '51']:
                    continue
                # The determinant changes sign for each reflection.
                sign = (1 if display_face else -1) * (-1 if 'M' in el.get('rot', '') else 1)
                graphics.append({'start': local(*transform(float(w.get('x1')), float(w.get('y1')), el)),
                                 'end': local(*transform(float(w.get('x2')), float(w.get('y2')), el)),
                                 'curve_deg': float(w.get('curve', 0)) * sign,
                                 'source_layer': w.get('layer')})
            result['component_graphics'][name] = {'package': el.get('package'), 'graphics': graphics}
    result['pads'].sort(key=lambda p: int(p['number']))
    return result


if __name__ == '__main__':
    output = [extract('5393', 36.83, 30.48, True), extract('6250', 30.48, 20.32, False)]
    (ROOT / 'adafruit-geometry.json').write_text(json.dumps(output, indent=2) + '\n', encoding='utf-8')
    for module in output:
        print(module['product'], module['view'], len(module['pads']), 'header pads,', len(module['mounts']), 'mounts')
