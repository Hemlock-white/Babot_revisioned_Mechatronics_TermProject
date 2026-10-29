# Four-layer Base fabrication package

`Gerbers/` contains the KiCad 10 Gerber set and metric Excellon plated/non-plated drill files for `BaBot_Base_THT_4067_Module.kicad_pcb`. Upload `../BaBot_Base_THT_4layer_Gerbers.zip` as the board fabrication file. This is the fully routed four-layer **Base**; the incomplete two-layer Base draft is in `history/`, and the routed two-layer Plate has its own [`plate_tht/`](../plate_tht/) package.

Specify a four-layer, 1.6 mm board. The layers are `F.Cu`, `In1.Cu`, `In2.Cu`, and `B.Cu`. The 5 V servo rail uses `In2.Cu`; choose **1 oz inner copper** to retain the design's provisional 2 A sizing assumption, and verify the actual combined servo stall current. This copper weight is a conservative recommendation, not a measured minimum; a 0.5 oz version has not been validated for the actual loads. Copper weight is selected in the fab order and is not encoded by Gerbers. Confirm the offered stack-up and copper weight with the fabricator before checkout.

The export includes top/bottom solder mask and silkscreen, board outline, `PTH.drl`, `NPTH.drl`, and the Gerber job file. The refreshed KiCad DRC report, `DRC_report.json`, has **0 violations and 0 unconnected items**. The BOMs are in `../../KiCad/`. Mechanical fit, module pin mapping, and power/thermal behavior still require a prototype check.
