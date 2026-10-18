"""Run inside FreeCAD via MCP execute_python; no document mutation.

exec(compile(open(PATH, encoding='utf-8').read(), PATH, 'exec'))
_result_ = audit(App.getDocument('assembly'))
_result_ = search_engaged_phase(App.getDocument('assembly'))

The numerical search is a local, nominal screening, NOT a 6DOF impossibility
proof. It omits unregistered components and samples cylindrical sections.
"""
import math
import FreeCAD as App
import Part


def full_probe(doc):
    """All existing front sketch openings plus the three rear openings."""
    cuts = []
    for wire in doc.Openings.Shape.Wires:
        face = Part.Face(wire)
        face.translate(App.Vector(0, 0, -20))
        cuts.append(face.extrude(App.Vector(0, 0, 40)))
    cuts += [doc.getObject(n).Shape for n in
             ('J2_PanelHole', 'J1_PanelEnvelope', 'MasterPanelHole')]
    return doc.Hammond_29830PSLA.Shape.cut(Part.makeCompound(cuts))


def audit(doc):
    shell = doc.Hammond_29830PSLA.Shape
    inner, outer = shell.Faces[386], shell.Faces[409]
    slope = abs(outer.normalAt(0, 0).z / outer.normalAt(0, 0).y)
    ci = inner.CenterOfMass.y + slope * inner.CenterOfMass.z
    co = outer.CenterOfMass.y + slope * outer.CenterOfMass.z
    wall = lambda z: co - slope * z
    r, zj = 17.45 / 2, 16.2
    tangent_y = wall(zj - r)
    ring = Part.makeCylinder(r, 4.75, App.Vector(12.5, tangent_y, zj), App.Vector(0, 1, 0))
    ring = ring.cut(Part.makeCylinder(6, 4.75, App.Vector(12.5, tangent_y, zj), App.Vector(0, 1, 0)))
    probe = full_probe(doc)
    src = Part.Shape()
    src.read(str(__import__('pathlib').Path(doc.FileName).parent / 'sources/Hammond_29830PSLA.stp'))
    original = src.Solids[0].copy()
    matrix = App.Matrix(0, 0, 1, 0, -1, 0, 0, 0, 0, -1, 0, 3.8, 0, 0, 0, 1)
    original.transformShape(matrix)
    result = {
        'scope': 'Nominal drawing geometry; not measured hardware or deformation analysis',
        'source_shell': {
            'valid': shell.isValid(), 'faces': len(shell.Faces),
            'source_vertex_count':len(original.Vertexes), 'assembly_vertex_count':len(shell.Vertexes),
            'source_vertex_max_residual_mm':max((a.Point-b.Point).Length for a,b in zip(shell.Vertexes,original.Vertexes)),
            'source_volume_difference_mm3':abs(shell.Volume-original.Volume),
            'includes': 'Original roof, walls, lip, screw columns and ribs; lid/screws excluded from insertion',
            'vertex_z_range_mm': [min(v.Point.z for v in shell.Vertexes), max(v.Point.z for v in shell.Vertexes)],
            'bbox_warning': 'OCC BoundBox of trimmed surfaces can be inflated; not used as material extent'},
        'apertures': {
            'front_closed_wires': len(doc.Openings.Shape.Wires),
            'old_probe_volume_mm3': doc.MountingAperturesProbe.Shape.Volume,
            'complete_probe_volume_mm3': probe.Volume,
            'valid': probe.isValid(), 'solids': len(probe.Solids),
            'centres_open': {str(p): not probe.isInside(App.Vector(p[0], p[1], 32.8), 1e-7, False)
                             for p in [(0,12), (32,12), (-33,-40.5), (-11,-40.5), (11,-40.5), (33,-40.5)]}},
        'rear_rigid_contact': {
            'wall_equation': 'y + tan(angle)*z = c; mm',
            'angle_deg': math.degrees(math.atan(slope)), 'inner_c_mm': ci, 'outer_c_mm': co,
            'wall_axial_thickness_mm': co-ci,
            'jack_axis': [0,1,0], 'nut_plane_normal': [0,1,0],
            'nut_first_contact_y_mm': tangent_y,
            'nut_gap_bottom_center_top_mm': [tangent_y-wall(zj+dz) for dz in (-r,0,r)],
            'native_tangent_nut_common_mm3': ring.common(probe).Volume,
            'native_tangent_nut_distance_mm': ring.distToShape(probe)[0],
            'proof': 'Translation changes the gap offset, not its slope. Normals differ by 1 degree: full annular contact without a wedge, machining or verified deformation is impossible in this rigid model.',
            'scope_limit': 'Does not prove every insertion path impossible; does not quantify plastic nut compliance.',
            'outer_spotface_D17_45_max_depth_mm': 17.45*slope,
            'jack_inner_seat_z_span_mm': 12.7,
            'jack_inner_spotface_max_depth_mm': 12.7*slope,
            'jack_parallel_seats_remaining_axial_mm': co-ci-(zj+r-10)*slope,
            'jack_inner_spotface_plane_y_mm': ci-slope*10,
            'jack_required_shoulder_advance_y_mm': ci-slope*10-58,
            'master_reference_D14_outer_depth_mm': 14*slope,
            'master_body_envelope_height_mm': 16.7,
            'master_reference_inner_depth_mm': 16.7*slope,
            'master_reference_remaining_axial_mm': co-ci-(12.7+7-6)*slope,
            'machining_note': 'Geometric budgets only. Seats/strength/tool access need design approval; no machining applied.'},
        'encoder_stack': {
            'source': 'PEC11R.pdf pp2-3; SameSky-SAK-023.pdf p1',
            'pcb_top_mm':24.3, 'body_depth_mm':6.5, 'shoulder_z_mm':30.8,
            'bushing_length_mm':7, 'bushing_end_z_mm':37.8,
            'shaft_L_mm':20, 'shaft_tip_z_mm':50.8, 'roof_outer_z_mm':33.8,
            'shoulder_gap_to_roof_inner_mm':1,
            'nut_and_washer_mm':2.5, 'bushing_above_roof_mm':4,
            'sak_depth_from_skirt_mm':7.1, 'sak_inner_protrusion_mm':1.1,
            'sak_lowest_skirt_z_mm':43.7, 'sak_skirt_gap_mm':9.9,
            'sak_lowest_inner_protrusion_z_mm':42.6,
            'sak_push_travel_mm':0.5, 'sak_gap_at_pressed_stop_mm':9.4,
            'shaft_flat_length_mm':10,
            'alternative_skirt_clearance_example_mm':0.5,
            'alternative_max_shaft_L_mm':10.6,
            'alternative_shaft_shortening_mm':9.4,
            'flat_remaining_after_simple_cut_mm':0.6,
            'alternative_knob_required_cavity_depth_mm':16.5,
            'shorter_catalogue_L15_skirt_gap_mm':4.9,
            'scope': 'Rigid nominal lower bound assuming full insertion. Actual press fit may stop sooner, increasing the gap. No shaft modified, cap replaced, or spacer selected.'}
    }
    # Explicit design examples, not selected accessories or approved placements.
    inner_min = ci-slope*22.7-58
    setback = 0.5-inner_min
    master_outer_wedge_plane=wall(12.7-7)+0.5
    result['compensation_examples']={
        'status':'FOR REVIEW ONLY; no component, aperture or placement changed',
        'wedge_minimum_thickness_assumed_design_mm':0.5,
        'jack_and_master_rearward_axis_setback_into_box_mm':setback,
        'jack_inner_wedge_thickness_range_mm':[0.5,0.5+12.7*slope],
        'jack_outer_wedge_thickness_range_mm':[0.5,0.5+17.45*slope],
        'jack_nose_beyond_nut_envelope_mm':66.85-setback-(tangent_y+0.5+4.75),
        'master_inner_wedge_thickness_range_mm':[0.5,0.5+16.7*slope],
        'master_outer_D14_reference_wedge_range_mm':[0.5,0.5+14*slope],
        'master_bushing_beyond_outer_wedge_mm':63-setback-master_outer_wedge_plane,
        'master_reference_nut_washer_shortfall_mm':2.5-(63-setback-master_outer_wedge_plane),
        'master_spotface_inner_plane_y_mm':ci-slope*6,
        'master_spotface_shoulder_advance_mm':ci-slope*6-58,
        'master_spotface_bushing_projection_mm':ci-slope*6+5-wall(12.7+7),
        'note':'The 0.5 mm wedge minima are proposed manufacturing parameters, not source dimensions. Jack thread engagement, actual MASTER bearing area/hardware and strength remain unverified.'}
    return result


