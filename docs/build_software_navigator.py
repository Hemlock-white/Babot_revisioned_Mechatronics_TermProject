#!/usr/bin/env python3
"""Build the self-contained BaBot software teaching navigator.

Run from anywhere with: python -B docs/build_software_navigator.py
The builder verifies the reviewed sketch hash, every line reference, graph layout,
and every orthogonal connector before writing the root HTML artifact.
"""

from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path

from software_navigator_data import FILE, GRAPHS, TOPICS

ROOT = Path(__file__).resolve().parent.parent
TEMPLATE = Path(__file__).with_name("software_navigator_template.html.in")
OUTPUT = ROOT / "BaBot_導覽.html"
SOURCE = ROOT / FILE
EXPECTED_SHA256 = "bd2dd29aa9ad553cc0aa443194a2307f4adaeaa2afdabc9d15155300acc94d29"
WIDTH = 1300


def port(node: dict, side: str) -> tuple[int, int]:
    x, y, w, h = node["cx"], node["y"], node["w"], node["h"]
    return {
        "top": (x, y), "bottom": (x, y + h),
        "left": (x - w // 2, y + h // 2),
        "right": (x + w // 2, y + h // 2),
    }[side]


def route(a: dict, b: dict, edge: dict) -> list[list[int]]:
    start = port(a, edge["out"])
    end = port(b, edge["into"])
    if "via" in edge:
        points = [start, *map(tuple, edge["via"]), end]
    elif start[0] == end[0] or start[1] == end[1]:
        points = [start, end]
    elif edge["out"] in ("left", "right") and edge["into"] in ("left", "right"):
        middle_x = (start[0] + end[0]) // 2
        points = [start, (middle_x, start[1]), (middle_x, end[1]), end]
    else:
        middle_y = (start[1] + end[1]) // 2
        points = [start, (start[0], middle_y), (end[0], middle_y), end]
    result = []
    for point in points:
        if not result or point != result[-1]:
            result.append(point)
    return [list(point) for point in result]


def overlap(a: dict, b: dict) -> bool:
    return (a["cx"] - a["w"] / 2 < b["cx"] + b["w"] / 2
            and a["cx"] + a["w"] / 2 > b["cx"] - b["w"] / 2
            and a["y"] < b["y"] + b["h"]
            and a["y"] + a["h"] > b["y"])


def hits_node(p: list[int], q: list[int], node: dict, margin: int = 6) -> bool:
    left = node["cx"] - node["w"] / 2 - margin
    right = node["cx"] + node["w"] / 2 + margin
    top = node["y"] - margin
    bottom = node["y"] + node["h"] + margin
    if p[0] == q[0]:
        return left < p[0] < right and max(min(p[1], q[1]), top) < min(max(p[1], q[1]), bottom)
    return top < p[1] < bottom and max(min(p[0], q[0]), left) < min(max(p[0], q[0]), right)


def validate_ref(ref: str, lines: list[str]) -> None:
    match = re.fullmatch(r"(.+):(\d+)-(\d+)", ref)
    if not match or match.group(1) != FILE:
        raise ValueError(f"Invalid or unreviewed source reference: {ref}")
    first, last = map(int, match.group(2, 3))
    if not 1 <= first <= last <= len(lines):
        raise ValueError(f"Out-of-range source reference: {ref}")
    if not any(line.strip() for line in lines[first - 1:last]):
        raise ValueError(f"Reference contains only blank lines: {ref}")


def build() -> None:
    raw = SOURCE.read_bytes()
    actual_sha = hashlib.sha256(raw).hexdigest()
    if actual_sha != EXPECTED_SHA256:
        raise ValueError("software/main.ino changed; review all flowchart and theory line references before rebuilding")
    source = raw.decode("utf-8")
    lines = source.splitlines()
    if len(lines) != 565:
        raise ValueError(f"Expected 565 upstream source lines, found {len(lines)}")

    graph_data = []
    for graph in GRAPHS:
        nodes = graph["nodes"]
        ids = [node["id"] for node in nodes]
        if len(ids) != len(set(ids)):
            raise ValueError(f"Duplicate node in {graph['id']}")
        by_id = {node["id"]: node for node in nodes}
        for i, a in enumerate(nodes):
            validate_ref(a["jump"], lines)
            if "iconJump" in a:
                validate_ref(a["iconJump"], lines)
            if a["cx"] - a["w"] / 2 < 0 or a["cx"] + a["w"] / 2 > WIDTH:
                raise ValueError(f"Node outside canvas: {graph['id']}/{a['id']}")
            if a["y"] < 0 or a["y"] + a["h"] > graph["height"]:
                raise ValueError(f"Node outside canvas: {graph['id']}/{a['id']}")
            for b in nodes[i + 1:]:
                if overlap(a, b):
                    raise ValueError(f"Overlapping nodes in {graph['id']}: {a['id']}, {b['id']}")
        built_edges = []
        for item in graph["edges"]:
            a_id, b_id = item["from_"], item["to"]
            if a_id not in by_id or b_id not in by_id:
                raise ValueError(f"Unknown node in {graph['id']} edge: {a_id} → {b_id}")
            points = route(by_id[a_id], by_id[b_id], item)
            for p, q in zip(points, points[1:]):
                if p[0] != q[0] and p[1] != q[1]:
                    raise ValueError(f"Non-orthogonal edge in {graph['id']}: {a_id} → {b_id}")
                for other in nodes:
                    if other["id"] not in (a_id, b_id) and hits_node(p, q, other):
                        raise ValueError(f"Connector crosses node in {graph['id']}: {a_id} → {b_id} through {other['id']}")
            built_edges.append({"from": a_id, "to": b_id, "label": item["label"], "points": points})
        graph_data.append({**{k: v for k, v in graph.items() if k not in ("edges",)}, "edges": built_edges})

    for topic in TOPICS:
        if len(topic["chapters"]) != 5:
            raise ValueError(f"Expected five graduated lessons in {topic['id']}")
        types = [chapter["exercise"]["type"] for chapter in topic["chapters"]]
        if types != ["choice", "order", "fill", "code", "debug"]:
            raise ValueError(f"Unexpected exercise ladder in {topic['id']}: {types}")
        for chapter in topic["chapters"]:
            validate_ref(chapter["ref"], lines)

    constants = {}
    for name in ("P_GAIN", "I_GAIN", "D_GAIN", "BALL_THRESHOLD", "IR_SMOOTH_ALPHA",
                 "OUTPUT_SMOOTH_ALPHA", "PLATE_HEIGHT", "R1", "R2", "DOUBLE_PRESS_GAP", "LONG_PRESS_TIME"):
        m = re.search(rf"^\s*(?:const\s+)?(?:float|int|unsigned long)\s+{name}\s*=\s*([^;]+);", source, re.M)
        if not m:
            raise ValueError(f"Could not find config constant {name}")
        constants[name] = m.group(1).strip()
    channel_match = re.search(r"\bambientLight\[(\d+)\]", source)
    scan_match = re.search(r"\b(\d+)Hz\b", source)
    servo_pins = re.findall(r"^\s*const int SERVO_PIN_[A-Z]\s*=", source, re.M)
    if not channel_match or not scan_match or not servo_pins:
        raise ValueError("Could not derive channel, scan-rate, or servo count from the sketch")
    constants["CHANNELS"] = channel_match.group(1)
    constants["SCAN_HZ"] = scan_match.group(1)
    constants["SERVO_COUNT"] = str(len(servo_pins))

    payload = {
        "source": source,
        "sourcePath": FILE,
        "sourceSha256": actual_sha,
        "sourceLineCount": len(lines),
        "graphs": graph_data,
        "topics": TOPICS,
        "constants": constants,
    }
    encoded = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).replace("<", "\\u003c")
    template = TEMPLATE.read_text(encoding="utf-8")
    if template.count("__BABOT_DATA__") != 1:
        raise ValueError("Template must contain one data insertion point")
    html = template.replace("__BABOT_DATA__", encoded)
    OUTPUT.write_text(html, encoding="utf-8")
    print(f"Wrote {OUTPUT.name}: {OUTPUT.stat().st_size:,} bytes")
    print(f"Verified {len(graph_data)} charts, {sum(len(g['nodes']) for g in graph_data)} nodes, "
          f"{sum(len(g['edges']) for g in graph_data)} clear orthogonal connectors, "
          f"{len(TOPICS)} topics / {sum(len(t['chapters']) for t in TOPICS)} exercises")


if __name__ == "__main__":
    build()
