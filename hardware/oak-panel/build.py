#!/usr/bin/env python3
"""Regenerate the public oak-panel concept and download bundles."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from zipfile import ZIP_DEFLATED, ZipFile

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=root / "exports")
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
env = {**os.environ, "OAK_PANEL_OUTPUT": str(out)}
subprocess.run(
    [sys.executable, str(root / "enclosure.py"), "--output", str(out)],
    check=True,
    env=env,
)
for script in [
    "export-prints.py",
    "make-drawing.py",
    "make-mounting-template.py",
    "make-preview.py",
]:
    subprocess.run([sys.executable, str(root / script)], check=True, env=env)
source_files = sorted(
    p
    for p in root.iterdir()
    if p.is_file() and p.suffix in {".py", ".html", ".md", ".json", ".lock", ".txt"}
)
(out / "source-sha256.json").write_text(
    json.dumps(
        {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files},
        indent=2,
    )
    + "\n"
)
first = out / "oak-panel-first-print-kit.zip"
with ZipFile(first, "w", ZIP_DEFLATED) as z:
    for n in ["FIRST_PRINT_mounting_gauge.stl", "FIRST_PRINT_hole_coupon.stl"]:
        z.write(out / "stl" / n, n)
    z.write(root / "first-print-instructions.txt", "READ-ME-FIRST.txt")
full = out / "oak-panel-cad-v0.5.zip"
with ZipFile(full, "w", ZIP_DEFLATED) as z:
    for p in source_files:
        z.write(p, Path("source") / p.name)
    for p in sorted(out.rglob("*")):
        if p.is_file() and p.suffix != ".zip":
            z.write(p, Path("exports") / p.relative_to(out))
print("Download bundles:", first, full, sep="\n")
