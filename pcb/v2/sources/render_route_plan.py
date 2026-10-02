"""Render saved MCP geometry and proposed routes; this is a review plot, not fabrication."""
import json,html
from pathlib import Path
import plan_remaining_routes as p
D=p.D
plan=json.loads((D/'remaining-route-plan.json').read_text())
for layer,label in [(0,'front'),(1,'back')]:
 out=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="4 0 117 137" width="936" height="1096">','<rect x="4" y="0" width="117" height="137" fill="#101820"/>','<rect x="6" y="3.5" width="113" height="130.5" fill="#192e35" stroke="white" stroke-width=".12"/>']
 for o in p.objs:
  if layer not in o['layers']:continue
  if o['g'].geom_type!='Polygon':continue
  points=' '.join(f'{x:.5f},{y:.5f}' for x,y in o['g'].exterior.coords)
  out.append(f'<polygon points="{points}" fill="#7f969f"/>')
 for h in p.holes+p.npths:
  points=' '.join(f'{x:.5f},{y:.5f}' for x,y in h.exterior.coords);out.append(f'<polygon points="{points}" fill="#101820" stroke="#d1dce0" stroke-width=".1"/>')
 for t in plan:
  color='#f0c66a' if t['net_name'] in ['+3V3','+5V','/VBUS_RAW','/VBUS_PROTECTED'] else '#72c7ff'
  if t['kind']=='via':out.append(f'<circle cx="{t["x"]}" cy="{t["y"]}" r="{t["pad_size"]/2}" fill="none" stroke="{color}" stroke-width=".15"/>')
  elif t['layer']==['F.Cu','B.Cu'][layer]:out.append(f'<path d="M {t["x1"]} {t["y1"]} L {t["x2"]} {t["y2"]}" stroke="{color}" stroke-width="{t["width"]}" stroke-linecap="round"/>')
 for c in p.C.values():
  if c['layer']==['F.Cu','B.Cu'][layer]:out.append(f'<text x="{c["x"]}" y="{c["y"]}" font-size="1.2" text-anchor="middle" fill="#fff">{html.escape(c["reference"])}</text>')
 out.append('</svg>');f=D/f'route-plan-{label}.svg';f.write_text('\n'.join(out));import cairosvg;cairosvg.svg2png(url=str(f),write_to=str(f.with_suffix('.png')))
