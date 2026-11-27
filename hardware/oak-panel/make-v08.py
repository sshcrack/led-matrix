#!/usr/bin/env python3
"""Build, check and export the v0.8 plywood rear (v08.py): STL/STEP per printed part, one 3MF per
print job, STEP for the plywood, a coloured assembly STEP and GLB, and a validation manifest.

Checks: every printed part one valid solid and a watertight mesh that fits the Prusa MK3S bed
(250 x 210 mm, 5 mm margin); no two parts, envelopes or cable zones overlap (panels, panel plugs,
boards, Pi, Pro Micro, WAGOs, fuse, DC socket and plug, halo strip, screws, wall, wire bend room,
paths between the quadrants); screw engagement and pilot depth; hanging on the wall screws;
clearances to the panels and the wall; HUB75 ribbon distances from the Pi.
"""

import argparse
import hashlib
import json
from itertools import combinations
from pathlib import Path

import cadquery as cq
import numpy as np
import trimesh
from trimesh.exchange.threemf import export_3MF

import electronics_plates as ep
import v07
import v08

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=ROOT / "exports" / "v08")
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
for old in out.glob("*"):
    if old.is_file():
        old.unlink()
BED, MARGIN = (250.0, 210.0), 5.0
Q = v08.QUADRANTS


def at(shape, sx, sy, z=0.0):
    cx, cy = v07.quadrant_centre(sx, sy)
    return shape.translate((cx, cy, z))


def flip(shape, z_face):
    return shape.rotate((0, 0, 0), (1, 0, 0), 180).translate((0, 0, z_face))


# --- Printed parts: asm (placed shapes), print (bed orientation), qty -----------------------
parts = {}
tube = v08.build_tube()
parts["spacer-tube"] = dict(asm=[tube.translate((x, y, v08.Z_INSERT)) for x, y in v08.inserts()], print=tube, qty=16)
pod = v08.build_pod()
parts["pi-pod"] = dict(asm=[pod], print=flip(pod, v08.POD_TOP), qty=1)
for bore in v08.halo_variants():
    shape = v08.build_halo_piece(bore)
    placed = [v08._to_edge(shape, rot, mir) for _, b, rot, mir in v08.halo_layout() if b == bore]
    parts[f"halo-{'side' if bore[1] > 180 else 'topbottom'}"] = dict(asm=placed, print=shape.translate((0, 0, -v08.Z_PLY)),
                                                                     qty=len(placed))
for sx, n in ((-1, "rail-left"), (1, "rail-right")):
    r = v08.build_rail(sx)
    parts[n] = dict(asm=[at(r, sx, 1)], print=r.translate((0, 0, -v08.Z_PLY)), qty=1)
bumpers = [v08.build_bumper(x, y) for x, y in v08.BUMPERS]
b0 = bumpers[0].val().BoundingBox()
parts["bumper"] = dict(asm=bumpers, print=bumpers[0].translate((-b0.center.x, -b0.center.y, -v08.Z_PLY)), qty=2)

# --- Wood ------------------------------------------------------------------------------------
wood = {"plywood-back": v08.build_ply()}

# --- Envelopes, cable room, screws -----------------------------------------------------------
refs = []  # (name, kind, shape)
for q in Q:
    h = v07.HALF
    refs.append((f"panel {v08.QNAME[q]}", "panel", at(v07.box(-h, h, -h, h, 0, v08.Z_INSERT), *q)))
refs += [(n, "plug", s) for n, s in v08.panel_connector_envelopes()]
refs += v08.component_envelopes()
refs += v08.pod_envelopes()
refs += v08.cable_zones()
sx, sz, y0 = v08.DC_SOCKET_X, v08.DC_SOCKET_Z, v08.POD["y0"]
refs.append(("DC socket body", "socket", v07.cyl_y(sx, sz, 12.6, y0, y0 + 14.0)))
pd, pl = v08.DC_PLUG
refs.append(("DC plug (envelope)", "plug-dc", v07.cyl_y(sx, sz, pd, y0 - pl, y0)))
strip = v08.halo_strip_envelope()
for name, _, rot, mir in v08.halo_layout():
    refs.append((f"halo strip {name}", "strip", v08._to_edge(strip, rot, mir)))
