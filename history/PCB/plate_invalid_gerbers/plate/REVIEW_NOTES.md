# Routed Plate Gerber review (2026-10-13)

The owner supplied [`plate_THT_2layers_Gerbers.zip`](../plate_THT_2layers_Gerbers.zip), containing a two-copper-layer `BaBot_Plate_v2THT` export. The ZIP opens cleanly and contains top/bottom copper, top/bottom silkscreen, edge cuts, top/bottom mask, top/bottom paste, and a Gerber job file. **Do not order this ZIP yet.**

JLCPCB's message, “There is no both top& bottom solder mask in your file,” is confusing because the files are *named* `F_Mask.gbr` and `B_Mask.gbr`. Their contents explain it: each is only 482 bytes, with Gerber headers and `M02*`, but **zero draw/flash commands**. Neither defines any pad openings. A manufacturer would see effectively no solder-mask pattern on either side. The paste files are similarly empty; this is expected for a hand-assembled THT board and they can be omitted from a future fabrication ZIP.

The ZIP also contains **no Excellon drill file** (`.drl`) for component leads, header pins, mounting holes, or vias. This is an independent reason it is not ready for fabrication. Its `.gbrjob` metadata reports two copper layers, 1.6 mm thickness, and a **100.0456 × 101.5921 mm** outline. The size slightly exceeds a 100 × 100 mm price category if that matters to the chosen order.

The older, **unrouted** [`BaBot_Plate_THT.kicad_pcb`](../../KiCad/BaBot_Plate_THT.kicad_pcb) in this repository has 143 through-hole pads with `(layers "*.Cu")` and no `"*.Mask"`. That would produce missing mask openings if carried into the routed `v2THT` board. This is a strong hypothesis, **not confirmation**, because the routed `BaBot_Plate_v2THT.kicad_pcb` source is not in the workspace. Do not regenerate files from the older unrouted PCB or attempt to infer drill diameters from the Gerbers alone.

## Correct export from the routed KiCad board

1. Open the actual routed `BaBot_Plate_v2THT.kicad_pcb` and save a copy. Check each through-hole pad's layer set. It should include the appropriate copper **and F.Mask/B.Mask openings**; ordinary THT pads typically use `*.Cu` and `*.Mask`. Repair the board's pads and their library footprints so the correction survives a footprint update.
2. Run KiCad DRC and inspect the result, including unconnected items. Refill copper zones. Check the 20-way Plate header against the current Base pin map and confirm the board outline and dimensions.
3. In **File → Fabrication Outputs → Gerbers** (or **File → Plot**), plot F.Cu, B.Cu, F.Mask, B.Mask, F.Silkscreen, B.Silkscreen, and Edge.Cuts. Paste layers are unnecessary without a solder-paste stencil. Use **Generate Drill Files** to export plated through-hole and any non-plated drill files in Excellon format.
4. Open the exported stack in KiCad GerbView or JLCPCB's preview. Confirm visible holes/openings on **both** mask layers, drill holes aligned with every THT pad and mounting hole, a closed board outline, and the expected two copper layers. Package all Gerbers and drills in one ZIP and re-upload that new package.

KiCad's [fabrication-output instructions](https://docs.kicad.org/8.0/fi/getting_started_in_kicad/getting_started_in_kicad.html) list the copper, solder-mask, silkscreen, outline, and separate drill export; [JLCPCB's instructions](https://jlcpcb.com/help/article/how-to-generate-gerber-files-in-different-software) likewise say to include Gerbers and drills in the same archive.

The routed KiCad source is still needed to make and validate the corrected export. The current ZIP and unpacked files are retained unchanged as the submitted revision for comparison.
