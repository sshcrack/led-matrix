#!/usr/bin/env python3
"""Export one real panel trial carrier plus five test coupons in a single-job geometry layout."""

import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

import cadquery as cq
import trimesh
from trimesh.exchange.threemf import export_3MF
from electronics_plates import DC_SOCKET, HALO_CHANNEL, HALO_STRIP_MM, build_dc_socket_coupon, build_halo_channel_coupon
from trial_carrier import (
    build_trial_carrier,
    JOINER_HOLES,
    NOTCH_MM,
    OUTER_MM,
    SIGHT_HOLES,
    SIGHT_HOLE_MM,
)

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--output", type=Path, default=ROOT / "exports" / "trial-carrier-kit"
)
args = parser.parse_args()
out = args.output.resolve()
measurements = json.loads((ROOT / "mounting-measurements.json").read_text())
basis = json.loads((ROOT / "measurement-basis.json").read_text())
fit = measurements.get("waveshare_four_corner_paper_fit")
if not fit or "100 mm" not in fit["scale_check"]:
    parser.error(
        "Owner-confirmed four-corner paper fit and 100 mm scale check required"
    )
centres = basis["official_mechanical_drawing_candidate"]["drawing_centred_corner_xy_mm"]
if sorted(centres) != [[-87, -79], [-87, 79], [87, -79], [87, 79]]:
    parser.error(
        "Mounting basis changed; review this trial-carrier envelope before export"
    )
out.mkdir(parents=True, exist_ok=True)

# Pixel grid sample: a square cell around each LED on the panel's 3 mm pitch, to compare
# separated square pixels (grid + diffuser) with the bare dot look. Rests on the LED tips;
# the walls sit over the 0.9 mm gaps between the 2.1 mm LEDs. Two depths in one piece.
PIXEL_GRID = {
    "pitch": 3.0,  # P3 panel
    "cells": 16,
    "wall": 0.8,  # two perimeters with a 0.4 mm nozzle
    "outer_wall": 1.0,
    "heights": (3.0, 6.0),  # left 8 columns, right 8 columns
}


def build_pixel_grid_coupon():
    g = PIXEL_GRID
    pitch, n, wall, rim = g["pitch"], g["cells"], g["wall"], g["outer_wall"]
    span = n * pitch + wall + 2 * (rim - wall)  # outer size
    half = n // 2
    low, high = g["heights"]
    # Low half (columns 0..7) and high half (columns 8..15) as one solid, then cut the cells.
    split = -span / 2 + (rim - wall) + half * pitch + wall  # the shared wall belongs to the high half
    grid = (
        cq.Workplane("XY").box(split + span / 2, span, low, centered=False).translate((-span / 2, -span / 2, 0))
        .union(cq.Workplane("XY").box(span / 2 - split + wall, span, high, centered=False).translate((split - wall, -span / 2, 0)))
    )
    x0 = -span / 2 + rim
    for i in range(n):
        for j in range(n):
            cell = pitch - wall
            grid = grid.cut(
                cq.Workplane("XY").box(cell, cell, high, centered=False).translate((x0 + i * pitch, x0 + j * pitch, 0))
            )
    return grid


# Cable tunnel gauge: three tunnels with the clearances a cable gets where it crosses a carrier
# edge: 6 mm under the 4 mm rim, 12 mm under the web band, and 9 mm in between. Lay the HUB75
# ribbon or the power cable as it will run and slide the gauge over it.
CABLE_TUNNELS = {"heights": (6.0, 9.0, 12.0), "width": 23.0, "length": 15.0, "wall": 2.5, "roof": 2.0}


def build_cable_tunnel_gauge():
    t = CABLE_TUNNELS
    w, wall, roof, length = t["width"], t["wall"], t["roof"], t["length"]
    gauge = None
    for i, h in enumerate(t["heights"]):
        y0 = i * (w + wall)
        block = cq.Workplane("XY").box(length, w + 2 * wall, h + roof, centered=False).translate((0, y0, 0))
        block = block.cut(cq.Workplane("XY").box(length, w, h, centered=False).translate((0, y0 + wall, 0)))
        gauge = block if gauge is None else gauge.union(block)
    span = len(t["heights"]) * (w + wall) + wall
    return gauge.translate((-length / 2, -span / 2, 0))


