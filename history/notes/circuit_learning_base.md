# BaBot Base board: learn the circuit, then the redesign

This guide follows the signal from the optical plate into the controller and out to the three servos. It first explains the original manufactured Base board, then the current THT/module version in [`KiCad/BaBot_Base_THT_4067_Module.kicad_pro`](../../PCB/KiCad/BaBot_Base_THT_4067_Module.kicad_pro). The current section describes the revised connectivity, not a bench-tested robot.

The source of truth matters: the original editable CAD was unavailable. I reconstructed connections from the supplied Base Gerber flying-probe data and BOM, cross-checked against the old schematic PDF and the published firmware. The old schematic and BOM disagree in places; those conflicts are called out below. Current board files and assumptions are summarized in [`README.md`](../../PCB/README.md), with trace-width advice in [`ROUTING.md`](../../PCB/ROUTING.md).

## 1. What the Base board does

The Base is the robot's controller and actuator hub. It receives 16 analog light-sensor signals from the Plate through a 20-way cable, selects them one at a time, and gives the resulting analog voltage to a microcontroller ADC. Firmware estimates where the ball is, calculates how the platform should tilt, and sends three servo-control pulses. The Base also supplies power to the servos and switches the Plate's IR emitters.

```text
Plate's 16 phototransistors
        │ PT00…PT15 (analog)
        ▼
CD74HC4067 analog multiplexer ── U1_SIG ── Arduino analog input A0
        ▲                                  │
  four select bits                        ├─ adjustable load resistor to ground
                                           │
Button ── Arduino ── SERVOA/B/C ── three servos ── tilting platform
                  └─ IR_LED_CONTROL ── Plate MOSFET ── 21 IR emitters
```

The distinction between signal and power is essential. `SERVOA`, `SERVOB`, and `SERVOC` are low-current command signals. Servo motor current flows on the separate `+5V_SERVO` and `GND` connections. The IR LED current also comes from the power rail, not from a microcontroller pin.

## 2. Original Base board

### Power, connectors, and protection

The original design used two USB-C connectors, `U12` and `U19`, plus SS24AF Schottky diodes `U20` and `U21` and solder bridges `U22`–`U26` to configure/steer USB power and data paths. The four 5.1 kΩ resistors `R14`–`R17` connect the USB-C CC pins to ground; these advertise a sink/device connection to a USB-C source. The diodes help prevent one power input from feeding back into another. This is more complicated than the current design's split logic/servo supplies.

The net named `VCC-5V-2A` powers the three servo headers `H1`–`H3` and goes to the Plate. Treat “2A” as a rail name, not proof that every source, connector, copper track, or servo can safely carry 2 A. Servo stall current and power wiring determine the real requirement. The original `C5` is 10 µF across this rail and ground; it supplies a small local charge reserve for changing loads. It cannot replace a supply that is too weak or long power wiring.

The three 3-pin headers each carry signal, servo power, and ground. Original MCU nets are `SERVOA` on `H1`, `SERVOB` on `H2`, and `SERVOC` on `H3`. Servo control is a timed logic pulse; the motor's energy does not come from that signal wire.

### Reading 16 optical sensors with one ADC

The 16 Plate signals `PT00`–`PT15` connect to channels 0–15 of `U1`, a TI CD74HC4067. This IC is an analog switch bank: four digital address bits select one of 16 analog inputs and connect it to its shared output, `U1_SIG`. The Base routes that output to the ATmega32U4 analog input. Four MCU pins drive `U1_S0`–`U1_S3`; the firmware's multiplexer library changes the address, waits 600 µs for the signal to settle, then calls `analogRead()`.

