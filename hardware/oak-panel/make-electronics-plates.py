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
    DC_SOCKET,
    MODULE_CENTRES,
    PI_CENTRE,
    PI_HOLES,
    PI_STANDOFF,
    PLATE_MM,
    SEAM_HOLES,
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
    "revision": "electronics-plates-v1",
    "status": "Prototype study for the off-the-shelf wiring plan; not printed or fitted.",
    "plate_mm": PLATE_MM,
    "mounting": "M3 through four carrier seam holes at (+-86, +-54), nut on the carrier side",
    "components": {k: {"x": v[0], "y": v[1], "height": v[2], "source": v[3]} for k, v in COMPONENTS.items()},
    "dc_socket": DC_SOCKET,
    "depth_budget": depth_budget(),
    "current_body_depth_mm": 60.0,
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

# Probes: seam holes open, plate solid next to them, Pi standoff pilots open and supported.
t = PLATE_MM[2]
for name in ("power-plate", "controller-plate"):
    s = parts[name].val()
    for x, y in SEAM_HOLES:
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
manifest["probes"] = "seam holes, Pi standoff pilots, module seats/slots and socket coupon holes checked"


def render_preview(path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    from matplotlib.patches import Rectangle
    from matplotlib.path import Path as MplPath

    fig, axes = plt.subplots(1, 3, figsize=(15, 5.6), gridspec_kw={"width_ratios": [1, 1, 0.45]})
    fig.suptitle("Electronics plates v1 (bed view, components on top) / PETG / 100 %", fontsize=13)

    def section(ax, mesh, z):
        loops = [MplPath(l[:, :2]) for l in mesh.section(plane_origin=[0, 0, z], plane_normal=[0, 0, 1]).discrete]
        loops.sort(key=lambda l: -abs(np.ptp(l.vertices, axis=0).prod()))
        for loop in loops:
            depth = sum(o is not loop and o.contains_point(loop.vertices[0]) for o in loops)
            ax.fill(*loop.vertices.T, color="white" if depth % 2 else "#36566b", lw=0)

    def outline(ax, cx, cy, w, hgt, label, color):
        ax.add_patch(Rectangle((cx - w / 2, cy - hgt / 2), w, hgt, fill=False, ec=color, lw=1.3, ls="--"))
        ax.text(cx, cy, label, ha="center", va="center", fontsize=7.5, color=color)

    ax = axes[0]
    section(ax, meshes["power-plate"], t / 2)
    for i, (mx, my) in enumerate(MODULE_CENTRES):
        outline(ax, mx, my, 74, 74, f"24→5 V 20 A\nmodule {'AB'[i]}\n(32 mm tall)", "#d9822b")
    outline(ax, 0, 47, 150, 18, "WAGO 221 row (cable-tied)", "#2b8a3e")
    outline(ax, 0, -47, 150, 10, "fuse holders (cable-tied)", "#2b8a3e")
    ax.set_title("Power plate: modules, WAGOs, fuses", fontsize=10)

    ax = axes[1]
    section(ax, meshes["controller-plate"], t / 2)
    px, py = PI_CENTRE
    outline(ax, px + 10, py, 85, 56, "Pi 3B + Electrodragon\non 6 mm standoffs", "#d9822b")
    outline(ax, 50, 30, 43, 21, "LM2596", "#2b8a3e")
    outline(ax, 42, -18, 33, 18, "Pro Micro", "#2b8a3e")
    outline(ax, 50, -40, 16, 13, "level conv.", "#2b8a3e")
    ax.set_title("Controller plate: Pi, LM2596, Pro Micro", fontsize=10)

    for ax in axes[:2]:
        ax.set(xlim=(-96, 96), ylim=(-64, 64), aspect="equal", xlabel="mm")

    ax = axes[2]
    section(ax, meshes["dc-socket-coupon"], 2.0)
    ax.set(aspect="equal", xlabel="mm")
    ax.set_title("DC socket wall coupon\n(Kycon KPJX-PM-4S test fit)", fontsize=10)

    db = depth_budget()
    fig.text(
        0.5,
        0.01,
        "Depth needed (LED tip → cover): power plate {:.0f} mm, controller plate {:.0f} mm (Pi stack assumed) "
        "vs current 60 mm body. Section at mid-plate; orange/green = component outlines, not geometry.".format(
            db["power_plate"]["body_depth_needed_mm"], db["controller_plate"]["body_depth_needed_mm"]
        ),
        ha="center",
        fontsize=9,
    )
    fig.savefig(path, bbox_inches="tight", dpi=100)
    plt.close(fig)


render_preview(out / "electronics-plates-preview.png")
(out / "electronics-plates-validation.json").write_text(json.dumps(manifest, indent=2) + "\n")
archive = out / "electronics-plates-v1.zip"
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
