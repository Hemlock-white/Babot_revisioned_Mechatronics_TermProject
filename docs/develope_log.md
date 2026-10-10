# BaBot development log

This log records the development sequence supported by the available files and conversation. Exact dates for the early conversion steps were not recorded. The new Git commits reconstruct milestones from KiCad's saved snapshots and the present files; their timestamps are **repository import dates**, not original design dates.

## Original project and local source archive

- **2025 source downloads:** Johan Link's [BABOT repository](https://github.com/JohanLink/BABOT) supplied `main.ino`, its README, and its CC BY-NC 4.0 license. Original Base and Plate PCBWay Gerbers/BOMs, the combined mechanical STL, a schematic PDF, and saved reference pages were downloaded separately. They are preserved in [`original/`](../original/) without editing their content. The PCBWay download names indicate 2025-05-26 for the Plate and 2025-05-27 for the Base; these are file labels, not verified manufacturing dates.
- **Early THT reconstruction; date not recorded:** Editable KiCad Base and Plate layouts were reconstructed from the original manufacturing Gerbers, flying-probe data, BOMs, and schematic information. The first Base version used an Arduino Micro module, through-hole parts, and the older digital-pot arrangement. The earliest surviving KiCad local-history snapshot is represented by the repository's initial THT commit; the superseded Base files remain in [`history/PCB/initial_THT_Base/`](../history/PCB/initial_THT_Base/).
- **Module and component revision; date not recorded:** The Base multiplexer IC was changed to a CD74HC4067 breakout module. A saved intermediate Base revision is captured as its own Git commit. The digital-pot module was then replaced by a Bourns 3296W 100 kΩ, 25-turn trimmer. The Plate design uses through-hole IR parts, a 2×10 header, and an FQP30N06L switch with a 220 Ω gate resistor. These circuit choices still need physical validation.
- **Routing attempts; date not recorded:** A two-layer Base layout was attempted, but its last recorded DRC had six signal and one GND connection open. The draft and trial files are archived in [`history/PCB/`](../history/PCB/). A separate four-layer Base was completed and routed; its last KiCad DRC reported zero violations and zero unconnected items.
- **2026-10-10 — Base fabrication and purchase data:** Four-layer Base Gerbers and drill files were exported and packaged as [`PCB/fabrication/BaBot_Base_THT_4layer_Gerbers.zip`](../PCB/fabrication/BaBot_Base_THT_4layer_Gerbers.zip). Both BOMs were grouped by purchasable part and given one-board/five-board quantities, stock fields, checkboxes, and supplier links. The Base power path uses a provisional 2 A assumption and an ordering recommendation of 1 oz inner copper; the actual servo loading has not been measured.
- **2026-10-11 — Repository organization:** Original downloads were separated from working files, early KiCad milestones were committed in sequence, and superseded Base work was moved to `history/`. `software/main.ino` is currently unchanged from the available upstream copy; there is no separately modified firmware yet. Hardware and system behavior have not been bench-tested.

## Routed Plate import — placeholder

**Status:** The project owner reports that the newer **two-layer THT Plate has been wired and its Gerbers exported**, but those files have **not yet been provided in this workspace**. The Plate KiCad file currently under `PCB/KiCad/` is the older unrouted copy. Do not confuse the original SMD Plate Gerbers in `original/pcbway/plate/` with the missing new THT Plate export.

When the files arrive, record and fill in:

- Source board/project files and their revision: **pending**
- New Gerber and drill package location: [`PCB/fabrication/plate_tht/`](../PCB/fabrication/plate_tht/) — **pending**
- Import date and designer notes: **pending**
- KiCad DRC, board dimensions, and layer count verified from the imported files: **pending**
- Base–Plate 20-pin header mapping and mechanical fit check: **pending**
- Prototype electrical/optical test results: **pending**

## Teaching documents

[`hardware_explained.md`](hardware_explained.md) is intentionally blank. The software and electronics/circuit teaching pages have not been created; their scope awaits the owner's instructions. Previous circuit-learning drafts are preserved in [`history/notes/`](../history/notes/).
