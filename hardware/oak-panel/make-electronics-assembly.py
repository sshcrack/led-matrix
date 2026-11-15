#!/usr/bin/env python3
"""Viewing assembly: 2x2 panels, trial carriers, both electronics plates and part envelopes.

For looking at the depth stack, not for printing. World frame, seen from the wall side
(behind the display): X right, Y up, Z from the LED tips (Z=0) toward the wall. Panels and
component boxes are envelopes (outer size only); the panel's rear connectors are not modelled
because their positions are not measured yet.

Writes electronics-assembly.step (coloured parts, opens in FreeCAD and most CAD viewers) and
electronics-assembly.glb (opens in browser viewers such as https://3dviewer.net).
"""

import argparse
from pathlib import Path

import cadquery as cq
from electronics_plates import (
    COMPONENTS,
    MODULE_CENTRES,
    PI_CENTRE,
    PI_STANDOFF,
    PLATE_MM,
    build_controller_plate,
    build_power_plate,
)
from trial_carrier import OUTER_MM, SPACER_MM, WEB_MM, build_trial_carrier

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=ROOT / "exports" / "electronics-plates")
parser.add_argument("--body-depth", type=float, default=80.0, help="rear cover position (mm from LED tips)")
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)

PANEL = 192.0
PANEL_DEPTH = 15.0  # LED tips to the insert face, measurement-basis.json
CARRIER_REAR = PANEL_DEPTH + WEB_MM + SPACER_MM  # 31 mm: plates sit here
PLATE_TOP = CARRIER_REAR + PLATE_MM[2]
TILES = {  # tile centre seen from behind; bottom row carries the plates
    "top-left": (-PANEL / 2, PANEL / 2),
    "top-right": (PANEL / 2, PANEL / 2),
    "bottom-left": (-PANEL / 2, -PANEL / 2),
    "bottom-right": (PANEL / 2, -PANEL / 2),
}
PLATES = {"bottom-left": ("power-plate", build_power_plate), "bottom-right": ("controller-plate", build_controller_plate)}

C = {
    "panel": cq.Color(0.08, 0.08, 0.08, 1.0),
    "carrier": cq.Color(0.55, 0.55, 0.58, 1.0),
    "plate": cq.Color(0.21, 0.34, 0.42, 1.0),
    "module": cq.Color(0.85, 0.51, 0.17, 1.0),
    "pi": cq.Color(0.10, 0.55, 0.25, 1.0),
    "small": cq.Color(0.17, 0.54, 0.24, 1.0),
    "wiring": cq.Color(0.95, 0.80, 0.20, 1.0),
    "cover": cq.Color(0.6, 0.8, 1.0, 0.25),
}


def box(x, y, z0, w, d, h):
    return cq.Workplane("XY").box(w, d, h, centered=(True, True, False)).translate((x, y, z0))


asm = cq.Assembly(name="electronics-assembly")
carrier = build_trial_carrier().rotate((0, 0, 0), (0, 1, 0), 180).translate((0, 0, CARRIER_REAR))
for name, (tx, ty) in TILES.items():
    asm.add(box(tx, ty, 0, PANEL - 0.4, PANEL - 0.4, PANEL_DEPTH), name=f"panel-{name}", color=C["panel"])
    asm.add(carrier.translate((tx, ty, 0)), name=f"carrier-{name}", color=C["carrier"])
    if name not in PLATES:
        continue
    plate_name, build = PLATES[name]
    asm.add(build().translate((tx, ty, CARRIER_REAR)), name=plate_name, color=C["plate"])
    if plate_name == "power-plate":
        w, d, h, _ = COMPONENTS["buck_20a"]
        for i, (mx, my) in enumerate(MODULE_CENTRES):
            asm.add(box(tx + mx, ty + my, PLATE_TOP, w, d, h), name=f"module-{'AB'[i]}", color=C["module"])
        asm.add(box(tx, ty + 47, PLATE_TOP, 150, 18, 10), name="wago-row", color=C["wiring"])
        asm.add(box(tx, ty - 47, PLATE_TOP, 150, 10, 12), name="fuse-holders", color=C["wiring"])
    else:
        w, d, h, _ = COMPONENTS["pi_stack"]
        z0 = PLATE_TOP + PI_STANDOFF[1]
        # Board centre sits 10 mm right of the hole-pattern centre (holes 3.5 mm from the left edge).
        asm.add(box(tx + PI_CENTRE[0] + 10, ty + PI_CENTRE[1], z0, w, d, h), name="pi-stack", color=C["pi"])
        for part, (x, y) in (("lm2596", (50, 30)), ("pro_micro", (42, -18)), ("level_shifter", (50, -40))):
            w, d, h, _ = COMPONENTS[part]
            asm.add(box(tx + x, ty + y, PLATE_TOP, w, d, h), name=part, color=C["small"])

# Rear cover plane at the chosen body depth (translucent), to see the margin.
asm.add(box(0, 0, args.body_depth - 2.0, 2 * PANEL, 2 * PANEL, 2.0), name=f"rear-cover-{args.body_depth:.0f}mm", color=C["cover"])

step, glb = out / "electronics-assembly.step", out / "electronics-assembly.glb"
asm.export(str(step))
asm.export(str(glb))
print(step)
print(glb)
print(f"carrier rear {CARRIER_REAR} mm, plate top {PLATE_TOP} mm, Pi stack top "
      f"{PLATE_TOP + PI_STANDOFF[1] + COMPONENTS['pi_stack'][2]} mm, rear cover inner face {args.body_depth - 2.0} mm")