carrier = build_trial_carrier()
# Screw test coupon (80 x 24 x 10 mm). Row A (+Y): clearance through holes. Row B (-Y): pilot
# holes for screws that cut their own thread in PETG (M3 for the plates and rear cover, M2.5 for
# the Pi) and an M3 nut trap for the seam joiners. Notch at the 3.2 mm end.
SCREW_COUPON = {
    "size": (80.0, 24.0, 10.0),
    "clearance_mm": [3.2, 3.4, 3.5, 3.6],  # row A at x -27, -9, 9, 27
    "pilot_mm": {"M3": [2.4, 2.6, 2.8], "M2.5": [2.0, 2.2]},  # row B at x -30, -18, -6, 6, 18
    "nut_trap": {"across_flats": 5.8, "depth": 2.5, "through": 3.4, "x": 30.0},  # M3 nut is 5.5 AF
}
CLEARANCE_X = [-27, -9, 9, 27]
PILOT_X = [-30, -18, -6, 6, 18]
ROW_A, ROW_B = 6.0, -6.0
sx, sy, sz = SCREW_COUPON["size"]
coupon = cq.Workplane("XY").box(sx, sy, sz, centered=(True, True, False))
for x, diameter in zip(CLEARANCE_X, SCREW_COUPON["clearance_mm"]):
    coupon = coupon.cut(cq.Workplane("XY").center(x, ROW_A).circle(diameter / 2).extrude(sz))
pilots = SCREW_COUPON["pilot_mm"]["M3"] + SCREW_COUPON["pilot_mm"]["M2.5"]
for x, diameter in zip(PILOT_X, pilots):
    coupon = coupon.cut(cq.Workplane("XY").center(x, ROW_B).circle(diameter / 2).extrude(sz))
