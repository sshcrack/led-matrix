#!/usr/bin/env python3
"""Export one real panel trial carrier plus a coupon in a single-job geometry layout."""

import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

import cadquery as cq
import trimesh
from trimesh.exchange.threemf import export_3MF
from trial_carrier import build_trial_carrier, JOINER_HOLES

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
carrier = build_trial_carrier()
coupon = cq.Workplane("XY").box(80, 24, 3, centered=(True, True, False))
for x, diameter in zip([-27, -9, 9, 27], [3.2, 3.4, 3.5, 3.6]):
    coupon = coupon.cut(cq.Workplane("XY").center(x, 0).circle(diameter / 2).extrude(3))
# Notched end identifies the first (3.2 mm) hole without font dependencies.
coupon = coupon.cut(cq.Workplane("XY").box(3, 3, 5).translate((-39, -11, 1.5)))
manifest = {
    "revision": "trial-carrier-v1",
    "status": "One-panel carrier geometry trial plus coupon; no wall-mount or finished product approval.",
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
    "coupon_from_notched_end_mm": [3.2, 3.4, 3.5, 3.6],
    "print_scale_percent": 100,
    "paper_scale_compensation_applied": False,
    "material": "PETG starting material; print service selects settings and quote.",
    "target_bed_mm": [210, 250, 210],
    "parts": [],
}
meshes = {}
for name, shape in [
    ("trial-panel-carrier", carrier),
    ("clearance-hole-coupon", coupon),
]:
    solid = shape.val()
    assert solid.isValid() and len(solid.Solids()) == 1, name
    bounds = solid.BoundingBox()
    # Both flat bases lie on Z=0; positive XY is convenient for print-service imports.
    shape = shape.translate((-bounds.xmin, -bounds.ymin, -bounds.zmin))
    bounds = shape.val().BoundingBox()
    assert bounds.xlen + 10 <= 210 and bounds.ylen + 10 <= 250 and bounds.zlen <= 210
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
            "bed_fit": "5 mm margin on all XY sides; check the slicer skirt/brim separately",
        }
    )
# Probe full through bores and supporting material at web and boss heights.
for x, y in centres:
    for z in [2, 12, 15.9]:
        assert not carrier.val().isInside((x, y, z))
        assert not carrier.val().isInside((x + 1.74, y, z))
        assert carrier.val().isInside((x + 1.76, y, z))
for x, y in JOINER_HOLES:
    assert not carrier.val().isInside((x, y, 2))
assert carrier.val().isInside((87, 0, 2))
assert not carrier.val().isInside((87, 0, 6))
assert carrier.val().isInside((94, 0, 8))
assert not carrier.val().isInside((94, 0, 12))
assert not carrier.val().isInside((0, 0, 2))
for x, diameter in zip([-27, -9, 9, 27], [3.2, 3.4, 3.5, 3.6]):
    assert not coupon.val().isInside((x + diameter / 2 - 0.01, 0, 1.5))
    assert coupon.val().isInside((x + diameter / 2 + 0.01, 0, 1.5))
manifest["coupon_radius_probes"] = "all four clearance radii checked within 0.01 mm"
manifest["carrier_probes"] = (
    "four 3.5 mm through bores at web/boss heights, seam bores, web, rim and open middle checked"
)
scene = trimesh.Scene()
scene.add_geometry(
    meshes["trial-panel-carrier"],
    geom_name="trial-panel-carrier",
    node_name="trial-panel-carrier",
    transform=trimesh.transformations.translation_matrix([5, 5, 0]),
)
scene.add_geometry(
    meshes["clearance-hole-coupon"],
    geom_name="clearance-hole-coupon",
    node_name="clearance-hole-coupon",
    transform=trimesh.transformations.translation_matrix([5, 206.6, 0]),
)
layout = out / "single-job-layout.3mf"
layout.write_bytes(export_3MF(scene))
loaded = trimesh.load_scene(layout)
assert len(loaded.graph.nodes_geometry) == 2
assert abs(loaded.bounds - scene.bounds).max() < 1e-6
assert abs(loaded.volume / scene.volume - 1) < 1e-6
assert loaded.bounds[0][0] >= 5 and loaded.bounds[0][1] >= 5
assert loaded.bounds[1][0] <= 205 and loaded.bounds[1][1] <= 245
assert loaded.bounds[0][2] >= -1e-6
assert loaded.bounds[1][2] <= 16.00001
manifest["single_job_layout"] = {
    "file": layout.name,
    "objects": 2,
    "bounds_mm": loaded.bounds.tolist(),
    "minimum_xy_margin_mm": 5,
    "gap_between_parts_mm": 10,
    "generic_3mf_roundtrip": "two objects, transformed bounds and volume preserved",
    "machine_settings": "none; print service must slice and quote",
}
(out / "trial-carrier-validation.json").write_text(
    json.dumps(manifest, indent=2) + "\n"
)
instructions = ROOT / "trial-carrier-instructions.txt"
(out / "READ-ME-FIRST.txt").write_text(instructions.read_text())
archive = out / "panel-trial-carrier-kit-v1.zip"
with ZipFile(archive, "w", ZIP_DEFLATED) as z:
    for name in [
        "trial-panel-carrier.stl",
        "trial-panel-carrier.step",
        "clearance-hole-coupon.stl",
        "clearance-hole-coupon.step",
        "trial-carrier-validation.json",
        "READ-ME-FIRST.txt",
        "single-job-layout.3mf",
    ]:
        z.write(out / name, name)
    z.write(Path(__file__), "source/make-trial-carrier.py")
    for name in [
        "trial_carrier.py",
        "measurement-basis.json",
        "mounting-measurements.json",
        "trial-carrier-instructions.txt",
        "requirements.lock",
        "README.md",
    ]:
        z.write(ROOT / name, "source/" + name)
print(json.dumps(manifest, indent=2))
print(archive)
