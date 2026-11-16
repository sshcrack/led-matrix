#!/usr/bin/env python3
"""Quote pack for the full printed enclosure (option A) and the hybrid set (option B).

FOR A PRICE QUOTE ONLY, NOT FOR PRINTING. The final design is not done: these files use the
current trial carrier and electronics plates v3, plus the v0.6 bezel quadrants, cover quadrants
and seam joiners as stand-ins of roughly the final size and volume. One 3MF per print bed
(Prusa MK3S, X 250 x Y 210 mm, 5 mm margin), so the service can slice and price each job.
"""

import argparse
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

import cadquery as cq
import trimesh
from trimesh.exchange.threemf import export_3MF

from electronics_plates import build_controller_plate, build_power_plate
from enclosure import Parameters, build
from trial_carrier import build_trial_carrier

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=ROOT / "exports" / "quote-pack")
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
BED = (250.0, 210.0)
MARGIN = 5.0

concept = {c["name"]: c["shape"] for c in build(Parameters()) if c["material"] == "PETG"}
bezels = sorted(n for n in concept if n.startswith("printed_bezel"))
covers = sorted(n for n in concept if n.startswith("printed_cover"))
joiners = sorted(n for n in concept if n.startswith("printed_joiner"))
assert len(bezels) == 4 and len(covers) == 4 and len(joiners) == 4

# job name: (option it belongs to, quantity, [(part name, shape)])
JOBS = {
    "01-carrier": ("B", 3, [("carrier", build_trial_carrier())]),
    "02-power-plate-and-joiners": ("B", 1, [("power-plate", build_power_plate())] + [(n, concept[n]) for n in joiners]),
    "03-controller-plate": ("B", 1, [("controller-plate", build_controller_plate())]),
}
for i, n in enumerate(bezels):
    JOBS[f"{4 + i:02d}-frame-quadrant-{i + 1}"] = ("A", 1, [(n, concept[n])])
for i, n in enumerate(covers):
    JOBS[f"{8 + i:02d}-rear-cover-quadrant-{i + 1}"] = ("A", 1, [(n, concept[n])])


def mesh_of(shape, name):
    b = shape.val().BoundingBox()
    shape = shape.translate((-b.xmin, -b.ymin, -b.zmin))
    path = out / f"_{name}.stl"
    cq.exporters.export(shape, str(path), tolerance=0.03, angularTolerance=0.12)
    mesh = trimesh.load(path, force="mesh")
    path.unlink()
    assert mesh.is_watertight, name
    assert abs(mesh.volume / shape.val().Volume() - 1) < 0.005, name
    return mesh


manifest = {"status": "QUOTE ONLY, NOT FOR PRINTING: stand-in geometry of the final size", "bed_mm": BED, "jobs": []}
for job, (option, qty, parts) in JOBS.items():
    scene = trimesh.Scene()
    volume = 0.0
    # The big part sits at the front-left corner; small parts (joiners) stack in a column to its right.
    column_x = column_y = None
    for name, shape in parts:
        mesh = mesh_of(shape, name)
        if column_x is None:
            offset = [MARGIN, MARGIN, 0]
            column_x, column_y = MARGIN + mesh.extents[0] + 10, MARGIN
        else:
            if mesh.extents[0] > mesh.extents[1]:  # stand long small parts upright in Y
                mesh.apply_transform(trimesh.transformations.rotation_matrix(1.5707963, [0, 0, 1]))
                mesh.apply_translation(-mesh.bounds[0])
            if column_y + mesh.extents[1] > BED[1] - MARGIN:  # next column
                column_x, column_y = column_x + mesh.extents[0] + 10, MARGIN
            offset = [column_x, column_y, 0]
            column_y += mesh.extents[1] + 10
        scene.add_geometry(mesh, geom_name=name, node_name=name, transform=trimesh.transformations.translation_matrix(offset))
        volume += mesh.volume
    lo, hi = scene.bounds
    assert lo[0] >= MARGIN - 1e-6 and lo[1] >= MARGIN - 1e-6, job
    assert hi[0] <= BED[0] - MARGIN and hi[1] <= BED[1] - MARGIN and hi[2] <= 210, (job, hi)
    path = out / f"QUOTE-ONLY-{job}.3mf"
    path.write_bytes(export_3MF(scene))
    loaded = trimesh.load_scene(path)
    assert len(loaded.geometry) == len(parts) and abs(loaded.volume / scene.volume - 1) < 1e-6, job
    manifest["jobs"].append({
        "file": path.name, "option": option, "quantity": qty, "objects": [n for n, _ in parts],
        "bounds_mm": [round(v, 1) for v in hi], "solid_volume_cm3": round(volume / 1000, 1),
    })

total = {o: sum(j["solid_volume_cm3"] * j["quantity"] for j in manifest["jobs"] if j["option"] == o) for o in "AB"}
manifest["solid_volume_cm3"] = {"B (carriers + plates + joiners)": round(total["B"], 1), "A (B + frame + rear cover)": round(total["A"] + total["B"], 1)}
(out / "quote-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

rows = "\n".join(f"| {j['file']} | {j['quantity']} | {'A + B' if j['option'] == 'B' else 'A'} | {j['bounds_mm'][0]} x {j['bounds_mm'][1]} x {j['bounds_mm'][2]} |" for j in manifest["jobs"])
(out / "ANFRAGE-LIES-MICH.md").write_text(f"""# Angebotsanfrage: Gehäuse für eine LED-Wandanzeige (NUR KOSTENSCHÄTZUNG)

**Bitte noch nicht drucken.** Ich möchte nur wissen, was der Druck ungefähr kosten würde, um
zwischen zwei Varianten zu entscheiden. Die Teile sind Platzhalter in der endgültigen Größe.

- Material: PETG, schwarz (falls möglich)
- Drucker: Prusa i3 MK3S (Bett 250 × 210 mm), jede Datei ist ein eigener Druck
- Bitte wenn möglich zwei Preise je Datei: **0,2 mm** und **0,3 mm** Schichthöhe, jeweils mit
  wenig Füllung (ca. 15 %). Ohne Stützmaterial, wo es geht.

| Datei | Anzahl | Variante | Größe (mm) |
|---|---|---|---|
{rows}

- **Variante B** = Dateien 01–03 (01 dreimal).
- **Variante A** = alle Dateien (B plus Rahmen 04–07 und Rückwand 08–11).

Vielen Dank!
""")

archive = out / "enclosure-quote-pack.zip"
with ZipFile(archive, "w", ZIP_DEFLATED) as z:
    for j in manifest["jobs"]:
        z.write(out / j["file"], j["file"])
    z.write(out / "ANFRAGE-LIES-MICH.md", "ANFRAGE-LIES-MICH.md")
    z.write(out / "quote-manifest.json", "quote-manifest.json")
print(json.dumps(manifest, indent=2))
print(archive)
