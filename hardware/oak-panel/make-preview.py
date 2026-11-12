"""Export the CAD tessellation for the self-contained concept viewer."""

import json
from pathlib import Path
import os
from enclosure import Parameters, build

root = Path(__file__).resolve().parent
output = Path(os.environ.get("OAK_PANEL_OUTPUT", root / "exports"))
output.mkdir(parents=True, exist_ok=True)
result = []
for c in build(Parameters()):
    vertices, triangles = c["shape"].val().tessellate(0.1)
    result.append(
        {k: c[k] for k in ["name", "role", "color", "note", "explode"]}
        | {
            "vertices": [
                [round(v.x, 3), round(v.y, 3), round(v.z, 3)] for v in vertices
            ],
            "triangles": triangles,
        }
    )
(output / "meshes.json").write_text(json.dumps(result, separators=(",", ":")))
print("Exported", len(result), "component meshes")

template = (root / "preview-template.html").read_text()
fragment = template.replace("__MESH_DATA__", (output / "meshes.json").read_text())
(output / "oak-enclosure.html").write_text(
    '<!doctype html>\n<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Oak enclosure concept</title></head><body>\n'
    + fragment
    + "\n</body></html>"
)