refs += [(n, "steel", s) for n, s in v08.wall_screws()]
refs += [(f"keeper bolt {sx}", "steel", v08.keeper_shank(sx)) for sx in (-1, 1)]
refs += [(n, "steel", s) for n, s in v08.screw_shapes()]
refs += [(f"vent {x:.0f},{y:.0f}", "vent", v07.cyl(x, y, v08.VENT_D, v08.Z_CEIL - 1.0, v08.Z_PLY + 1.0))
         for x, y in v08.vent_holes()]
refs.append(("wall", "wall", v07.box(-300, 300, -300, 300, v08.Z_WALL, v08.Z_WALL + 5)))

# --- Interference ----------------------------------------------------------------------------
solids = []
for n, p in parts.items():
    solids += [(f"{n}#{i}", "print", s.val()) for i, s in enumerate(p["asm"])]
solids += [(n, "ply", s.val()) for n, s in wood.items()]
solids += [(n, k, s.val()) for n, k, s in refs]

# Kinds that may share space: wires run through each other's room and into the plugs; the WAGO
# levers only open while the plywood is off the panels.
ALLOWED_KINDS = {frozenset(p) for p in [("zone",), ("zone", "corridor"), ("corridor",), ("corridor", "plug"),
                                        ("lever", "panel"), ("lever", "plug"), ("lever", "corridor"), ("lever", "zone"),
                                        ("vent", "zone"), ("vent", "corridor"), ("vent", "plug"), ("vent", "lever")]}
ALLOWED_NAMES = [("DC plug", "DC socket"), ("wall screw shank", "wall"), ("wall screw head", "wall screw shank")]


def overlap(a, b):
    ba, bb = a.BoundingBox(), b.BoundingBox()
    if ba.xmax <= bb.xmin or bb.xmax <= ba.xmin or ba.ymax <= bb.ymin or bb.ymax <= ba.ymin \
            or ba.zmax <= bb.zmin or bb.zmax <= ba.zmin:
        return 0.0
    return a.intersect(b).Volume()


clashes = []
for (na, ka, a), (nb, kb, b) in combinations(solids, 2):
    if frozenset((ka, kb)) in ALLOWED_KINDS:
        continue
    if any((x in na and y in nb) or (x in nb and y in na) for x, y in ALLOWED_NAMES):
        continue
    # a screw's own parts touch each other
    if ka == kb == "steel" and na.rsplit(" ", 1)[0] == nb.rsplit(" ", 1)[0]:
        continue
    v = overlap(a, b)
    if v > 1e-2:
        clashes.append((na, nb, round(v, 2)))
assert not clashes, "\n".join(map(str, clashes))

# --- Inside screws: each lands in a printed pilot deep enough, with material around it ---------
printed = [(n, s) for n, k, s in solids if k == "print"]
pilot_report = {}
for name, x, y, length, _ in v08.inside_screws():
    tip = v08.Z_PLY + length - v08.PLY_T
    hosts = [n for n, s in printed if s.isInside(cq.Vector(x + 2.6, y, v08.Z_PLY + 1.0))]
    assert len(hosts) == 1, (name, hosts)
    host = dict(printed)[hosts[0]]
    assert not host.isInside(cq.Vector(x, y, tip - 0.3)), (name, "pilot too short")
    for ang in range(0, 360, 45):
        a = np.radians(ang)
        assert host.isInside(cq.Vector(x + 2.4 * np.cos(a), y + 2.4 * np.sin(a), tip - 0.3)), (name, ang, "wall")
    pilot_report[name] = hosts[0].split("#")[0]

checks = v08.screw_checks()
for what, (tip, grip) in checks.items():
    if what.startswith("panel"):
        assert 3.5 <= grip <= 5.0, (what, grip)
    else:
        assert grip >= 6.0, (what, grip)

