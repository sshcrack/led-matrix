# Oak-frame LED panel: interim CAD

Editable **CadQuery/Python** sources for a wall-mounted display made from four nominal 192 × 192 mm, 64 × 64 P3 modules. Current concept: **v0.5**, a 420 × 420 × 60 mm oak frame with modular printed internals. The provisional 10 mm wall gap makes front-to-wall depth 70 mm.

This is an unfinished prototype, not a fabrication release. No PCB has been designed. The electronic blocks are space reservations; geometry checks do not prove fit, stiffness, heat performance or load capacity.

Work and physical test results are tracked in [issue #165](https://github.com/sshcrack/led-matrix/issues/165): [fit verification #170](https://github.com/sshcrack/led-matrix/issues/170), [mechanical completion #171](https://github.com/sshcrack/led-matrix/issues/171), and [electronics #172](https://github.com/sshcrack/led-matrix/issues/172). Do not print the full assembly yet.

## Downloads and viewing

[Prototype downloads](https://github.com/sshcrack/led-matrix/releases/tag/hardware-oak-panel) provide:

- `oak-panel-cad-v0.5.zip`: sources, assembly/exploded/part STEP, STL concepts, a dimensioned SVG, a single-panel paper template, the interactive viewer and validation manifests.
- `oak-panel-first-print-kit.zip`: **only** the two suggested fit-test prints and their instructions.

Extract the CAD bundle and open `exports/oak-enclosure.html` in a browser. For a CAD application such as FreeCAD, open `exports/assembly.step` or an individual part STEP. STEP contains solids; the original parameters and construction history remain in `enclosure.py`. There is no native FreeCAD `.FCStd` project.

Generated exports are download assets, not committed blobs. Source history is kept in Git. Private photographs, third-party uploaded meshes/PDFs, personal context and machine-specific paths are excluded.

## Reproduce on another machine

From this directory, using [uv](https://docs.astral.sh/uv/):

```sh
uv venv --python 3.12 .venv
uv pip install --python .venv/bin/python -r requirements.lock
.venv/bin/python build.py
```

On Windows, use `.venv/Scripts/python.exe` in place of `.venv/bin/python`. Existing Python 3.12 virtual environments and pip also work. Generated files go to ignored `exports/`; use `build.py --output /path/to/output` to choose a different directory. No application/emulator build is required.

`enclosure.py` holds the CAD parameters. Its `--parameters overrides.json` option exports a custom assembly; the complete `build.py` pipeline regenerates the default concept. Helpers use the same default parameters and accept `OAK_PANEL_OUTPUT` as their output directory.

## Current mechanical basis

- Supplier nominal panel envelope: 192 × 192 mm. Approximate measured depth: 15 mm from LED tips to rear mounting face, and 32 mm including the connected cable bend.
- The supplied Adafruit 4732 drawing is adopted as a matching-panel assumption: M3, columns ±85 mm and rows -73/0/+73 mm from each panel centre. Physical agreement is unverified. The drawing frame is 191.70 mm square and is centred within the nominal module envelope; the imported reference meshes were not used as drilling templates.
- Four 191.6 mm PETG carrier tiles on 192 mm pitch leave 0.4 mm seams. Each has a 4 mm web, a 6 mm forward rim and six integrated 12 mm spacer bosses. The 3.5 mm bores, 10 mm boss outside diameter and screw-head spaces are provisional.
- Four 50 × 16 × 4 mm bolted joiners connect tile seams. Four vented cover quadrants are approximately 193.3 mm square. All exported printed parts fit a 210 × 250 × 210 mm bed with a 5 mm XY margin on every side.
- Carriers are oriented for printing rear face down, with the bosses rising from the bed-facing web. PETG is a starting material, not a validated operating-temperature specification. The print service determines settings, filament availability and price.
- The oak surround remains a bought frame or conventional woodworking item. The old thin diffuser carrier is an optical reference, excluded from printable exports.

The material/manufacturing constraints and measurement confidence are in `measurement-basis.json`. Supplier references: [Waveshare P3 panel](https://www.waveshare.com/rgb-matrix-p3-64x64.htm), [Adafruit 4732](https://www.adafruit.com/product/4732), [PETG guidance](https://help.prusa3d.com/article/petg_2059).

## Verify before the next paid print

1. Print `single-panel-mounting-check.pdf` at Actual size / 100%. Confirm the calibration bar is 100 mm. Check the six centres against one panel's rear inserts; the panels may remain glued together. Record accessible matches/misses in the issue.
2. If the pattern agrees, submit only the first-print kit, preferably as one print job. Print at 100% scale. The gauge aligns by its six holes, not its outside edges; it has no 192 mm panel-outline datum.
3. Use the notched coupon to try 3.2 / 3.4 / 3.5 / 3.6 mm clearances with actual M3 hardware. This tests printed clearance, not internal thread pitch or safe insert engagement.
4. After the result, the linked implementation issues govern a one-panel carrier/diffuser trial and the complete mechanical/electrical design. Do not force screws or use unestablished screw lengths.

The reproducible build checks each represented component as a valid solid, pairwise positive-volume intersections and assembly STEP round-trip solid count/volume. Each printed mesh is checked for watertightness, mesh-to-CAD volume agreement and bed fit. The build does not slice the job or verify real cable routing, tolerances, strength/creep, wall fastening, cooling, RF, electronics or diffuser behaviour.

## Revision record

| Concept | Recorded change |
| --- | --- |
| v0.1 | Initial 420 mm square, 50 mm deep packaging study with assumed module depth. |
| v0.2 | Measured connected-depth allowance; conservative 65 mm stacked layout. |
| v0.3 | Magnetic-contact-depth study; shared cable/electronics depth, 55 mm frame. |
| v0.4 | Adopted drawing-based M3 screws and spacers; 55 mm frame. |
| v0.5 | FDM carriers/joiners and split cover replace the metal chassis; 60 mm frame. |

Earlier local concept exports are superseded by v0.5. This source publication starts Git history for the hardware work; future iterations should be committed and their validation evidence attached to the issue.
