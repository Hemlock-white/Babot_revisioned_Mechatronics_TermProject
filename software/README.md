# Working firmware

[`main.ino`](main.ino) is currently a byte-for-byte copy of [Johan Link's upstream sketch](../original/upstream/BABOT-main/main.ino). Keep changes to this working copy here and leave the file in `original/` untouched. The present copy has not been bench-tested against the redesigned PCBs; servo calibration and IR sensing must be checked during prototype bring-up.

The [offline software navigator](../docs/BaBot_導覽.html) teaches the upstream sketch's architecture and control logic.

For a soldered Base PCB, use the separate [Base-only bring-up sketch](test_code_base/test_code_base.ino) and its [step-by-step test guide](test_code_base/README.md). It starts with all servo outputs inactive and includes a resistor-fixture method to verify the fixed 10 kΩ resistor at the RV1 pads and the mux/ADC path. The upstream `main.ino` remains unchanged.

For the assembled Plate, use [test_code_plate.ino](test_code_plate/test_code_plate.ino) and its [standalone wiring and optical test guide](test_code_plate/README.md). It runs on a 5 V Uno with a breadboard load resistor while the Base is being tested; a compile-time option later uses the Base's multiplexer and fixed load resistor for integrated optical checks.