nut = SCREW_COUPON["nut_trap"]
coupon = coupon.cut(cq.Workplane("XY").center(nut["x"], ROW_B).circle(nut["through"] / 2).extrude(sz))
coupon = coupon.cut(
    cq.Workplane("XY").center(nut["x"], ROW_B).polygon(6, nut["across_flats"] / 0.8660254)
    .extrude(nut["depth"]).translate((0, 0, sz - nut["depth"]))
)
# Notched corner identifies the 3.2 mm end without font dependencies.
coupon = coupon.cut(cq.Workplane("XY").box(3, 3, 2 * sz).translate((-sx / 2 + 1, sy / 2 - 1, 0)))
manifest = {
    "revision": "trial-carrier-v6",
    "status": "One-panel carrier geometry trial plus screw-test, halo-channel, DC-socket and pixel-grid coupons and a cable tunnel gauge; no wall-mount or finished product approval.",
    "pattern_mm": centres,
    "corner_span_mm": [174, 158],
    "carrier_clearance_diameter_mm": 3.5,
    "carrier_geometry_mm": {
        "body_side": 191.6,
        "rear_web": 4,
        "rim_above_web": 6,
        "boss_above_web": 12,
        "total_height": 16,
    },
    "provisional_seam_holes_mm": JOINER_HOLES,
    "seam_holes_note": "Symmetric on all four edges, so one carrier fits every tile of the 2x2 array",
    "intermediate_insert_sight_holes_mm": {"diameter": SIGHT_HOLE_MM, "centres": SIGHT_HOLES},
    "sight_holes_note": "Both handedness variants of the drawing's +/-7.5 mm stagger; inspection only, not mounting holes",
    "orientation_notch": "8 x 2 x 2 mm notch in the top outer corner of the +Y rib, centred on X=0",
    "xy_view": "Bed view: looking down on the printed part, bosses toward the viewer, X right, Y up (= panel rear seen from behind)",
    "screw_test_coupon": SCREW_COUPON,
    "halo_channel_coupon": {"strip_mm": HALO_STRIP_MM, **HALO_CHANNEL},
    "dc_socket_coupon": DC_SOCKET,
    "pixel_grid_coupon": PIXEL_GRID,
    "cable_tunnel_gauge": CABLE_TUNNELS,
    "print_scale_percent": 100,
    "paper_scale_compensation_applied": False,
    "material": "PETG starting material; print service selects settings and quote.",
    "target_printer": "Prusa i3 MK3S: X 250 x Y 210 x Z 210 mm (owner's '210 x 250' is the same bed)",
    "layout_fits_beds_mm": [[250, 210, 210], [210, 250, 210], [400, 400, 500]],
    "parts": [],
}
meshes = {}
for name, shape in [
    ("trial-panel-carrier", carrier),
    ("screw-test-coupon", coupon),
    # Print orientations that need no supports (owner's slicer, 2026-11-16): the halo coupon
    # lies upside down, lips on the bed, so each channel's roof is a short bridge (the rear
    # cover will print the same way); the tunnel gauge stands on its end, tunnels vertical.
    ("halo-channel-coupon", build_halo_channel_coupon().rotate((0, 0, 0), (0, 1, 0), 180)),
    ("dc-socket-coupon", build_dc_socket_coupon()),
    ("pixel-grid-coupon", build_pixel_grid_coupon()),
    ("cable-tunnel-gauge", build_cable_tunnel_gauge().rotate((0, 0, 0), (0, 1, 0), 90)),
]:
    solid = shape.val()
    assert solid.isValid() and len(solid.Solids()) == 1, name
    bounds = solid.BoundingBox()
    # Both flat bases lie on Z=0; positive XY is convenient for print-service imports.
    shape = shape.translate((-bounds.xmin, -bounds.ymin, -bounds.zmin))
    bounds = shape.val().BoundingBox()
    assert bounds.xlen + 10 <= 210 and bounds.ylen + 10 <= 210 and bounds.zlen <= 210
    stl, step = out / (name + ".stl"), out / (name + ".step")
    cq.exporters.export(shape, str(stl), tolerance=0.02, angularTolerance=0.1)
    cq.exporters.export(shape, str(step))
    mesh = trimesh.load(stl, force="mesh")
    meshes[name] = mesh
    assert mesh.is_watertight and mesh.is_winding_consistent and mesh.body_count == 1, (
        name
    )
    mesh_error = abs(mesh.volume - shape.val().Volume()) / shape.val().Volume()
    assert mesh_error < 0.005, (name, mesh_error)
    imported = cq.importers.importStep(str(step)).val()
    assert imported.isValid() and len(imported.Solids()) == 1
    assert abs(imported.Volume() / shape.val().Volume() - 1) < 1e-6
    manifest["parts"].append(
        {
            "stl": stl.name,
            "step": step.name,
            "bounds_mm": [
                round(bounds.xlen, 3),
                round(bounds.ylen, 3),
                round(bounds.zlen, 3),
            ],
            "volume_cm3": round(shape.val().Volume() / 1000, 3),
            "stl_sha256": hashlib.sha256(stl.read_bytes()).hexdigest(),
            "valid_single_solid": True,
            "watertight_single_mesh": True,
            "mesh_volume_relative_error": mesh_error,
            "step_roundtrip": "valid single solid, volume agreement within 1 ppm",
            "bed_fit": "5 mm margin on all XY sides of a 210 mm square; check the slicer skirt/brim separately",
        }
    )
# Probe full through bores and supporting material at web and boss heights.
for x, y in centres:
    for z in [2, 12, 15.9]:
        assert not carrier.val().isInside((x, y, z))
        assert not carrier.val().isInside((x + 1.74, y, z))
        assert carrier.val().isInside((x + 1.76, y, z))
assert len(JOINER_HOLES) == 8 and len(set(JOINER_HOLES)) == 8
for x, y in JOINER_HOLES:
    # One STL for every tile: the seam-hole set is symmetric under X and Y mirror.
    assert (-x, y) in JOINER_HOLES and (x, -y) in JOINER_HOLES
    assert not carrier.val().isInside((x, y, 2))
    assert carrier.val().isInside((x + 1.76, y, 2))
for x, y in SIGHT_HOLES:
    assert not carrier.val().isInside((x, y, 2))
    assert not carrier.val().isInside((x + SIGHT_HOLE_MM / 2 - 0.01, y, 2))
    assert carrier.val().isInside((x + SIGHT_HOLE_MM / 2 + 0.01, y, 2))
