#!/usr/bin/env python3
"""Mechanical interface data for the enclosure: outline, holes, component height envelope, plug keepouts.

KiCad's packaged 3D models are not required (the plain STEP from check_board.py is board-only when
they are not installed), so this script writes its own envelope model:

  1. ``python3 tools/gen_envelope.py`` (system Python with pcbnew) reads the board and writes
     ``exports/mechanical/envelope.json``: every footprint as a box (courtyard x/y, height from
     HEIGHTS below), plus a mating-plug keepout box in front of each horizontal connector.
  2. ``<python with cadquery> tools/gen_envelope.py --step`` turns that JSON into
     ``exports/mechanical/power-halo-board-envelope.step`` (board with M3 holes, one solid per
     part, keepouts as separate solids named ``KEEPOUT-<ref>``).

Coordinates: board origin at the top-left corner seen from the component (top) side, +x right,
+y down (KiCad), z up from the board's bottom face.  Heights are conservative envelopes over the
manufacturer's nominal dimension where one is published; ``source`` says which.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PCB = ROOT / "power-halo-board" / "power-halo-board.kicad_pcb"
OUT = ROOT / "exports" / "mechanical"
W, H, T = 136.0, 86.0, 1.6
OX = OY = 50.0

# footprint-name regex -> (height above board top [mm], source)
HEIGHTS = [
    (r"Mini-Fit", 14.0, "assumed envelope; Molex 39-30-0040 drawing not checked"),
    (r"JST_VH_", 11.0, "assumed envelope; JST eVH side-entry drawing height not extracted"),
    (r"JST_XH_", 8.0, "assumed envelope; JST eXH side-entry drawing height not extracted"),
    (r"CP_Elec_8x10", 10.8, "Panasonic FK 8x10.2 body + 0.5 mm"),
    (r"CP_Elec_8x6", 6.8, "Panasonic FK 8x6.2 body + 0.5 mm"),
    (r"PinHeader_1x03", 11.5, "2.54 mm header 8.5 mm + shunt/programming plug allowance"),
    (r"TI_RDF0022A", 4.05, "TI TPSM63610 RDF0022A max height"),
    (r"SOIC-20W", 2.65, "JEDEC MS-013 max"),
    (r"D_SMB", 2.45, "DO-214AA max"),
    (r"D_SMA", 2.45, "DO-214AC max"),
    (r"C_1210", 2.8, "1210 50 V 10 uF MLCC, max thickness allowance"),
    (r"Fuse_Littelfuse-NANO2", 2.6, "NANO2 2410 allowance"),
    (r"LFPAK56", 1.2, "LFPAK56 max"),
    (r"SOT-23", 1.45, "SOT-23 max"),
    (r"SOD-323", 1.1, "SOD-323 max"),
    (r"TI_RPW0010A", 1.0, "VQFN-HR max"),
    (r"_0603_", 0.95, "0603 max"),
    (r"TestPoint", 0.1, "bare pad"),
]
SKIP = re.compile(r"MountingHole|Fiducial")
# depth of the clear zone in front of a horizontal connector (mating plug + wire bend radius)
PLUG_DEPTH = {"Mini-Fit": 30.0, "JST_VH_": 28.0, "JST_XH_": 20.0}


def height_for(fpname):
    for rx, h, src in HEIGHTS:
        if re.search(rx, fpname):
            return h, src
    raise SystemExit(f"no height rule for footprint {fpname}")


def extract():
    import pcbnew
    board = pcbnew.LoadBoard(str(PCB))
    parts, keepouts, holes = [], [], []
    for f in board.GetFootprints():
        ref, fp = f.GetReference(), f.GetFPIDAsString().split(":")[-1]
        if "MountingHole" in fp:
            p = f.GetPosition()
            holes.append({"ref": ref, "x": round(p.x / 1e6 - OX, 3), "y": round(p.y / 1e6 - OY, 3), "d": 3.2})
            continue
        if SKIP.search(fp):
            continue
        bb = f.GetCourtyard(pcbnew.F_CrtYd).BBox()
        x1, y1 = bb.GetLeft() / 1e6 - OX, bb.GetTop() / 1e6 - OY
        x2, y2 = bb.GetRight() / 1e6 - OX, bb.GetBottom() / 1e6 - OY
        h, src = height_for(fp)
        parts.append({"ref": ref, "footprint": fp, "box": [round(v, 3) for v in (x1, y1, x2, y2)],
                      "height": h, "source": src})
        depth = next((d for k, d in PLUG_DEPTH.items() if k in fp), None)
        if depth and "Horizontal" in fp:
            # the mating face is the courtyard side nearest a board edge
            sides = {"-x": x1, "+x": W - x2, "-y": y1, "+y": H - y2}
            side = min(sides, key=sides.get)
            kx1, ky1, kx2, ky2 = {"-x": (x1 - depth, y1, x1, y2), "+x": (x2, y1, x2 + depth, y2),
                                  "-y": (x1, y1 - depth, x2, y1), "+y": (x1, y2, x2, y2 + depth)}[side]
            keepouts.append({"ref": ref, "side": side, "box": [round(v, 3) for v in (kx1, ky1, kx2, ky2)],
                             "height": h + 2.0, "why": "mating plug, latch release and wire bend"})
    data = {"units": "mm", "origin": "board top-left, component side up, +x right, +y down, z from board bottom",
            "board": {"w": W, "h": H, "t": T, "max_part_height_above_top": max(p["height"] for p in parts)},
            "holes": sorted(holes, key=lambda h: h["ref"]), "parts": sorted(parts, key=lambda p: p["ref"]),
            "keepouts": sorted(keepouts, key=lambda k: k["ref"])}
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "envelope.json").write_text(json.dumps(data, indent=1) + "\n")
    print(f"{len(parts)} parts, {len(keepouts)} plug keepouts, {len(holes)} holes -> {OUT / 'envelope.json'}")


def step():
    import cadquery as cq
    data = json.loads((OUT / "envelope.json").read_text())
    # cadquery: +y up, so mirror KiCad y
    board = cq.Workplane("XY").box(W, H, T, centered=False).translate((0, -H, 0))
    for h in data["holes"]:
        board = board.cut(cq.Workplane("XY").circle(h["d"] / 2).extrude(T).translate((h["x"], -h["y"], 0)))
    asm = cq.Assembly(name="power-halo-board-envelope")
    asm.add(board, name="PCB", color=cq.Color(0.1, 0.4, 0.1))
    for p in data["parts"]:
        x1, y1, x2, y2 = p["box"]
        solid = cq.Workplane("XY").box(x2 - x1, y2 - y1, p["height"], centered=False).translate((x1, -y2, T))
        asm.add(solid, name=p["ref"], color=cq.Color(0.6, 0.6, 0.6))
    for k in data["keepouts"]:
        x1, y1, x2, y2 = k["box"]
        solid = cq.Workplane("XY").box(x2 - x1, y2 - y1, k["height"], centered=False).translate((x1, -y2, T))
        asm.add(solid, name=f"KEEPOUT-{k['ref']}", color=cq.Color(1.0, 0.3, 0.2, 0.3))
    out = OUT / "power-halo-board-envelope.step"
    asm.save(str(out))
    print("wrote", out)


if __name__ == "__main__":
    step() if "--step" in sys.argv else extract()
