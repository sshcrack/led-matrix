#!/usr/bin/env python3
"""Build, check and export the v0.7 rear (v07.py): STL/STEP per printed part, one 3MF per print job,
STEP per wood part, an assembly STEP with reference envelopes and a validation manifest.
The wood cut/drill sheets come from wood_drawings.py.

Checks: every part one valid solid and a watertight mesh; every print job fits the Prusa MK3S
bed (250 x 210 mm) with a 5 mm margin; no two parts or reference envelopes (panels, components,
DC socket body, wall, wall screws) overlap in the assembly; key clearances asserted; downward
overhangs per part listed for the print orientation.
"""

import argparse
import hashlib
import json
from itertools import combinations
from pathlib import Path

import cadquery as cq
import trimesh
from trimesh.exchange.threemf import export_3MF

import electronics_plates as ep
import v07

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=ROOT / "exports" / "v07")
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
BED, MARGIN = (250.0, 210.0), 5.0
QUADRANTS = [(-1, 1), (1, 1), (-1, -1), (1, -1)]
QNAME = {(-1, 1): "top-left", (1, 1): "top-right", (-1, -1): "bottom-left", (1, -1): "bottom-right"}


def at(shape, sx, sy, z=0.0):
    cx, cy = v07.quadrant_centre(sx, sy)
    return shape.translate((cx, cy, z))


def flip(shape, z_face):
    """Rotate 180 deg about X so the face at assembly Z = z_face lies on the bed (z = 0)."""
    return shape.rotate((0, 0, 0), (1, 0, 0), 180).translate((0, 0, z_face))


# --- Parts: (assembly shape, print shape, quantity per design) --------------------------------
parts = {}
carrier_local = v07.build_carrier()
parts["carrier"] = dict(asm=[at(carrier_local, sx, sy) for sx, sy in QUADRANTS], print=flip(carrier_local, v07.Z_REAR), qty=4)
piece = v07.build_halo_piece()
parts["halo-piece"] = dict(
    asm=[v07.place_halo_piece(piece, e, h) for _, e, h in v07.halo_piece_placements()], print=piece, qty=8
)
power = v07.build_power_plate_v07()
ctrl = v07.build_controller_plate_v07()
parts["power-plate"] = dict(asm=[at(power, *v07.POWER_QUADRANT, v07.plate_z())], print=power, qty=1)
parts["controller-plate"] = dict(asm=[at(ctrl, *v07.CONTROLLER_QUADRANT, v07.plate_z())], print=ctrl, qty=1)
joiners = [v07.build_joiner(*p) for p in v07.joiner_positions()]
parts["joiner"] = dict(asm=joiners, print=v07.build_joiner(0, 0, True).translate((0, 0, -v07.Z_REAR)), qty=8)
lip_front, rail_back = v07.rail_z()
rails = {sx: v07.build_rail(sx) for sx in (-1, 1)}
parts["rail-left"] = dict(asm=[at(rails[-1], -1, 1)], print=flip(rails[-1], rail_back), qty=1)
parts["rail-right"] = dict(asm=[at(rails[1], 1, 1)], print=flip(rails[1], rail_back), qty=1)
bumper = v07.build_bumper(1)
parts["bumper"] = dict(asm=[at(v07.build_bumper(sx), sx, -1) for sx in (-1, 1)], print=flip(bumper, v07.Z_WALL), qty=2)

# --- Wood (bought, cut and drilled by hand) ---------------------------------------------------
wood = dict(v07.build_strips())
wood["hdf-back"] = v07.build_hdf()

# --- Reference envelopes ---------------------------------------------------------------------
refs = {}
for sx, sy in QUADRANTS:
    h = v07.HALF
    refs[f"panel-{QNAME[(sx, sy)]}"] = at(v07.box(-h, h, -h, h, 0, v07.Z_INSERT), sx, sy)
for plate, quad in (("power-plate", v07.POWER_QUADRANT), ("controller-plate", v07.CONTROLLER_QUADRANT)):
    for name, env in v07.component_envelopes(plate):
        refs[f"{plate}:{name}"] = at(env, *quad)
