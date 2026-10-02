"""Align the unmodified manufacturer RCJ-2123 STEP to the carrier footprint.

Run with FreeCAD's Python. Rigid transform only: no tessellation or rescaling.
Model X/Y use KiCad's 3D convention; model Y is opposite footprint-local Y.
"""
from pathlib import Path
import FreeCAD
import Part

models = Path(__file__).resolve().parents[1] / '3dmodels'
shape = Part.read(str(models/'SameSky_RCJ-2123.step'))
matrix = FreeCAD.Matrix()
matrix.A11, matrix.A12, matrix.A13, matrix.A14 = -1, 0, 0, .290298296874877
matrix.A21, matrix.A22, matrix.A23, matrix.A24 = 0, 0, 1, 2.80222558586811
matrix.A31, matrix.A32, matrix.A33, matrix.A34 = 0, 1, 0, 9.25535350182772
shape.transformShape(matrix)
assert shape.isValid() and len(shape.Solids) == 3
shape.exportStep(str(models/'SameSky_RCJ-2123_aligned.step'))
print(shape.BoundBox)
