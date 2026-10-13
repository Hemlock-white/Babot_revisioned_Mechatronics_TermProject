# BaBot through-hole adaptation

This repository develops a hand-assemblable version of [Johan Link's BaBot](https://github.com/JohanLink/BABOT), a three-servo robot that balances a ball using an infrared sensor array. It preserves the downloaded original material, the current PCB and mechanical working files, an Arduino sketch copy, and the superseded design experiments.

## Start here

| Folder | Contents |
| --- | --- |
| [`hardware/`](hardware/) | Working mechanical STL copied from the original download. |
| [`PCB/`](PCB/) | Current KiCad electronics, grouped purchase BOMs, routing/validation notes, four-layer Base fabrication files, and supplied two-layer Plate Gerbers under review. |
| [`software/`](software/) | Working Arduino sketch; currently byte-for-byte identical to the upstream `main.ino`. |
| [`history/`](history/) | Earlier Base versions, two-layer routing experiments, and prior circuit notes. |
| [`original/`](original/) | Untouched upstream sketch/license plus original PCBWay, mechanical, schematic, and saved-page downloads. |
| [`docs/`](docs/) | [Development log](docs/develope_log.md), illustrated [original circuit lesson](docs/electronics_original.html) and [current THT circuit lesson](docs/electronics_tht.html), their Markdown sources, and a blank `hardware_explained.md`. |

Open the [four-layer Base KiCad project](PCB/KiCad/BaBot_Base_THT_4067_Module.kicad_pro) or the [two-layer Plate KiCad project](PCB/KiCad/BaBot_Plate_THT.kicad_pro) in KiCad 10. Keep their `BaBot_THT.pretty` footprint folder, symbol library, and library tables beside the project files. Use the [Base BOM](PCB/KiCad/BaBot_Base_THT_4067_Module_BOM.csv) and [Plate BOM](PCB/KiCad/BaBot_Plate_THT_BOM.csv) to plan one or five assemblies and mark parts on hand. The [Base Gerber ZIP](PCB/fabrication/BaBot_Base_THT_4layer_Gerbers.zip) is the four-layer fabrication package; read its [order notes](PCB/fabrication/BaBot_Base_THT_4layer/ORDER_NOTES.md) before ordering.

The Base in `PCB/` is routed and its last DRC had **0 violations and 0 unconnected items**. Its 2 A servo-power assumption and 1 oz inner-copper recommendation still need confirmation against the chosen servos and mechanical assembly. The Plate file currently in `PCB/KiCad/` is an **older unrouted copy**. New routed Plate Gerbers have been supplied, but their [fabrication review](PCB/fabrication/plate/REVIEW_NOTES.md) found empty solder-mask plots and missing drills. **Do not order the current Plate ZIP.** The routed Plate KiCad source is still needed for repair. The unfinished two-layer Base is in `history/`, not in the fabrication package.

The software folder is currently a copy for future changes. Open the two self-contained HTML circuit lessons in a browser for schematics, graphs, and interactive examples; their Markdown source guides remain alongside them. Software teaching is deferred until the owner's next requirements. Earlier Base/Plate circuit notes remain accessible in `history/notes/`.

## Credit and provenance

Original BaBot design and firmware: **Johan Link**, [BABOT GitHub repository](https://github.com/JohanLink/BABOT), [BaBot project site](https://ba-bot.com/), and [build article](https://www.hackster.io/johanlinkemail/babot-build-your-own-ball-balancing-robot-2c1a5a). The upstream repository states a **Creative Commons Attribution-NonCommercial 4.0** license; its original license is preserved in [`original/upstream/BABOT-main/LICENSE`](original/upstream/BABOT-main/LICENSE). Original PCBWay downloads and reference captures are identified in [`original/README.md`](original/README.md). This repository's local commits reconstruct development milestones from available files and KiCad snapshots; their commit dates are import dates, not claims about when the earlier engineering work originally happened.
