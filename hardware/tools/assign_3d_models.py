"""Assign the project 3D models (hardware/cad/3dmodels) to board and local library footprints.

Run with KiCad's Python while the board is closed in KiCad:
    "C:/Program Files/KiCad/10.0/bin/python.exe" -I hardware/tools/assign_3d_models.py
"""
import os
import re

import pcbnew

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KICAD = os.path.join(ROOT, "kicad")
BOARD = os.path.join(KICAD, "ESP-DubSiren.kicad_pcb")
M = "${KIPRJMOD}/../cad/3dmodels/"

# footprint name -> (model path, offset xyz in mm), or a list of them. Rotation 0, scale 1 for all.
MODELS = {
    "Bourns_PTV111-3": [(M + "Bourns_PTV111-3420A.step", (0, 0, 0)),
                        (M + "Davies_1100_envelope_on_PTV111-3.step", (0, 0, 0))],
    "Bourns_PTV111-1": [(M + "Bourns_PTV111-1420A.step", (0, 0, 0)),
                        (M + "Cliff_K85M_CL170805B_envelope_on_PTV111-1.step", (0, 0, 0))],
    "Bourns_PEC11R-4120F-S0018": [(M + "Bourns_PEC11R-4120F-S0018.step", (0, 0, 0)),
                                  (M + "Same_Sky_SAK-023_on_PEC11R.step", (0, 0, 0))],
    "CK_PTS636SM43SMTR_LFS": (M + "CK_PTS636SM43SMTR_simplified.step", (0, 0, 0)),
    "Bel_0ZCG0110CF2B_1812": (M + "Bel_0ZCG0110CF2B_envelope.step", (0, 0, 0)),
    "MPS_MPM3612GLQ-33_LGA-8-1EP_3x3mm": (M + "MPS_MPM3612GLQ-33_envelope.step", (0, 0, 0)),
    # 2.50 mm SLxG1 insulator between carrier and DAC (hardware/README.md)
    "Adafruit_6250_ComponentFace": (M + "Adafruit_6250_PCM510x_no_jack.step", (0, 0, 2.5)),
    # Espressif footprint uses offset (-9, -9.75, 0) with pad 1 at y=-8.26; ours has pad 1 at y=-8.41
    "Espressif_ESP32-S3-WROOM-1U": (M + "vendor/ESP32-S3-WROOM-1U.STEP", (-9, -9.60, 0)),
    # standard KiCad footprint (board only): its library model file does not exist
    "SW_Cherry_MX_1.00u_PCB": (M + "CHERRY_MX2A-11NW_simplified.step", (0, 0, 0)),
}
LOCAL_LIBS = [os.path.join(KICAD, "footprints", d) for d in
              ("ESP-DubSiren-V2-Electrical.pretty", "ESP-DubSiren-V2-Mechanical.pretty")]


def models(name):
    v = MODELS[name]
    return v if isinstance(v, list) else [v]


def model_block(path, off, unit):
    a, b = unit, unit * 2
    return (f'{a}(model "{path}"\n'
            f'{b}(offset (xyz {off[0]} {off[1]} {off[2]}))\n'
            f'{b}(scale (xyz 1 1 1))\n'
            f'{b}(rotate (xyz 0 0 0))\n'
            f'{a})\n')


# Local library footprints: replace any model block, or add one before the closing parenthesis.
for lib in LOCAL_LIBS:
    for fn in os.listdir(lib):
        name = fn[:-len(".kicad_mod")]
        if name not in MODELS:
            continue
        p = os.path.join(lib, fn)
        s = open(p, encoding="utf-8").read()
        indent = "\t" if "\n\t(" in s else "  "
        s = re.sub(r"\n[ \t]*\(model \".*?\n[ \t]*\)\n(?=[ \t]*\)\s*$)", "\n", s, flags=re.S)
        blocks = "".join(model_block(*m, indent) for m in models(name))
        s = re.sub(r"\)\s*$", "", s.rstrip()).rstrip() + "\n" + blocks + ")\n"
        open(p, "w", encoding="utf-8", newline="\n").write(s)
        print("library", name)

board = pcbnew.LoadBoard(BOARD)
for fp in board.GetFootprints():
    name = str(fp.GetFPID().GetLibItemName())
    if name not in MODELS:
        continue
    fp.Models().clear()
    for path, off in models(name):
        m = pcbnew.FP_3DMODEL()
        m.m_Filename = path
        m.m_Offset = pcbnew.VECTOR3D(*off)
        m.m_Scale = pcbnew.VECTOR3D(1, 1, 1)
        m.m_Rotation = pcbnew.VECTOR3D(0, 0, 0)
        fp.Models().push_back(m)
        print("board", fp.GetReference(), path)
pcbnew.SaveBoard(BOARD, board)
