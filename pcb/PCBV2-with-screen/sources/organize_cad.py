"""Organize the variant documents into native, independently visible groups."""
import json
import FreeCAD as A
import FreeCADGui as G

definitions=[('Housing','01 Hammond - corpo e fondo'),('RearPlate','02 Pannello posteriore removibile'),
 ('MainPCB','03 PCB principale - ingombro provvisorio'),('Pots','04 Potenziometri frontali'),
 ('Encoder','05 Encoder - a destra del display'),('Keys','06 Pulsanti Cherry MX2A'),
 ('Display','07 Display Adafruit 5393'),('DAC','08 DAC Adafruit 6250'),
 ('Connectors','09 RCA, USB-C e Master'),('Electronics','10 ESP32 ed elettronica'),
 ('Hardware','11 Ferramenta'),('Verification','12 Ingombri di verifica'),
 ('Reference','13 Riferimenti originali e volumi di taglio')]
summary={}
for name in ('HammondPlacementCandidate','HammondRearPanelOption'):
    doc=A.listDocuments().get(name)
    if not doc:continue
    groups={}
    for key,label in definitions:
        g=doc.getObject('Group'+key) or doc.addObject('App::DocumentObjectGroup','Group'+key)
        g.Label=label;groups[key]=g
    for g in groups.values():
        for child in list(g.Group):
            if child.TypeId=='App::DocumentObjectGroup':g.removeObject(child)
    for o in doc.Objects:
        if not o.isDerivedFrom('Part::Feature'):continue
        n=o.Name;kind=getattr(o,'AssemblyGroup','')
        if n=='OriginalBody' or kind=='CUT_TOOL':key='Reference'
        elif n=='OriginalBottomCover' or kind=='ENCLOSURE':key='Housing'
        elif n=='RemovableRearPlate':key='RearPlate'
        elif kind=='PCB':key='MainPCB'
        elif kind.startswith('POT'):key='Pots'
        elif kind=='ENCODER':key='Encoder'
        elif kind.startswith('KEY'):key='Keys'
        elif kind in ('DISPLAY','U5_HEADER'):key='Display'
        elif kind in ('DAC','U6_HEADER'):key='DAC'
        elif kind in ('MASTER','USB','RCA'):key='Connectors'
        elif kind in ('MCU','ELECTRONICS','SERVICE'):key='Electronics'
        elif kind=='RESERVE':key='Verification'
        else:key='Hardware'
        if o not in groups[key].Group:groups[key].addObject(o)
        if key in ('Reference','Verification'):o.ViewObject.Visibility=False
    groups['Reference'].ViewObject.Visibility=False
    groups['Verification'].ViewObject.Visibility=False
    doc.recompute();doc.save()
    summary[name]={g.Label:len(g.Group) for g in groups.values()}
G.activeDocument().activeView().fitAll()
print(json.dumps(summary,indent=2))
