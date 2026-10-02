"""Native KiCad fallback for oval drills, absent from Konnect 0.12.1 schema.

All other footprint construction uses Konnect. Run with KiCad's Python.
Reread via Konnect and render in scratch before accepting the footprint.
"""
from pathlib import Path
import pcbnew

library = Path(__file__).resolve().parents[1] / 'footprints/ESP-DubSiren-V2-Mechanical.pretty'
name = 'SameSky_RCJ-2123'
footprint = pcbnew.FootprintLoad(str(library), name)
assert footprint is not None
mounts = [p for p in footprint.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH]
assert len(mounts) == 2
for pad in mounts:
    pad.SetDrillShape(pcbnew.PAD_DRILL_SHAPE_OBLONG)
    pad.SetDrillSize(pcbnew.VECTOR2I(pcbnew.FromMM(2.5), pcbnew.FromMM(4.2)))
    pad.SetNumber('')  # Mechanical-only NPTH; no electrical symbol pin.
pcbnew.FootprintSave(str(library), footprint)
reopened = pcbnew.FootprintLoad(str(library), name)
assert sum(p.GetDrillShape() == pcbnew.PAD_DRILL_SHAPE_OBLONG for p in reopened.Pads()) == 2
print('Saved and reopened two NPTH slots, 2.5 x 4.2 mm, native KiCad API')