d, depth = v07.DC_SOCKET_BODY
refs["dc-socket-body"] = v07.cyl_y(v07.DC_SOCKET_X, v07.DC_SOCKET_Z, d, -v07.STRIP_OUT, -v07.STRIP_OUT + depth)
refs["wall"] = v07.box(-300, 300, -300, 300, v07.Z_WALL, v07.Z_WALL + 5)
ws = v07.WALL_SCREW
screw_y = v07.PITCH / 2 + v07.RAIL_LIP_Y0 - ws["shank_d"] / 2  # shank top touches the lip edge
for s in (-1, 1):
    x = s * ws["spacing"] / 2
    refs[f"wall-screw-shank-{s}"] = v07.cyl(x, screw_y, ws["shank_d"], v07.Z_WALL - ws["head_gap"], v07.Z_WALL)
    refs[f"wall-screw-head-{s}"] = v07.cyl(
        x, screw_y, ws["head_d"], v07.Z_WALL - ws["head_gap"] - ws["head_t"], v07.Z_WALL - ws["head_gap"]
    )

# --- Assembly interference -------------------------------------------------------------------
solids = []
for name, p in parts.items():
    for i, s in enumerate(p["asm"]):
        solids.append((f"{name}#{i}", s.val()))
solids += [(n, s.val()) for n, s in wood.items()]
solids += [(n, s.val()) for n, s in refs.items()]
for n, s in solids:
    assert s.isValid() and len(s.Solids()) == 1, n
clashes = []
for (na, a), (nb, b) in combinations(solids, 2):
    ba, bb = a.BoundingBox(), b.BoundingBox()
    if ba.xmax <= bb.xmin or bb.xmax <= ba.xmin or ba.ymax <= bb.ymin or bb.ymax <= ba.ymin or ba.zmax <= bb.zmin or bb.zmax <= ba.zmin:
        continue
    v = a.intersect(b).Volume()
    if v > 1e-3:
        clashes.append((na, nb, round(v, 3)))
assert not clashes, clashes

# --- Key clearances --------------------------------------------------------------------------
tallest = {}
for n, s_ in refs.items():
    if ":" in n:
        bb = s_.val().BoundingBox()
        tallest[n] = round(bb.zmax, 2)
        # Components stay clear of the strips' inner faces.
        side = v07.STRIP_IN - max(abs(bb.xmin), abs(bb.xmax), abs(bb.ymin), abs(bb.ymax))
        assert side >= 2.0, (n, side)
assert max(tallest.values()) <= v07.Z_STRIP_BACK - 1.0, tallest
# Every carrier outer-edge pilot and every plate post under a strip gets an M3 x 40.
screws = v07.strip_screws()
assert len(screws) == 22, len(screws)
tip = v07.Z_STRIP_BACK - v07.STRIP_CBORE[1] - 0.8 - 40.0  # M3 x 40 on a 0.8 mm washer
grip = {
    "carrier": v07.Z_REAR - max(tip, v07.Z_WEB_FRONT - v07.JOINT_BOSS[1]),
    "plate post": v07.Z_WEB_FRONT - max(tip, v07.plate_z()),
}
assert tip >= v07.Z_INSERT + 1.0 and tip >= v07.plate_z(), tip  # never touches a panel
assert min(grip.values()) >= 8.0, grip
# Wood screws keep clear of the M3 holes and counterbores.
for hx, hy in v07.halo_screws() + [(x, y) for x, y, _, _ in v07.hdf_holes("corner")]:
    for x, y, _ in screws:
        dist = ((hx - x) ** 2 + (hy - y) ** 2) ** 0.5
        assert dist - v07.STRIP_CBORE[0] / 2 - v07.WOOD_SCREW[0] / 2 >= 1.5, (hx, hy, x, y, dist)
# The screws through the wood must land in wood.
boxes = v07.strip_boxes()
for hx, hy in v07.halo_screws():
    assert any(x0 + 3 <= hx <= x1 - 3 and y0 + 3 <= hy <= y1 - 3 for x0, x1, y0, y1 in boxes.values()), (hx, hy)
joint_boss_bottom = v07.Z_WEB_FRONT - v07.JOINT_BOSS[1]
plate_top = v07.plate_z() + ep.PLATE_T
assert joint_boss_bottom - plate_top >= 1.0
lip_overlap = (screw_y + ws["head_d"] / 2) - (v07.PITCH / 2 + v07.RAIL_LIP_Y0)
assert lip_overlap >= 2.0, lip_overlap  # how far the lip reaches down behind a 10 mm head
head_room = (v07.PITCH / 2 + v07.RAIL_WEB_Y[0]) - (screw_y + ws["head_d"] / 2)
assert head_room >= 5.0, head_room
assert v07.Z_WALL - rail_back >= 5.0  # rail's back stays off the wall
shift = []
for s in (-1, 1):
    xs = sorted((s * v07.RAIL_X_IN, s * v07.RAIL_X_OUT))
    lo, hi = s * v07.PITCH / 2 + xs[0], s * v07.PITCH / 2 + xs[1]
    shift.append((lo - s * ws["spacing"] / 2, hi - s * ws["spacing"] / 2))
