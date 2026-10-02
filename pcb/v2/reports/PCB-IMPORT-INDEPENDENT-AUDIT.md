# Populated PCB import audit

Read-only independent review, 2026-10-02. Scope: the 64 library mismatches and 64 schematic-field parity warnings in `pcb-placement-drc.json`. No PCB, schematic or library mutation was performed. This report does not approve routing or fabrication.

## Geometry evidence

Fresh live MCP inventory returned 68 footprints. Fresh `get_component_pads` and `get_footprint_info` queries compared **all 282 pads across 20 distinct libraries**, transforming library coordinates for the actual footprint side and rotation. No differences were found in pad center, number, pad type, shape, X/Y size, or actual drill shape/size. The four board-only mounting holes were included. Nominal pad-center tolerance used for numeric comparison was 0.00001 mm. Therefore these warnings do not establish copper or drill distortion by the importer.

The readback is not a lossless footprint serialization. It materializes unset defaults: 179 non-roundrect pad shapes report a 0.25 corner ratio where the library reports null; 273 pads report default 0.2 chamfer ratio / zero offset where the library reports null. SMD pads report a zero-size drill where the library reports null. These are representation differences, not evidence of holes being drilled in SMD pads.

The board pad response has an actual internal inconsistency: U7 and other bottom-side SMD parts have `layers = [B.Cu, B.Mask, B.Paste]` while their nested `copper_layers` entry says `F.Cu`. Board-space pad locations follow the expected bottom-side reflection; DRC descriptions independently identify bottom-side copper. This points to a layer-reporting defect in `get_component_pads`' pad-stack representation, not proof of copper on the wrong side. Do not use that nested F.Cu label alone for routing decisions.

Fresh board graphics queries show the expected layer swaps and mirrored local coordinates. There are concrete 0.000001 mm (1 nm) numeric differences: SW1 silk Y = 12.064999 versus library 12.065; U7 polygon X = -1.690001 versus -1.69 and -0.400001 versus -0.4. They are physically negligible but may matter to exact library equality. The graphics API omits stroke width, fill, footprint text and attributes from its board response, so this audit **cannot prove that every remaining library mismatch is metadata-only**, or isolate one exact cause for all 64 warnings. No blanket waiver is justified from this partial serialization comparison.

## Field parity and tool limitations

The parity category is separate from geometric DRC. Its 64 field warnings report **48 missing `DesignStatus` fields and 16 missing `MPN` fields**. They are not missing-net or incorrect-pad assignments. Four additional parity findings identify the intentional board-only mounting-hole footprints as extra footprints.

A fresh `update_pcb_from_schematic` dry run returned `noop`, with zero footprint updates and zero pad reassignments, despite the recorded missing custom fields. Thus repeating that MCP sync currently does not repair the parity omission. The installed `edit_component` schema exposes only board, reference and value; it does not provide a custom-field write route.

The saved native refresh plan refuses SW1 because `property 'Datasheet' clause 'unlocked' is not supported losslessly`. This is an explicit tool coverage limitation, not a library error. A separate independent dry run limited to RV1, SW7 and U2 succeeded and planned refreshes, preserving position, rotation, side, pad nets, symbol path and identity. That plan was not applied. Its generic `changed_domains` list is not evidence of an actual copper difference.

## Safe resolution

1. The design owner can test a supported custom-footprint refresh on one reference, save, re-query pads/nets/position and rerun DRC. If the library warning disappears without geometry changes, it narrows the importer serialization issue. Keep this experiment bounded and preserve the current placement.
2. Native footprints require a lossless update route that supports their `unlocked` property; the present MCP handler refuses before applying it. Resolve that handler coverage or use an already-authorized native KiCad update workflow. Do not strip native properties, silently recreate a footprint, or suppress `lib_footprint_mismatch` globally.
3. Copy the exact schematic custom fields through a supported field-preserving sync/native workflow, or repair the MCP synchronizer to preserve them. Do not delete schematic engineering metadata merely to make parity warnings disappear.
4. After correction, save and rerun DRC with schematic parity, plus the 282-pad comparison. Confirm original nets, positions and layers remain intact. Library warnings and field warnings must be assessed separately.

Detailed MCP responses and the dry-run plans are retained in the task's temporary tools directory as `independent-import-audit.json` and `independent-parity-plan.json`; they are diagnostic working files rather than fabrication artifacts. This bounded report is the only repository file written during this follow-up audit.
