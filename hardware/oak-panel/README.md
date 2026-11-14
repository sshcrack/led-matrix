# Black-bezel LED panel: interim CAD

Editable **CadQuery/Python** sources for a wall-mounted display made from four nominal 192 × 192 mm, 64 × 64 P3 modules. Current concept: **v0.6**, a 400 × 400 × 60 mm narrow black printed bezel with modular internals. A provisional 25 mm rear-halo wall gap makes front-to-wall depth 85 mm. The new appearance supersedes the original oak surround.

This is an unfinished prototype, not a fabrication release. No PCB has been designed. The electronic blocks are space reservations; geometry checks do not prove fit, stiffness, heat performance or load capacity.

Work and physical test results are tracked in [issue #165](https://github.com/sshcrack/led-matrix/issues/165): [fit verification #170](https://github.com/sshcrack/led-matrix/issues/170), [mechanical completion #171](https://github.com/sshcrack/led-matrix/issues/171), [electronics #172](https://github.com/sshcrack/led-matrix/issues/172), and [halo lighting #208](https://github.com/sshcrack/led-matrix/issues/208). Do not print the full assembly yet.

**Mounting update: measured-v1.** The actual panel has **eight mounting points**, including staggered intermediate holes on its opposite edges. The six-hole drawing assumption is superseded. The v0.6 carrier and STL mounting gauge still contain that old pattern and must not be printed. Only the new paper check reflects these measurements; the complete enclosure has not yet been corrected.

## Downloads and viewing

[Prototype downloads](https://github.com/sshcrack/led-matrix/releases/tag/hardware-oak-panel) provide:

- `oak-panel-cad-v0.6.zip`: sources, assembly/exploded/part STEP, STL concepts, a dimensioned SVG, a single-panel paper template, the interactive viewer and validation manifests.
- `single-panel-mounting-check.pdf`: current **measured-v1 eight-hole paper check**, also published with a revision-specific filename. Download it separately; the PDFs inside older CAD bundles are superseded.
- `oak-panel-first-print-kit.zip`: historical six-hole gauge and clearance coupon, **ON HOLD**. Do not submit the mounting gauge for printing.

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
- The supplied Adafruit 4732 drawing is adopted as a matching-panel assumption: M3, columns ±85 mm and rows -73/0/+73 mm from each panel centre. This assumption is now superseded for fit verification; the modeled carriers have not yet been updated. The drawing frame is 191.70 mm square and is centred within the nominal module envelope; the imported reference meshes were not used as drilling templates.
- Four 191.6 mm PETG carrier tiles on 192 mm pitch leave 0.4 mm seams. Each has a 4 mm web, a 6 mm forward rim and six integrated 12 mm spacer bosses. The 3.5 mm bores, 10 mm boss outside diameter and screw-head spaces are provisional.
- Four 50 × 16 × 4 mm bolted joiners connect tile seams. Four vented cover quadrants are approximately 193.3 mm square. All exported printed parts fit a 210 × 250 × 210 mm bed with a 5 mm XY margin on every side.
- Carriers are oriented for printing rear face down, with the bosses rising from the bed-facing web. PETG is a starting material, not a validated operating-temperature specification. The print service determines settings, filament availability and price.
- Four black PETG L-shaped bezel quadrants replace the oak surround. Each quadrant is at most 199.8 × 199.8 × 60 mm, with a 0.4 mm seam at each edge midpoint. The 8 mm visible border and 5 mm outer-corner radius are design assumptions. Print-joint details, alignment and finishing remain unvalidated. The old thin diffuser carrier is still an optical reference, excluded from printable exports.
- Eight rear-facing strip envelopes (160 × 10 × 3 mm) and a small driver/harness allowance reserve space for a halo light. A cover passage is modeled; actual strip mounting, edge protection, hardware, power budget and software are not implemented. The strip allowance ends 23 mm from the assumed wall. Separate brightness/off, fixed colour and optional soft display matching are intended. Derive matching colours from the existing render pipeline, not always-on web-preview captures. The strip type/voltage and control transport must be selected with the electronics.
- The 25 mm wall gap is an optical starting point to test, not a guaranteed halo width or brightness. The frame body remains 60 mm deep; its internal electronics are not yet selected.

The material/manufacturing constraints and measurement confidence are in `measurement-basis.json`. Supplier references: [Waveshare P3 panel](https://www.waveshare.com/rgb-matrix-p3-64x64.htm), [Adafruit 4732](https://www.adafruit.com/product/4732), [PETG guidance](https://help.prusa3d.com/article/petg_2059).

## Measured relative pattern / measured-v1

Raw digital readings, exclusions and confidence are in `mounting-measurements.json`; `mounting_measurements.py` calculates the relative pattern. The reference features are the **narrow smooth shafts below the original magnetic discs**, approximately coaxial with the inserts. They are not the disc diameters or inside threads. The method uses `(mean outside + mean inside) / 2`, or `mean outside - (diameter 1 + diameter 2) / 2` where the inside jaws cannot reach. D uses the two unequal shaft diameters 5.50 and 5.15 mm; invalid D inside readings and all early analog trials are excluded.

| Span | Direct derived centre distance (mm) | Paper-check choice |
| --- | ---: | --- |
| A / upper right | 78.787 | A; provisionally F = A |
| B / lower right | 79.112 | B; provisionally E = B |
| C / bottom right | 94.395 | G via C = G; first C inside reading excluded, others provisional |
| D / bottom left | 79.285 | D; provisionally H = D |
| G / top left | 94.445 | G, a single outside measurement with measured shafts |
| H / top right | 79.275 | Control only; template uses D |

These are calculation digits, **not a certified tolerance**. B/C include photo-transcribed readings; A and D were explicitly text-confirmed. Repeated measurements do not establish shaft coaxiality or caliper alignment. The user confirmed **H = D, G = C, F = A, E = B** as provisional fit-template assumptions; the control readings are retained separately rather than silently averaged into them.

The resulting outer centre spans are approximately **173.73 x 157.90 mm**. With hole `(0,0)` as the local origin, the bottom intermediate hole is at x = 79.285 mm and the top intermediate hole at x = 94.445 mm. IDs follow the owner's sketch, not a uniform grid. This pattern has **no centre hole**. Its orientation relative to HUB75 connectors and its position within the panel perimeter remain unconfirmed; it must not be centred within the 192 mm outline by assumption.

## Verify before the next paid print

1. Download the separate measured-v1 PDF. Print Actual size / 100%, with Fit/Shrink disabled. Measure **both** horizontal and vertical bars: each must be 100 mm. The previous 96 mm result was a 96% print and cannot establish hole-position errors reliably.
2. On the unplugged panel, align the sheet by insert centres, not panel edges. Check all eight accessible points and record match/miss by ID and approximate error direction/amount. Panels may remain joined. Mark the HUB75 IN edge and panel outline on the sheet to establish orientation; do not force screws.
3. Establish accurate edge-to-centre offsets and check the relative pattern physically before transferring it into the full enclosure. The old six-hole STL gauge and carrier are on hold. The clearance coupon alone cannot validate mounting positions.
4. Once the corrected pattern is verified, issue #170 governs the revised small fit gauge and clearance/engagement checks, followed by the one-panel carrier/diffuser trial under #171. Screw length remains unresolved.

Regenerate only the paper check without a CAD build:

```sh
.venv/bin/python make-mounting-template.py
```

This writes `exports/single-panel-mounting-check.pdf` and `exports/mounting-pattern-derived.json`. `build.py` includes the new paper check but still exports the historical six-hole concept solids; its first-print instructions and manifest flag that hold explicitly.

If the print app offers no scaling controls and a 100 mm bar prints as approximately 95 mm, use the separate `single-panel-mounting-check-compensated-95mm.pdf` download with the **same printer, app and settings**. Its geometry is enlarged by `100 / 95 = 1.05263`, while the PDF remains A4. This is a provisional workaround for uniform, repeatable shrinking; **both bars must still print at 100 mm before checking the holes**. Measure the original bars with the caliper for a more precise correction; if the horizontal and vertical results differ, one uniform factor is insufficient. The canonical template and physical CAD dimensions remain unchanged. Generate a compensated copy with:

```sh
.venv/bin/python make-mounting-template.py --printed-bar-mm 95
```

The reproducible build checks each represented component as a valid solid, pairwise positive-volume intersections and assembly STEP round-trip solid count/volume. Each printed mesh is checked for watertightness, mesh-to-CAD volume agreement and bed fit. The build does not slice the job or verify real cable routing, tolerances, strength/creep, wall fastening, cooling, RF, electronics, diffuser or halo behaviour. Geometry checks do not validate the obsolete six-hole mounting assumption. The paper check now uses measured-v1; the STL mounting gauge remains superseded.

## Revision record

| Concept | Recorded change |
| --- | --- |
| v0.1 | Initial 420 mm square, 50 mm deep packaging study with assumed module depth. |
| v0.2 | Measured connected-depth allowance; conservative 65 mm stacked layout. |
| v0.3 | Magnetic-contact-depth study; shared cable/electronics depth, 55 mm frame. |
| v0.4 | Adopted drawing-based M3 screws and spacers; 55 mm frame. |
| v0.5 | FDM carriers/joiners and split cover replace the metal chassis; 60 mm frame. |
| v0.6 | Narrow black printed bezel replaces oak; rear halo reservations and provisional 25 mm wall gap. |

Earlier appearance concepts are superseded by v0.6. All six-hole mounting checks in v0.5/v0.6 bundles are superseded by the separate measured-v1 paper check. This source publication starts Git history for the hardware work; future iterations should be committed and their validation evidence attached to the issue.
