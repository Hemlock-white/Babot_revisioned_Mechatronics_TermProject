# Routing widths — BaBot THT boards

The current Base in `KiCad/` is fully routed on four layers (F.Cu, In1.Cu, In2.Cu, B.Cu). The superseded [two-layer Base draft](../history/PCB/base_2layer_draft/) has six signal nets and one GND-pour connection open and must not be fabricated. The Plate copy in this repository is an older unrouted two-layer board; the owner's newer routed Plate is pending import. The four-layer Base power width was chosen using a **provisional 2 A total-load assumption** and **1 oz / approximately 35 µm copper on In2.Cu**, where its servo supply is routed. Specify 1 oz inner copper to preserve that design assumption: [JLCPCB says 0.5 oz is its default inner copper](https://jlcpcb.com/help/article/jlcpcb-copper-weight). This is a conservative recommendation, not a proven minimum. Check the servos' simultaneous stall current; the net name does not establish a current rating. Recalculate if the current or copper weight differs.

| Board / net or connection | Recommended width |
| --- | ---: |
| Both: ordinary signals, including PT00–PT15, U1_SIG, POT_H, U1_S0–U1_S3, U1_EN, BUTTON, LED_CONTROL/LED_K, SERVOA/B/C, MOSFET_GATE and IR_LED_CONTROL | **0.25 mm** |
| Both: +5V_LOGIC | **0.60 mm** |
| Base: +5V_SERVO common trunk from J1 | **2.00 mm target**. The four-layer reference uses In2.Cu with short 1.2–1.7 mm neckdowns. The two-layer draft runs on F.Cu/B.Cu with local widths as low as 0.8 mm; its current capacity is unverified. |
| Base: separate branch to each servo header | Check actual branch current and the short pad escape before assuming a 2 A rating |
| Plate: +5V_SERVO feeding all 21 IR resistors | **0.60 mm** |
| Plate: IR_CATHODES collector bus to Q17 drain | **0.60 mm** |
| Plate: each individual resistor-to-IR-anode net ($1N...) | **0.25 mm** |
| GND | Base has filled F.Cu and B.Cu pours; Plate should use pours on both sides. Verify the Base servo return under load and keep the Plate common return 0.60 mm or wider. |

A narrow pad escape may be necessary, but keep it short and widen immediately. Do not reduce a long servo supply or return route to signal width. Servo control wires SERVOA/B/C carry logic signals; the separate +5V_SERVO rail carries motor current.

## KiCad settings already applied

- Default signal width: 0.25 mm; copper clearance: 0.20 mm.
- Copper-to-board-edge minimum: Base 0.25 mm; Plate 0.30 mm. Aim for 0.50 mm where space allows. The Base's outer perimeter was adjusted to remain 99.95 mm wide while its three mounting holes stayed in place.
- Standard signal via: 0.80 mm diameter / 0.40 mm drill.
- Preset widths: 0.25, 0.30, 0.60, 1.00, 1.50 and 2.00 mm.
- Net classes: Default, Logic_IR_Power, Ground, and Base-only Servo_Power. Both current net names and schematic-prefixed names are covered.

The four-layer reference has no unconnected items and passes PCB DRC. Most of its +5V_SERVO path is 2.00 mm on In2.Cu, with about 10.5 mm total of short 1.2–1.7 mm neckdowns near pads. Its provisional 2 A sizing assumed 1 oz inner copper and still needs a loaded voltage-drop/temperature check. A 0.5 oz version has not been assessed for the actual servo loads. The two-layer draft is incomplete and its narrower servo sections need a current and voltage-drop review before it can replace the reference. If future edits put motor current through vias, use several parallel vias sized for the current; one 0.80/0.40 mm signal via is not a substitute for the wide trunk. Check thermal-relief spokes and narrow ground-pour necks as well as visible tracks.

For the Plate and any future Base edits, route short analog paths around U1_SIG, POT_H and the multiplexer first; keep them away from servo power and switched IR current. Keep C1 close to U1's supply pins. Route power next, then remaining signals, and refill/stitch GND. Keep servo return currents out of the narrow analog return path while maintaining a continuous common ground. SW1 has duplicated pad numbers for internally connected switch terminals: physically connecting each same-number pad pair avoids KiCad's unconnected-pad reports.

Do not route USB differential signals on these carrier boards: they stay on the assembled Arduino Micro. Do not connect +5V_LOGIC and +5V_SERVO together. Supply logic through the Micro USB port and the servos/IR emitters through Base J1.

Widths are starting values, not thermal certification. At 1 oz, a 50 mm long, 2 mm wide track is approximately 12 mΩ, or 25 mV drop at 2 A, before connectors and the return path. Verify temperature rise and voltage drop for actual lengths and currents with the [KiCad Track Width calculator](https://docs.kicad.org/9.0/en/pcb_calculator/pcb_calculator.html#track_width). Run DRC after any routing or ground-zone changes.
