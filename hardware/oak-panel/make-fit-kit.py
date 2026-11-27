#!/usr/bin/env python3
"""Export ONLY the paper-verified four-corner fit gauge and clearance coupon."""

import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

import cadquery as cq
import trimesh

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--output", type=Path, default=ROOT / "exports" / "four-corner-fit-kit"
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
    parser.error("Mounting basis changed; review this gauge envelope before export")
out.mkdir(parents=True, exist_ok=True)
# 10 mm wide rails, with each hole 5 mm from the nearest two outer edges.
# The outer perimeter is a gauge envelope, NOT a panel-edge alignment datum.
gauge = cq.Workplane("XY").rect(184, 168).rect(164, 148).extrude(3)
for x, y in centres:
    gauge = gauge.cut(cq.Workplane("XY").center(x, y).circle(1.75).extrude(3))
coupon = cq.Workplane("XY").box(80, 24, 3, centered=(True, True, False))
for x, diameter in zip([-27, -9, 9, 27], [3.2, 3.4, 3.5, 3.6]):
    coupon = coupon.cut(cq.Workplane("XY").center(x, 0).circle(diameter / 2).extrude(3))
# Notched end identifies the first (3.2 mm) hole without font dependencies.
coupon = coupon.cut(cq.Workplane("XY").box(3, 3, 5).translate((-39, -11, 1.5)))
manifest = {
    "revision": "four-corner-fit-v1",
    "status": "Two prototype fit tools only; not structural mounting parts or fabrication approval.",
    "pattern_mm": centres,
    "corner_span_mm": [174, 158],
    "gauge_clearance_diameter_mm": 3.5,
    "coupon_from_notched_end_mm": [3.2, 3.4, 3.5, 3.6],
    "print_scale_percent": 100,
    "paper_scale_compensation_applied": False,
    "material": "PETG starting material; print service selects settings and quote.",
    "target_bed_mm": [210, 250, 210],
    "parts": [],
}
for name, shape in [
    ("four-corner-mounting-gauge", gauge),
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
# Verify CAD hole axes by probing material immediately around the four targets.
for x, y in centres:
    assert not gauge.val().isInside((x, y, 1.5)), (x, y)
    assert not gauge.val().isInside((x + 1.74, y, 1.5)), (x, y)
    assert gauge.val().isInside((x + 1.76, y, 1.5)), (x, y)
    assert gauge.val().isInside((x, y + 2, 1.5)), (x, y)
for x, diameter in zip([-27, -9, 9, 27], [3.2, 3.4, 3.5, 3.6]):
    assert not coupon.val().isInside((x + diameter / 2 - 0.01, 0, 1.5))
    assert coupon.val().isInside((x + diameter / 2 + 0.01, 0, 1.5))
manifest["coupon_radius_probes"] = "all four clearance radii checked within 0.01 mm"
manifest["gauge_axis_probes"] = (
    "all four target axes empty; surrounding rail material present"
)
(out / "fit-kit-validation.json").write_text(json.dumps(manifest, indent=2) + "\n")
instructions = ROOT / "four-corner-fit-instructions.txt"
(out / "READ-ME-FIRST.txt").write_text(instructions.read_text())
archive = out / "panel-four-corner-fit-kit-v1.zip"
with ZipFile(archive, "w", ZIP_DEFLATED) as z:
    for name in [
        "four-corner-mounting-gauge.stl",
        "four-corner-mounting-gauge.step",
        "clearance-hole-coupon.stl",
        "clearance-hole-coupon.step",
        "fit-kit-validation.json",
        "READ-ME-FIRST.txt",
    ]:
        z.write(out / name, name)
    z.write(Path(__file__), "source/make-fit-kit.py")
    for name in [
        "measurement-basis.json",
        "mounting-measurements.json",
        "four-corner-fit-instructions.txt",
        "requirements.lock",
        "README.md",
    ]:
        z.write(ROOT / name, "source/" + name)
print(json.dumps(manifest, indent=2))
print(archive)
