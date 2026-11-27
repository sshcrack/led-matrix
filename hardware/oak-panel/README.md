# LED panel enclosure (v0.7)

A wall-mounted 128 × 128 display: four Waveshare P3 64 × 64 modules glued into a 2 × 2 block,
held by printed carriers in a pine box with a 3 mm HDF back, a rear halo light facing the wall,
and a bought black aluminium picture frame at the front. The electronics are in
[`../wiring/`](../wiring/README.md).

This is a one-off hobby build. Sources are CadQuery/Python; generated files go to the ignored
`exports/` folder and are published on the
[`hardware-oak-panel` release](https://github.com/sshcrack/led-matrix/releases/tag/hardware-oak-panel).

Tracking: [#165](https://github.com/sshcrack/led-matrix/issues/165) (umbrella),
[#170](https://github.com/sshcrack/led-matrix/issues/170) (fit tests; its **Result status**
table is the one place for measured results), [#171](https://github.com/sshcrack/led-matrix/issues/171)
(enclosure and front), [#172](https://github.com/sshcrack/led-matrix/issues/172) (electronics),
[#208](https://github.com/sshcrack/led-matrix/issues/208) (halo).

## Status (2026-11-23)

| Part | State |
|---|---|
| Panel mounting (carriers on the 174 × 158 mm corner inserts) | Verified with the printed trial carrier |
| Electronics | Run on the bench: both panel groups and the halo |
| v0.7 rear print jobs | Designed and CAD-checked, **not yet ordered** |
| Wood box, crossbar, HDF back | Drawn (German cut and drill plan), not built |
| v0.8 plywood rear (62 mm deep, replaces the v0.7 box) | CAD-checked candidate, see below; not ordered |
| Front (frame + diffuser) | Frame bought; canvas vs. black acrylic still open |
| Heat, wall load, final harness fit | Not tested |

## Layout

From the LED tips (Z = 0) toward the wall:

| Plane | Distance |
|---|---|
| Panel frame / insert faces | 15 mm |
| Carrier bosses 15–27, rim 21–27, rear web | 27–31 mm |
| Pine strips (inside of the box) | 31–61 mm |
| HDF rear face | 64 mm |
| Halo channel openings (LEDs face the wall) | 70.1 mm |
| Wall | 95.1 mm |

Seen from the wall: power plate bottom left, controller plate (Pi) bottom right, both lying in
their carriers' open middle on the connector-free half of the panel.

## v0.8 candidate: plywood back

`v08.py` / `make-v08.py` (checks and exports to `exports/v08/`), `ply_drawing_v08.py` (German
drill sheet `exports/v08/sperrholz-bohrplan.pdf`). Not built yet; it would replace the carriers,
the pine box and the HDF.

- One 383 × 383 × 6 mm poplar plywood sheet on 16 printed spacer tubes (Ø9 × 19 mm), one per
  panel insert, held by 16 M3 × 30 through Ø6 holes. The panels are strapped together face down
  first; the ±1.3 mm play lets the sheet follow the closed seams.
- Under the sheet (Z 16–34): five Hailege 24 V → 5.2 V buck boards (listing size, 66 × 27 × 10 mm
  envelope), the fuse holder and two WAGO 221-415, on foam tape, boards and fuse also tied.
  `make-v08.py` keeps free: wire room at both board ends (20 mm), fuse leads (12 + 30 mm),
  WAGO wire entry (20 mm), the DC socket pigtail (25 mm), and paths between the quadrants along
  both seams and for the four HUB75 ribbons. WAGO levers only open with the sheet off the panels.
- Vents work as a chimney: a row of Ø10 holes near the bottom edge lets air in, a row near the top
  lets it out into the gap behind the display. The boards' PCBs lie flat on the wood, so holes
  over them would not help. `v08.vent_holes()` places the holes wherever nothing sits on either
  face of the sheet.
- On the back: a printed pod (Z 40–58) with the Pi hanging upside down from its roof through a
  133 × 62 mm window, the Pro Micro and the DC socket; halo pieces, hanging rails and bumpers
  as before, all screwed on from the sheet's inner face. Eight of the panel screws pass through
  the halo pieces under the strip.
- Two print jobs: halo pieces (~€20) and pod + rails + bumpers + tubes (~€17), PrusaSlicer
  0.2 mm, 4 perimeters, 25 % infill at €25/kg + €1/h.

## Downloads

- [`led-matrix-v07-print-jobs.zip`](https://github.com/sshcrack/led-matrix/releases/download/hardware-oak-panel/led-matrix-v07-print-jobs.zip):
  four 3MF job layouts and the German order sheet. Order **4 × carrier** and **1 ×** each of the
  halo, power and controller jobs (27 parts), 100 % scale, millimetres. The 3MFs carry geometry
  only; the print service slices them.
- [`holz-zuschnitt-bohrplan.pdf`](https://github.com/sshcrack/led-matrix/releases/download/hardware-oak-panel/holz-zuschnitt-bohrplan.pdf):
  wood cut list, drill plan, 1:1 templates and a Bauhaus shopping list (German).
- [`assembly-v07-color.FCStd`](https://github.com/sshcrack/led-matrix/releases/download/hardware-oak-panel/assembly-v07-color.FCStd)
  (FreeCAD) and [`assembly-v07.step`](https://github.com/sshcrack/led-matrix/releases/download/hardware-oak-panel/assembly-v07.step).

Everything else on the release (v0.5/v0.6 bundles, mounting-check PDFs, the first-print kit,
`power-halo-board-revA-*`) is history; see [History](#history).

## Printed parts

Source `v07.py`, built and checked by `make-v07.py` (writes `exports/v07/`).

- **4 identical carriers** (191.6 × 191.6 mm, the trial carrier's outline and bosses): 3.4 mm
  bores for M3 × 20 into the panel's corner inserts, 8 M3 pilots with bosses for the joiners
  and the pine strips, low cable notches on every edge. One part fits every tile.
- **8 seam joiners**, M3 × 12, slotted for 0–1 mm seams.
- **8 halo channel pieces**, 10.7 mm inner width, 2.4 mm walls, two per edge on the HDF, held by
  countersunk 3 × 20 wood screws.
- **Power plate** with a removable WAGO deck (two M3 × 12): three 5-way and two 3-way WAGO
  221 seats, from left (rear view): A return, fused +24 V, 24 V return, B return, B +5 V. The
  two converters are strapped with 4.8 mm cable ties through raised tunnels; 20 mm is kept
  free at each converter's lead exit for the bend.
- **Controller plate**: Pi on 7 mm standoffs for M2.5 × 10, tie anchors for the LM2596 and the
  Pro Micro.
- **2 hanging rails** that hook behind the two existing wall screws (≈260 mm apart), fastened
  with six M3 × 40 through the HDF into the hardwood crossbar. One fully threaded M5 × 45 steel
  keeper per rail, driven from the side into a captive nut, stops the display lifting off;
  back both out to take it down. ±8 mm sideways tolerance.
- **2 bumpers**, M3 × 12 from inside through the HDF.

**Pilot holes** (chosen on the trial coupon with the bought screws, three re-insertions):
2.8 mm for M3, 2.4 mm for M2.5. Clearance for M3: 3.4 mm.

**Material.** Ordered in black HTPLA so the halo channels can be printed with supports that come
off cleanly. As printed, HTPLA softens like PLA (about 55–60 °C). It only gets its higher heat
resistance after annealing, and annealing shrinks the part, which would move the measured hole
patterns. The parts nearest to heat are the power plate (under the converters) and the carriers
(on the panel backs). Measure the converter case temperature in the bright-scene soak (#172)
before relying on unannealed HTPLA there.

**Print orientation**: carrier webs, plate floors and the rails' wall-facing faces on the bed.
Check the bridges over the halo lips, strap tunnels and keeper pockets in the slicer. Keep
supports out of the strip channels and tie tunnels.

`make-v07.py` checks: one valid solid and a watertight mesh per part; every job fits the
Prusa MK3S bed with 5 mm margin; no overlaps between printed parts, wood, panels, component
envelopes, the DC socket, the wall or the wall screws; M3 × 40 engage ≥ 8 mm and their tips
clear the plates and panels; M3 × 20 heads clear the pine; halo screws clear the socket bore and
crossbar bolts; tie tunnels pass a 4.8 × 2 mm tie; the keepers block lift-off.

## Wood

Cut and drilled by hand from `holz-zuschnitt-bohrplan.pdf` (`wood_drawings.py`).

- **Pine 19 × 30 mm**: 2 × 390 mm (top, bottom), 2 × 352 mm (sides, between them), standing on
  the carriers' outer bands, outer faces 3 mm outside the panel block. 22 M3 × 40 with
  3.2 × 7 mm washers go through 5 mm holes into the carrier pilots (16) and plate posts (6);
  Ø9 × 3.5 mm pockets at the back let the HDF lie flat, Ø8 × 3.5 mm reliefs at the front clear
  the panel screw heads.
- **DC socket** (Kycon KPJX-PM-4S-S) centred in the bottom strip, flange outside: 14 mm through,
  two 6 × 7 mm pockets for the plastic bosses behind the flange, flange holes opened to 3.5 mm
  for 3 × 16 pan-head wood screws in 2 mm pilots. Solder the wires to the cups before mounting:
  the cups end inside the hole.
- **Hardwood crossbar** 352 × 48 × 18 mm between the upper side strips, four M6 × 45 bolts into
  M6 cross dowels. It carries the rail load to the side strips.
- **HDF back** 3 mm, 390 × 390 mm, held by the halo channel screws and four corner screws, with
  holes for the rails, bumpers, halo cable and vents.
- Fuse holder tie holes are left unmarked on purpose: dry-fit the four holders and their
  ≈30 mm lead bends on the HDF first, then mark from the real parts.

Fasteners for the hardware store: 3.2 × 7 mm washers (28 + spares), 3 × 20 countersunk wood
screws (28 + spares), 3 × 16 pan-head wood screws (2 + spares). Full list in #170.

## Front

Frame: boesner Pears aluminium frame 40 × 40 cm, black satin, PE124040: 9 mm face, 19 mm deep,
13 mm rebate. Its glass is 400 × 400 × 2 mm; the spring clips also close over a 3 mm acrylic
sample plus canvas. The panel block is about 383–384 mm square; plan the opening at about
385 mm with shims.

Diffuser: open (#171). The pixel grid was tried and rejected. Candidates are canvas directly on
the LEDs and matte black LED acrylic (PLEXIGLAS 9H04 SC sample in hand). Measure the frame's
usable pocket before ordering a full sheet.

## Panel facts

Measured by the owner; details and confidence in `measurement-basis.json` and the #170 table.

- **Mounting**: four corner inserts on **174 × 158 mm** (official Waveshare DWG, confirmed on
  paper and with the printed carrier). Thread M3, a full M3 × 20 screws in. The intermediate
  inserts are not used.
- **Module**: ≈191 mm (CAD 191.6), glued seams ≤ 1 mm with no visible line.
- **Panel rear** (2026-11-15): LED tip to insert face ≈15 mm. Inside the PCB windows the board
  sits 12 mm below the frame top (≈3 mm behind the LED tips); the tallest parts in the
  connector-free half are two 220 µF electrolytics (≈5 mm). Plugged HUB75 connectors reach
  20 mm above the board (≈23 mm from the LED tips); the power cable bend reaches ≈32 mm.
- **Connectors** (rear view, connector half = +Y, `connectors_xy_mm`): HUB75 IN x −66…−53,
  power x 5…19, HUB75 OUT x 42…61, all at y 33…64 (±3 mm), only on the upper 64 × 32 board.
  All four panels hang the same way up. The plugs sit in the carrier's open middle; ribbon and
  power cables pass a 6 mm notch. HUB75 IN is upper left seen from the back.
- **Halo strip** (owner's WS2812B 5 V, 30 LEDs/m, IP65): 10.0 × 2.63 mm; holds well in a
  10.7 mm channel. Eight 5-LED segments (167 mm each).

## Rebuild the files

```sh
uv venv --python 3.12 .venv
uv pip install --python .venv/bin/python -r requirements.lock
.venv/bin/python make-v07.py        # parts, checks, 3MF jobs, STEP/FCStd → exports/v07/
.venv/bin/python wood_drawings.py   # German cut and drill plan
```

On Windows use `.venv/Scripts/python.exe`. No application build is needed.

## Still to check

- After printing: fit the real harness, WAGO lever and fuse-cap access, the real screw head and
  washer stack.
- Converter and panel temperatures in a closed box after a long bright scene (see Material).
- Rails and keepers on the empty rear first, then the wall anchors under the full display.
- All 64 rows and no timing artefacts with the final harness (GPIO15 is the E line).
- Front: diffuser choice, frame pocket size, sheet cut size.

## History

How the design got here, in short. Details stay in the issues and `measurement-basis.json`.

| Version | Change |
|---|---|
| v0.1–v0.4 | Packaging studies with an oak surround and a metal chassis; depth grew with measured connector heights |
| v0.5 | Printed carriers, joiners and split cover replace the metal chassis |
| v0.6 | Narrow black printed bezel replaces the oak; halo reservations and 25 mm wall gap |
| trial kit v6 | One carrier plus screw, halo channel, DC socket, pixel grid and cable coupons in one job; its results set v0.7 |
| v0.7 | Wood box instead of printed rear trays (≈460 cm³ less print), bought aluminium frame instead of printed bezels, electronics plates inside the carriers |

Mounting dead ends, all withdrawn: the six-hole pattern from the Adafruit 4732 drawing, and an
eight-hole pattern reconstructed from caliper readings (`measured-v1`), which assumed the
intermediate inserts were in line with the corners. The official Waveshare drawing settled it.
Do not print `single-panel-mounting-check*.pdf`, `mounting-check-measured-v1.zip` or
`oak-panel-first-print-kit.zip`. The paper calibration and comparison packs
(`make-comparison-pack.py`, printer route measured at 96 mm per 100 mm) were only for that
check. A custom power/halo PCB was designed and paused for cost on 2026-11-15 (branch
`t3/kicad-electronics-pcb-design`; do not fabricate it).

Older generators kept for reference: `enclosure.py` + `build.py` (v0.6, still stops before the
withdrawn eight-hole model), `trial_carrier.py` + `make-trial-carrier.py` (trial kit),
`electronics_plates.py` (plate study v3), `make-fit-kit.py`, `make-mounting-template.py`,
`make-quote-pack.py`, `make-comparison-pack.py`. Research notes:
`panel-mechanical-source-research.md`.