nx, ny, nz = NOTCH_MM
assert not carrier.val().isInside((0, OUTER_MM / 2 - ny / 2, 4 + 6 - nz / 2))
assert carrier.val().isInside((0, OUTER_MM / 2 - ny / 2, 4 + 6 - nz - 0.5))
assert carrier.val().isInside((0, OUTER_MM / 2 - ny - 0.5, 9))
assert carrier.val().isInside((0, -(OUTER_MM / 2 - ny / 2), 9))
assert carrier.val().isInside((90, 0, 2))
assert not carrier.val().isInside((87, 0, 6))
assert carrier.val().isInside((94, 0, 8))
assert not carrier.val().isInside((94, 0, 12))
assert not carrier.val().isInside((0, 0, 2))
holes = [(x, ROW_A, d) for x, d in zip(CLEARANCE_X, SCREW_COUPON["clearance_mm"])]
holes += [(x, ROW_B, d) for x, d in zip(PILOT_X, pilots)]
for x, y, diameter in holes:
    for z in (0.5, sz / 2, sz - 0.5):
        assert not coupon.val().isInside((x + diameter / 2 - 0.01, y, z))
        assert coupon.val().isInside((x + diameter / 2 + 0.01, y, z))
af = nut["across_flats"]
assert not coupon.val().isInside((nut["x"], ROW_B + af / 2 - 0.01, sz - 0.5))  # trap flat (flats face +/-Y)
assert coupon.val().isInside((nut["x"], ROW_B + af / 2 + 0.01, sz - 0.5))
assert coupon.val().isInside((nut["x"], ROW_B + af / 2 - 0.01, sz - nut["depth"] - 0.5))  # trap floor
assert not coupon.val().isInside((nut["x"], ROW_B, 1.0))  # through hole
grid = build_pixel_grid_coupon().val()
g = PIXEL_GRID
span = g["cells"] * g["pitch"] + g["wall"] + 2 * (g["outer_wall"] - g["wall"])
first = -span / 2 + g["outer_wall"] + (g["pitch"] - g["wall"]) / 2  # centre of the first cell
for i in range(g["cells"]):
    for j in (0, g["cells"] - 1):
        assert not grid.isInside((first + i * g["pitch"], first + j * g["pitch"], 1.5))  # open cell
for i in range(g["cells"] - 1):
    wall_x = first + i * g["pitch"] + g["pitch"] / 2
    assert grid.isInside((wall_x, first, 1.5))  # wall between cells
assert grid.isInside((first + g["pitch"] / 2, first, g["heights"][0] - 0.1))
assert not grid.isInside((first + g["pitch"] / 2, first, g["heights"][0] + 0.1))  # low half
last_wall = first + (g["cells"] - 2) * g["pitch"] + g["pitch"] / 2
assert grid.isInside((last_wall, first, g["heights"][1] - 0.1))  # high half
assert abs(grid.BoundingBox().xlen - span) < 1e-6 and abs(grid.BoundingBox().zlen - g["heights"][1]) < 1e-6
tg = build_cable_tunnel_gauge().val()
t = CABLE_TUNNELS
span = len(t["heights"]) * (t["width"] + t["wall"]) + t["wall"]
for i, h in enumerate(t["heights"]):
    yc = -span / 2 + t["wall"] + i * (t["width"] + t["wall"]) + t["width"] / 2
    assert not tg.isInside((0, yc, h - 0.1)) and not tg.isInside((0, yc + t["width"] / 2 - 0.1, 0.5))  # open tunnel
    assert tg.isInside((0, yc, h + t["roof"] / 2))  # roof
