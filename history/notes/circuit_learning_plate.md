# BaBot Plate board: learn the circuit, then the redesign

This guide explains the original Plate's optical sensor array and emitter switch, then the current THT version in [`KiCad/BaBot_Plate_THT.kicad_pro`](../../PCB/KiCad/BaBot_Plate_THT.kicad_pro). It pairs with [`circuit_learning_base.md`](circuit_learning_base.md), which explains the controller, multiplexer, firmware, and servos. The current Plate is intentionally unrouted and untested.

The original editable CAD was unavailable. Connections were reconstructed from the supplied Plate Gerber flying-probe data and BOM, then cross-checked against the old schematic and firmware. Where the old schematic and manufacturing data disagree, this guide identifies the conflict rather than quietly choosing one.

## 1. What the Plate does

The Plate has two optical jobs. Twenty-one infrared LEDs shine toward a ball, and sixteen phototransistors measure light reflected back. The receivers are laid out as a 4×4 grid so firmware can estimate which part of the plate the ball is over. The Plate also contains a transistor switch for controlling LED power and a 20-way cable connector that carries the sensor signals and power to the Base.

```text
+5 V ── 21 × series resistor ── IR LED anodes
                                  IR LED cathodes ── MOSFET ── GND
                                                        ▲
Base MCU IR_LED_CONTROL ── gate resistor ───────────────┘

+5 V logic ── phototransistor collectors
              phototransistor emitters ── PT00…PT15 ── Base multiplexer/ADC
```

The IR emitters are in several rows around and between the receiver locations. A ball reflects some 940 nm light toward nearby sensors; the firmware compares the 16 measurements to estimate a two-dimensional position. The optical response depends on ball surface, height, tilt, ambient light, emitter current, sensor angle, and physical spacing. The sensor grid therefore needs real-world calibration, not just correct wiring.

## 2. Original Plate board

### IR emitters and their current-limiting resistors

`IR1`–`IR21` are Everlight IR204/H60 3 mm through-hole infrared LEDs. Each has one series resistor, `R1`–`R21`, and all LED return/cathode connections join the controlled `IR_CATHODES` node. The 21 resistors feed from the original `VCC-5V-2A` rail. A diode emits light only when forward current flows in its forward direction; it is not safe to drive directly from a 5 V rail without setting that current.

For a first resistor estimate, use Ohm's law:

```text
R = (V_supply − V_F − V_switch) / I_LED
```

With the manufacturing BOM's 330 Ω, a 5 V rail, an IR LED forward drop around 1.2 V, and a small MOSFET drop, the estimate is `(5 − 1.2 − 0.1) / 330 ≈ 11 mA` per LED. If all 21 are on together, this is about 0.23 A total. This is an estimate; forward voltage varies with current and temperature, so verify actual current and optical range. The old schematic gives 500 Ω for these resistors while the manufactured BOM gives 330 Ω. The reconstructed boards preserve the manufacturing BOM value, but the intended brightness/current needs a prototype measurement. ([Everlight IR204/H60 datasheet](https://en.everlight.com/wp-content/plugins/ItemRelationship/product_files/pdf/IR204-H60.pdf))

Why one resistor per LED? A single shared resistor would let current split unevenly as LED forward voltages vary. An individual resistor makes each current largely independent and predictable. Resistor power is `P = I²R`: at 11 mA and 330 Ω, each resistor dissipates about 0.04 W. A quarter-watt axial part provides a comfortable nominal margin, subject to actual current, enclosure temperature, and lead-spacing fit.

### Turning all emitters on and off

Original `Q17` is an AO3400A N-channel MOSFET. Its drain switches the common LED cathode bus; its source goes to ground; its gate receives `IR_LED_CONTROL` from the Base. Driving the gate high turns on a conductive channel between drain and source, completing the LED current path. Driving it low interrupts the path. This is a low-side switch: the MOSFET handles LED current while the MCU pin only charges/discharges the MOSFET's small gate capacitance.

`R22` (10 kΩ) pulls the gate down to ground when the Base is disconnected or starting up, preventing a floating gate from accidentally turning the LEDs on. Gate-to-source pulldowns are common whenever the controller pin can be high-impedance at reset. In firmware the emitters are off for the ambient measurement, then on for the active-light measurement; the Base subtracts the two readings to reject steady room light.