def search_engaged_phase(doc):
    """Optimize all translations, roll and yaw at increasing pitch, both signs.

    Exact transformed cylinder generators intersect actual roof/rear planes.
    32 azimuth samples are necessary constraints for nominal cylinders, not a
    continuous surface proof. Body boxes are conservative. Native OCC checks
    are required for candidate poses. No search outside this engagement phase.
    """
    import numpy as np
    from scipy.optimize import minimize
    from scipy.spatial.transform import Rotation
    p = np.array([12.5, 59.18340486684618, 16.2])
    slope = math.tan(math.radians(1))
    rear = doc.Hammond_29830PSLA.Shape.Faces[386].CenterOfMass
    ci = rear.y + slope*rear.z
    theta = np.arange(32)*2*np.pi/32
    circle = np.column_stack((np.cos(theta), np.sin(theta)))
    cylinders = []
    for y in (42,-15):
        for x in (-28,0,28):
            cylinders.append((np.array([x,y,31.8]), np.array([0,0,1]), np.array([1,0,0]), np.array([0,1,0]),4.5,5,np.array([x,y]),(31.8,33.8),'front',5))
    cylinders.append((np.array([32,12,30.8]),np.array([0,0,1]),np.array([1,0,0]),np.array([0,1,0]),3.5,3.6,np.array([32,12]),(31.8,33.8),'front',7))
    cylinders.append((np.array([12.5,58,16.2]),np.array([0,1,0]),np.array([1,0,0]),np.array([0,0,1]),5.6,6,np.array([12.5,16.2]),(ci,ci+2/math.cos(math.radians(1))),'rear',8.85))
    cylinders.append((np.array([-31,58,12.7]),np.array([0,1,0]),np.array([1,0,0]),np.array([0,0,1]),4.5,5,np.array([-31,12.7]),(ci,ci+2/math.cos(math.radians(1))),'rear',5))
    roofpoints = np.array([[x+dx,y+dy,31.8] for x in (-28,0,28) for y in (42,-15) for dx in (-6,6) for dy in (-6.7,6.7)])
    shoulder = np.array([list(v.Point) for v in doc.J2_NRJ4HF.Shape.Vertexes if abs(v.Point.y-58)<1e-6 and v.Point.x>21])
    master = np.array([[x,58,z] for x in (-37,-25) for z in (6,22.7)])
    shoulder = np.vstack((shoulder,master))
    usbpoints=np.array([list(v.Point) for v in doc.J1_USB4085.Shape.Vertexes if v.Point.y>59.9])

    def constraints(q,pitch,with_phase=False):
        r = Rotation.from_euler('xyz',[pitch,q[3],q[4]],degrees=True).as_matrix()
        trans = lambda points: (points-p)@r.T+p+q[:3]
        out = [31.8-trans(roofpoints)[:,2]]
        sp = trans(shoulder)
        out.append(ci-sp[:,1]-slope*sp[:,2])
        usb=trans(usbpoints)
        # Necessary mouth-vertex containment while the USB mouth is in the wall.
        # No cable clearance is invented; this uses the current nominal opening.
        mask=(usb[:,1]+slope*usb[:,2]>=ci)&(usb[:,1]+slope*usb[:,2]<=ci+2/math.cos(math.radians(1)))
        out += [np.where(mask,usb[:,0]+21.97,10),np.where(mask,-13.03-usb[:,0],10),
                np.where(mask,usb[:,2]-19.25,10),np.where(mask,22.7-usb[:,2],10)]
        phase = []
        for b,a,u,v,radius,hole,center,planes,kind,length in cylinders:
            a = r@a
            generators = trans(b + radius*(circle[:,0,None]*u+circle[:,1,None]*v))
            normal = np.array([0,0,1]) if kind=='front' else np.array([0,1,slope])
            for c in planes:
                t=(c-generators@normal)/(a@normal)
                points=generators+t[:,None]*a
                section=points[:,[0,1]] if kind=='front' else points[:,[0,2]]
                out.append(hole-np.linalg.norm(section-center,axis=1))
                phase += [float(t.min()),float(length-t.max())]
        clear=np.concatenate(out)
        return (clear,phase) if with_phase else clear-q[5]

    rows=[]
    for sign in (1,-1):
        previous=np.zeros(6)
        for angle in (0,0.25,0.5,0.75,1,1.5,2,3,4):
            pitch=sign*angle
            candidates=[]
            for roll,yaw in [(0,0),(-1,-1),(1,1),(-1,1),(1,-1)]:
                seed=previous.copy(); seed[3:5]=[roll,yaw]; seed[5]=-2
                opt=minimize(lambda q:-q[5],seed,method='SLSQP',bounds=[(-5,5)]*5+[(-20,1)],
                    constraints={'type':'ineq','fun':lambda q:constraints(q,pitch)},options={'maxiter':120,'ftol':1e-9})
                clear,phase=constraints(opt.x,pitch,True)
                candidates.append((float(clear.min()),opt,phase))
            margin,opt,phase=max(candidates,key=lambda v:v[0]); previous=opt.x
            rows.append({'pitch_deg':pitch,'xyz_translation_mm':opt.x[:3].tolist(),'roll_y_deg':float(opt.x[3]),
                         'yaw_z_deg':float(opt.x[4]),'sampled_min_clearance_mm':margin,
                         'generator_phase_min_mm':min(phase),'optimizer_success':bool(opt.success)})
    return {'pivot_mm':p.tolist(),'transform':'P + Rz(yaw) Ry(roll) Rx(pitch) (point-P) + translation',
            'bounds':'Translations +/-5 mm; roll/yaw +/-5 deg; five starts per pitch; 32 generators per section; USB mouth vertices in current minimum aperture',
            'scope':'Local engaged-bushing search, conservative bodies, nominal cylinders; negative optima are NOT a global infeasibility certificate. Negative generator_phase_min means the infinite-cylinder phase approximation is inapplicable there.',
            'rows':rows}