# --- Hanging (same rules as v0.7, #171) ------------------------------------------------------
ws = v08.WS
screw_y = v08.wall_screw_y()
lip_overlap = (screw_y + ws["head_d"] / 2) - (v08.PITCH / 2 + v08.RAIL_LIP_Y0)
assert lip_overlap >= 2.0, lip_overlap  # how far the lip reaches down behind the head
head_room = (v08.PITCH / 2 + v08.RAIL_WEB_Y[0]) - (screw_y + ws["head_d"] / 2)
assert head_room >= 5.0, head_room  # room above the head to lift the display on
head_front = v08.Z_WALL - ws["head_gap"] - ws["head_t"]
assert head_front - v08.Z_PLY >= 3.0, head_front  # head clears the plywood
rail_back = v08.rail_z()[1]
assert v08.Z_WALL - rail_back >= 5.0
sideways = (-8.0, 8.0)
for sx, rn in ((-1, "rail-left"), (1, "rail-right")):
    rail = parts[rn]["asm"][0].val()
    keeper = v08.keeper_shank(sx).val()
    for shift in sideways:
        for n, scr in v08.wall_screws(shift):
            if n.split()[-1] == str(sx):
                assert rail.intersect(scr.val()).Volume() < 1e-3, (rn, shift, n)
                if "shank" in n:  # lifting by the lip overlap makes the keeper hit the shank
                    assert keeper.translate((0, lip_overlap, 0)).intersect(scr.val()).Volume() > 1e-3, (rn, shift)

# --- Clearances ------------------------------------------------------------------------------
by_kind = {}
for n, k, s in refs:
    by_kind.setdefault(k, []).append((n, s.val().BoundingBox()))
under_ply = {n: [round(b.zmin, 2), round(b.zmax, 2)] for k in ("board", "wago", "fuse") for n, b in by_kind[k]}
for n, (z0, z1) in under_ply.items():
    assert z1 <= v08.Z_CEIL + 1e-6 and z0 >= v08.Z_INSERT + 3.0, (n, z0, z1)
pod_low = {n: round(b.zmin, 2) for k in ("pi", "pro_micro") for n, b in by_kind[k]}
for n, z in pod_low.items():
    assert z >= v08.Z_INSERT + 5.0, (n, z)
behind = {n: round(s.BoundingBox().zmax, 2) for n, k, s in solids if n.startswith(("pi-pod", "halo-", "DC plug", "rail-"))}
for n, z in behind.items():
    assert v08.Z_WALL - z >= 1.5, (n, z)
corner_gap = 2 * (v08.E - v08.HALO_SPAN[1])
# every halo piece carries exactly one panel screw, and those are the screws without a washer
assert len(v08.halo_bores()) == 8 and sum(1 for *_, w in v08.panel_screws() if not w) == 8

# HUB75 ribbons: straight-line distance from the Pi stack centre to each chain input, + 30 mm
# for the fold down to the plug. The real path is longer; the owner plans 30 cm ribbons.
px0, px1, py0, py1 = v08.PI_BOARD
pc = np.array([(px0 + px1) / 2, (py0 + py1) / 2])
ribbon = {}
for q in [(-1, 1), (-1, -1)]:
    cx, cy = v07.quadrant_centre(*q)
    x0, x1, y0_, y1_ = ep.PANEL_CONNECTORS["hub75_in"]
    ribbon[v08.QNAME[q]] = round(float(np.linalg.norm(np.array([cx + (x0 + x1) / 2, cy + (y0_ + y1_) / 2]) - pc)) + 30.0)

# --- Probes ----------------------------------------------------------------------------------
hp = parts["halo-topbottom"]["asm"][0].val()
u, z = v08._halo_map(0, v08.HALO["floor"] + 1.0)
assert not hp.isInside((100.0, u, z))  # cavity
u, z = v08._halo_map(0, v08.HALO_D - 0.3)
assert not hp.isInside((100.0, u, z))  # opening
u, z = v08._halo_map(v08.HALO["inner_w"] / 2 - 0.3, v08.HALO_D - 0.5)
assert hp.isInside((100.0, u, z))  # lip
assert hp.isInside((100.0, v08.E - 0.5, v08.Z_PLY + 5.0))  # vertical outer wall
podv = pod.val()
assert not podv.isInside((sx, y0 + 5.0, sz))  # socket bore
ply = wood["plywood-back"].val()
wx0, wx1, wy0, wy1 = v08.pod_window()
assert not ply.isInside(((wx0 + wx1) / 2, (wy0 + wy1) / 2, v08.Z_CEIL + 3))

