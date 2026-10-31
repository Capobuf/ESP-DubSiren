"""Native KiCad DSN export with explicit project rule transfer; never routes or edits source.

Run with KiCad's bundled Python. Output directory must be new.
Only the exported DSN is adapted. KiCad remains the rule/geometry authority.
"""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path
import pcbnew as k


def parse(text):
    tokens=re.findall(r'"[^"\n]*"|\(|\)|[^\s()]+',text.replace('(string_quote ")','(string_quote DOUBLEQUOTE)'))
    stack=[]; roots=[]
    for t in tokens:
        if t=='(':
            n=[]; (stack[-1] if stack else roots).append(n); stack.append(n)
        elif t==')': stack.pop()
        else: stack[-1].append(t)
    assert not stack and len(roots)==1
    return roots[0]


def emit(n):
    if isinstance(n,str): return n
    return '('+' '.join(emit(x) for x in n)+')'


def child(n,name):
    return next(x for x in n if isinstance(x,list) and x[0]==name)


def export(board_path,out):
    board_path=board_path.resolve(); out=out.resolve(); out.mkdir(parents=True,exist_ok=False)
    project_path=board_path.with_suffix('.kicad_pro')
    assert not board_path.with_suffix('.kicad_dru').exists(), 'Custom rules require explicit transfer review'
    hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (board_path,project_path)}
    project=json.loads(project_path.read_text(encoding='utf-8'))
    manager=k.SETTINGS_MANAGER(); assert manager.LoadProject(str(project_path))
    b=k.LoadBoard(str(board_path)); b.SetProject(manager.GetProject(str(project_path)))
    assert b.GetCopperLayerCount()==2
    ds=b.GetDesignSettings()
    assert len(b.Zones())==5 and all(z.GetIsRuleArea() for z in b.Zones()), 'Expected only the approved M3/U2 keepouts'
    raw=out/'native.dsn'; assert k.ExportSpecctraDSN(b,str(raw))
    tree=parse(raw.read_text()); structure=child(tree,'structure'); rule=child(structure,'rule')
    classes={c['name']:c for c in project['net_settings']['classes']}
    assert set(classes)=={'Default','Power','USB'}, 'Review transfer for new classes'
    assert all(c['clearance']==classes['Default']['clearance'] for c in classes.values())
    clearance=classes['Default']['clearance']*1000
    # KiCad DSN exports a quarter-clearance SMD exception; project has no such exception.
    smd=next(x for x in rule if isinstance(x,list) and x[0]=='clearance' and ['type','smd_smd'] in x)
    smd[1]=str(clearance)
    # Internal Edge.Cuts contours are exported as signal-layer keepouts. Give them
    # a separate clearance class; ordinary M3/U2 keepout geometry is untouched.
    cutouts=[n for n in structure if isinstance(n,list) and n[0]=='keepout' and n[2][:2]==['polygon','signal']]
    assert len(cutouts)==16, 'Review changed mechanical contours before export'
    # Preserve the physical 0.5-mm rule despite KiCad's chord approximation and
    # 0.1-um DSN coordinate quantization. This margin exists only in the DSN.
    edge_um=ds.m_CopperEdgeClearance/1000
    approximation_um=math.ceil(ds.m_MaxError/1000+0.1)
    translated_edge=edge_um+approximation_um
    for n in cutouts: n.append(['clearance_class','cutout'])
    for kind in ('wire','via','smd','pin','area'):
        rule.append(['clearance',str(translated_edge),['type','cutout_'+kind]])
        rule.append(['clearance','0.1',['type','reserved_'+kind]])
    for kind in ('Power,Default','USB,Default'):
        rule.append(['clearance',str(translated_edge),['type','"cutout"_"'+kind+'"']])
        rule.append(['clearance','0.1',['type','"reserved"_"'+kind+'"']])
    rule.append(['clearance','0.1',['type','reserved_reserved']])
    # KiCad rule-area borders already describe the prohibited copper extent.
    areas=[n for n in structure if isinstance(n,list) and n[0]=='keepout' and n not in cutouts]
    assert len(areas)==9
    for n in areas: n.append(['clearance_class','reserved'])
    # KiCad has ALREADY inflated every circular NPTH by the project's hole
    # clearance. Verify this against native drills before adding one DSN quantum
    # (0.1 um). Exactly zero would hide overlaps in Freerouting's DRC, whose
    # shortfall check is strictly positive. A global hole override counts twice.
    images={n[1]:n for n in child(tree,'library') if isinstance(n,list) and n[0]=='image'}
    hole_count=0
    for component in child(tree,'placement'):
        if not isinstance(component,list) or component[0]!='component': continue
        image=images[component[1]]
        holes=[n for n in image if isinstance(n,list) and n[0]=='keepout']
        assert all(n[2][0]=='circle' for n in holes), 'Review noncircular package keepouts'
        for index,n in enumerate(holes): n[1]='npth'+str(index)
        for place in component[2:]:
            assert place[0]=='place'
            fp=b.FindFootprintByReference(place[1].strip('"'))
            expected=[]
            for pad in fp.Pads():
                if pad.GetAttribute()==k.PAD_ATTRIB_NPTH:
                    assert pad.GetDrillSize().x==pad.GetDrillSize().y
                    expected.extend([(pad.GetDrillSize().x+2*ds.m_HoleClearance)/1000]*2)
                elif pad.GetAttribute()==k.PAD_ATTRIB_PTH:
                    # PTH copper clearance + annulus must also satisfy drill clearance.
                    annulus=min(pad.GetSize().x-pad.GetDrillSize().x,pad.GetSize().y-pad.GetDrillSize().y)/2
                    assert annulus+k.FromMM(classes['Default']['clearance'])>=ds.m_HoleClearance
            assert sorted(float(n[2][2]) for n in holes)==sorted(expected), fp.GetReference()
            hole_count+=len(holes)
            for n in holes: place.append(['keepout',n[1],['clearance_class','reserved']])
    assert hole_count==54
    network=child(tree,'network')
    actual={}
    for n in network:
        if not isinstance(n,list) or n[0]!='class': continue
        name=n[1].strip('"')
        name={'kicad_default':'Default','Power,Default':'Power','USB,Default':'USB'}[name]
        r=child(n,'rule')
        width=float(child(r,'width')[1])/1000
        assert width==classes[name]['track_width'], (name,width)
        actual[name]={'track_width_mm':width,'clearance_mm':float(child(r,'clearance')[1])/1000}
    assert set(actual)==set(classes)
    dsn=out/'ESP-DubSiren.dsn'
    dsn.write_text(emit(tree).replace('(string_quote DOUBLEQUOTE)','(string_quote ")')+'\n')
    settings={
        'router.automatic_neckdown':False,'router.neck_width_um':0,
        'router.fanout.enabled':False,'router.optimizer.enabled':False,
        'router.autorouter.max_passes':8,'router.job_timeout':'00:09:50',
        'router.copper_to_edge_clearance_um':translated_edge,
        'router.hole_clearance_um':0,
        'router.clearance_tolerance_um':0,
    }
    args=['--'+name+'='+str(value).lower() for name,value in settings.items()]
    (out/'freerouting-arguments.json').write_text(json.dumps(args,indent=2))
    manifest={'source_sha256':hashes,'classes':actual,'cutout_count':len(cutouts),
              'native_keepouts':len(b.Zones()),'kicad_edge_clearance_um':edge_um,
              'dsn_approximation_margin_um':approximation_um,'settings':settings,
              'npth_keepouts_with_hole_clearance_already_in_geometry':hole_count,
              'reserved_geometry_extra_clearance_um':0.1,
              'kicad_hole_clearance_um':ds.m_HoleClearance/1000,
              'copper_layers':['F.Cu','B.Cu'],'source_unchanged':True,
              'handoff_validated':False,
              'required_validation':'Compare Freerouting connectivity and DRC with KiCad before routing; this exporter never runs the autorouter.'}
    for path,digest in hashes.items(): assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest
    (out/'transfer.json').write_text(json.dumps(manifest,indent=2))
    print(json.dumps(manifest,indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('board',type=Path); p.add_argument('output',type=Path)
    a=p.parse_args(); export(a.board,a.output)
