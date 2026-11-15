#!/usr/bin/env python3
"""Export the electronics plates and DC-socket coupon, check them and render a preview."""

import argparse
import json
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

import cadquery as cq
import trimesh
from electronics_plates import (
    BOSS_CLEARANCE,
    CARRIER_OPENING_HALF,
    COMPONENTS,
    CONNECTOR_BAND_Y0,
    DC_SOCKET,
    LOW_PART_H,
    MODULE_CENTRES,
    MOUNT_HOLES,
    OPENING_MARGIN,
    PANEL_CONNECTORS,
    PANEL_GAP,
    PANEL_LOW_PARTS,
    PCB_RECESS,
    PI_CENTRE,
    PI_HOLES,
    PI_STANDOFF,
    PLATE_MM,
    PLATE_T,
    PLATE_TOP_Y,
    PLATE_Z0,
    PLUG_TOP,
    POST_H,
    POST_OD,
    POST_PILOT,
    build_controller_plate,
    build_dc_socket_coupon,
    build_power_plate,
    depth_budget,
    placements,
)
from trial_carrier import PATTERN

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=ROOT / "exports" / "electronics-plates")
out = parser.parse_args().output.resolve()
out.mkdir(parents=True, exist_ok=True)
REVISION = "electronics-plates-v3"

parts = {
    "power-plate": build_power_plate(),
    "controller-plate": build_controller_plate(),
    "dc-socket-coupon": build_dc_socket_coupon(),
}
manifest = {
    "revision": REVISION,
    "status": "Prototype study for the off-the-shelf wiring plan; not printed or fitted.",
    "placement": f"Inside the trial carrier, plate underside {PANEL_GAP} mm above the panel frame "
    f"({PLATE_Z0} mm from the LED tips); posts {POST_H} mm tall up to the carrier web; M3 x 12 "
    "thread-forming screws from the carrier's rear face through the seam holes into the posts.",
    "plate_mm": PLATE_MM,
    "mount_holes": MOUNT_HOLES,
    "panel_rear": {
        "pcb_recess_below_frame_mm": PCB_RECESS,
        "low_parts_height_mm": LOW_PART_H,
        "connectors_xy_mm": PANEL_CONNECTORS,
        "plug_tops_from_led_tips_mm": PLUG_TOP,
    },
    "components": {k: {"x": v[0], "y": v[1], "height": v[2], "source": v[3]} for k, v in COMPONENTS.items()},
    "placements": {p: placements(p) for p in ("power-plate", "controller-plate")},
    "dc_socket": DC_SOCKET,
    "depth_budget": depth_budget(),
    "target_body_depth_mm": 60.0,
    "parts": [],
}
meshes = {}
for name, shape in parts.items():
    solid = shape.val()
    assert solid.isValid() and len(solid.Solids()) == 1, name
    bb = solid.BoundingBox()
    assert bb.xlen + 10 <= 210 and bb.ylen + 10 <= 210 and bb.zlen <= 210, name
    stl, step = out / f"{name}.stl", out / f"{name}.step"
    cq.exporters.export(shape, str(stl), tolerance=0.02, angularTolerance=0.1)
    cq.exporters.export(shape, str(step))
    mesh = trimesh.load(stl, force="mesh")
    assert mesh.is_watertight and mesh.body_count == 1, name
    assert abs(mesh.volume / solid.Volume() - 1) < 0.005, name
    meshes[name] = mesh
    manifest["parts"].append(
        {
            "stl": stl.name,
            "step": step.name,
            "bounds_mm": [round(bb.xlen, 2), round(bb.ylen, 2), round(bb.zlen, 2)],
            "volume_cm3": round(solid.Volume() / 1000, 2),
        }
    )

# Probes and layout checks.
t = PLATE_T
assert PLATE_TOP_Y + 15.0 <= CONNECTOR_BAND_Y0, "plates must stay clear of the panel connectors"
limit = CARRIER_OPENING_HALF - OPENING_MARGIN
for name in ("power-plate", "controller-plate"):
    s = parts[name].val()
    bb = s.BoundingBox()
    assert bb.ymax <= PLATE_TOP_Y + 0.01, (name, bb.ymax)  # OCC bounding boxes carry a small tolerance
    assert abs(bb.zmax - (t + POST_H)) < 0.01, "posts end at the carrier web underside"
    for x0, x1, y0, y1 in PANEL_CONNECTORS.values():
        assert not s.isInside(((x0 + x1) / 2, (y0 + y1) / 2, t / 2)), name
    for bx, by in PATTERN:
        if by < PLATE_TOP_Y:
            assert not s.isInside((bx, by, t / 2)), ("boss notch", name, bx, by)
            assert not s.isInside((bx + BOSS_CLEARANCE / 2 - 0.5, by, t / 2)), ("boss notch", name)
    for x, y in MOUNT_HOLES:
        assert not s.isInside((x, y, t + POST_H / 2)), ("post pilot", name, x, y)
        assert s.isInside((x + (POST_PILOT + POST_OD) / 4, y, t + POST_H / 2)), ("post wall", name, x, y)
    # Every component taller than the posts must pass the carrier web's opening.
    for part, comp, x, y, z0 in placements(name):
        w, d, h = COMPONENTS[comp][:3]
        if z0 + h > POST_H:
            assert abs(x) + w / 2 <= limit and abs(y) + d / 2 <= limit, ("outside carrier opening", part)
        assert y + d / 2 <= CONNECTOR_BAND_Y0 - 10.0, ("too close to the connectors", part)
cp = parts["controller-plate"].val()
od, h, pilot = PI_STANDOFF
for hx, hy in PI_HOLES:
    x, y = PI_CENTRE[0] + hx, PI_CENTRE[1] + hy
    assert not cp.isInside((x, y, t + h / 2))
    assert cp.isInside((x + (pilot / 2 + od / 2) / 2, y, t + h / 2))