sideways = (max(a for a, _ in shift), min(b for _, b in shift))
assert sideways[0] <= -20 and sideways[1] >= 20, sideways
# Probe: carrier pilot, bores, notches; strip holes; halo channel.
c = carrier_local.val()
for x, y in v07.JOINT_HOLES:
    assert not c.isInside((x, y, 25.0)) and c.isInside((x + v07.M3_PILOT / 2 + 0.3, y, 25.0))
for x, y in v07.PATTERN:
    assert not c.isInside((x, y, 20.0)) and c.isInside((x + v07.M3_CLEAR / 2 + 0.3, y, 20.0))
assert not c.isInside((94.0, 50.0, 22.5)) and c.isInside((94.0, 0.0, 22.5))
top = wood["strip-top"].val()
x, y, _ = next(p for p in screws if p[1] > v07.STRIP_IN)
assert not top.isInside((x, y, v07.Z_REAR + 5)) and top.isInside((x + v07.STRIP_HOLE / 2 + 0.5, y, v07.Z_REAR + 5))
assert not top.isInside((x + 4.5, y, v07.Z_STRIP_BACK - 1)) and top.isInside((x + 4.5, y, v07.Z_STRIP_BACK - 4))
bottom = wood["strip-bottom"].val()
assert not bottom.isInside((v07.DC_SOCKET_X, -v07.HALF - 1, v07.DC_SOCKET_Z))
h = parts["halo-piece"]["asm"][0].val()
assert not h.isInside((60.0, v07.HALO_C, v07.Z_BACK + v07.HALO_DEPTH - 0.5))  # opening
assert h.isInside((60.0, v07.HALO_C + v07.HALO["inner_w"] / 2 - 0.3, v07.Z_BACK + v07.HALO_DEPTH - 0.5))  # lip
assert not h.isInside((60.0, v07.HALO_C + v07.HALO["inner_w"] / 2 - 0.3, v07.Z_BACK + v07.HALO["floor"] + 1))  # cavity
assert h.isInside((60.0, v07.HALO_C + v07.HALO_BAND_W / 2 - 0.5, v07.Z_BACK + 1))  # wall

# --- Export parts ----------------------------------------------------------------------------
manifest = {
    "design": "led-matrix v0.7 rear parts (front frame/diffuser not included)",
    "basis": "Trial kit v6 results, issue #170; fasteners #172; wall mount #171 (2026-11-17)",
    "provisional": {
        "m3_pilot_mm": v07.M3_PILOT,
        "m2_5_pilot_mm": v07.M25_PILOT,
        "note": "set from the screw-coupon test before the final print",
    },
    "stack_mm_from_led_tips": {
        "insert_face": v07.Z_INSERT,
        "carrier_rear": v07.Z_REAR,
        "strip_back_inside_ceiling": v07.Z_STRIP_BACK,
        "hdf_rear_face": v07.Z_BACK,
        "halo_channel_opening": round(v07.Z_BACK + v07.HALO_DEPTH, 2),
        "wall": round(v07.Z_WALL, 2),
        "tallest_component": max(tallest.values()),
        "m3x40_tip": round(tip, 2),
        "m3x40_thread_engagement": {k: round(v, 2) for k, v in grip.items()},
    },
    "hanging": {
        "wall_screw_spacing_mm": ws["spacing"],
        "lip_overlap_behind_10mm_head_mm": round(lip_overlap, 2),
        "sideways_adjustment_mm": [round(v, 1) for v in sideways],
        "screw_height_above_display_centre_mm": round(screw_y, 1),
    },
    "assembly_interference": "none (printed parts, wood, panels, components, DC socket body, wall, wall screws)",
    "wood": {n: [round(v, 1) for v in (lambda b: (b.xlen, b.ylen, b.zlen))(w.val().BoundingBox())] for n, w in wood.items()},
    "parts": {},
    "jobs": {},
}
meshes = {}
for name, p in parts.items():
    shape = p["print"]
    b = shape.val().BoundingBox()
    shape = shape.translate((-b.xmin, -b.ymin, -b.zmin))
    stl, step = out / f"{name}.stl", out / f"{name}.step"
    cq.exporters.export(shape, str(stl), tolerance=0.02, angularTolerance=0.1)
    cq.exporters.export(shape, str(step))
    mesh = trimesh.load(stl, force="mesh")
    assert mesh.is_watertight and mesh.body_count == 1, name
    err = abs(mesh.volume / shape.val().Volume() - 1)
    assert err < 0.005, (name, err)
    meshes[name] = mesh
    down = mesh.face_normals[:, 2] < -0.7
    lifted = mesh.triangles_center[:, 2] > 0.2
    sel = down & lifted
    levels = {}
    for zc, area in zip(mesh.triangles_center[sel][:, 2], mesh.area_faces[sel]):
        levels[round(float(zc), 1)] = levels.get(round(float(zc), 1), 0) + float(area)
    bb = mesh.extents
    assert bb[0] + 2 * MARGIN <= BED[0] and bb[1] + 2 * MARGIN <= BED[1] and bb[2] <= 210, (name, bb)
    manifest["parts"][name] = {
        "quantity": p["qty"],
        "size_mm": [round(float(v), 1) for v in bb],
        "volume_cm3": round(shape.val().Volume() / 1000, 1),
        "downward_overhang_mm2_by_height": {k: round(v, 1) for k, v in sorted(levels.items())},
        "stl_sha256": hashlib.sha256(stl.read_bytes()).hexdigest(),
    }