# --- Export ----------------------------------------------------------------------------------
E = v08.E
holes = [{"what": w, "d": d, "from_left": round(x + E, 1), "from_bottom": round(y + E, 1)} for x, y, w, d in v08.ply_holes()]
holes.sort(key=lambda h: (h["what"], h["from_bottom"], h["from_left"]))
manifest = {
    "design": "led-matrix v0.8 plywood rear with Pi pod (front frame retention not included)",
    "basis": "panel inserts 174 x 158 (trial kit v6, #170); Hailege H-1-1543 listing (B07XFMMY1F); #171 wall screws",
    "estimates_to_confirm": v08.ESTIMATES,
    "stack_mm_from_led_tips": {
        "insert_face": v08.Z_INSERT, "plywood_inner": v08.Z_CEIL, "plywood_rear": v08.Z_PLY,
        "pod_back": v08.POD_TOP, "halo_shield_top": round(max(z for n, z in behind.items() if n.startswith("halo-")), 2),
        "wall": v08.Z_WALL,
    },
    "hanging": {"wall_screws": "existing pair, ~260 mm apart, heads set 10 mm off the wall (#171)",
                "lip_overlap_mm": round(lip_overlap, 2), "head_room_above_mm": round(head_room, 2),
                "sideways_adjust_mm": list(sideways), "screw_height_above_centre_mm": round(screw_y, 1)},
    "under_plywood_z": under_ply,
    "pod_lowest_z": pod_low,
    "halo_corner_gap_mm": round(corner_gap, 1),
    "vents": {"rows_y": list(v08.VENT_ROWS), "count": len(v08.vent_holes()), "d": v08.VENT_D},
    "screws": {k: {"tip_z": round(t, 2), "engagement": round(g, 2)} for k, (t, g) in checks.items()},
    "inside_screw_hosts": pilot_report,
    "hub75_ribbon_estimate_mm": ribbon,
    "plywood": {"size_mm": [round(2 * E, 1), round(2 * E, 1), v08.PLY_T],
                "window_mm": {"from_left": [round(wx0 + E, 1), round(wx1 + E, 1)],
                              "from_bottom": [round(wy0 + E, 1), round(wy1 + E, 1)]},
                "view": "rear (from the wall): display face down, top edge away from you",
                "holes": holes},
    "parts": {},
    "jobs": {},
}
meshes = {}
for name, p in parts.items():
    shape = p["print"]
    b = shape.val().BoundingBox()
    shape = shape.translate((-b.xmin, -b.ymin, -b.zmin))
    assert len(shape.solids().vals()) == 1 and shape.val().isValid(), name
    stl, step = out / f"{name}.stl", out / f"{name}.step"
    cq.exporters.export(shape, str(stl), tolerance=0.02, angularTolerance=0.1)
    cq.exporters.export(shape, str(step))
    mesh = trimesh.load(stl, force="mesh")
    assert mesh.is_watertight and mesh.body_count == 1, name
    meshes[name] = mesh
    bb = mesh.extents
    assert bb[0] + 2 * MARGIN <= BED[0] and bb[1] + 2 * MARGIN <= BED[1] and bb[2] <= 210, (name, bb)
    manifest["parts"][name] = {"quantity": p["qty"], "size_mm": [round(float(v), 1) for v in bb],
                               "volume_cm3": round(shape.val().Volume() / 1000, 1),
                               "stl_sha256": hashlib.sha256(stl.read_bytes()).hexdigest()}


def layout(job, items):
    scene = trimesh.Scene()
    boxes = []
    for i, (name, (x, y)) in enumerate(items):
        m = meshes[name]
        scene.add_geometry(m, geom_name=f"{name}@{i}", transform=trimesh.transformations.translation_matrix([x, y, 0]))
        boxes.append((name, x, y, x + m.extents[0], y + m.extents[1]))
    for n, x0, y0, x1, y1 in boxes:
        assert x0 >= MARGIN and y0 >= MARGIN and x1 <= BED[0] - MARGIN and y1 <= BED[1] - MARGIN, (job, n, x1, y1)
    for (na, *a), (nb, *b) in combinations(boxes, 2):
        assert max(b[0] - a[2], a[0] - b[2], b[1] - a[3], a[1] - b[3]) >= 4.0, (job, na, nb)
    (out / f"job-{job}.3mf").write_bytes(export_3MF(scene))
    manifest["jobs"][f"job-{job}.3mf"] = {"objects": [n for n, _ in items]}