assert abs(tg.BoundingBox().ylen - span) < 1e-6
manifest["cable_tunnel_probes"] = "6, 9 and 12 mm tunnels open to full width, roofs present"
manifest["pixel_grid_probes"] = "all edge cells open, walls on the 3 mm pitch, 3 mm and 6 mm halves checked"
manifest["coupon_radius_probes"] = "4 clearance and 5 pilot radii at three heights, nut trap flats, floor and through hole checked within 0.01 mm"
manifest["carrier_probes"] = (
    "four 3.5 mm through bores at web/boss heights, eight symmetric seam bores, six sight holes, "
    "orientation notch, web, rim and open middle checked"
)
scene = trimesh.Scene()
CARRIER_AT = (5.0, 5.0)
CENTRE = (CARRIER_AT[0] + OUTER_MM / 2, CARRIER_AT[1] + OUTER_MM / 2)
scene.add_geometry(
    meshes["trial-panel-carrier"],
    geom_name="trial-panel-carrier",
    node_name="trial-panel-carrier",
    transform=trimesh.transformations.translation_matrix([*CARRIER_AT, 0]),
)
# The coupons nest in the carrier's 159.6 mm open middle (centres relative to the carrier
# centre), so the whole job stays a 201.6 mm square and fits the MK3S bed either way round.
NESTED = {
    "screw-test-coupon": (-29.7, 57.5),
    "halo-channel-coupon": (-50.25, -34.5),
    "dc-socket-coupon": (-8.0, -49.5),
    "pixel-grid-coupon": (45.1, 45.1),
    "cable-tunnel-gauge": (40.0, -30.1),
}
nested_bounds = {}
for name, (cx, cy) in NESTED.items():
    size = meshes[name].extents
    offset = [CENTRE[0] + cx - size[0] / 2, CENTRE[1] + cy - size[1] / 2, 0]
    scene.add_geometry(
        meshes[name],
        geom_name=name,
        node_name=name,
        transform=trimesh.transformations.translation_matrix(offset),
    )
    nested_bounds[name] = meshes[name].bounds + [offset] * 2
layout = out / "single-job-layout.3mf"
layout.write_bytes(export_3MF(scene))
loaded = trimesh.load_scene(layout)
assert len(loaded.graph.nodes_geometry) == 1 + len(NESTED)
assert abs(loaded.bounds - scene.bounds).max() < 1e-6
assert abs(loaded.volume / scene.volume - 1) < 1e-6
assert loaded.bounds[0][0] >= 5 and loaded.bounds[0][1] >= 5
assert loaded.bounds[1][0] <= 205 and loaded.bounds[1][1] <= 205
opening = [CARRIER_AT[0] + 16, CARRIER_AT[0] + OUTER_MM - 16]
coupon_gap = min(
    min(b[0][0] - opening[0], b[0][1] - opening[0], opening[1] - b[1][0], opening[1] - b[1][1])
    for b in nested_bounds.values()
)
names = list(nested_bounds)
for i, a in enumerate(names):
    for b in names[i + 1 :]:
        A, B = nested_bounds[a], nested_bounds[b]
        apart = max(B[0][0] - A[1][0], A[0][0] - B[1][0], B[0][1] - A[1][1], A[0][1] - B[1][1])
        coupon_gap = min(coupon_gap, apart)
assert coupon_gap >= 10, coupon_gap
assert loaded.bounds[0][2] >= -1e-6
assert loaded.bounds[1][2] <= 16.00001
manifest["single_job_layout"] = {
    "file": layout.name,
    "objects": 1 + len(NESTED),
    "bounds_mm": loaded.bounds.tolist(),
    "minimum_xy_margin_mm": 5,
    "coupon_placement": "all five coupons inside the carrier's open middle",
    "gap_between_parts_mm": round(float(coupon_gap), 2),
    "fits": "Prusa MK3S 250 x 210 in either orientation, and the 400 x 400 printer",
    "generic_3mf_roundtrip": "six objects, transformed bounds and volume preserved",
    "machine_settings": "none; print service must slice and quote",
}