Multiplexing saves 15 MCU analog inputs and a lot of connector wires. It does not digitize the signal: the mux passes an analog voltage, and the MCU ADC converts that voltage into a number. The switch's on-resistance, source impedance, stray capacitance, ADC sampling capacitor, settling time, and noise all affect accuracy. If you redesign this stage, check that the sensor node settles before each ADC sample and keep this path away from servo-current paths. The IC's enable is active low; original `R1` is 10 kΩ from `U1_EN` to ground, keeping the mux enabled. The TI datasheet states that a high enable input disables all channels. ([TI CD74HC4067 datasheet](https://www.ti.com/lit/ds/symlink/cd74hc4067.pdf))

The original U1 was `CD74HC4067SM96` in a small SSOP surface-mount package. Its function is retained in the redesign with an assembled through-hole breakout module. The module contains a CD74HC4067 chip and exposes its pins on 2.54 mm headers; verify the module's printed pin order against the PCB footprint before fitting it.

### Optical signal loading and digital adjustment

Original `U2` is a 100 kΩ TPL0501 digital potentiometer. The original net data show the top terminal `H` connected to `U1_SIG` through `R18`; the wiper `W` and low terminal `L` are grounded. In this connection it acts as a digitally adjustable resistance from the mux/ADC signal node to ground, with `R18` in series. The firmware writes code 230 at startup using three SPI-like wires (`U2_CS`, `U2_DIN`, `U2_SCLK`). A digital potentiometer is still a resistor in the analog circuit: software changes its tap; the tap does not itself measure or digitize the sensor.

Why put an adjustable load on the signal? Each phototransistor produces a light-dependent current. That current flowing through an effective load produces a voltage for the ADC. Changing the load changes the voltage response, much like changing gain/sensitivity. Too little resistance gives a small voltage swing; too much can push the ADC node toward a rail, make it noisy, or slow settling. The selected code is part of the analog design and should be measured with real sensors and lighting.

The current 500 Ω manufacturing BOM value for `R18` conflicts with 100 kΩ printed on the old schematic. The source manufacturing net data identify the connection, but do not tell us which resistor value performs best. The current BOM follows the manufacturing BOM and retains 500 Ω; validate sensor range before relying on it. The TPL0501 is a 256-step, 100 kΩ SPI digital potentiometer. Its analog terminals must stay within its supply rails and its terminal current limits. ([TI TPL0501 datasheet](https://www.ti.com/lit/ds/symlink/tpl0501-100.pdf))

### Controller, timing, and user controls

Original `U4` is an ATmega32U4-AU (44-pin surface-mount MCU). It runs the loop, reads the ADC and button, performs the control calculation, and generates servo pulses. `U9` is a 16 MHz crystal and `U8`/`U10` are its 22 pF load capacitors. A crystal oscillates mechanically at a frequency set by its cut; the MCU uses it as a stable clock. Its capacitive load must be compatible with the crystal and MCU oscillator. In the module redesign, these clock and USB-support circuits move onto the Arduino Micro module.

`SW1` connects logic +5 V to the `BUTTON` input while pressed, and `R7` (10 kΩ) pulls the input to ground while released. This is an active-high input. A pull resistor gives an MCU input a defined logic level instead of letting it float and randomly switch. The firmware recognizes single, triple, and quadruple presses for operating modes, and a long press during calibration advances the selected servo.

`U15` is the MCU-controlled status LED, with `R10` (500 Ω in the manufacturing BOM) limiting current. The second LED, `U14`, is a power indicator with `R11`; both LEDs are SMD in the original board. `C1` is 220 nF. `C2` is 1 µF across logic power and ground; `C3` is 1 µF on AREF, and `C4` is 1 µF on the ATmega32U4 UCAP pin. These are not interchangeable generic “extra capacitors”: AREF and UCAP have MCU-specific requirements. The remaining 1 µF capacitor values and connections come from original probe nets, while the old schematic/BOM should be consulted for the intended revision.

`R6` (10 kΩ) pulls the MCU reset line high for normal operation; a programmer or reset circuit can briefly pull it low to restart the MCU. `H4`/`H5` provide ISP programming/debug signals such as reset, clock, MISO, MOSI, power, and ground. The USB data lines from the original connectors reach the MCU through the solder-bridge arrangement. The obsolete schematic also shows an EEPROM (`U6`) that is absent from both the original manufacturing BOM and probe data; it is not part of the reconstructed physical board. The firmware does use the ATmega32U4's internal EEPROM for the three servo calibration offsets.

### Firmware turns readings into movement

The published firmware names the relevant pins: analog input `A0`, button `A1`, status LED `D8`, IR switch `D7`, servo outputs `D11/D10/D9`, mux select `D5/D13/D6/D12`, and digital-pot pins `D4/D1/D0`. Each measurement cycle reads all 16 channels with the IR emitters off (ambient reading), then reads all 16 again with them on. It subtracts `ambient` from `IR-on` for each sensor and smooths the result. This is synchronous ambient-light rejection: steady room light appears in both measurements and mostly cancels. Rapidly changing light, sensor mismatch, reflections, and electrical noise will not perfectly cancel.

If the sensor range exceeds a firmware threshold, the code estimates a weighted center from the 4×4 readings, then runs proportional-integral-derivative (PID) control to turn position error into platform roll and pitch. “P” responds to current error, “I” accumulates persistent error, and “D” responds to how quickly error is changing. Filtering helps smooth noisy sensor readings and servo commands, but adds delay. The firmware then uses inverse kinematics and the mechanism dimensions to convert desired tilt into three servo angles. The PCB supplies electrical connections; the control algorithm and mechanical geometry are just as important as the parts.

## 3. Current Base board: THT parts plus preassembled modules

The current design keeps the original sensor, mux, button, and three servo signal paths, but changes assembly and power entry. All carrier-board pads are through-hole. The Arduino Micro and mux breakout are factory-assembled modules; the adjustable load is a through-hole trimmer on the Base board.

| Current item | What it does and what changed |
| --- | --- |
| `U4` Arduino Micro A000053 module | Replaces the bare ATmega32U4, 16 MHz crystal, crystal capacitors, original USB-C ports, and ISP header requirement. It supplies the same mapped signals on headers and has onboard USB, clock, reset, regulator, and power LED circuits. This is a full Arduino Micro, not the visually similar Pro Micro. Pin mapping was checked against Arduino's schematic. ([Arduino Micro hardware and schematic](https://docs.arduino.cc/hardware/micro)) |
| `U1` CD74HC4067 breakout module | Same 16-to-1 analog mux, assembled on a headered board. The 16 PT lines, four address bits, active-low enable, and common analog output are preserved. The footprint maps physical module pads to the CD74HC4067 schematic pin numbers. |
| `RV1` Bourns 3296W-1-104LF | A 100 kΩ, 25-turn through-hole trimmer replaces the TPL0501, DIGI POT 2 Click, and J2 header. Pad 1 is `POT_H`; the wiper (pad 2) and far end (pad 3) are grounded. This makes a manually adjustable resistance from the mux signal, through `R18`, to ground. Tying wiper to the grounded end avoids an open signal path if the wiper contact momentarily fails. The trimmer is reversible; 25 turns simply gives finer control. With power disconnected, set about 10 kΩ between pad 1 and ground as a starting point. The available upstream firmware contains no digital-pot startup command, so the current software copy needed no removal. ([Bourns 3296 datasheet](https://www.bourns.com/docs/product-datasheets/3296.pdf)) |
| `R1`, `R7`, `R10`, `R18`, `C1`, `C5` | Through-hole versions retain mux-enable pull-down, button pull-down, status LED current limiting, the 500 Ω minimum series resistor in the adjustable load, mux/logic bypass capacitor, and 10 µF servo-rail bulk capacitance. Axial resistor wattage and capacitor pitch/size must fit the footprints. |
| `H1`–`H3` | Original servo connector locations retained. They carry a separate high-current power rail, ground, and logic command. |
| `U3` 2×10 header | Replaces the original 0.5 mm FPC connector with a 20-way through-hole inter-board cable. Check pin-1 orientation end to end. |
| `J1` two-pin power terminal | New regulated +5 V input for the servo and IR power rail. The logic rail is supplied through the Micro's USB; both supplies share ground, but their positive rails remain separate. This replaces original USB-C power entry, diodes, and solder bridges. |

The module approach makes assembly and debugging easier with ordinary through-hole soldering, but it adds height and mechanical fit risks. Check the Micro and mux module against the printed mechanism, screws, and underside. The requested two-layer Base is a draft with six signal connections and one ground-pour connection still open; it is not ready to fabricate. The retained, completed four-layer reference puts servo power on the second inner layer and assumes 1 oz inner copper and a real current/temperature check. The trimmer changes the analog signal amplitude, so log all 16 sensor channels while adjusting it; changing `BALL_THRESHOLD` in firmware may then be useful, but changing a code constant does not replace the physical adjustment. Do not connect the logic and servo +5 V rails together. The board has not been bench-tested.

### How to learn from and extend this circuit

When designing a similar sensor-and-actuator controller, work from behavior toward components:

1. **Define the measurement.** Decide what physical quantity changes the sensor output, the sensing area and spatial resolution, lighting range, sample rate, and acceptable error. Here, reflected 940 nm light changes phototransistor current; 16 receivers form a 4×4 position grid.
2. **Choose the sensor electrical mode.** Convert photocurrent into a voltage with a load resistor or transimpedance amplifier. Check dark current, bright current, ADC range, noise, ambient light, and source impedance. This design's sensor/mux/pot topology has no individual receiver resistor on the Plate; its adjustment is shared at the Base signal node, so measure it carefully.
3. **Budget channels and time.** Select a mux for channel count and voltage range, map channel order explicitly, allow settling after address changes, then sample. For another MCU, confirm its ADC input limits and sampling behavior.
4. **Separate energy from commands.** Size servo supply, connector, copper, return path, and capacitors for measured/stated stall current. Do not route motor current through MCU I/O. Share ground as a reference while keeping motor-current loops compact and away from small sensor signals.
5. **Choose the actuator and control law together.** A position estimate is useful only if it is calibrated into physical coordinates and the mechanism can reach the requested correction. Tune control gains with the real mass, geometry, servo speed, sampling delay, and safety stops.
6. **Plan for bring-up.** Start with power rails unloaded, verify polarity and common ground, test the ADC and mux one channel at a time, check IR on/off difference, then exercise one servo at a time before enabling balancing. Keep routing and documentation easy to inspect.

Useful first exercises: calculate approximate LED/servo rail current; log all 16 raw/ambient-subtracted ADC channels; cover one sensor at a time to verify PT channel order; and measure how turning RV1 changes signal amplitude. These teach more than copying footprints because they connect the schematic to physical measurements.

## Source references

- Original design and firmware: [JohanLink/BABOT repository](https://github.com/JohanLink/BABOT)
- Multiplexer: [TI CD74HC4067 datasheet](https://www.ti.com/lit/ds/symlink/cd74hc4067.pdf)
- Digital potentiometer: [TI TPL0501-100 datasheet](https://www.ti.com/lit/ds/symlink/tpl0501-100.pdf)
- Replacement MCU module: [Arduino Micro](https://docs.arduino.cc/hardware/micro) and [official schematic](https://docs.arduino.cc/resources/schematics/A000053-schematics.pdf)
- Replacement trimmer: [Bourns 3296W datasheet](https://www.bourns.com/docs/product-datasheets/3296.pdf)