def native_candidate_screen(doc, search):
    """Screen interpolated candidate paths from seated pose; stop at first hit.

    No collision-free full-path assertion is made from sampled configurations.
    Cylinders are rebuilt from transformed base/axis, not copied and rotated.
    """
    wall=full_probe(doc)
    pivot=App.Vector(*search['pivot_mm'])
    outcomes=[]
    for sign in (1,-1):
        waypoints=[[0,0,0,0,0,0]]
        selected=[r for r in search['rows'] if r['sampled_min_clearance_mm']>=0 and
                  (r['pitch_deg']>0 if sign>0 else r['pitch_deg']<0)]
        if not selected:
            outcomes.append({'direction':sign,'status':'No nonzero-angle candidate passed the local screening; no native path asserted.'})
            continue
        for row in selected:
            waypoints.append(row['xyz_translation_mm']+[row['pitch_deg'],row['roll_y_deg'],row['yaw_z_deg']])
        hit=None; count=0
        for start,end in zip(waypoints,waypoints[1:]):
            for step in range(1,11):
                q=[a+(b-a)*step/10 for a,b in zip(start,end)]
                rotation=App.Rotation(App.Vector(0,0,1),q[5]).multiply(App.Rotation(App.Vector(0,1,0),q[4])).multiply(App.Rotation(App.Vector(1,0,0),q[3]))
                pos=lambda p:pivot+rotation.multVec(p-pivot)+App.Vector(*q[:3])
                placement=App.Placement(pivot-rotation.multVec(pivot)+App.Vector(*q[:3]),rotation)
                shapes={n:doc.getObject(n).Shape.transformGeometry(placement.toMatrix()) for n in ('J2_NRJ4HF','J1_USB4085','MasterBodyEnvelope')}
                for i in range(1,7):
                    b=doc.getObject('FrontPotBushing'+str(i)).Placement.Base
                    shapes['FrontPotBushing'+str(i)]=Part.makeCylinder(4.5,5,pos(b),rotation.multVec(App.Vector(0,0,1)))
                shapes['EncoderBushing']=Part.makeCylinder(3.5,7,pos(App.Vector(32,12,30.8)),rotation.multVec(App.Vector(0,0,1)))
                shapes['MasterBushing']=Part.makeCylinder(4.5,5,pos(App.Vector(-31,58,12.7)),rotation.multVec(App.Vector(0,1,0)))
                collision={n:s.common(wall).Volume for n,s in shapes.items()}
                count+=1
                if any(v>1e-6 or v < -1e-6 for v in collision.values()):
                    hit={'q_xyz_mm_pitch_roll_yaw_deg':q,'common_mm3':collision,
                         'note':'Positive intersection rejects this candidate. Negative volumes would be a kernel error, not clearance.'}
                    break
            if hit: break
        outcomes.append({'direction':sign,'waypoints_xyz_pitch_roll_yaw':waypoints,'tested_samples':count,'first_rejected_sample':hit})
    return {'scope':'Interpolated local extraction candidates with translations, yaw, roll and pitch, reversible to insertion. Screened with complete aperture shell and existing jack/USB models. Not a global search or complete populated PCB certification.', 'paths':outcomes}
