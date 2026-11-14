"""Export FDM-oriented concept parts and two low-cost verification prints."""

from pathlib import Path
import os
import json
import cadquery as cq
import trimesh
from enclosure import Parameters, build, box

root = Path(__file__).resolve().parent
out = Path(os.environ.get("OAK_PANEL_OUTPUT", root / "exports")) / "stl"
out.mkdir(parents=True, exist_ok=True)
manifest = {
    "target": "Prusa i3 MK3s, owner-provided 210 x 250 x 210 mm",
    "material": "PETG, confirm actual stock",
    "status": "ON HOLD: six-hole gauge and carriers are superseded by measured-v1 paper verification. Do not print them. All assembly parts are unvalidated concepts.",
    "parts": [],
}


def save(name, shape, group, orientation):
    if orientation == "rear face down":
        shape = shape.rotate((0, 0, 0), (1, 0, 0), 180)
    b = shape.val().BoundingBox()
    shape = shape.translate((-b.xmin, -b.ymin, -b.zmin))
    b = shape.val().BoundingBox()
    assert b.xlen + 10 <= 210 and b.ylen + 10 <= 250 and b.zlen <= 210, name
    target = out / (name + ".stl")
    cq.exporters.export(shape, str(target), tolerance=0.03, angularTolerance=0.12)
    mesh = trimesh.load(target, force="mesh")
    assert mesh.is_watertight, name
    error = abs(mesh.volume - shape.val().Volume()) / shape.val().Volume()
    assert error < 0.005, (name, error)
    manifest["parts"].append(
        {
            "file": target.name,
            "group": group,
            "bounds_mm": [round(b.xlen, 3), round(b.ylen, 3), round(b.zlen, 3)],
            "volume_cm3": round(shape.val().Volume() / 1000, 3),
            "orientation": orientation,
            "watertight": True,
            "fits_bed_with_5mm_xy_margin": True,
            "note": "CAD solid volume is not sliced filament usage or a quote.",
        }
    )


for c in build(Parameters()):
    if c["material"] == "PETG":
        save(
            c["name"],
            c["shape"],
            "assembly concept",
            "rear face down"
            if c["name"].startswith("printed_carrier")
            else "flat face down",
        )
# 180 x 160 gauge: a rigid open frame around the six adopted mounting positions.
gauge = cq.Workplane("XY").rect(180, 160).rect(160, 140).extrude(2.4)
for x in [-85, 85]:
    for y in [-73, 0, 73]:
        gauge = gauge.cut(
            cq.Workplane("XY").circle(1.75).extrude(4).translate((x, y, -0.5))
        )
save("FIRST_PRINT_mounting_gauge", gauge, "first print", "flat face down")
coupon = box(80, 24, 3)
for x, d in zip([-27, -9, 9, 27], [3.2, 3.4, 3.5, 3.6]):
    coupon = coupon.cut(
        cq.Workplane("XY").circle(d / 2).extrude(5).translate((x, 0, -1))
    )
# A shallow corner notch identifies the 3.2 mm end without printed text.
coupon = coupon.cut(box(3, 3, 5, -39, -11, -1))
save("FIRST_PRINT_hole_coupon", coupon, "first print", "flat face down")
manifest["coupon_holes"] = {
    "from_notched_end_mm": [3.2, 3.4, 3.5, 3.6],
    "check": "Try actual M3 hardware gently; holes may require tolerance adjustment.",
}
(out / "print-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(
    "Validated",
    len(manifest["parts"]),
    "watertight STL meshes; each fits the Prusa bed with 5 mm margin per side.",
)
