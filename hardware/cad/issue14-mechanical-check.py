"""Run inside FreeCAD via MCP execute_python or the Python console.

exec(compile(open(PATH, encoding='utf-8').read(), PATH, 'exec'))
_result_ = audit(App.getDocument('assembly'))
_result_ = search_engaged_phase(App.getDocument('assembly'))
_result_ = solution_study(App.getDocument('assembly'))  # CAD-only proposal
_result_ = local_pose_check(App.getDocument('assembly'), search)

The numerical search is a local, nominal screening, NOT a 6DOF impossibility
proof. It omits unregistered components and samples cylindrical sections.
No full path is certified by local screening. No KiCad file is edited.
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


def search_engaged_phase(doc, usb_margin=0.0, angles=None, pot_radius=5, encoder_radius=3.6,
                         jack_radius=6, master_radius=5, translation_bound=5, baseline=True):
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
    setback=0 if baseline or 'BeforeCompensation' not in doc.J2_NRJ4HF.PropertiesList else doc.J2_NRJ4HF.Placement.Base.y-doc.J2_NRJ4HF.BeforeCompensation.Base.y
    for y in (42,-15):
        for x in (-28,0,28):
            cylinders.append((np.array([x,y,31.8]), np.array([0,0,1]), np.array([1,0,0]), np.array([0,1,0]),4.5,pot_radius,np.array([x,y]),(31.8,33.8),'front',5))
    cylinders.append((np.array([32,12,30.8]),np.array([0,0,1]),np.array([1,0,0]),np.array([0,1,0]),3.5,encoder_radius,np.array([32,12]),(31.8,33.8),'front',7))
    cylinders.append((np.array([12.5,58+setback,16.2]),np.array([0,1,0]),np.array([1,0,0]),np.array([0,0,1]),5.6,jack_radius,np.array([12.5,16.2]),(ci,ci+2/math.cos(math.radians(1))),'rear',8.85))
    cylinders.append((np.array([-31,58+setback,12.7]),np.array([0,1,0]),np.array([1,0,0]),np.array([0,0,1]),4.5,master_radius,np.array([-31,12.7]),(ci,ci+2/math.cos(math.radians(1))),'rear',5))
    for b,a,u,v,radius,hole,center,planes,kind,length in list(cylinders):
        if kind == 'rear' and radius == 5.6:
            continue
        cylinders.append((b+a*length,a,u,v,3,hole,center,planes,kind,20-length))
    roofpoints = np.array([[x+dx,y+dy,31.8] for x in (-28,0,28) for y in (42,-15) for dx in (-6,6) for dy in (-6.7,6.7)])
    jack = doc.J2_NRJ4HF.Shape.copy()
    if baseline and 'BeforeCompensation' in doc.J2_NRJ4HF.PropertiesList:
        jack.Placement = doc.J2_NRJ4HF.BeforeCompensation
    shoulder = np.array([list(v.Point) for v in jack.Vertexes if abs(v.Point.y-(58+setback))<1e-6 and v.Point.x>21])
    master = np.array([[x,58+setback,z] for x in (-37,-25) for z in (6,22.7)])
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
        out += [np.where(mask,usb[:,0]+21.97+usb_margin,10),np.where(mask,-13.03-usb[:,0]+usb_margin,10),
                np.where(mask,usb[:,2]-19.25+usb_margin,10),np.where(mask,22.7+usb_margin-usb[:,2],10)]
        phase = []
        for b,a,u,v,radius,hole,center,planes,kind,length in cylinders:
            a = r@a
            generators = trans(b + radius*(circle[:,0,None]*u+circle[:,1,None]*v))
            normal = np.array([0,0,1]) if kind=='front' else np.array([0,1,slope])
            for c in planes:
                t=(c-generators@normal)/(a@normal)
                points=generators+t[:,None]*a
                section=points[:,[0,1]] if kind=='front' else points[:,[0,2]]
                radial = hole-np.linalg.norm(section-center,axis=1)
                # A generator intersects the wall only on its finite segment.
                out.append(np.maximum.reduce([radial, -t, t-length]))
                phase += [float(t.min()),float(length-t.max())]
            for t in (0,length):
                points=generators+t*a
                section=points[:,[0,1]] if kind=='front' else points[:,[0,2]]
                depth=points@normal
                out.append(np.maximum.reduce([hole-np.linalg.norm(section-center,axis=1),planes[0]-depth,depth-planes[1]]))
        clear=np.concatenate(out)
        return (clear,phase) if with_phase else clear-q[5]

    rows=[]
    for sign in (1,-1):
        previous=np.zeros(6)
        for angle in (angles if angles is not None else (0,0.25,0.5,0.75,1,1.5,2,3,4,6,8,12,16,20)):
            pitch=sign*angle
            candidates=[]
            for roll,yaw in [(0,0),(-1,-1),(1,1),(-1,1),(1,-1)]:
                seed=previous.copy(); seed[3:5]=[roll,yaw]; seed[5]=-2
                opt=minimize(lambda q:-q[5],seed,method='SLSQP',bounds=[(-translation_bound,translation_bound)]*3+[(-5,5)]*2+[(-20,1)],
                    constraints={'type':'ineq','fun':lambda q:constraints(q,pitch)},options={'maxiter':120,'ftol':1e-9})
                clear,phase=constraints(opt.x,pitch,True)
                candidates.append((float(clear.min()),opt,phase))
            margin,opt,phase=max(candidates,key=lambda v:v[0]); previous=opt.x
            rows.append({'pitch_deg':pitch,'xyz_translation_mm':opt.x[:3].tolist(),'roll_y_deg':float(opt.x[3]),
                         'yaw_z_deg':float(opt.x[4]),'sampled_min_clearance_mm':margin,
                         'generator_phase_min_mm':min(phase),'optimizer_success':bool(opt.success)})
    return {'pivot_mm':p.tolist(),'transform':'P + Rz(yaw) Ry(roll) Rx(pitch) (point-P) + translation',
            'bounds':'Translations +/-'+str(translation_bound)+' mm; roll/yaw +/-5 deg; five starts per pitch; 32 generators per section; finite lengths and end circles',
            'usb_margin_mm':usb_margin,
            'hole_radii_mm':{'pot':pot_radius,'encoder':encoder_radius,'jack':jack_radius,'master':master_radius},
            'baseline':baseline,'rear_y_offset_mm':setback,'translation_bound_mm':translation_bound,
            'scope':'Finite bushings AND shafts; end circles and wall-plane sections. Local screening only, not a global infeasibility or continuous-path certificate. Negative generator_phase_min means that generator constraint is inactive, not a failed pose.',
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


def _feature(doc, name, shape, label, group, color):
    obj = doc.getObject(name) or doc.addObject('Part::Feature', name)
    obj.Placement = App.Placement()
    obj.Shape = shape
    obj.Label = label
    obj.ViewObject.ShapeColor = tuple(float(c) for c in color)
    obj.ViewObject.Visibility = True
    group.addObject(obj)
    return obj


def solution_study(doc):
    """Build the local clamp/PCB study with closed rear apertures.

    The source shell and panel sketches are retained. All added dimensions are
    design parameters, not purchased hardware dimensions or production approval.
    """
    group = doc.getObject('InsertionSolution') or doc.addObject('App::DocumentObjectGroup','InsertionSolution')
    group.Label = 'Studio locale: sagoma e compensatori; aperture chiuse'
    if 'WedgeMinimum' not in group.PropertiesList:
        group.addProperty('App::PropertyLength','WedgeMinimum','Design')
        group.WedgeMinimum = 0.25
        group.addProperty('App::PropertyString','Scope','Design')
    group.Scope = 'Closed apertures only. Annular compensators and candidate PCB: nominal study; stiffness, tolerances and missing solids NOT validated'
    t = float(group.WedgeMinimum)
    slope = math.tan(math.radians(1))
    ci, co = 58.466024591, 60.466329247
    setback = t - (ci-slope*22.7-58)
    for name in ('J2_NRJ4HF','MasterBodyEnvelope','MasterBushing','MasterShaftEnvelope'):
        obj = doc.getObject(name)
        if 'BeforeCompensation' not in obj.PropertiesList:
            obj.addProperty('App::PropertyPlacement','BeforeCompensation','Study')
            obj.BeforeCompensation = obj.Placement
        obj.Placement = obj.BeforeCompensation
        obj.Placement.Base += App.Vector(0,-setback,0)

    shell = full_probe(doc)
    _feature(doc,'InsertionShellProposal',shell,'Scocca di studio: aperture posteriori CHIUSE',group,(0.75,0.78,0.83))

    outline = [(-38,-56),(38,-56),(43,-50),(43,50),(38,56),(-38,56),(-43,50),(-43,-50)]
    points = [App.Vector(x,y,22.7) for x,y in outline]
    pcb = Part.Face(Part.makePolygon(points+[points[0]])).extrude(App.Vector(0,0,1.6))
    _feature(doc,'CandidatePCB',pcb,'PCB candidata 1.6; sagoma CAD, foratura/pad non trasferiti',group,(0.12,0.5,0.28))
    for i,x in enumerate((-33,-11,11,33),1):
        # Full XY envelope throughout the known 18.5 mm total height is conservative.
        s=Part.makeBox(15.63,15.63,18.5,App.Vector(x-7.815,-48.315,21))
        _feature(doc,'CherryEnvelope'+str(i),s,'CHERRY inviluppo: 15.63x15.63; Z21..39.5 (3.3 pin +15.2)',group,(0.25,0.25,0.27))
    esp=Part.makeBox(18.4,19.6,3.35,App.Vector(-35.7,2.59,19.35))
    _feature(doc,'ESP32Envelope',esp,'ESP32-1U max18.4x19.6x3.35; posa U1 da PCB',group,(0.6,0.6,0.63))
    # A design keep-in volume, deliberately not a fabricated DAC board thickness.
    dac=Part.makeBox(30.48,20.32,12.7,App.Vector(-1.74,2.73,10))
    _feature(doc,'DACKeeIn',dac,'DAC: RISERVA Z10..22.7; stack reale NON validato',group,(0.3,0.55,0.7))

    def wedge(x0,width,z0,z1,y0,y1):
        pts=[App.Vector(x0,y0(z0),z0),App.Vector(x0,y1(z0),z0),
             App.Vector(x0,y1(z1),z1),App.Vector(x0,y0(z1),z1)]
        return Part.Face(Part.makePolygon(pts+[pts[0]])).extrude(App.Vector(width,0,0))

    records={}
    for name,x,z,width,height,hole,diameter,nut_height in (
            ('Jack',12.5,16.2,15.75,12.7,11.2,17.45,4.75),
            ('Master',-31,12.7,12,16.7,9.1,14,2)):
        x0 = 6.07 if name=='Jack' else x-width/2
        z0=10 if name=='Jack' else 6
        inside=wedge(x0,width,z0,z0+height,lambda z:58-setback,lambda z:ci-slope*z)
        bore=Part.makeCylinder(hole/2,12,App.Vector(x,55,z),App.Vector(0,1,0))
        inside=inside.cut(bore)
        _feature(doc,name+'InnerCompensator',inside,name+' compensatore interno 1 grado; min0.25',group,(0.9,0.65,0.2))
        nut_y=co-slope*(z-diameter/2)+t
        # The outer wedge replaces the flat washer; no open slot is permitted.
        outside=wedge(x-diameter/2,diameter,z-diameter/2,z+diameter/2,lambda z:co-slope*z,lambda z:nut_y).common(Part.makeCylinder(diameter/2,15,App.Vector(x,55,z),App.Vector(0,1,0))).cut(bore)
        _feature(doc,name+'OuterCover',outside,name+' compensatore anulare esterno, sostituisce rondella; custom',group,(0.9,0.65,0.2))
        if name=='Master':
            r=11/math.sqrt(3)
            pts=[App.Vector(x+r*math.cos(k*math.pi/3),nut_y,z+r*math.sin(k*math.pi/3)) for k in range(6)]
            nut=Part.Face(Part.makePolygon(pts+[pts[0]])).extrude(App.Vector(0,2,0)).cut(bore)
        else:
            nut=Part.makeCylinder(diameter/2,nut_height,App.Vector(x,nut_y,z),App.Vector(0,1,0)).cut(bore)
        _feature(doc,name+'SolutionNut',nut,name+' dado: inviluppo non filettato',group,(0.55,0.56,0.58))
        records[name]={'setback_mm':setback,'shoulder_y_mm':58-setback,'nut_seat_y_mm':nut_y,
                       'inner_min_mm':t,'inner_max_mm':t+height*slope,
                       'outer_bearing_min_mm':t,'outer_bearing_max_mm':t+diameter*slope,
                       'bushing_after_seat_mm':(63 if name=='Master' else 66.85)-setback-nut_y,
                       'beyond_nut_mm':(63 if name=='Master' else 66.85)-setback-nut_y-nut_height,
                       'scope':'Geometric axial budget only; useful thread, torque and cover stiffness unverified'}
    # Encoder shoulder support closes the known 1 mm gap without pulling the PCB.
    ring=Part.makeCylinder(6,1,App.Vector(32,12,30.8)).cut(Part.makeCylinder(3.6,1,App.Vector(32,12,30.8)))
    _feature(doc,'EncoderSupport',ring,'Encoder appoggio interno 1mm D12/D7.2 - custom da approvare',group,(0.9,0.65,0.2))
    washer=Part.makeCylinder(6,0.5,App.Vector(32,12,33.8)).cut(Part.makeCylinder(3.55,0.5,App.Vector(32,12,33.8)))
    r=10/math.sqrt(3)
    pts=[App.Vector(32+r*math.cos(k*math.pi/3),12+r*math.sin(k*math.pi/3),34.3) for k in range(6)]
    nut=Part.Face(Part.makePolygon(pts+[pts[0]])).extrude(App.Vector(0,0,2)).cut(Part.makeCylinder(3.5,2,App.Vector(32,12,34.3)))
    _feature(doc,'EncoderHardware',Part.makeCompound([washer,nut]),'PEC11R ferramenta nominale: SW10 x2 / rondella12x7.1x0.5',group,(0.6,0.6,0.63))
    for i in range(1,7):
        c=doc.getObject('FrontPotBushing'+str(i)).Placement.Base
        washer=Part.makeCylinder(7,0.5,App.Vector(c.x,c.y,33.8)).cut(Part.makeCylinder(4.55,0.5,App.Vector(c.x,c.y,33.8)))
        r=11/math.sqrt(3)
        pts=[App.Vector(c.x+r*math.cos(k*math.pi/3),c.y+r*math.sin(k*math.pi/3),34.3) for k in range(6)]
        nut=Part.Face(Part.makePolygon(pts+[pts[0]])).extrude(App.Vector(0,0,2)).cut(Part.makeCylinder(4.5,2,App.Vector(c.x,c.y,34.3)))
        _feature(doc,'PotHardware'+str(i),Part.makeCompound([washer,nut]),'Alps RKZ00023UA: riferimento reale M9, da approvare',group,(0.6,0.6,0.63))
    doc.recompute()
    return {'compensators':records,'pcb_outline_xy_mm':outline,'USB_width_lower_bound_mm':9.14,
            'USB_production_width':'9.14 + 2*(mounting_gap + placement_error + cut_error); unknown terms not set to zero for production',
            'excluded':['Display Z stack','DAC physical stack/retention (keep-in volume only)','keycaps (installed later)','pot/encoder clips and solder excess','antenna cable','RESET/BOOT and other unregistered electronics','compensator material, torque and strength'],
            'scope':'Closed rear holes; insertion remains unproved; no bottom slots permitted'}


def moving_shapes(doc):
    names=['J2_NRJ4HF','J1_USB4085','MasterBodyEnvelope','MasterBushing','MasterShaftEnvelope',
           'EncoderBodyEnvelope','EncoderBushingEnvelope','EncoderShaftEnvelope','EncoderSupport',
           'CandidatePCB','ESP32Envelope','DACKeeIn','JackInnerCompensator','MasterInnerCompensator']
    names += [prefix+str(i) for prefix in ('FrontPotEnvelope','FrontPotBushing','FrontShaft') for i in range(1,7)]
    names += ['CherryEnvelope'+str(i) for i in range(1,5)]
    return {n:doc.getObject(n).Shape.copy() for n in names}


def local_pose_check(doc, search, angle=2):
    """Native rejection witness for a baseline finite-cylinder search pose."""
    row=next(r for r in search['rows'] if r['pitch_deg']==angle)
    rotation=App.Rotation(App.Vector(0,0,1),row['yaw_z_deg']).multiply(App.Rotation(App.Vector(0,1,0),row['roll_y_deg'])).multiply(App.Rotation(App.Vector(1,0,0),angle))
    pivot=App.Vector(*search['pivot_mm'])
    placement=App.Placement(pivot-rotation.multVec(pivot)+App.Vector(*row['xyz_translation_mm']),rotation)
    margin=search['usb_margin_mm']
    wall=full_probe(doc).cut(Part.makeBox(8.94+2*margin,15,3.45+2*margin,App.Vector(-21.97-margin,55,19.25-margin)))
    radii=search.get('hole_radii_mm',{'pot':5,'encoder':3.6,'jack':6,'master':5})
    cuts=[Part.makeCylinder(radii['pot'],10,App.Vector(x,y,28)) for x in (-28,0,28) for y in (42,-15)]
    cuts += [Part.makeCylinder(radii['encoder'],10,App.Vector(32,12,28)),
             Part.makeCylinder(radii['jack'],15,App.Vector(12.5,55,16.2),App.Vector(0,1,0)),
             Part.makeCylinder(radii['master'],15,App.Vector(-31,55,12.7),App.Vector(0,1,0))]
    wall=wall.cut(Part.makeCompound(cuts))
    names=['J2_NRJ4HF','J1_USB4085','MasterBodyEnvelope','MasterBushing','MasterShaftEnvelope','EncoderBushingEnvelope','EncoderShaftEnvelope']
    names += ['EncoderBodyEnvelope']
    names += [prefix+str(i) for prefix in ('FrontPotEnvelope','FrontPotBushing','FrontShaft') for i in range(1,7)]
    collisions={}
    for name in names:
        obj=doc.getObject(name);shape=obj.Shape.copy()
        if search.get('baseline',True) and 'BeforeCompensation' in obj.PropertiesList:
            shape.Placement=obj.BeforeCompensation
        shape=shape.transformGeometry(placement.toMatrix())
        common=shape.common(wall)
        if abs(common.Volume)>1e-6:
            collisions[name]={'volume_mm3':common.Volume,'valid':common.isValid(),
                              'reverse_volume_mm3':wall.common(shape).Volume}
    return {'pose':row,'collisions':collisions,'scope':'This pose only. No assertion of global infeasibility.'}


def prepare_study_view(doc, section=None):
    """Prepare a native view. Capture after the GUI has returned to its event loop."""
    import FreeCADGui as Gui
    group=doc.getObject('SolutionViews') or doc.addObject('App::DocumentObjectGroup','SolutionViews')
    group.Label='Sezioni appoggi anulari; nessun percorso dichiarato'
    if section not in (None,'Jack','Master'):
        raise ValueError('section must be Jack, Master or None')
    if section:
        x=12.5 if section=='Jack' else -31
        clip=Part.makeBox(0.2,45,38,App.Vector(x-0.1,33,0))
        sources=['InsertionShellProposal',section+'InnerCompensator',section+'OuterCover',section+'SolutionNut']
        sources += ['J2_NRJ4HF'] if section=='Jack' else ['MasterBodyEnvelope','MasterBushing','MasterShaftEnvelope']
        names=[]
        for i,n in enumerate(sources):
            color=(0.75,0.78,0.83) if i==0 else ((0.9,0.65,0.2) if i in (1,2) else (0.3,0.5,0.7))
            obj=_feature(doc,section+'Section'+str(i),doc.getObject(n).Shape.common(clip),n+' sezione X'+str(x),group,color)
            names.append(obj.Name)
        orientation=(0.5,0.5,0.5,0.5)
    else:
        names=list(moving_shapes(doc))+['InsertionShellProposal','JackOuterCover','MasterOuterCover','JackSolutionNut','MasterSolutionNut','EncoderHardware','SAK023SkirtLimit']+['PotHardware'+str(i) for i in range(1,7)]
        doc.InsertionShellProposal.ViewObject.Transparency=70
        orientation=(0.4247082003,0.1759198966,0.3398511429,0.8204732386)
    for obj in doc.Objects:
        if obj.TypeId=='App::DocumentObjectGroup':obj.ViewObject.Visibility=True
    for obj in doc.Objects:
        if obj.TypeId!='App::DocumentObjectGroup' and hasattr(obj,'Shape'):
            obj.ViewObject.Visibility=obj.Name in names
    doc.recompute()
    Gui.activeDocument().activeView().setCameraOrientation(orientation)
    Gui.activeDocument().activeView().fitAll()
    Gui.updateGui()
