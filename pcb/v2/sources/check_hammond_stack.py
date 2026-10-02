"""FreeCAD dimensional feasibility check; not a full assembly or machining model.

Run using FreeCAD's bundled Python. Imports the original Hammond and RCA STEP.
Never modifies KiCad files. Uses optimalBoundingBox: ordinary BoundBox includes
untrimmed surface extents in the Hammond STEP and produces incorrect dimensions.
"""
from pathlib import Path
import json
import FreeCAD as App
import Part

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'mechanical'
OUT.mkdir(exist_ok=True)
doc = App.newDocument('HammondV2StackFeasibility')

def feature(name, shape):
    obj = doc.addObject('PartDesign::Feature', name)
    obj.Shape = shape
    return obj

original = Part.read(str(ROOT.parent / '3dmodels/Hammond_29830PSLA.stp'))
assert original.isValid() and len(original.Solids) == 6
# Manufacturer X becomes assembly X; original Z becomes assembly Y;
# original -Y becomes assembly Z. This is a rigid rotation, not rescaling.
matrix = App.Matrix()
matrix.A11, matrix.A12, matrix.A13 = 1, 0, 0
matrix.A21, matrix.A22, matrix.A23 = 0, 0, 1
matrix.A31, matrix.A32, matrix.A33 = 0, -1, 0
matrix.A34 = 3.8
body = original.Solids[0].copy()
lid = original.Solids[1].copy()
body.transformShape(matrix)
lid.transformShape(matrix)
feature('OriginalHammondBody', body)
feature('OriginalHammondLid', lid)

# Obtain the two large internal horizontal faces from imported CAD geometry.
top = max((f for f in body.Faces if f.Area > 9000
           and f.optimalBoundingBox().ZLength < 1e-5),
          key=lambda f: -f.CenterOfMass.z).CenterOfMass.z
floor = max((f for f in lid.Faces if f.Area > 9000
             and f.optimalBoundingBox().ZLength < 1e-5),
            key=lambda f: f.CenterOfMass.z).CenterOfMass.z
rca = Part.read(str(ROOT / '3dmodels/SameSky_RCJ-2123_aligned.step'))
assert rca.isValid()
rca_height = rca.optimalBoundingBox().ZMax
pcb_thickness = 1.6
pot_body_lower_bound = 7.5  # Bourns PTV/PTT 09/23 p2, excludes stand-off.
clearance_each = 0.5       # Explicit engineering allowance, not vendor tolerance.
required = rca_height + pcb_thickness + pot_body_lower_bound + 2*clearance_each
result = {
    'status': 'REJECTED_FOR_SINGLE_PLANAR_BOARD_WITH_OPPOSITE_FACE_RCA',
    'scope': 'Necessary Z-stack feasibility; not complete XY assembly validation',
    'enclosure_outer_mm': [120.8, 95.5, 33.8],
    'inner_floor_z_mm': floor, 'inner_top_z_mm': top,
    'available_z_mm': top-floor,
    'rca_height_above_pcb_from_step_mm': rca_height,
    'pcb_thickness_mm': pcb_thickness,
    'pot_body_only_lower_bound_mm': pot_body_lower_bound,
    'minimum_clearance_each_face_mm': clearance_each,
    'required_lower_bound_mm': required,
    'shortfall_mm': required-(top-floor),
    'exclusions': ['pot stand-off', 'solder protrusions', 'display stack',
                   'manufacturing tolerances', 'nut and washer stack'],
    'same_face_rca': '20.5 mm RCA height exceeds front-pot body plus 5 mm bushing; top panel cannot clear RCA while engaging pot bushing',
}
assert abs(top-floor-29.7) < 1e-4
assert required > top-floor

# Show the imported RCA opposite a trial PCB, with 0.5 mm lid clearance.
pcb_bottom = floor + clearance_each + rca_height
pcb_top = pcb_bottom + pcb_thickness
rca.rotate(App.Vector(), App.Vector(1,0,0),180)
rca.translate(App.Vector(0,-30,pcb_bottom))
feature('RCAOppositeFaceTrial', rca)
feature('TrialPCBEnvelope_NotFinalBoard',
        Part.makeBox(100,74,pcb_thickness,App.Vector(-50,-37,pcb_bottom)))
# A datum stack gauge is explicitly not a component model.
feature('PotBodyDepthGauge_NotComponentModel',
        Part.makeBox(8,8,pot_body_lower_bound,App.Vector(-4,-4,pcb_top)))
result['trial_pot_body_zmax_mm'] = pcb_top+pot_body_lower_bound
result['trial_body_top_intrusion_mm'] = pcb_top+pot_body_lower_bound-top
# Portrait floorplan uses the long enclosure axis vertically. These are
# explicitly clearance envelopes, not fabricated component models.
def portrait(x, y):
    return App.Vector(y-60.4, x-47.75, 33.8)

knob_diameter = 25.4*25/32  # Davies manufacturer catalog 1100, dimension A.
knob_height = 25.4*15/32    # Conservative manufacturer catalog envelope.
pot_centers = [(x,y) for y in (22,83.6) for x in (22,47.75,73.5)]
for i,(x,y) in enumerate(pot_centers,1):
    feature('Davies1100ClearanceEnvelope%d'%i,
            Part.makeCylinder(knob_diameter/2,knob_height,portrait(x,y)))
# In portrait mapping, X/Y dimensions swap in the world XY plane.
dc = portrait(47.75,53.8)
feature('Adafruit5393BoardXYEnvelope', Part.makeBox(
    30.48,36.83,0.1,dc+App.Vector(-15.24,-18.415,0)))
feature('CliffEncoderKnobEnvelope',Part.makeCylinder(7,17.8,portrait(79.5,53.8)))
for i,x in enumerate((18,38,58,78),1):
    feature('UnselectedKeycap19p05Envelope%d'%i,Part.makeBox(
        19.05,19.05,0.1,portrait(x,107.7)+App.Vector(-9.525,-9.525,0)))
result['portrait_trial'] = {
    'panel_width_height_mm':[95.5,120.8],
    'davies_envelope_diameter_height_mm':[knob_diameter,knob_height],
    'pot_centers_mm':pot_centers,
    'display_board_center_mm':[47.75,53.8],
    'encoder_center_mm':[79.5,53.8],
    'keycap_centers_mm':[[x,107.7] for x in (18,38,58,78)],
    'knob_knob_edge_gap_mm':25.75-knob_diameter,
    'lower_knob_to_display_board_gap_mm':83.6-knob_diameter/2-(53.8+15.24),
    'encoder_knob_to_display_board_gap_mm':79.5-7-(47.75+18.415),
    'lower_knob_to_keycap_row_gap_mm':107.7-9.525-(83.6+knob_diameter/2),
    'keycap_selected':False,
    'status':'XY trial only; rejected by necessary Z-stack test',
}
result['landscape_trial'] = {
    'height_available_mm':95.5,
    'row_envelopes_plus_4mm_gaps_and_4mm_end_margins_mm':
        2*knob_diameter+30.48+19.05+3*4+2*4,
    'status':'Does not fit locked topology with these explicitly stated clearances',
}
doc.recompute()
doc.saveAs(str(OUT / 'hammond-stack-feasibility.FCStd'))
(ROOT / 'reports/hammond-stack-feasibility.json').write_text(
    json.dumps(result,indent=2)+'\n', encoding='utf-8')
print(json.dumps(result,indent=2))
