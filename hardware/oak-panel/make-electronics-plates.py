#!/usr/bin/env python3
"""Export the electronics plates and DC-socket coupon, check them and render a preview."""

import argparse
import json
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

import cadquery as cq
import trimesh
from electronics_plates import (
    COMPONENTS,
    CONNECTOR_BAND_Y0,
    CONTROLLER_SMALL,
    DC_SOCKET,
    MODULE_CENTRES,
    MOUNT_HOLES,
    PANEL_CONNECTORS,
    PANEL_LOW_PARTS,
    PI_BOARD_OFFSET_X,
    PI_CENTRE,
    PI_HOLES,
    PI_STANDOFF,
    PLATE_MM,
    PLATE_TOP_Y,
    POWER_TIE_ROWS,
    build_controller_plate,
    build_dc_socket_coupon,
    build_power_plate,
    depth_budget,
)

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=ROOT / "exports" / "electronics-plates")
out = parser.parse_args().output.resolve()
out.mkdir(parents=True, exist_ok=True)

parts = {
    "power-plate": build_power_plate(),
    "controller-plate": build_controller_plate(),
    "dc-socket-coupon": build_dc_socket_coupon(),
}
manifest = {
    "revision": "electronics-plates-v2",
    "status": "Prototype study for the off-the-shelf wiring plan; not printed or fitted.",
    "plate_mm": PLATE_MM,
    "mounting": "M3 through four carrier seam holes at (+-86, -54) and (+-54, -86), nut on the carrier side",
    "panel_connectors_keepout": {"connectors": PANEL_CONNECTORS, "plate_top_y": PLATE_TOP_Y},
    "components": {k: {"x": v[0], "y": v[1], "height": v[2], "source": v[3]} for k, v in COMPONENTS.items()},
    "dc_socket": DC_SOCKET,
    "depth_budget": depth_budget(),
    "current_body_depth_mm": 60.0,
    "target_body_depth_mm": 80.0,
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

# Probes: mount holes open, plate solid next to them, connector half left free, Pi standoff
# pilots open and supported.
t = PLATE_MM[2]
assert PLATE_TOP_Y + 15.0 <= CONNECTOR_BAND_Y0, "plates must stay clear of the panel connectors"
for name in ("power-plate", "controller-plate"):
    s = parts[name].val()
    bb = s.BoundingBox()
    assert bb.ymax <= PLATE_TOP_Y + 0.01, (name, bb.ymax)  # OCC bounding boxes carry a small tolerance
    for x0, x1, y0, y1 in PANEL_CONNECTORS.values():
        assert not s.isInside(((x0 + x1) / 2, (y0 + y1) / 2, t / 2)), name
    for x, y in MOUNT_HOLES:
        assert not s.isInside((x, y, t / 2)), (name, x, y)
        assert s.isInside((x + 2.6 * (1 if x < 0 else -1), y, t / 2)), (name, x, y)
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
manifest["probes"] = "mount holes, connector keep-out, Pi standoff pilots, module seats/slots and socket coupon holes checked"


def render_preview(path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    from matplotlib.patches import Rectangle
    from matplotlib.path import Path as MplPath

    fig, axes = plt.subplots(1, 3, figsize=(15, 7), gridspec_kw={"width_ratios": [1, 1, 0.45]})
    fig.suptitle("Electronics plates v2: half plates, panel connector half left open (seen from the wall) / PETG", fontsize=13)

    def section(ax, mesh, z):
        loops = [MplPath(l[:, :2]) for l in mesh.section(plane_origin=[0, 0, z], plane_normal=[0, 0, 1]).discrete]
        loops.sort(key=lambda l: -abs(np.ptp(l.vertices, axis=0).prod()))
        for loop in loops:
            depth = sum(o is not loop and o.contains_point(loop.vertices[0]) for o in loops)
            ax.fill(*loop.vertices.T, color="white" if depth % 2 else "#36566b", lw=0)

    def outline(ax, cx, cy, w, hgt, label, color):
        ax.add_patch(Rectangle((cx - w / 2, cy - hgt / 2), w, hgt, fill=False, ec=color, lw=1.3, ls="--"))
        ax.text(cx, cy, label, ha="center", va="center", fontsize=7.5, color=color)

    def panel_back(ax):
        ax.add_patch(Rectangle((-96, -96), 192, 192, fill=False, ec="#888", lw=1))
        for name, (x0, x1, y0, y1) in PANEL_CONNECTORS.items():
            ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc="#c92a2a", alpha=0.35, ec="#c92a2a"))
            ax.text((x0 + x1) / 2, y1 + 3, name.replace("_", " ").upper(), ha="center", fontsize=7, color="#c92a2a")
        for x, y in PANEL_LOW_PARTS.values():
            ax.add_patch(Rectangle((x - 4, y - 4), 8, 8, fc="#c92a2a", alpha=0.2, ec="none"))
        ax.text(0, 80, "panel connector half: left open for plugs and cables", ha="center", fontsize=8, color="#c92a2a")

    ax = axes[0]
    panel_back(ax)
    section(ax, meshes["power-plate"], t / 2)
    for i, (mx, my) in enumerate(MODULE_CENTRES):
        outline(ax, mx, my, 74, 74, f"24→5 V 20 A\nmodule {'AB'[i]}\n(32 mm tall)", "#d9822b")
    outline(ax, 0, POWER_TIE_ROWS["wago"], 150, 8, "WAGO 221 row", "#2b8a3e")
    outline(ax, 0, POWER_TIE_ROWS["fuse"], 150, 8, "fuse holders", "#2b8a3e")
    ax.set_title("Power plate (one bottom panel)", fontsize=10)

    ax = axes[1]
    panel_back(ax)
    section(ax, meshes["controller-plate"], t / 2)
    px, py = PI_CENTRE
    outline(ax, px + PI_BOARD_OFFSET_X, py, 85, 56, "Pi 3B + Electrodragon\non 6 mm standoffs", "#d9822b")
    for name, (comp, x, y, _) in CONTROLLER_SMALL.items():
        w, d = COMPONENTS[comp][:2]
        outline(ax, x, y, w, d, name.replace("_", " "), "#2b8a3e")
    ax.set_title("Controller plate (other bottom panel)", fontsize=10)

    for ax in axes[:2]:
        ax.set(xlim=(-100, 100), ylim=(-100, 100), aspect="equal", xlabel="mm")

    ax = axes[2]
    section(ax, meshes["dc-socket-coupon"], 2.0)
    ax.set(aspect="equal", xlabel="mm")
    ax.set_title("DC socket wall coupon\n(Kycon KPJX-PM-4S test fit)", fontsize=10)

    db = depth_budget()
    fig.text(
        0.5,
        0.01,
        "Depth needed (LED tip → cover): power plate {:.0f} mm, controller plate {:.0f} mm (Pi stack ~30 mm measured, 33 mm budgeted) "
        "vs current 60 mm body; target 80 mm. Red: panel connectors (from the owner's photo). Section at mid-plate; orange/green = component outlines, not geometry.".format(
            db["power_plate"]["body_depth_needed_mm"], db["controller_plate"]["body_depth_needed_mm"]
        ),
        ha="center",
        fontsize=9,
    )
    fig.savefig(path, bbox_inches="tight", dpi=100)
    plt.close(fig)


render_preview(out / "electronics-plates-preview.png")
(out / "electronics-plates-validation.json").write_text(json.dumps(manifest, indent=2) + "\n")
archive = out / "electronics-plates-v2.zip"
with ZipFile(archive, "w", ZIP_DEFLATED) as z:
    for name in parts:
        z.write(out / f"{name}.stl", f"{name}.stl")
        z.write(out / f"{name}.step", f"{name}.step")
    for name in ("electronics-plates-validation.json", "electronics-plates-preview.png"):
        z.write(out / name, name)
    for name in ("electronics_plates.py", "make-electronics-plates.py", "requirements.lock"):
        z.write(ROOT / name, "source/" + name)
print(json.dumps(manifest["depth_budget"], indent=2))
print(archive)
