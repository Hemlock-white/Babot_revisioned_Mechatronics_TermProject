# Routed two-layer Plate: source file still needed

The owner supplied the routed Plate Gerbers as [`plate_THT_2layers_Gerbers.zip`](../plate_THT_2layers_Gerbers.zip) and unpacked copies in [`plate/`](../plate/). Their [review notes](../plate/REVIEW_NOTES.md) explain why the current ZIP must **not** be ordered: both solder-mask layers have no pad-opening graphics, and drill files are missing.

The editable routed `BaBot_Plate_v2THT.kicad_pcb` is still needed for a safe repair and re-export. The current `PCB/KiCad/BaBot_Plate_THT.kicad_pcb` is an older **unrouted** copy. The original SMD Gerbers in `original/pcbway/plate/` are a different board. This folder remains a placeholder for the corrected, validated two-layer Plate fabrication package.