def column(names, x=5.0, y=5.0, gap=5.5):
    res = []
    for n in names:
        res.append((n, (x, y)))
        y += meshes[n].extents[1] + gap
    return res


halo_names = [n for n in parts if n.startswith("halo-") for _ in range(parts[n]["qty"])]
layout("halo-pieces", column(halo_names))
pe, re_ = meshes["pi-pod"].extents, meshes["rail-left"].extents
items = [("pi-pod", (5, 5)), ("rail-left", (5, 5 + pe[1] + 6)), ("rail-right", (5, 5 + pe[1] + re_[1] + 12))]
x0 = 5 + max(pe[0], re_[0]) + 6  # bumpers, then the tubes in rows of five, right of the pod and rails
items += [("bumper", (x0, 5)), ("bumper", (x0 + 22, 5))]
items += [("spacer-tube", (x0 + (i % 5) * 14, 27 + (i // 5) * 14)) for i in range(16)]
layout("pod-rails-tubes", items)
manifest["total_solid_volume_cm3"] = round(sum(p["volume_cm3"] * p["quantity"] for p in manifest["parts"].values()), 1)

# --- Coloured assembly (STEP for FreeCAD, GLB for the browser) ------------------------------
COL = {"print": (0.12, 0.12, 0.13), "ply": (0.82, 0.68, 0.46), "panel": (0.2, 0.2, 0.22),
       "plug": (0.85, 0.85, 0.8), "board": (0.1, 0.45, 0.2), "pi": (0.1, 0.5, 0.25), "pro_micro": (0.1, 0.3, 0.6),
       "wago": (0.9, 0.55, 0.1), "fuse": (0.8, 0.1, 0.1), "socket": (0.6, 0.6, 0.62), "plug-dc": (0.15, 0.15, 0.15),
       "strip": (1.0, 0.9, 0.5), "steel": (0.7, 0.7, 0.72), "zone": (0.95, 0.35, 0.6), "lever": (0.95, 0.75, 0.3),
       "corridor": (0.4, 0.7, 0.95), "vent": (0.6, 0.9, 1.0)}
items = []
for n, p in parts.items():
    items += [(f"{n} {i + 1}", "print", s) for i, s in enumerate(p["asm"])]
items += [(n, "ply", s) for n, s in wood.items()]
items += [(n, k, s) for n, k, s in refs if k != "wall"]
asm = cq.Assembly(name="v08_rear")
scene = trimesh.Scene()
for i, (n, k, s) in enumerate(items):
    alpha = 0.35 if k in ("zone", "lever", "corridor", "vent") else 1.0
    asm.add(s, name=f"{i:03d}_{n}".replace(" ", "_").replace(",", "_").replace("#", "_").replace(">", "")
            .replace("(", "").replace(")", "").replace("+", "p"), color=cq.Color(*COL[k], alpha))
    verts, tris = s.val().tessellate(0.3, 0.3)
    m = trimesh.Trimesh(np.array([(p.x, p.y, p.z) for p in verts]), np.array(tris))
    m.visual.face_colors = [int(c * 255) for c in COL[k]] + [int(alpha * 255)]
    scene.add_geometry(m, node_name=n, geom_name=f"g{i}")
asm.export(str(out / "assembly-v08.step"))
(out / "assembly-v08.glb").write_bytes(scene.export(file_type="glb"))
for n, s in wood.items():
    cq.exporters.export(s, str(out / f"wood-{n}.step"))

(out / "v08-validation.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps({k: manifest[k] for k in ("stack_mm_from_led_tips", "under_plywood_z", "pod_lowest_z", "screws",
                                            "hub75_ribbon_estimate_mm", "halo_corner_gap_mm", "total_solid_volume_cm3", "jobs")},
                 indent=1))
for n, p in manifest["parts"].items():
    print(n, p["quantity"], p["size_mm"], p["volume_cm3"])
