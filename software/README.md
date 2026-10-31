# Working firmware

[`main/main.ino`](main/main.ino) is the working sketch based on [Johan Link's upstream sketch](../original/upstream/BABOT-main/main.ino). For the current Base + Plate + servo integration test, it runs only the active sensing/control path: button modes, assembly sweep, EEPROM offset loading, and interactive calibration are bypassed. The three offsets use the sketch's default value of 100. The ball threshold is temporarily 200 ADC counts; that value still needs to be checked against repeatable empty-plate and ball-present measurements. The sketch reports `BALL`/`NO BALL`, sensor spread, estimated center, and smoothed tilt at 115200 baud. The status LED is on only when the software classifies a ball. This test variant is not yet a calibrated balancing build.

The [offline software navigator](../docs/BaBot_導覽.html) teaches the unchanged upstream sketch's architecture and control logic; its line references do not describe this temporary working-sketch variant.

For a soldered Base PCB, use the separate [Base-only bring-up sketch](test_code_base/test_code_base.ino) and its [step-by-step test guide](test_code_base/README.md). It starts with all servo outputs inactive and includes a resistor-fixture method to verify the fixed 10 kΩ resistor at the RV1 pads and the mux/ADC path.

For the assembled Plate, use [test_code_plate.ino](test_code_plate/test_code_plate.ino) and its [standalone wiring and optical test guide](test_code_plate/README.md). It runs on a 5 V Uno with a breadboard load resistor while the Base is being tested; a compile-time option later uses the Base's multiplexer and fixed load resistor for integrated optical checks.

To compare the beam angle of a loose unknown IR LED, use the paired [Arduino Micro emitter and Arduino Uno receiver sketches](test_ir_angle/README.md). The independent breadboards need no electrical connection; the receiver records dark and lit readings at each angle.