The old MOSFET is SMD, and the current THT design uses a TO-220 FQP30N06L with pin 1 gate, pin 2 drain, and pin 3 source (tab also drain). Its pin order differs from the AO3400A, so the conversion remaps the pins. It also adds `R23` at 220 Ω in series with the gate. A gate resistor limits brief peak current from the MCU while the gate capacitance charges and damps ringing; the 10 kΩ pulldown remains from gate to ground. The FQP30N06L datasheet specifies low on-resistance at 5 V gate drive, appropriate to an MCU-driven low-side switch at the modest emitter-array current. Check the package pinout against the exact purchased part before soldering. ([onsemi FQP30N06L datasheet](https://www.onsemi.com/download/data-sheet/pdf/fqp30n06l-d.pdf))

### Phototransistor receiver array

`Q1`–`Q16` are Würth Elektronik 1540031NC6090 through-hole phototransistors. The probe data show one transistor terminal connected to +5 V logic and the other to an individual `PT00`–`PT15` net. A phototransistor is a light-sensitive transistor: photons create charge carriers in its base region, allowing collector-emitter current to rise with received light. The signal is therefore a current first; the Base's input load converts that changing current into a voltage which the ADC can measure.

The receiver peak sensitivity is about 940 nm, which matches the infrared emitter wavelength. Visible darkness/black packaging filters some visible light but does not remove all ambient infrared. Each receiver's field of view and alignment matter: neighboring LEDs and sensors overlap optically, which helps form a smooth position estimate but can blur the sensor map. The datasheet gives a typical 60° viewing angle and peak sensitivity at 940 nm. ([Würth 1540031NC6090 datasheet](https://www.we-online.com/components/products/datasheet/1540031NC6090.pdf))

Notice that the Plate has no separate per-sensor pull-up resistor in its manufacturing BOM. The analog loading/adjustment is shared on the Base at `U1_SIG` by the original TPL0501 and `R18`; the latest Base revision uses a manually adjusted trimmer and `R18` instead. This is one reason a Plate-only sensor schematic can look incomplete. A general design alternative is to give every phototransistor its own collector load resistor, chosen so its dark-to-bright voltage span fits the ADC range. The BaBot's shared loading arrangement instead saves parts and is tuned as a bank. If redesigning it, measure one channel from dark to full illumination, check the 0–5 V input limits, and make sure one bright sensor cannot saturate or distort other channels through the shared path.

### Inter-board connection

The original `U1` is a 20-contact, 0.5 mm pitch FPC connector. Its signals include 16 sensor outputs `PT00`–`PT15`, `IR_LED_CONTROL`, logic power, switched emitter power, and ground. The pin numbers are not a simple “sensor 0, sensor 1…” sequence, so use the connection table and pin-1 marker rather than relying on visual left/right orientation. The cable also links the Plate's MOSFET gate to the Base controller, enabling synchronized emitter switching.

## 3. Current Plate board: THT conversion

The current layout retains all optical positions and the 20 signal/power connections, but changes the FPC connector into a 2×10 through-hole header and moves the small MOSFET to a larger through-hole transistor. All 21 current-limiting resistors are axial through-hole parts on the board underside; the board keeps emitters and receivers on the original side. This preserves the optical geometry while making hand soldering practical.

| Current item | What it does and what changed |
| --- | --- |
| `IR1`–`IR21` IR204/H60 | Same 21 through-hole emitters and original positions/polarity. Their anodes connect individually through their resistors to `+5V_SERVO`; cathodes share `IR_CATHODES`. |
| `R1`–`R21`, 330 Ω axial | Replace the original 0805 SMD resistors, one resistor per emitter, routed to the back side. The old schematic says 500 Ω; the manufacturing BOM and current BOM say 330 Ω. Measure LED current/brightness before finalizing any substitution. |
| `Q1`–`Q16` 1540031NC6090 | Same through-hole phototransistors and optical locations. Each collector is on `+5V_LOGIC`; each emitter exports its own `PTxx` signal to the Base. Match flat/lens orientation and pin mapping to the footprint and datasheet. |
| `Q17` FQP30N06L TO-220 | THT replacement for the AO3400A low-side MOSFET. Pin 1 = gate, pin 2 = drain, pin 3 = source, tab = drain. Confirm the physical part's front-view pinout; do not copy the old SOT-23 pad order. |
| `R22` 10 kΩ, `R23` 220 Ω | R22 holds the gate low at startup; R23 is a new series gate resistor between `IR_LED_CONTROL` and the MOSFET gate. |
| `U1` 2×10 header | Replaces the fine-pitch FPC socket. It mates to Base U3 with a straight-through 20-way cable, pin 1 to pin 1. Plate U1 is mounted on the underside in this layout; double-check orientation so PT channels do not reverse. |

This Plate uses separate `+5V_LOGIC` for the phototransistors and `+5V_SERVO` for the high-current IR emitter array. Grounds are common. The distinction prevents emitter current pulses from sharing the same positive feed as the analog receivers; a shared ground still provides the voltage reference for the MOSFET and sensor signals. Keep LED current routes short and wide enough, and keep the small `PTxx` analog signals away from the switched cathode bus and servo currents. See [`ROUTING.md`](../../PCB/ROUTING.md) for suggested starting widths.

### Reconstructing the optical measurement

The original firmware performs these steps about 30 times per second:

1. Turn `IR_LED_CONTROL` low so the emitter array is dark. Select and measure each mux channel to get the ambient value.
2. Turn the emitters on. Measure the same 16 channels again.
3. Subtract ambient from active-light readings, then smooth the results over time.
4. If the range among sensor values exceeds a threshold, calculate a weighted center. Stronger nearby reflections receive more weight.
5. Send estimated X/Y error to the PID controller on the Base; the controller moves the three servos to tilt the platform.

The core idea is synchronous detection. By changing the illumination deliberately and sampling both states, the circuit distinguishes reflected LED light from much steady background IR. It is a simple optical analogue of switching a known test signal on/off and comparing the response. Timing matters: wait for emitters and mux signals to settle, use the same sampling sequence in both states, and avoid strong flickering IR sources. Subtraction cancels common ambient levels, not saturation or fast changes.

## 4. Designing a similar sensing plate yourself

1. **Draw the physical grid first.** Decide sensor spacing, viewing direction, emitter positions, ball height, and the plate's optical barriers. Check that the ball reflects enough light to several sensors at every usable location.
2. **Choose matched wavelengths and parts.** Compare emitter peak wavelength to receiver sensitivity; 940 nm is the intended pair here. Verify lens angle, current, ambient-light response, and mounting height from datasheets.
3. **Set emitter current from worst cases.** Use the series-resistor equation with minimum supply, LED forward-voltage range, switch drop, and desired current. Estimate resistor dissipation and total current if every emitter is on. A real prototype should measure current and check temperature.
4. **Design the switch path.** Choose a low-side N-MOSFET whose on-resistance is specified at the MCU's actual gate voltage. Include a gate pulldown; add a small series gate resistor where useful. Make sure the controller and emitter grounds share a return reference and the MOSFET pinout matches the layout.
5. **Design the receiver voltage range.** Convert photocurrent to a voltage and choose the load based on dark/bright extremes, ADC rails, noise, and response time. Avoid ADC saturation. If sensors share a load or amplifier, check how channels interact when muxed.
6. **Verify channel maps electrically and optically.** Before fitting the ball, turn on one LED or cover one receiver and record which PT channel changes. Then test ambient-only, IR-off, and IR-on readings. A channel spreadsheet catches connector flips early.
7. **Keep power returns out of the measurement.** High-current LED edges create voltage drop and magnetic/electric noise. Route the emitter supply and cathode bus as a power loop; keep receiver signals together with a quiet ground reference and place bypassing near the connector/electronics.

The most instructive small experiment is to read each receiver in a dark room, with room lights on, with emitters off, and with emitters on. Plot raw ADC readings against sensor location. That reveals which changes come from the ball, background light, sensor orientation, and electrical loading before control software hides the pattern with filtering.

## Source references

- Original design and firmware: [JohanLink/BABOT repository](https://github.com/JohanLink/BABOT)
- IR emitter: [Everlight IR204/H60 datasheet](https://en.everlight.com/wp-content/plugins/ItemRelationship/product_files/pdf/IR204-H60.pdf)
- Phototransistor: [Würth Elektronik 1540031NC6090 datasheet](https://www.we-online.com/components/products/datasheet/1540031NC6090.pdf)
- THT MOSFET: [onsemi FQP30N06L datasheet](https://www.onsemi.com/download/data-sheet/pdf/fqp30n06l-d.pdf)
- Mux/ADC signal path: [TI CD74HC4067 datasheet](https://www.ti.com/lit/ds/symlink/cd74hc4067.pdf)
