# Black-bezel LED panel: interim CAD

Editable **CadQuery/Python** sources for a wall-mounted display made from four nominal 192 × 192 mm, 64 × 64 P3 modules. Current concept: **v0.6**, a 400 × 400 × 60 mm narrow black printed bezel with modular internals. A provisional 25 mm rear-halo wall gap makes front-to-wall depth 85 mm. The new appearance supersedes the original oak surround.

This is an unfinished prototype, not a fabrication release. No PCB has been designed. The electronic blocks are space reservations; geometry checks do not prove fit, stiffness, heat performance or load capacity.

Work and physical test results are tracked in [issue #165](https://github.com/sshcrack/led-matrix/issues/165): [fit verification #170](https://github.com/sshcrack/led-matrix/issues/170), [mechanical completion #171](https://github.com/sshcrack/led-matrix/issues/171), [electronics #172](https://github.com/sshcrack/led-matrix/issues/172), and [halo lighting #208](https://github.com/sshcrack/led-matrix/issues/208). Do not print the full assembly yet.

**Mounting correction: measured-v1 is WITHDRAWN.** The intermediate inserts also have offsets perpendicular to the panel edges relative to the corner-insert lines. The eight-hole template incorrectly assumed collinearity; both its standard and 95 mm compensated versions are unsuitable for fit verification. Raw digital readings remain evidence, but their measuring-axis directions and individual X/Y coordinates need establishing. The v0.6 six-hole carrier/gauge also remain on hold. Do not print withdrawn mounting patterns or the old full-enclosure carriers. The new one-panel trial kit below is a separate prototype path.

## Downloads and viewing

[Prototype downloads](https://github.com/sshcrack/led-matrix/releases/tag/hardware-oak-panel) provide:

- `oak-panel-cad-v0.6.zip`: sources, assembly/exploded/part STEP, STL concepts, a dimensioned SVG, a single-panel paper template, the interactive viewer and validation manifests.
- All `single-panel-mounting-check*.pdf` files and `mounting-check-measured-v1.zip` are **WITHDRAWN**. Published standalone PDFs now show a withdrawal notice. Templates embedded in older CAD bundles are also unsuitable.
- `oak-panel-first-print-kit.zip`: historical six-hole gauge and clearance coupon, **ON HOLD**. Do not submit the mounting gauge for printing.

Extract the CAD bundle and open `exports/oak-enclosure.html` in a browser. For a CAD application such as FreeCAD, open `exports/assembly.step` or an individual part STEP. STEP contains solids; the original parameters and construction history remain in `enclosure.py`. There is no native FreeCAD `.FCStd` project.

Generated exports are download assets, not committed blobs. Source history is kept in Git. Private photographs, third-party uploaded meshes/PDFs, personal context and machine-specific paths are excluded.

## Next print: one-panel trial carrier and coupon

`panel-trial-carrier-kit-v2.zip` is the next paid test job (v2 replaces v1; do not submit v1). It replaces the standalone gauge as the recommended step, so the mounting check also trials a usable carrier. It contains one **191.6 x 191.6 x 16 mm** carrier and one **80 x 24 x 3 mm** clearance coupon. The carrier has a 4 mm rear web, 6 mm perimeter rib above the web, four 12 mm bosses above the web, and four 3.5 mm bores on the confirmed **174 x 158 mm** pattern. Eight more 3.5 mm holes, two per edge, are symmetric provisional seam interfaces, so one carrier fits any tile; they are not panel inserts. Six 5 mm sight holes in the web sit over the drawing's intermediate inserts (both handedness variants of the 7.5 mm stagger) for visual confirmation only, and a notch on the +Y rib marks orientation. Body/perimeter and spacer fit remain assumptions to test.

Submit **`single-job-layout.3mf` as one job**: the coupon is nested in the carrier's open middle (about 40 mm clear), so the job is a 201.6 mm square that fits the Prusa MK3S bed (X 250 x Y 210 mm) either way round with at least 5 mm XY margin, and the 400 x 400 mm printer. The file has geometry only, no printer settings. Alternatively submit the two individual STLs together for the service to arrange; do not print both formats and duplicate the parts. The service must check brim/adhesion, slice and quote. There is no minimum-price guarantee and no slicer time estimate.

**PETG, millimetres, 100% scale in all axes. Never apply the paper correction to a 3D print.** Print flat rear/base faces down, bosses up. Individual STEP solids, editable Python, dependencies, [instructions](trial-carrier-instructions.txt) and validation evidence are included. The coupon retains the 3.2/3.4/3.5/3.6 mm clearances from its notched end.

The panel insert faces touch the boss tops. There is a 16 mm total grip from the rear base to that contact plane; **this does not specify screw length**. Actual thread/usable engagement, head and washer details remain unresolved. **Remove all eight magnetic feet first**: a foot left in an intermediate insert reaches the web and stops the carrier seating. Check fit without screws or pressure: flatness on a flat surface first, then rest the carrier on the panel under its own weight, pin each corner bore with a 3 mm rod, and check paper gaps and a marker contact pattern per boss. Also record which sight holes show inserts, where cables cross the carrier outline, glued pitch/body size, coupon clearance and the HUB75 IN edge relative to the notch. Never force an unknown screw. Joined panels may stay joined.

This is one actual carrier mounting trial, not a finished wall bracket or fabric clamp. It can be retained for later one-panel prototypes if its fit/support prove suitable. Full enclosure printing remains on hold. Work is tracked under [#170](https://github.com/sshcrack/led-matrix/issues/170) and [#171](https://github.com/sshcrack/led-matrix/issues/171).

```sh
.venv/bin/python make-trial-carrier.py
```

`trial_carrier.py` holds the reusable trial geometry. The generator validates single solids, bores at web/boss heights, web/rim/open-middle, eight-seam-hole symmetry, sight-hole and notch probes, single watertight mesh bodies, CAD/mesh volume, STEP round-trip and the two-object 3MF's transformed bounds/volume, nesting gap and bed fit, then renders `trial-carrier-preview.png`. Source and exports are packaged independently of the withdrawn full-assembly pipeline; historical v0.6 full solids have not changed.

The earlier `panel-four-corner-fit-kit-v1.zip` remains an optional standalone diagnostic, not an additional print to order with this trial. The old six-hole `oak-panel-first-print-kit.zip` remains obsolete. Do not print either gauge as an extra unless a later fit problem calls for it.

## Electronics plates (off-the-shelf wiring plan)

`electronics-plates-v3.zip` ([preview](https://github.com/sshcrack/led-matrix/releases/download/hardware-oak-panel/electronics-plates-preview.png)) holds three PETG parts for [`hardware/wiring/`](../wiring/README.md). **Prototype study: not printed or fitted.** Source: `electronics_plates.py`, `make-electronics-plates.py`.

Panel rear (owner, 2026-11-15): inside the PCB windows the board sits 12 mm below the frame top (≈3 mm behind the LED tips); the tallest parts in the connector-free half are two 220 µF electrolytics (≈5 mm). Plugged HUB75 connectors reach 20 mm above the board (≈23 mm from the LED tips, below the carrier web at 27 mm); the power cable bend reaches ≈32 mm. All four panels hang the same way up and have their connectors in the same half, 33–64 mm from the panel centre (ruler photo, ±3 mm).

- **Half plates inside the carrier** (184 × 110 × 2.5 mm): each plate lies in its carrier's open middle, 1 mm above the panel frame, and covers only the connector-free half; it ends 15 mm below the lowest connector, so plugs and cables pass freely. Notches clear the carrier's corner bosses; four 8.5 mm posts reach the underside of the carrier's rear web. Assembly: lay the plate on the panel, fit the carrier over it and screw the carrier to the panel, then drive M3 × 12 thread-forming screws from the carrier's rear face through the seam holes (±86, −54) and (±54, −86) into the posts. The hole set is symmetric under 90° turns, so a plate fits whichever half of its panel is free.
- **Power plate**: both 24→5 V 20 A modules on M4 slots (the listing gives 5.3 mm flange holes; the ~60 mm spacing is estimated from photos, so the slots cover 54–68 mm) and a row of five WAGO saddles. Inline fuse holders ride along their cables.
- **Controller plate**: four 3.5 mm standoffs on the Pi's 58 × 49 mm pattern (M2.5 thread-forming screws), saddles for the LM2596, Pro Micro and level converter, vents under the Pi.
- Small parts sit on **saddles** (two 2 mm ribs); a cable tie loops over the part and through the gap under it, so nothing passes under the plate.
- **DC socket coupon**: a 6 mm wall section with the Kycon KPJX-PM-4S cutout (14 mm, assumed from the drawing), 2 × M2.5 holes on 19 mm and a 1 mm flange pocket. The flange sits outside, the screws (6.6 mm + wall, so M2.5 × 12) come from inside into the flange's 2.3 mm holes. Print it first to check the cutout and that the brick's plug latches.

Parts taller than the posts pass through the carrier's 159.6 mm opening; `make-electronics-plates.py` checks that, the connector keep-out, the boss notches and the posts, and a solid-intersection check found no overlap with the carrier.

**Depth:** from the LED tips the tallest parts end at 50.5 mm (modules) and 55 mm (Pi + Electrodragon stack, 33 mm budgeted for the owner's ≈30 mm), so the body needs **≈60 mm** including 3 mm air and a 2 mm rear cover. That keeps the v0.6 body depth; with the 25 mm halo wall gap the panel front sits ≈85 mm off the wall. (Plates on the carrier's rear face, v1/v2, needed ≈78 mm.)

**Viewing assembly:** `make-electronics-assembly.py` writes `electronics-assembly.glb` (browser viewers such as https://3dviewer.net) and `electronics-assembly.step` (FreeCAD): 2×2 panels, carriers, both plates with part envelopes, the panel plugs (red) and a translucent rear cover at the body depth (`--body-depth`, default 60). Seen from the wall: power plate bottom left, controller plate bottom right.

**Still to check in the fit test:** ribbons between panels cross the seam where the carriers' rims sit (rim 6 mm above the frame top); a ribbon either folds flat under the rim or loops over the carrier's rear face.

## Copy-shop calibration and comparison pack

Two new diagnostic downloads are separate from the withdrawn templates:

- `panel-print-calibration.pdf`: **one uncorrected A4 page**, with horizontal and vertical 100 mm bars. Print this first through the intended printer/app/settings and report both physical lengths. The calibration PDF itself is never enlarged.
- `panel-mounting-source-comparison.pdf` (also `panel-mounting-source-comparison-96mm.pdf`): **three A4 four-corner pages**: A, official Waveshare DWG (174 x 158 mm); B, different Mightary/Furi frame (166 x 172 mm); C, approximate owner insets within a nominal 192 mm module (175.2 x 160 mm). These are alternative hypotheses, not approved carrier geometry. Intermediate holes are deliberately omitted.

The owner reports both uncorrected 100 mm calibration bars printed as **9.6 cm / 96 mm**. The current comparison pack therefore enlarges geometry and check bars to **104.167%** in each axis (100/96). This supersedes the earlier approximate 95 mm assumption. These factors belong to that print route only. A copy shop printing at true 100% requires a newly generated un-enlarged pack; using the compensated pack there would oversize the pattern. Use the same print route/settings as the calibration sheet, one PDF page per A4 sheet, and verify that **both comparison-page bars print at 100 mm** before assessing fit. The dashed rectangle joins hole centres, not panel edges. Record matches by page A/B/C and corner TL/TR/BL/BR, and mark HUB75 IN orientation.

Regenerate with the physical readings from the **uncorrected** calibration sheet (the current reported values are 96 mm in each axis):

```sh
.venv/bin/python make-comparison-pack.py --printed-x-mm 96 --printed-y-mm 96
```

The helper always produces a fresh, uncorrected calibration page and writes the assumed readings, applied scales and candidate page mapping into `comparison-print-basis.json`. It is independent of `build.py` and the withdrawn eight-hole solver; it changes no enclosure solids or raw measurements. Sources are linked inside the PDF. No private photos or vendor drawing binaries are embedded.

## Reproduce on another machine

From this directory, using [uv](https://docs.astral.sh/uv/):

```sh
uv venv --python 3.12 .venv
uv pip install --python .venv/bin/python -r requirements.lock
.venv/bin/python build.py
```

On Windows, use `.venv/Scripts/python.exe` in place of `.venv/bin/python`. Existing Python 3.12 virtual environments and pip also work. Generated files go to ignored `exports/`; use `build.py --output /path/to/output` to choose a different directory. No application/emulator build is required.

`enclosure.py` holds the CAD parameters. Its `--parameters overrides.json` option exports a custom assembly; the complete `build.py` pipeline regenerates the default concept. Helpers use the same default parameters and accept `OAK_PANEL_OUTPUT` as their output directory.

## v0.6 enclosure basis (on hold, historical six-hole mounts)

This records the v0.6 full-enclosure concept. Its six-hole mounts and carrier tiles are superseded by the four-corner trial carrier above.


- Supplier nominal panel envelope: 192 × 192 mm. Approximate measured depth: 15 mm from LED tips to rear mounting face, and 32 mm including the connected cable bend.
- The supplied Adafruit 4732 drawing is adopted as a matching-panel assumption: M3, columns ±85 mm and rows -73/0/+73 mm from each panel centre. This assumption is now superseded for fit verification; the modeled carriers have not yet been updated. The drawing frame is 191.70 mm square and is centred within the nominal module envelope; the imported reference meshes were not used as drilling templates.
- Four 191.6 mm PETG carrier tiles on 192 mm pitch leave 0.4 mm seams. Each has a 4 mm web, a 6 mm forward rim and six integrated 12 mm spacer bosses. The 3.5 mm bores, 10 mm boss outside diameter and screw-head spaces are provisional.
- Four 50 × 16 × 4 mm bolted joiners connect tile seams. Four vented cover quadrants are approximately 193.3 mm square. All exported printed parts fit a 210 × 210 × 210 mm square within the MK3S bed (X 250 × Y 210 × Z 210 mm) with a 5 mm XY margin on every side.
- Carriers are oriented for printing rear face down, with the bosses rising from the bed-facing web. PETG is a starting material, not a validated operating-temperature specification. The print service determines settings, filament availability and price.
- Four black PETG L-shaped bezel quadrants replace the oak surround. Each quadrant is at most 199.8 × 199.8 × 60 mm, with a 0.4 mm seam at each edge midpoint. The 8 mm visible border and 5 mm outer-corner radius are design assumptions. Print-joint details, alignment and finishing remain unvalidated. The old thin diffuser carrier is still an optical reference, excluded from printable exports.
- Eight rear-facing strip envelopes (160 × 10 × 3 mm) and a small driver/harness allowance reserve space for a halo light. A cover passage is modeled; actual strip mounting, edge protection, hardware, power budget and software are not implemented. The strip allowance ends 23 mm from the assumed wall. Separate brightness/off, fixed colour and optional soft display matching are intended. Derive matching colours from the existing render pipeline, not always-on web-preview captures. The strip type/voltage and control transport must be selected with the electronics.
- The 25 mm wall gap is an optical starting point to test, not a guaranteed halo width or brightness. The frame body remains 60 mm deep; its internal electronics are not yet selected.

The material/manufacturing constraints and measurement confidence are in `measurement-basis.json`. Supplier references: [Waveshare P3 panel](https://www.waveshare.com/rgb-matrix-p3-64x64.htm), [Adafruit 4732](https://www.adafruit.com/product/4732), [PETG guidance](https://help.prusa3d.com/article/petg_2059).

## Measured relative pattern / measured-v1

**This section records the withdrawn construction, not usable drilling/fit coordinates.** Raw digital readings, exclusions and confidence are in `mounting-measurements.json`; `mounting_measurements.py` calculates the relative pattern. The reference features are the **narrow smooth shafts below the original magnetic discs**, approximately coaxial with the inserts. They are not the disc diameters or inside threads. The method uses `(mean outside + mean inside) / 2`, or `mean outside - (diameter 1 + diameter 2) / 2` where the inside jaws cannot reach. D uses the two unequal shaft diameters 5.50 and 5.15 mm; invalid D inside readings and all early analog trials are excluded.

| Span | Direct derived centre distance (mm) | Paper-check choice |
| --- | ---: | --- |
| A / upper right | 78.787 | A; provisionally F = A |
| B / lower right | 79.112 | B; provisionally E = B |
| C / bottom right | 94.395 | G via C = G; first C inside reading excluded, others provisional |
| D / bottom left | 79.285 | D; provisionally H = D |
| G / top left | 94.445 | G, a single outside measurement with measured shafts |
| H / top right | 79.275 | Control only; template uses D |

These are calculation digits, **not a certified tolerance**. B/C include photo-transcribed readings; A and D were explicitly text-confirmed. Repeated measurements do not establish shaft coaxiality or caliper alignment. The user confirmed **H = D, G = C, F = A, E = B** as provisional fit-template assumptions; the control readings are retained separately rather than silently averaged into them.

The withdrawn construction summed these values into 173.73 x 157.90 mm and placed the bottom/top intermediate holes at x = 79.285/94.445 mm. These are **not established Cartesian coordinates or overall spans**: the intermediate inserts have offsets normal to the edge as well as along it, and the caliper measuring-axis directions were not recorded. Preserve the readings; do not silently reinterpret diagonal distances as X/Y projections. The eight sketch IDs and provisional pairings remain records, with no centre hole. Connector orientation and perimeter offsets remain unconfirmed.

## Official drawing and reported four-corner fit

The official Waveshare wiki supplies a [DWG drawing](https://files.waveshare.com/upload/e/e1/RGB-Matrix-P3-64x64-2D.zip) whose eight-insert layout matches the observed along-edge and normal offsets. Its left view gives corner centres at X = +/-87 and Y = +/-79 mm, a **174 x 158 mm** corner rectangle, within a **191.6 mm** square frame. On 2026-11-15 the owner reports page A fits perfectly and explicitly confirms both printed bars measured 100 mm. The four-corner pattern is therefore paper-verified for a prototype gauge; the complete frame revision remains unproven; the view, connector orientation, exact frame revision and thread engagement are unspecified. The file's inch unit metadata conflicts with its millimetre-sized dimensions and must not drive automatic import scaling.

[Research and verification evidence](panel-mechanical-source-research.md) records geometry-derived versus dimensioned values, alternative OEM sources and the uncertainty in comparing earlier caliper projections with diagonal distances. `measurement-basis.json` preserves the candidate separately from measured evidence. The old templates remain withdrawn and no enclosure solids have been updated. The next physical check under [#170](https://github.com/sshcrack/led-matrix/issues/170) now combines the rigid pattern check with the one-panel trial carrier and clearance coupon in one job. The paper fit does not validate thread depth, rigidity or full enclosure mechanics.

## Four-corner prototype mounting

Use the four corner inserts only, leaving intermediate inserts unused. The owner has confirmed the 174 x 158 mm source pattern fits on paper, with both scale bars at 100 mm. That replaces the need to repeat the short corner-inset measurements now; earlier approximate readings remain evidence in JSON.

The current trial is the separate carrier/coupon kit above. Record actual contact/interference and the HUB75 IN edge relative to the long side of the gauge. The next mechanical trial is a one-panel carrier/diffuser assembly after rigid fit and clearance results. Final thread engagement, coplanar support, load paths, temperature and print retention remain unverified.

`make-mounting-template.py` and `build.py` still stop before exporting the rejected eight-hole coordinate model. `make-fit-kit.py` is a separate, explicit four-corner path based on the confirmed paper pattern. Full enclosure solids retain historical mounting geometry; the standalone trial carrier uses the corrected four-corner pattern. Track acceptance under [#170](https://github.com/sshcrack/led-matrix/issues/170) and mechanical completion under [#171](https://github.com/sshcrack/led-matrix/issues/171).

The printer route previously shrank a nominal 100 mm bar to approximately 95 mm. A 100/95 correction addresses only uniform print scaling; it cannot fix missing hole offsets. Keep this as separate calibration evidence for a future corrected template, whose horizontal and vertical bars must both be verified.

The reproducible build checks each represented component as a valid solid, pairwise positive-volume intersections and assembly STEP round-trip solid count/volume. Each printed mesh is checked for watertightness, mesh-to-CAD volume agreement and bed fit. The build does not slice the job or verify real cable routing, tolerances, strength/creep, wall fastening, cooling, RF, electronics, diffuser or halo behaviour. Geometry checks do not validate the obsolete six-hole mounting assumption. The measured-v1 paper check is withdrawn and the STL mounting gauge remains superseded.

## Revision record

| Concept | Recorded change |
| --- | --- |
| v0.1 | Initial 420 mm square, 50 mm deep packaging study with assumed module depth. |
| v0.2 | Measured connected-depth allowance; conservative 65 mm stacked layout. |
| v0.3 | Magnetic-contact-depth study; shared cable/electronics depth, 55 mm frame. |
| v0.4 | Adopted drawing-based M3 screws and spacers; 55 mm frame. |
| v0.5 | FDM carriers/joiners and split cover replace the metal chassis; 60 mm frame. |
| v0.6 | Narrow black printed bezel replaces oak; rear halo reservations and provisional 25 mm wall gap. |

Earlier appearance concepts are superseded by v0.6. All six-hole mounting checks in v0.5/v0.6 bundles and the later measured-v1 paper check are unsuitable for fit verification. This source publication starts Git history for the hardware work; future iterations should be committed and their validation evidence attached to the issue.