pp = parts["power-plate"].val()
for mx, my in MODULE_CENTRES:
    assert pp.isInside((mx, my, t / 2)), "module seat must stay solid"
    for side in (-1, 1):
        assert not pp.isInside((mx + side * 30.0, my, t / 2)), "module slot at ~60 mm spacing"
coupon = parts["dc-socket-coupon"].val()
assert not coupon.isInside((0, 0, 3))
for dx in (-DC_SOCKET["screw_spacing"] / 2, DC_SOCKET["screw_spacing"] / 2):
    assert not coupon.isInside((dx, 0, 3))
manifest["probes"] = (
    "posts and pilots, boss notches, connector keep-out, carrier-opening fit of tall parts, "
    "Pi standoff pilots, module seats/slots and socket coupon holes checked"
)


def render_preview(path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    from matplotlib.patches import Rectangle
    from matplotlib.path import Path as MplPath

    fig, axes = plt.subplots(1, 3, figsize=(15, 7), gridspec_kw={"width_ratios": [1, 1, 0.45]})
    fig.suptitle("Electronics plates v3: half plates inside the carrier, connector half left open (seen from the wall)", fontsize=12)

    def section(ax, mesh, z):
        loops = [MplPath(l[:, :2]) for l in mesh.section(plane_origin=[0, 0, z], plane_normal=[0, 0, 1]).discrete]
        loops.sort(key=lambda l: -abs(np.ptp(l.vertices, axis=0).prod()))
        for loop in loops:
            depth = sum(o is not loop and o.contains_point(loop.vertices[0]) for o in loops)
            ax.fill(*loop.vertices.T, color="white" if depth % 2 else "#36566b", lw=0)

    def outline(ax, cx, cy, w, hgt, label, color):
        ax.add_patch(Rectangle((cx - w / 2, cy - hgt / 2), w, hgt, fill=False, ec=color, lw=1.3, ls="--"))
        ax.text(cx, cy, label, ha="center", va="center", fontsize=7, color=color)

    def panel_back(ax):
        ax.add_patch(Rectangle((-96, -96), 192, 192, fill=False, ec="#888", lw=1))
        o = CARRIER_OPENING_HALF
        ax.add_patch(Rectangle((-o, -o), 2 * o, 2 * o, fill=False, ec="#888", lw=0.8, ls=":"))
        ax.text(o - 1, o - 4, "carrier opening", ha="right", fontsize=6.5, color="#666")
        for name, (x0, x1, y0, y1) in PANEL_CONNECTORS.items():
            ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc="#c92a2a", alpha=0.35, ec="#c92a2a"))
            ax.text((x0 + x1) / 2, y1 + 3, name.replace("_", " ").upper(), ha="center", fontsize=7, color="#c92a2a")
        for x, y in PANEL_LOW_PARTS.values():
            ax.add_patch(Rectangle((x - 4, y - 4), 8, 8, fc="#c92a2a", alpha=0.2, ec="none"))
        ax.text(0, 82, "panel connector half: left open for plugs and cables", ha="center", fontsize=8, color="#c92a2a")

    for ax, name, title in (
        (axes[0], "power-plate", "Power plate (one bottom panel)"),
        (axes[1], "controller-plate", "Controller plate (other bottom panel)"),
    ):
        panel_back(ax)
        section(ax, meshes[name], t / 2)
        for part, comp, x, y, _ in placements(name):
            w, d, hgt = COMPONENTS[comp][:3]
            big = comp in ("buck_20a", "pi_stack")
            label = {"buck_20a": f"24→5 V 20 A\n{part.replace('_', ' ')}\n(32 mm tall)",
                     "pi_stack": "Pi 3B + Electrodragon\non 3.5 mm standoffs"}.get(comp, part.split("_")[0] if comp == "wago_221_415" else part.replace("_", " "))
            outline(ax, x, y, w, d, label, "#d9822b" if big else "#2b8a3e")
        ax.set_title(title, fontsize=10)
        ax.set(xlim=(-100, 100), ylim=(-100, 100), aspect="equal", xlabel="mm")

    ax = axes[2]
    section(ax, meshes["dc-socket-coupon"], 2.0)
    ax.set(aspect="equal", xlabel="mm")
    ax.set_title("DC socket wall coupon\n(Kycon KPJX-PM-4S test fit)", fontsize=10)

    db = depth_budget()
    fig.text(
        0.5,
        0.01,
        "Depth needed (LED tip → rear cover): power plate {:.0f} mm, controller plate {:.0f} mm; target 60 mm body. "
        "Red: panel connectors (owner's photo). Orange/green: component outlines.".format(
            db["power-plate"]["body_depth_needed_mm"], db["controller-plate"]["body_depth_needed_mm"]
        ),
        ha="center",
        fontsize=9,
    )
    fig.savefig(path, bbox_inches="tight", dpi=100)
    plt.close(fig)


render_preview(out / "electronics-plates-preview.png")
(out / "electronics-plates-validation.json").write_text(json.dumps(manifest, indent=2) + "\n")
archive = out / f"{REVISION}.zip"
with ZipFile(archive, "w", ZIP_DEFLATED) as z:
    for name in parts:
        z.write(out / f"{name}.stl", f"{name}.stl")
        z.write(out / f"{name}.step", f"{name}.step")
    for name in ("electronics-plates-validation.json", "electronics-plates-preview.png"):
        z.write(out / name, name)
    for name in ("electronics_plates.py", "trial_carrier.py", "make-electronics-plates.py", "requirements.lock"):
        z.write(ROOT / name, "source/" + name)
print(json.dumps(manifest["depth_budget"], indent=2))
print(archive)
