#!/usr/bin/env python3
"""Build two offline, illustrated HTML lessons from the electronics Markdown guides.

The SVGs are explanatory schematics, not KiCad fabrication schematics. No network,
JavaScript package, font download, or external CSS is required to read the output.
"""

from __future__ import annotations

from html import escape
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent


def e(value: object) -> str:
    return escape(str(value), quote=True)


def svg(body: str, title: str, height: int = 360) -> str:
    return (
        f'<svg class="drawing" xmlns="http://www.w3.org/2000/svg" '
        f'viewBox="0 0 1000 {height}" role="img" aria-label="{e(title)}">'
        f'<title>{e(title)}</title>{body}</svg>'
    )


def wire(x1: int, y1: int, x2: int, y2: int, cls: str = "") -> str:
    return f'<path class="wire {cls}" d="M{x1} {y1}L{x2} {y2}"/>'


def label(x: int, y: int, text: str, cls: str = "", anchor: str = "start") -> str:
    return f'<text class="svg-label {cls}" x="{x}" y="{y}" text-anchor="{anchor}">{e(text)}</text>'


def box(x: int, y: int, w: int, h: int, title: str, subtitle: str = "", cls: str = "") -> str:
    return (
        f'<rect class="svg-box {cls}" x="{x}" y="{y}" width="{w}" height="{h}" rx="16"/>'
        + label(x + w // 2, y + 30, title, "heavy", "middle")
        + (label(x + w // 2, y + 53, subtitle, "small", "middle") if subtitle else "")
    )


def resistor(x: int, y1: int, y2: int, title: str) -> str:
    h = y2 - y1
    pts = [(x, y1), (x - 12, y1 + h * .2), (x + 12, y1 + h * .4),
           (x - 12, y1 + h * .6), (x + 12, y1 + h * .8), (x, y2)]
    p = " ".join(f"{a},{b:g}" for a, b in pts)
    return f'<polyline class="wire resistor" points="{p}"/>' + label(x + 24, (y1 + y2) // 2 + 5, title, "small")


def ground(x: int, y: int) -> str:
    return (wire(x, y - 18, x, y) + wire(x - 17, y, x + 17, y)
            + wire(x - 11, y + 6, x + 11, y + 6)
            + wire(x - 5, y + 12, x + 5, y + 12))


def figure(kind: str, body: str, caption: str) -> str:
    return f'<figure class="figure" id="{e(kind)}">{body}<figcaption>{caption}</figcaption></figure>'


def flow_diagram(current: bool) -> str:
    if current:
        chips = [(18, "21 IR LEDs", "+5V_SERVO"), (212, "16 sensors", "+5V_LOGIC"),
                 (410, "4067 module", "one channel at a time"),
                 (620, "Arduino Micro", "ADC + control"), (827, "3 servos", "tilt the plate")]
    else:
        chips = [(18, "21 IR LEDs", "original Plate"), (212, "16 sensors", "photocurrent"),
                 (410, "bare 4067", "analog switch"),
                 (620, "ATmega32U4", "ADC + control"), (827, "3 servos", "tilt the plate")]
    parts = ['<defs><marker id="arrow-flow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0L10 5L0 10Z" fill="#527eb1"/></marker></defs>']
    for x, title, sub in chips:
        parts.append(box(x, 80, 160, 70, title, sub, "flow-box"))
    for x in [178, 372, 570, 780]:
        parts.append(wire(x, 115, x + 30, 115, "arrow"))
    parts += [
        label(196, 65, "reflected light", "small", "middle"),
        label(392, 65, "PT00–PT15", "small", "middle"),
        label(596, 65, "U1_SIG → A0", "small", "middle"),
        label(804, 65, "3 pulse signals", "small", "middle"),
        '<path class="wire feedback" d="M700 151V224H102V151" marker-end="url(#arrow-flow)"/>',
        box(355, 193, 258, 60, "IR switch Q17", "LEDs off / LEDs on", "feedback-box"),
        label(700, 194, "MCU controls illumination", "small", "end")
    ]
    caption = ('The light path is optical between emitter and receiver; all other links are electrical. '
               'The controller deliberately switches the LEDs so it can compare ambient and illuminated readings.')
    return figure("system-flow", svg("".join(parts), "BaBot optical sensing and servo-control loop", 280), caption)


def led_schematic(current: bool) -> str:
    p = ['<defs><marker id="light-out" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0 0L10 5L0 10Z" fill="#cc7b20"/></marker></defs>']
    p += [wire(110, 65, 585, 65, "power"), label(110, 45, "+5 V emitter rail", "power-text")]
    for x, n in [(185, "1"), (335, "2"), (485, "21")]:
        p += [wire(x, 65, x, 115, "power"), resistor(x, 115, 165, f"R{n} 330 Ω"),
              wire(x, 165, x, 209),
              f'<path class="wire" d="M{x-15} 211H{x+15}L{x} 236Z"/>',
              wire(x - 18, 238, x + 18, 238), wire(x, 238, x, 295),
              f'<path class="light-ray" d="M{x+20} 211L{x+48} 187M{x+22} 222L{x+50} 198"/>',
              label(x - 46, 226, f"IR{n}", "small")]
    p += [wire(185, 295, 625, 295, "power"), label(345, 321, "shared cathode bus", "small", "middle"),
          wire(625, 295, 625, 312, "power"),
          box(583, 312, 84, 48, "Q17", "MOSFET", "switch-box"),
          wire(625, 360, 625, 381, "power"), ground(625, 391),
          label(570, 354, "drain", "small", "end"), label(571, 382, "source", "small", "end")]
    gate_x = 750
    p += [wire(667, 336, gate_x, 336), wire(gate_x, 336, gate_x, 352),
          resistor(gate_x, 352, 388, "R22 10 kΩ"), ground(gate_x, 405)]
    if current:
        p += [wire(750, 336, 808, 336),
              '<polyline class="wire resistor" points="808,336 816,325 824,347 832,325 840,347 848,336"/>',
              wire(848, 336, 958, 336), label(827, 313, "R23 220 Ω", "small", "middle"),
              label(960, 341, "Micro D7", "power-text", "end")]
        title = "THT LED bank and FQP30N06L switch"
        caption = ('Each LED has its own 330 Ω resistor; 21 branches share Q17. R22 holds the gate off at reset. '
                   'The new R23 limits brief gate-charging current. This is a simplified electrical schematic, not a PCB pinout.')
    else:
        p += [wire(750, 336, 958, 336), label(960, 341, "MCU D7", "power-text", "end")]
        title = "Original LED bank and AO3400A switch"
        caption = ('Each LED has its own 330 Ω resistor; 21 branches share Q17. '
                   'R22 pulls the gate low until the MCU drives it high. This is a simplified electrical schematic, not a PCB pinout.')
    p += [label(548, 145, "× 21 branches total", "small")]
    return figure("led-schematic", svg("".join(p), title, 430), caption)


def sensing_schematic(current: bool) -> str:
    p = ['<defs><marker id="sense-arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0 0L10 5L0 10Z" fill="#527eb1"/></marker></defs>']
    p += [wire(65, 62, 390, 62, "logic"), label(65, 43, "+5V_LOGIC", "logic-text")]
    for x, y, name in [(125, 139, "PT00"), (235, 199, "PT01"), (345, 259, "PT15")]:
        p += [wire(x, 62, x, y - 44, "logic"),
              f'<circle class="sensor-symbol" cx="{x}" cy="{y-17}" r="27"/>',
              f'<path class="wire" d="M{x-12} {y-17}L{x+10} {y-17}M{x+10} {y-35}V{y+1}"/>',
              wire(x, y + 10, x, y + 23), label(x, y + 43, name, "small", "middle"),
              wire(x, y + 23, 460, y + 23, "signal")]
    p += [box(460, 99, 206, 188, "CD74HC4067", "16 → 1 analog switch", "mux-box"),
          label(560, 227, "S0–S3 choose a channel", "small", "middle"),
          wire(666, 194, 839, 194, "signal"),
          box(839, 159, 130, 72, "A0 ADC", "5 V domain", "adc-box"),
          label(744, 177, "U1_SIG", "signal-text", "middle"),
          wire(752, 194, 752, 244), resistor(752, 244, 289, "R18 500 Ω")]
    if current:
        p += [wire(752, 289, 752, 307), resistor(752, 307, 352, "RV1 0–100 kΩ"),
              ground(752, 370), label(766, 398, "wiper + end 3 → GND", "small")]
        title = "THT sensor path with a manual adjustable load"
        caption = ('The selected phototransistor supplies light-dependent current. R18 and RV1 convert it to a voltage at A0. '
                   'Other PT wires are disconnected by the mux while this channel is read. Simplified schematic; physical module pins differ.')
    else:
        p += [wire(752, 289, 752, 307), box(705, 307, 95, 55, "TPL0501", "100 kΩ", "pot-box"),
              ground(752, 380), label(813, 377, "W and L → GND", "small")]
        title = "Original sensor path with a digital-pot load"
        caption = ('The selected phototransistor supplies light-dependent current. R18 and the TPL0501 load convert it to a voltage at A0. '
                   'The TPL0501 control pins exist on the board, but the archived sketch does not set a known tap. Simplified schematic.')
    return figure("sensing-schematic", svg("".join(p), title, 420), caption)


def timing_diagram() -> str:
    p = [wire(105, 42, 105, 285, "axis"), wire(105, 285, 950, 285, "axis"),
         label(72, 65, "voltage", "small", "middle"), label(942, 311, "time →", "small", "end"),
         '<path class="grid-line" d="M105 235H950M105 176H950M105 112H950"/>',
         '<path class="switch-step" d="M112 252H440V220H900"/>',
         '<path class="ambient-step" d="M112 214H440V214H900"/>',
         '<path class="sensor-step" d="M112 214H440V132H900"/>',
         wire(440, 69, 440, 284, "divider"),
         label(275, 57, "IR OFF: ambient scan", "heavy", "middle"),
         label(685, 57, "IR ON: illuminated scan", "heavy", "middle"),
         label(786, 119, "ambient + reflection", "signal-text"),
         label(786, 202, "steady ambient", "small"),
         label(786, 245, "LED control", "small"),
         '<path class="difference" d="M745 132H760M753 132V214M745 214H760"/>',
         label(765, 175, "subtract = reflected IR", "small"),
         label(210, 340, "Each half contains 16 sequential mux selections; the sketch waits 600 µs after each selection.", "small")]
    return figure("timing", svg("".join(p), "Conceptual IR-off and IR-on sensor readings over time", 365),
                  'Conceptual voltage trace, not measured data. Background light is present in both scans; reflected light from the robot’s emitters appears in the ON scan. Subtraction removes steady ambient, but cannot undo saturation or rapidly changing light.')


def power_diagram(current: bool) -> str:
    p = ['<defs><marker id="power-arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0L10 5L0 10Z" fill="#bd6b3c"/></marker></defs>']
    if current:
        p += [box(55, 55, 184, 72, "Micro USB", "logic 5 V source", "logic-box"),
              box(55, 214, 184, 72, "J1", "regulated external 5 V", "power-box"),
              box(395, 40, 210, 108, "+5V_LOGIC", "Micro • mux • sensors", "logic-box"),
              box(395, 198, 210, 108, "+5V_SERVO", "3 servos • 21 IR LEDs", "power-box"),
              box(760, 117, 184, 75, "common GND", "voltage reference", "ground-box"),
              wire(239, 92, 395, 92, "logic arrow"), wire(239, 250, 395, 250, "power arrow"),
              wire(220, 127, 260, 127, "ground-return"),
              wire(260, 127, 260, 341, "ground-return"),
              wire(148, 286, 148, 341, "ground-return"),
              wire(148, 341, 852, 341, "ground-return"),
              wire(852, 192, 852, 341, "ground-return"),
              label(500, 179, "positive rails stay separate", "heavy", "middle"),
              label(500, 328, "shared ground return", "small", "middle"),
              label(500, 383, "C1 220 nF bypasses logic • C5 10 µF buffers the servo rail locally", "small", "middle")]
        title = "Current two-supply architecture with one ground reference"
        caption = ('The Micro’s USB powers sensitive logic. J1 supplies servo and emitter current. The grounds join so D7, A0, and servo command voltages have a common reference. C1 and C5 address different transient frequencies and do not replace adequate supply wiring.')
    else:
        p += [box(30, 56, 190, 80, "USB-C inputs", "U12 + U19", "logic-box"),
              box(30, 218, 190, 75, "CC pull-downs", "4 × 5.1 kΩ to GND", "logic-box"),
              box(360, 56, 220, 80, "diodes + bridges", "U20–U26", "power-box"),
              box(773, 56, 190, 80, "5 V rails", "logic + power", "power-box"),
              wire(220, 96, 360, 96, "arrow"), wire(580, 96, 773, 96, "arrow"),
              wire(125, 136, 125, 218, "logic"), ground(125, 311),
              box(300, 218, 225, 75, "bare ATmega32U4", "crystal • reset • caps", "logic-box"),
              box(650, 218, 250, 75, "servos + LED bank", "current pulses", "power-box"),
              wire(868, 136, 868, 183, "power"), wire(868, 183, 775, 183, "power"), wire(775, 183, 775, 218, "power"),
              wire(868, 183, 412, 183, "logic"), wire(412, 183, 412, 218, "logic"),
              label(500, 355, "CC pins signal a USB-C sink; they do not carry the 5 V power path.", "small", "middle"),
              label(500, 378, "“VCC-5V-2A” is a net label, not a proven current rating.", "small", "middle")]
        title = "Original USB-C entry and bare-controller support"
        caption = ('The CC resistors advertise a USB-C sink. Diodes and solder bridges steer and isolate connection options. The bare MCU needs its own clock, reset, capacitors, and programming connections. The actual current limit must be checked beyond the net name.')
    return figure("power-map", svg("".join(p), title, 402), caption)


def connector_diagram() -> str:
    p = [label(500, 36, "U3 Base ↔ U1 Plate: straight-through 20-way cable", "heavy", "middle")]
    for i in range(1, 21):
        col, row = (i - 1) % 2, (i - 1) // 2
        x, y = 122 + col * 440, 54 + row * 30
        signal = f"PT{i - 1:02d}" if i <= 16 else {17: "IR_LED_CONTROL", 18: "+5V_SERVO", 19: "+5V_LOGIC", 20: "GND"}[i]
        cls = "pin-power" if i in (18, 19, 20) else "pin-control" if i == 17 else "pin-sensor"
        p += [f'<rect class="{cls}" x="{x}" y="{y}" width="308" height="25" rx="6"/>',
              label(x + 12, y + 18, f"{i:02d}", "pin-number"), label(x + 70, y + 18, signal, "pin-label")]
    p += [label(500, 375, "Pin 1 must meet pin 1; a flipped ribbon can put power on a sensor line.", "small", "middle")]
    return figure("connector", svg("".join(p), "Current 20-way Base–Plate connector map", 405),
                  'The two columns represent connector pin numbers, not a physical view from the top or bottom. Use the KiCad footprints and a continuity meter to establish physical orientation before power-up.')


def mask_diagram() -> str:
    p = [label(250, 45, "Required THT pad", "heavy", "middle"), label(750, 45, "Current Plate export", "heavy", "middle")]
    for x in (80, 580):
        p += [f'<rect class="board-core" x="{x}" y="115" width="340" height="115" rx="5"/>',
              f'<rect class="copper-pad" x="{x+120}" y="108" width="100" height="12"/>',
              f'<rect class="copper-pad" x="{x+120}" y="225" width="100" height="12"/>',
              f'<rect class="drill-hole" x="{x+162}" y="104" width="16" height="137"/>']
    p += ['<path class="mask-line" d="M80 103H190M310 103H420M80 242H190M310 242H420"/>',
          '<path class="mask-line" d="M580 103H920M580 242H920"/>',
          label(250, 276, "mask has an opening; drill is exported", "small", "middle"),
          label(750, 276, "mask files draw no openings; drill missing", "small", "middle"),
          label(500, 335, "Cross-section is conceptual: copper, mask, substrate, and hole are not to scale.", "small", "middle")]
    return figure("mask-cross-section", svg("".join(p), "Through-hole pad and solder-mask opening compared with current Gerber export", 360),
                  'A Gerber filename alone does not create a mask opening. The supplied F.Mask and B.Mask contain zero pad graphics, and the ZIP has no drill file. Repair and re-export from the routed KiCad source before ordering.')


def load_simulator(current: bool) -> str:
    name = "RV1 trimmer" if current else "TPL0501 tap (conceptual)"
    body = '''<div class="lab" data-lab="load">
      <div class="lab-controls">
        <label>Adjustable resistance <output data-out="r">10.0 kΩ</output>
          <input data-input="r" type="range" min="0" max="100000" step="250" value="10000"></label>
        <label>Example photocurrent <output data-out="i">40 µA</output>
          <input data-input="i" type="range" min="0" max="150" step="1" value="40"></label>
        <div class="readout"><strong data-out="v">0.42 V</strong><span>simplified A0 voltage</span></div>
        <div class="readout"><strong data-out="adc">86 / 1023</strong><span>illustrative 10-bit ADC count</span></div>
      </div>
      <svg class="drawing graph" viewBox="0 0 650 290" role="img" aria-label="Conceptual sensor voltage versus photocurrent graph">
        <title>Conceptual sensor voltage versus photocurrent</title>
        <path class="grid-line" d="M72 30H612M72 81H612M72 132H612M72 183H612M72 234H612"/>
        <path class="axis" d="M72 30V234H612"/>
        <text x="46" y="36" class="svg-label small">5 V</text><text x="42" y="238" class="svg-label small">0 V</text>
        <text x="550" y="265" class="svg-label small">photocurrent →</text>
        <text x="80" y="48" class="svg-label small">ADC ceiling</text>
        <path data-curve="load" class="curve" d=""/>
        <circle data-point="load" class="curve-point" cx="72" cy="234" r="7"/>
      </svg>
      <p class="fine-print">Model: V ≈ I × (500 Ω + R), limited to 5 V. This is an teaching approximation; real phototransistors, mux resistance, ADC loading and supply headroom change the measured result.</p>
    </div>'''
    caption = (f'Move the {e(name)} and hypothetical photocurrent sliders. A larger load makes weak light easier to see, '
               'but reaches the 5 V ADC ceiling sooner. The graph is illustrative, not a measured BaBot transfer curve.')
    return figure("load-simulator", body, caption)


def mux_lab() -> str:
    channels = "".join(f'<span data-channel="{n}">{n:02d}</span>' for n in range(16))
    body = ('<div class="mux-lab" data-lab="mux"><label>Choose a receiver channel '
            '<output data-out="channel">06</output><input data-input="channel" type="range" min="0" max="15" step="1" value="6"></label>'
            f'<div class="mux-channels">{channels}</div>'
            '<div class="mux-address"><span>S3 S2 S1 S0</span><strong data-out="bits">0 1 1 0</strong>'
            '<span>Only PT<output data-out="pt">06</output> connects to U1_SIG → A0.</span></div></div>')
    return figure("mux-selector", body,
                  'Four select bits encode sixteen states: 2⁴ = 16. Move the slider to see the binary address and the single channel connected to A0. The mux passes an analog voltage; the ADC performs the conversion.')


def sensor_lab() -> str:
    cells = "".join(f'<div class="sensor-cell" data-cell="{i}" role="img" aria-label="sensor {i:02d}"><span>{i:02d}</span></div>' for i in range(16))
    body = ('<div class="lab sensor-lab" data-lab="grid"><div class="sensor-grid" aria-label="4 by 4 receiver response map">'
            + cells + '</div><div class="lab-controls"><label>Ball horizontal position <output data-out="x">1.5</output>'
            '<input data-input="x" type="range" min="0" max="3" step="0.1" value="1.5"></label>'
            '<label>Ball vertical position <output data-out="y">1.5</output>'
            '<input data-input="y" type="range" min="0" max="3" step="0.1" value="1.5"></label>'
            '<div class="readout"><strong data-out="centroid">(1.50, 1.50)</strong><span>example weighted center</span></div>'
            '<p class="fine-print">Colors represent hypothetical IR-on minus IR-off values, not recorded readings. In a build, cell 00 is PT00 and cell 15 is PT15; confirm physical orientation against the assembled plate.</p>'
            '</div></div>')
    return figure("sensor-map", body,
                  'Move the ball sliders. Neighboring receivers respond with different strengths, giving the controller a smooth position estimate. One yes/no detector could not provide this two-dimensional pattern.')


def supplemental(kind: str, current: bool) -> str:
    if kind == "flow":
        return flow_diagram(current) + sensor_lab()
    if kind == "led":
        return led_schematic(current)
    if kind == "sense":
        return sensing_schematic(current)
    if kind == "mux":
        return mux_lab() + (connector_diagram() if current else "")
    if kind == "load":
        return load_simulator(current)
    if kind == "timing":
        return timing_diagram()
    if kind == "power":
        return power_diagram(current)
    if kind == "layout":
        return mask_diagram() if current else ""
    return ""


def inline(raw: str) -> str:
    tokens: list[str] = []

    def save(value: str) -> str:
        tokens.append(value)
        return f"\uE000{len(tokens)-1}\uE001"

    raw = re.sub(r'\[([^\]]+)\]\(([^)]+)\)',
                 lambda m: save(f'<a href="{e(m.group(2))}">{e(m.group(1))}</a>'), raw)
    raw = re.sub(r'`([^`]+)`', lambda m: save(f'<code>{e(m.group(1))}</code>'), raw)
    result = e(raw)
    result = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', result)
    result = re.sub(r'(?<!\*)\*([^*]+)\*(?!\*)', r'<em>\1</em>', result)
    for index, value in enumerate(tokens):
        result = result.replace(f"\uE000{index}\uE001", value)
    return result


def slug(text: str) -> str:
    return re.sub(r'[^a-z0-9]+', '-', text.lower()).strip('-')


def markdown_to_html(source: str, current: bool) -> tuple[str, list[tuple[str, str]]]:
    lines = source.splitlines()
    out: list[str] = []
    toc: list[tuple[str, str]] = []
    i = 0
    visuals = {
        "The complete cause-and-effect chain": "flow",
        "Start with the problem, then the blocks": "flow",
        "1. Make a controlled source of light": "led",
        "2. Turn reflected light into an electrical quantity": "sense",
        "3. Share one ADC among sixteen analog signals": "mux",
        "4. Why the original board has an adjustable resistance": "load",
        "5. Reject steady room light by measuring twice": "timing",
        "6. Turn a position estimate into movement": "power",
        "1. Power: why there are two 5 V nets and one ground": "power",
        "3. Why the 4067 is still required even as a module": "mux",
        "4. The trimmer replaces the digital potentiometer's *electrical job*": "load",
        "5. The THT Plate creates light and detects its reflection": "led",
        "6. Why the physical PCB details matter to the circuit": "layout",
    }
    while i < len(lines):
        s = lines[i].strip()
        if not s:
            i += 1
            continue
        if s.startswith("# "):
            i += 1
            continue
        if s.startswith("## "):
            title = s[3:]
            anchor = slug(title)
            toc.append((anchor, title.replace('*', '')))
            out.append(f'<section class="lesson-section" id="{anchor}"><h2>{inline(title)}</h2>')
            visual = visuals.get(title)
            if visual:
                out.append(supplemental(visual, current))
            i += 1
            continue
        if s.startswith("```" ):
            language = s[3:].strip()
            block: list[str] = []
            i += 1
            while i < len(lines) and not lines[i].strip().startswith("```"):
                block.append(lines[i])
                i += 1
            i += 1
            if language != "mermaid":
                out.append(f'<pre><code>{e(chr(10).join(block))}</code></pre>')
            continue
        if s.startswith("| "):
            rows: list[list[str]] = []
            while i < len(lines) and lines[i].lstrip().startswith("|"):
                rows.append([x.strip() for x in lines[i].strip().strip('|').split('|')])
                i += 1
            if len(rows) >= 2:
                out.append('<div class="table-wrap"><table><thead><tr>'
                           + ''.join(f'<th>{inline(x)}</th>' for x in rows[0])
                           + '</tr></thead><tbody>')
                for row in rows[2:]:
                    out.append('<tr>' + ''.join(f'<td>{inline(x)}</td>' for x in row) + '</tr>')
                out.append('</tbody></table></div>')
            continue
        if re.match(r'^\d+\. ', s) or s.startswith("- "):
            ordered = bool(re.match(r'^\d+\. ', s))
            tag = "ol" if ordered else "ul"
            out.append(f'<{tag}>')
            while i < len(lines):
                item = lines[i].strip()
                if ordered:
                    m = re.match(r'^\d+\. (.*)$', item)
                    if not m:
                        break
                    content = m.group(1)
                else:
                    if not item.startswith("- "):
                        break
                    content = item[2:]
                out.append(f'<li>{inline(content)}</li>')
                i += 1
            out.append(f'</{tag}>')
            continue
        paragraph = [s]
        i += 1
        while i < len(lines) and lines[i].strip() and not re.match(r'^(#|```|\||\d+\. |- )', lines[i].strip()):
            paragraph.append(lines[i].strip())
            i += 1
        out.append(f'<p>{inline(" ".join(paragraph))}</p>')
    if toc:
        out.append('</section>')
    # Close each preceding section before the next section starts.
    rendered = "\n".join(out).replace('<section class="lesson-section"', '</section>\n<section class="lesson-section"')
    rendered = rendered.replace('</section>\n<section', '<section', 1)
    return rendered, toc


STYLE = r"""
:root { color-scheme:light; --cardinal:#8c1515; --cardinal-dark:#820000; --ink:#2e2d29; --muted:#5f5b55; --rule:#d8d1c7; --paper:#f8f6f2; --sand:#ede7dd; --white:#fff; --signal:#315f79; --power:#af663b; }
* { box-sizing:border-box; }
html { scroll-behavior:smooth; }
body { margin:0; color:var(--ink); background:var(--paper); font:17px/1.72 "Source Sans 3",system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif; }
a { color:var(--cardinal-dark); text-underline-offset:3px; }
a:hover { color:var(--cardinal); }
a:focus-visible,input:focus-visible { outline:3px solid #b78240; outline-offset:3px; }
.topbar { min-height:76px; background:var(--white); border-top:6px solid var(--cardinal); border-bottom:1px solid var(--rule); padding:13px max(24px,calc((100vw - 1280px)/2)); display:flex; align-items:center; gap:24px; flex-wrap:wrap; }
.brand { display:flex; align-items:center; gap:12px; color:var(--cardinal-dark); font:700 1.18rem/1.2 "Source Serif 4",Georgia,"Times New Roman",serif; letter-spacing:-.015em; }
.brand::before { content:"B"; display:grid; place-items:center; width:34px; height:34px; border:1.5px solid var(--cardinal); border-radius:50%; font:700 1.25rem Georgia,serif; }
.topbar a { color:#514c45; font-size:.86rem; font-weight:650; text-decoration:none; border-bottom:1px solid transparent; }
.topbar a:hover { color:var(--cardinal); border-color:currentColor; }
.topbar .sibling { margin-left:auto; }
.hero { position:relative; overflow:hidden; background:radial-gradient(circle at 88% 15%,#e7d9c6 0,transparent 32%),linear-gradient(105deg,#f1e9dc,#f9f6ef 64%,#efe6d9); border-bottom:1px solid var(--rule); padding:64px max(24px,calc((100vw - 1180px)/2)) 67px; }
.hero::after { content:""; position:absolute; right:-40px; top:-25px; width:350px; height:350px; opacity:.22; pointer-events:none; background:radial-gradient(circle,#8c1515 0 2px,transparent 3px) 0 0/32px 32px,linear-gradient(90deg,transparent 0 46%,#8c1515 46% 47%,transparent 47%) 0 0/96px 96px; mask-image:linear-gradient(to left,#000,transparent); }
.hero .eyebrow { color:var(--cardinal); text-transform:uppercase; letter-spacing:.18em; font-weight:800; font-size:.75rem; }
h1,h2 { font-family:"Source Serif 4",Georgia,"Times New Roman",serif; font-weight:700; }
h1 { font-size:clamp(2.5rem,5vw,4.35rem); line-height:1.06; max-width:800px; margin:14px 0 18px; letter-spacing:-.035em; }
.hero p { max-width:750px; font-size:1.15rem; color:#4c4841; margin-bottom:0; }
.hero .keypoints { display:flex; gap:9px; flex-wrap:wrap; margin-top:27px; }
.keypoints span { border:1px solid #c9b8a8; border-radius:3px; padding:5px 10px; color:#5f342f; font-size:.77rem; font-weight:700; letter-spacing:.025em; background:#fffaf2c9; }
.layout { max-width:1250px; margin:0 auto; display:grid; grid-template-columns:225px minmax(0,1fr); gap:52px; padding:42px 26px 90px; }
.toc { position:sticky; top:20px; align-self:start; max-height:calc(100vh - 40px); overflow:auto; padding:16px 0 16px 18px; border-left:2px solid var(--cardinal); }
.toc strong { display:block; font-size:.72rem; text-transform:uppercase; letter-spacing:.17em; margin:0 0 13px; color:var(--cardinal); }
.toc a { display:block; padding:7px 4px; font-size:.88rem; line-height:1.35; text-decoration:none; color:#514c45; }
.toc a:hover { color:var(--cardinal); text-decoration:underline; }
main { min-width:0; max-width:930px; }
.intro { background:#fffdf9; border:1px solid var(--rule); border-left:4px solid var(--cardinal); padding:20px 24px; margin-bottom:38px; box-shadow:0 4px 16px #3528190a; }
.intro strong { color:var(--cardinal-dark); }
.lesson-section { scroll-margin-top:25px; padding:12px 0 36px; border-bottom:1px solid var(--rule); }
.lesson-section:last-child { border-bottom:0; }
h2 { line-height:1.17; margin:28px 0 18px; font-size:clamp(1.8rem,2.4vw,2.25rem); letter-spacing:-.025em; }
h2::before { content:""; display:block; width:36px; height:3px; background:var(--cardinal); margin-bottom:15px; }
p { margin:14px 0; }
li { padding-left:4px; margin:8px 0; }
li::marker { color:var(--cardinal); }
ul,ol { padding-left:26px; }
code { background:#eee9e2; color:#4f2521; padding:.11em .34em; border-radius:3px; font:.88em/1.45 ui-monospace,SFMono-Regular,Consolas,monospace; overflow-wrap:anywhere; }
pre { overflow:auto; padding:17px; background:#292723; color:#fffaf2; border-radius:4px; }
pre code { background:none; color:inherit; padding:0; }
.figure { margin:26px 0 32px; padding:19px; overflow-x:auto; background:var(--white); border:1px solid var(--rule); border-top:3px solid var(--cardinal); border-radius:4px; box-shadow:0 6px 24px #3528190b; }
.figure figcaption { border-top:1px solid var(--rule); padding:12px 3px 2px; color:#5d5850; font-size:.88rem; line-height:1.55; }
.figure figcaption::before { content:"FIGURE / "; color:var(--cardinal); font-size:.71rem; font-weight:800; letter-spacing:.12em; }
.drawing { width:100%; height:auto; display:block; }
.svg-label { font:16px system-ui,sans-serif; fill:#373b3c; }
.svg-label.small { font-size:13px; fill:#656a69; }
.svg-label.heavy { font-weight:750; font-size:17px; fill:#2f3335; }
.svg-label.power-text { fill:#98552f; font-weight:700; }
.svg-label.logic-text,.svg-label.signal-text { fill:#315f79; font-weight:700; }
.svg-label.pin-number { font:700 14px ui-monospace,monospace; fill:#4b3934; }
.svg-label.pin-label { font:14px ui-monospace,monospace; fill:#3f4547; }
.svg-box { fill:#f8f6f2; stroke:#a6a09a; stroke-width:2; }
.flow-box,.mux-box,.adc-box,.logic-box { fill:#eef2f2; stroke:#8da7ad; }
.feedback-box,.switch-box,.power-box { fill:#fbefe4; stroke:#cb9a77; }
.pot-box { fill:#f5eded; stroke:#bc9291; }
.ground-box { fill:#eef2ec; stroke:#9cae9a; }
.wire { fill:none; stroke:#5d6668; stroke-width:3; stroke-linecap:round; stroke-linejoin:round; }
.wire.power { stroke:#b87345; stroke-width:4; }
.wire.logic,.wire.signal { stroke:#4e8098; }
.wire.feedback { stroke:#879a9d; stroke-dasharray:8 7; }
.wire.ground-return { stroke:#819a85; stroke-width:3; }
.wire.arrow,.wire.power.arrow,.wire.logic.arrow { marker-end:url(#arrow-flow); }
#power-map .wire.arrow { marker-end:url(#power-arrow); }
.light-ray { fill:none; stroke:#bd8a3c; stroke-width:2.5; marker-end:url(#light-out); }
.sensor-symbol { fill:#f5f8f7; stroke:#6d929c; stroke-width:2.5; }
.grid-line { fill:none; stroke:#e2dfda; stroke-width:1.5; stroke-dasharray:5 5; }
.axis { fill:none; stroke:#77766f; stroke-width:2.5; }
.switch-step { fill:none; stroke:#b87345; stroke-width:4; }
.ambient-step { fill:none; stroke:#a6aca9; stroke-width:3; stroke-dasharray:7 6; }
.sensor-step,.curve { fill:none; stroke:#477f83; stroke-width:4; }
.difference { fill:none; stroke:#b87345; stroke-width:3; }
.divider { stroke-dasharray:7 6; stroke:#b7b4ae; }
.curve-point { fill:#b87345; stroke:#fff; stroke-width:2; }
.pin-sensor { fill:#e9f0f1; }.pin-control { fill:#eaf0e9; }.pin-power { fill:#f9ecdf; }
.board-core { fill:#d5c5ad; stroke:#9e8c72; }.copper-pad { fill:#cf8c51; }.drill-hole { fill:#fff; stroke:#8798a1; }.mask-line { fill:none; stroke:#4da986; stroke-width:8; }
.lab { display:grid; grid-template-columns:minmax(260px,1fr) minmax(260px,1.4fr); align-items:center; gap:22px; padding:10px 0; }
.lab-controls { display:grid; gap:18px; }
.lab label,.mux-lab label { font-weight:700; color:#4b3833; }
.lab output { float:right; color:var(--cardinal); font-variant-numeric:tabular-nums; }
.lab input[type=range],.mux-lab input[type=range] { display:block; width:100%; accent-color:var(--cardinal); margin:12px 0 4px; cursor:pointer; }
.readout { padding:11px 14px; border-left:3px solid var(--cardinal); background:#f5f0e8; display:flex; justify-content:space-between; gap:12px; align-items:baseline; }
.readout strong { font-size:1.2rem; color:var(--cardinal-dark); font-variant-numeric:tabular-nums; }
.readout span { color:#696158; font-size:.85rem; }
.fine-print { color:#68645e; font-size:.81rem; line-height:1.55; margin:0; }
.sensor-lab { grid-template-columns:minmax(240px,360px) 1fr; }
.sensor-grid { display:grid; grid-template-columns:repeat(4,1fr); gap:8px; padding:10px; background:#f2ede6; border:1px solid var(--rule); }
.sensor-cell { aspect-ratio:1; display:grid; place-items:center; background:#ece6dc; border:1px solid #c9b9a8; transition:background .14s; }
.sensor-cell span { background:#ffffffe0; border-radius:2px; padding:0 5px; font:700 12px ui-monospace,monospace; }
.mux-lab { padding:17px; background:#f5f0e9; border:1px solid var(--rule); }
.mux-lab label { display:block; }
.mux-lab input { display:block; width:100%; margin:12px 0 16px; accent-color:var(--cardinal); }
.mux-lab output { color:var(--cardinal); font-variant-numeric:tabular-nums; }
.mux-channels { display:grid; grid-template-columns:repeat(8,1fr); gap:5px; }
.mux-channels span { display:grid; place-items:center; background:#e9e3da; border:1px solid #c2b8ac; border-radius:2px; padding:7px 2px; font:700 13px ui-monospace,monospace; }
.mux-channels span.active { background:var(--cardinal); border-color:var(--cardinal-dark); color:white; }
.mux-address { display:flex; flex-wrap:wrap; gap:10px 20px; align-items:baseline; margin-top:16px; }
.mux-address strong { font:700 1.25rem ui-monospace,monospace; color:var(--cardinal-dark); }
.table-wrap { overflow-x:auto; }
table { border-collapse:collapse; width:100%; min-width:560px; font-size:.9rem; }
th { background:#e9e1d6; color:#4b312d; text-align:left; }
th,td { padding:11px 13px; border:1px solid #d1c8bb; vertical-align:top; }
tbody tr:nth-child(2n) { background:#f4f0e9; }
.footer { padding:32px 20px; text-align:center; color:#686158; border-top:1px solid var(--rule); font-size:.85rem; }
@media (max-width:850px) { .layout { display:block; padding:26px 18px 65px; }.toc { position:static; max-height:none; border-left:0; border-top:2px solid var(--cardinal); padding:14px 0 18px; margin-bottom:26px; }.toc a { display:inline-block; margin-right:12px; }.lab,.sensor-lab { grid-template-columns:1fr; }.hero { padding:48px 24px; } }
@media (max-width:620px) { body { font-size:16px; }.topbar { gap:12px 20px; padding:12px 18px; }.topbar .sibling { margin-left:0; }.hero::after { opacity:.08; }.figure { padding:10px; }.drawing { min-width:620px; }.readout { display:block; }.readout span { display:block; }.mux-channels { grid-template-columns:repeat(4,1fr); } }
@media (prefers-reduced-motion:reduce) { html { scroll-behavior:auto; }.sensor-cell { transition:none; } }
@media print { body { background:white; color:black; } .topbar,.toc { display:none; } .hero { background:white; padding:10px 0; } .hero::after { display:none; } .layout { display:block; padding:0; } main { max-width:none; } .lesson-section,.figure { break-inside:avoid; } .figure { box-shadow:none; } a { color:black; } }
"""


SCRIPT = r"""
(() => {
  'use strict';
  const load = document.querySelector('[data-lab="load"]');
  if (load) {
    const rInput = load.querySelector('[data-input="r"]');
    const iInput = load.querySelector('[data-input="i"]');
    const value = key => load.querySelector(`[data-out="${key}"]`);
    const plot = load.querySelector('[data-curve="load"]');
    const point = load.querySelector('[data-point="load"]');
    const update = () => {
      const r = Number(rInput.value), current = Number(iInput.value);
      const voltage = amp => Math.min(5, amp * 1e-6 * (r + 500));
      value('r').textContent = (r / 1000).toFixed(2) + ' kΩ';
      value('i').textContent = current + ' µA';
      value('v').textContent = voltage(current).toFixed(2) + ' V';
      value('adc').textContent = Math.round(voltage(current) / 5 * 1023) + ' / 1023';
      const x = amp => 72 + amp / 150 * 540;
      const y = amp => 234 - voltage(amp) / 5 * 204;
      let d = '';
      for (let amp = 0; amp <= 150; amp += 2) d += (amp ? 'L' : 'M') + x(amp).toFixed(2) + ' ' + y(amp).toFixed(2);
      plot.setAttribute('d', d);
      point.setAttribute('cx', x(current));
      point.setAttribute('cy', y(current));
    };
    rInput.addEventListener('input', update);
    iInput.addEventListener('input', update);
    update();
  }
  const grid = document.querySelector('[data-lab="grid"]');
  if (grid) {
    const xInput = grid.querySelector('[data-input="x"]');
    const yInput = grid.querySelector('[data-input="y"]');
    const cells = [...grid.querySelectorAll('[data-cell]')];
    const update = () => {
      const x = Number(xInput.value), y = Number(yInput.value);
      let sum = 0, wx = 0, wy = 0;
      for (let n = 0; n < 16; n++) {
        const cx = n % 4, cy = Math.floor(n / 4);
        const intensity = Math.exp(-((cx-x)**2 + (cy-y)**2) / 1.15);
        cells[n].style.background = `hsl(${30 - intensity*28} ${35 + intensity*15}% ${92 - intensity*46}%)`;
        cells[n].setAttribute('aria-label', `sensor ${String(n).padStart(2,'0')}: illustrative response ${Math.round(intensity*100)} percent`);
        sum += intensity; wx += cx*intensity; wy += cy*intensity;
      }
      grid.querySelector('[data-out="x"]').textContent = x.toFixed(1);
      grid.querySelector('[data-out="y"]').textContent = y.toFixed(1);
      grid.querySelector('[data-out="centroid"]').textContent = `(${(wx/sum).toFixed(2)}, ${(wy/sum).toFixed(2)})`;
    };
    xInput.addEventListener('input', update);
    yInput.addEventListener('input', update);
    update();
  }
  const mux = document.querySelector('[data-lab="mux"]');
  if (mux) {
    const input = mux.querySelector('[data-input="channel"]');
    const channels = [...mux.querySelectorAll('[data-channel]')];
    const update = () => {
      const n = Number(input.value);
      mux.querySelector('[data-out="channel"]').textContent = String(n).padStart(2,'0');
      mux.querySelector('[data-out="pt"]').textContent = String(n).padStart(2,'0');
      mux.querySelector('[data-out="bits"]').textContent = n.toString(2).padStart(4,'0').split('').join(' ');
      channels.forEach((cell, index) => cell.classList.toggle('active', index === n));
    };
    input.addEventListener('input', update);
    update();
  }
})();
"""


def build(name: str, current: bool) -> None:
    source = (HERE / f"electronics_{name}.md").read_text(encoding="utf-8")
    content, toc = markdown_to_html(source, current)
    if current:
        title = "Current THT / module electronics"
        summary = "Understand how the hand-solderable Base and Plate turn reflected infrared light into a ball position and three servo commands."
        chips = ["Arduino Micro", "4067 module", "25-turn trimmer", "THT Plate"]
        sibling = '<a class="sibling" href="electronics_original.html">← Original design</a>'
    else:
        title = "Original BaBot electronics"
        summary = "Follow the original circuit from its IR emitter bank through sixteen receivers, an analog mux, a bare ATmega32U4, and three servos."
        chips = ["Original manufactured boards", "16 sensor channels", "Digital-pot load", "LED switching"]
        sibling = '<a class="sibling" href="electronics_tht.html">Current THT version →</a>'
    nav = "".join(f'<a href="#{e(anchor)}">{e(text)}</a>' for anchor, text in toc)
    pills = "".join(f'<span>{e(chip)}</span>' for chip in chips)
    doc = f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="{e(summary)}">
<title>{e(title)} · BaBot EE notes</title>
<style>{STYLE}</style>
</head>
<body>
<header class="topbar"><span class="brand">BaBot / EE notes</span><a href="../README.md">Repository README</a>{sibling}</header>
<div class="hero"><div class="eyebrow">Electrical engineering · illustrated lecture notes</div><h1>{e(title)}</h1><p>{e(summary)}</p><div class="keypoints">{pills}</div></div>
<div class="layout"><nav class="toc" aria-label="Lesson sections"><strong>In this lesson</strong>{nav}</nav><main>
<div class="intro"><strong>How to use this page.</strong> Read each schematic from the source of energy or signal toward the result. The drawings simplify repeated branches and are not physical PCB pinouts. Blue marks sensing/logic; orange marks power and switched light. Move the two teaching simulators to see how resistance and ball position change the signals. All interactive calculations are illustrative.</div>
{content}
</main></div><footer class="footer">Source guide: <a href="electronics_{name}.md">electronics_{name}.md</a> · Local, self-contained HTML · BaBot open-source attribution is in the repository README.</footer>
<script>{SCRIPT}</script>
</body></html>'''
    target = HERE / f"electronics_{name}.html"
    target.write_text(doc, encoding="utf-8")
    print(f"Wrote {target.name}: {len(doc):,} characters, {len(toc)} sections")


if __name__ == "__main__":
    build("original", False)
    build("tht", True)