def render_preview(path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    from matplotlib.patches import Circle, Rectangle
    from matplotlib.path import Path as MplPath
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    fig = plt.figure(figsize=(12.2, 7.6), dpi=100)
    fig.suptitle("Carrier + five test coupons v6 / PETG / 100% scale", fontsize=15)
    ax3 = fig.add_subplot(1, 2, 1, projection="3d")
    mesh = meshes["trial-panel-carrier"]
    light = mesh.face_normals @ [0.3, -0.4, 0.86]
    shade = 0.35 + 0.45 * (light.clip(-1, 1) + 1) / 2
    ax3.add_collection3d(
        Poly3DCollection(
            mesh.triangles,
            facecolors=[(0.21 * k / 0.6, 0.34 * k / 0.6, 0.42 * k / 0.6) for k in shade],
            linewidths=0,
        )
    )
    ax3.set(xlim=(0, OUTER_MM), ylim=(0, OUTER_MM), zlim=(-60, 80))
    ax3.view_init(elev=32, azim=-62)
    ax3.set_box_aspect((1, 1, 0.73))
    ax3.set_axis_off()
    ax3.set_title(
        "One-panel trial carrier (bosses up)\n4 mounting bosses, 8 symmetric seam holes,\n"
        "6 sight holes over intermediate inserts, notch on +Y rib",
        fontsize=10,
    )
    ax = fig.add_subplot(1, 2, 2)
    ax.add_patch(Rectangle((0, 0), 250, 210, fill=False, lw=1.4, ls="--", ec="0.45"))
    ax.text(247, 203, "MK3S bed X 250 x Y 210", ha="right", va="top", fontsize=8, color="0.35")
    loops = []
    for node in loaded.graph.nodes_geometry:
        transform, geom = loaded.graph[node]
        part = loaded.geometry[geom].copy().apply_transform(transform)
        section = part.section(plane_origin=[0, 0, 1], plane_normal=[0, 0, 1])
        loops += [MplPath(loop[:, :2]) for loop in section.discrete]
    # Nesting depth decides solid (even) or hole (odd); draw outermost first.
    loops.sort(key=lambda loop: -abs(np.ptp(loop.vertices, axis=0).prod()))
    for loop in loops:
        depth = sum(
            other is not loop and other.contains_point(loop.vertices[0]) for other in loops
        )
        ax.fill(*loop.vertices.T, color="white" if depth % 2 else "#36566b", lw=0)
    for x, y in centres:
        ax.add_patch(Circle((5 + OUTER_MM / 2 + x, 5 + OUTER_MM / 2 + y), 5, fill=False, ec="#d9822b", lw=1.2))
    ax.set(xlim=(0, 250), ylim=(0, 210), aspect="equal", xlabel="mm", ylabel="mm")
    ax.set_title(
        "Single job: screw-test, halo-channel, DC-socket, pixel-grid and\ncable-tunnel coupons nested in the carrier's open middle, "
        "201.6 mm square, fits the bed either way round",
        fontsize=10,
    )
    fig.text(
        0.5,
        0.02,
        "Section at the 4 mm web; orange = boss outlines. Geometry preview, not a slicer quote "
        "or wall-mount approval.",
        ha="center",
        fontsize=9,
    )
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)


render_preview(out / "trial-carrier-preview.png")
(out / "trial-carrier-validation.json").write_text(
    json.dumps(manifest, indent=2) + "\n"
)
instructions = ROOT / "trial-carrier-instructions.txt"
(out / "READ-ME-FIRST.txt").write_text(instructions.read_text())
archive = out / "panel-trial-carrier-kit-v6.zip"
with ZipFile(archive, "w", ZIP_DEFLATED) as z:
    for name in [
        "trial-panel-carrier.stl",
        "trial-panel-carrier.step",
        "screw-test-coupon.stl",
        "screw-test-coupon.step",
        "halo-channel-coupon.stl",
        "halo-channel-coupon.step",
        "dc-socket-coupon.stl",
        "dc-socket-coupon.step",
        "pixel-grid-coupon.stl",
        "pixel-grid-coupon.step",
        "cable-tunnel-gauge.stl",
        "cable-tunnel-gauge.step",
        "trial-carrier-validation.json",
        "READ-ME-FIRST.txt",
        "single-job-layout.3mf",
        "trial-carrier-preview.png",
    ]:
        z.write(out / name, name)
    z.write(Path(__file__), "source/make-trial-carrier.py")
    for name in [
        "trial_carrier.py",
        "electronics_plates.py",
        "measurement-basis.json",
        "mounting-measurements.json",
        "trial-carrier-instructions.txt",
        "requirements.lock",
        "README.md",
    ]:
        z.write(ROOT / name, "source/" + name)
print(json.dumps(manifest, indent=2))
print(archive)
