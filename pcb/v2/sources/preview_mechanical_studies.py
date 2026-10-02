"""Generate read-only orthographic projections of saved FreeCAD studies."""
from pathlib import Path
import FreeCAD as App
import Part
import TechDraw

ROOT=Path(__file__).resolve().parents[1]
doc=App.openDocument(str(ROOT/'mechanical/enclosure-study-v2.FCStd'))
base=doc.getObject('BaseWithTrialSupports').Shape
body=doc.getObject('Body').Shape.copy()
body.translate(App.Vector(0,0,50))
svg=TechDraw.projectToSVG(Part.makeCompound([base,body]),App.Vector(1,1,1))
svg=svg.replace('stroke-width="1.0"','stroke-width="0.3"')
(ROOT/'reports/enclosure-study-preview.svg').write_text(
    '<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="1000" '
    'viewBox="-120 -140 260 260"><rect x="-120" y="-140" width="260" height="260" '
    'fill="white"/><text x="-112" y="-130" font-size="5">V2 enclosure study - exploded view</text>'
    '<text x="-112" y="-122" font-size="3.7">No cutouts or assembly acceptance; dimensions in FreeCAD model.</text>'
    +svg+'</svg>', encoding='utf-8')
print('Saved enclosure-study-preview.svg')