# --- Print jobs (one 3MF each; quantities say how often to print the job) --------------------
def layout(job, items, qty):
    scene = trimesh.Scene()
    boxes = []
    for name, (x, y) in items:
        m = meshes[name]
        scene.add_geometry(m, geom_name=f"{name}@{x},{y}", transform=trimesh.transformations.translation_matrix([x, y, 0]))
        boxes.append((name, x, y, x + m.extents[0], y + m.extents[1]))
    for _, x0, y0, x1, y1 in boxes:
        assert x0 >= MARGIN and y0 >= MARGIN and x1 <= BED[0] - MARGIN and y1 <= BED[1] - MARGIN, (job, x0, y0, x1, y1)
    for (na, *a), (nb, *b) in combinations(boxes, 2):
        apart = max(b[0] - a[2], a[0] - b[2], b[1] - a[3], a[1] - b[3])
        assert apart >= 5.0, (job, na, nb, apart)
    path = out / f"job-{job}.3mf"
    path.write_bytes(export_3MF(scene))
    loaded = trimesh.load_scene(path)
    assert len(loaded.graph.nodes_geometry) == len(items)
    manifest["jobs"][path.name] = {"print_times": qty, "objects": [n for n, _ in items]}


layout("carrier", [("carrier", (5, 5))], 4)
hw = meshes["halo-piece"].extents
layout("halo-pieces", [("halo-piece", (5, 5 + i * (hw[1] + 7))) for i in range(8)], 1)
jw = meshes["joiner"].extents
layout(
    "power-plate-joiners-bumpers",
    [("power-plate", (5, 5))]
    + [("joiner", (5 + i * (jw[0] + 8), 130)) for i in range(4)]
    + [("bumper", (5 + 4 * (jw[0] + 8) + i * 24, 130)) for i in range(2)],
    1,
)
rw = meshes["rail-left"].extents
layout(
    "controller-plate-joiners-rails",
    [("controller-plate", (5, 5))]
    + [("rail-left", (5, 125)), ("rail-right", (5, 125 + rw[1] + 8))]
    + [("joiner", (5 + rw[0] + 10 + (i % 2) * (jw[0] + 8), 125 + (i // 2) * (jw[1] + 8))) for i in range(4)],
    1,
)
total = sum(manifest["parts"][n]["volume_cm3"] * manifest["parts"][n]["quantity"] for n in manifest["parts"])
manifest["total_solid_volume_cm3"] = round(total, 1)

# --- Assembly STEP (parts + reference envelopes) ---------------------------------------------
asm = cq.Assembly(name="v07_rear")
for name, p in parts.items():
    for i, s in enumerate(p["asm"]):
        asm.add(s, name=f"{name}_{i}", color=cq.Color(0.12, 0.12, 0.13))
for n, s in wood.items():
    asm.add(s, name=n, color=cq.Color(0.55, 0.42, 0.28))
    cq.exporters.export(s, str(out / f"wood-{n}.step"))
for n, s in refs.items():
    asm.add(s, name=n.replace(":", "_"), color=cq.Color(0.5, 0.6, 0.7))
asm.export(str(out / "assembly-v07.step"))


(out / "v07-validation.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps({k: manifest[k] for k in ("stack_mm_from_led_tips", "hanging", "total_solid_volume_cm3", "jobs")}, indent=2))
for n, p in manifest["parts"].items():
    print(n, p["quantity"], p["size_mm"], p["volume_cm3"], "overhang z:", list(p["downward_overhang_mm2_by_height"].items())[:8])
