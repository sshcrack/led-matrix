# Independent design review: one-panel trial carrier vs. appearance reference

Review date 2026-11-15. Reviewed revision: `c5960480` (shared checkout clean). Scope: mechanical/product design of the current one-panel trial carrier + clearance coupon (`trial_carrier.py`, `make-trial-carrier.py`, `trial-carrier-instructions.txt`), and how it leads to the intended product (four 64×64 P3 modules in a 2×2, 384 mm active area, narrow black bezel, close-contact fabric, rear halo, Wi-Fi only). The v0.6 enclosure solids are considered only as a path, since they are explicitly on hold. Tracked under #170 / #171 and PR #173.

Reference used: the approved public crop [`panel-appearance-reference.png`](https://github.com/sshcrack/led-matrix/releases/download/hardware-oak-panel/panel-appearance-reference.png), inspected visually and with simple pixel measurements. No private material is reproduced here.

> **Resolution (2026-11-15, trial-carrier-v2):** R1 fixed by nesting the coupon in the carrier's open middle (201.6 mm square job fits the MK3S bed either way round). R2 and R3 added to the instructions (remove all eight feet; no-pressure flatness, pin, paper-gap and marker checks). R4 added as six sight holes (both handedness variants of the 7.5 mm stagger). R5 added as eight symmetric seam holes plus a +Y orientation notch. R7 and R8 added as recording steps. R10 section retitled. R6 left to the marker contact test. R9 partly addressed: the carrier source now names its bed view; the v0.6 enclosure frame is unchanged while on hold.

## Verdict

**The trial is sensible to quote and print.** It is a low-cost way to answer the right questions: four-corner pattern fit in a rigid part, all-four seating, interference with ribs/PCB/glue/cables, and clearance-hole size. I found **no geometry blocker** in the carrier or coupon. Mounting bores sit on the paper-confirmed 174 × 158 mm pattern, every boss is fully supported by the web, there are no overhangs, and the regenerated kit passes all of its own checks.

Make three no-cost corrections before submission (R1–R3). They do not change the carrier geometry, but without them the job may be rejected or the fit result misread:

1. **R1**: the "preplaced" 3MF is laid out for a 210 (X) × 250 (Y) bed. A Prusa MK3S bed is 250 (X) × 210 (Y), so the coupon lands 20.6 mm past the back edge.
2. **R2**: the instructions never tell the owner to remove **all eight** magnetic feet. A foot left in an intermediate insert reaches exactly to the web surface and alongside the rib, so the carrier cannot seat.
3. **R3**: the instructions do not say how to judge "seats flat". On a flexible frame, hand pressure hides gaps.

Three optional zero-cost geometry additions (R4–R6) would make this one print yield much more information and reuse. If the parent regenerates the kit for R1 anyway, include them.

## What was verified (and what was not)

- Regenerated the kit with the documented command into a temporary directory. All internal assertions passed: single valid solids, watertight single-body meshes (mesh/CAD volume error 1.4e-5 carrier, 8.2e-6 coupon), STEP round-trip, bore/web/rim probes and 3MF round-trip. Carrier 191.6 × 191.6 × 16.0 mm, 66.03 cm³. Coupon 80 × 24 × 3 mm, 5.63 cm³.
- `single-job-layout.3mf`: three package parts (model, rels, content types), `unit="millimeter"`, two mesh objects, two build items with **translation-only** transforms (5, 5, 0) and (5, 206.6, 0). No metadata, no machine/slicer settings, no thumbnail. It uses optional production-extension `p:UUID` attributes without `requiredextensions`, which is valid and harmless. **It is geometry only.**
- STEP headers are the generic OCCT header (`'Author'`, `'Open CASCADE'`) and the STL header is generic. A search of the kit files found no personal path or user name.
- Extra probe: at all four drawing intermediate-insert positions the web top is at z = 4, which is 12 mm behind the insert-face plane at z = 16. The rib inner face is 5.8 mm (side middles) and 4.8 mm (top/bottom intermediates) from those insert axes.
- **Not verified, and not claimable from CAD or the paper fit:** physical fit, insert coplanarity, thread identity/engagement, stiffness, creep, thermal behaviour, slicer acceptance, print time or price.

## Reference alignment

From the public crop (520 px square, frame ≈ 380 px wide):

| Aspect | Reference | Current concept | Assessment |
| --- | --- | --- | --- |
| Overall form | Square, flat, black, wall-hung, single cable exits at the bottom | 400 × 400 mm square black printed bezel, external brick | **Aligned** |
| Border | Dark border ≈ 9–11 px per side ≈ 2.5 % of width (≈ 10 mm if scaled to 400 mm) | 8 mm border = 2.0 % (`enclosure.py:24`); black margin from outermost LED edge ≈ 8.45 mm | **Aligned**, slightly narrower than the reference, which is fine |
| LED look | Discrete round dots with dark gaps, crisp edges, slight bloom; looks bare or in contact with a dark cover | 0.3 mm fabric 0.5 mm in front of LED tips (`enclosure.py:33-35`) | **Intent aligned.** See P-A: an unsupported 0.5 mm air gap over 384 mm is not a stable design, and white fabric would lose the dark gaps |
| Dot density | Grid period ≈ 6.4–6.8 px ⇒ only ≈ 56–60 dots across the active area, i.e. 64-class density | 128 × 128 at 3 mm pitch | **Expectation gap, not a defect.** On the real product each dot is about half the reference's relative size: finer and smoother, less "chunky". Confirm by viewing one real panel at the intended distance |
| Halo | Warm wall glow, strongest left/right, extends roughly a frame-border-to-several-cm beyond the frame | Rear strips inset 21 mm from the outer edge, 25 mm wall gap (#208) | **Intent aligned.** Halo width/brightness is untested (known gap) |
| Depth | Not measurable from a frontal photo; visually reads as slim | 60 mm body + 25 mm gap = 85 mm proud of the wall | **Unknown.** Decide after electronics (P-C). Do not infer a depth target from the image |
| Seams | Invisible at the centre | Four modules on 192 mm pitch, 191.6 mm bodies | Depends on glued-array pitch (R7) |

The trial carrier itself has no visible role in the appearance. It matters to the look only through z-stack consistency (LED plane vs. bezel/fabric) and through pitch/seam continuity.

## Ranked findings

Severity: **S1** = fix before submission (cheap, avoids a wasted or misread job); **S2** = recommended now at zero or near-zero cost; **S3** = path-to-product items that need no action for this print.

### R1 (S1, confidence: high for the bed spec, medium for slicer behaviour): the 3MF layout and docs transpose the MK3S bed

- **Evidence:** `make-trial-carrier.py:73` asserts `xlen+10 <= 210 and ylen+10 <= 250`. `:136` places the coupon at Y = 206.6. `:144-145` assert the layout against X ≤ 205, Y ≤ 245. `:59` sets `target_bed_mm [210, 250, 210]`. The same orientation appears in `README.md:27`, `trial-carrier-instructions.txt:7`, `measurement-basis.json:204` (and `manufacturing.provided_build_volume_mm`), and in the published `trial-carrier-preview.png` ("210 x 250 mm bed", coupon above the carrier). Prusa's official MK3S+ specification is **25 × 21 × 21 cm, i.e. X 250 × Y 210 × Z 210 mm** ([Prusa MK3S+](https://www.prusa3d.com/original-prusa-i3-mk3/)).
- **Failure scenario:** the operator loads the "already placed" 3MF with an MK3S profile. If the slicer honours the build transforms (as 3MF intends), the coupon occupies Y 206.6–230.6, 20.6 mm beyond the bed. The slicer then flags it as outside the print volume, and the operator rejects the file or re-arranges it. Either way the preplacement claim is wrong and adds a round trip. The STL route and the 400 × 400 alternative printer are unaffected.
- **Fix:** keep the carrier at (5, 5) and rotate the coupon 90° to X 206.6–230.6, Y 5–85. Margins become right 19.4 mm and back 13.4 mm, still ≥ 5 mm with a 10 mm gap. Change the assertions to a 250 (X) × 210 (Y) bed, and update the README, instructions, basis JSON and preview text. Optionally record the owner's "210 × 250" phrasing as the same printer, written Y × X. Alternatively, drop "preplaced" and ask the service to auto-arrange.
- **Test:** re-run the generator. Assert `bounds[1][0] <= 245` and `bounds[1][1] <= 205` for the MK3S orientation.

### R2 (S1, confidence: high for geometry, medium for the feet's disc size): instructions do not require removing all eight magnetic feet

- **Evidence:** intermediate inserts at drawing (±86, 0) and (±7.5, ±87) (`measurement-basis.json:159-176`) all lie over the carrier's 16 mm web band. With the insert faces on the boss tops (z = 16), the web top is at z = 4, which is **12.0 mm** behind the insert faces. The owner's basis gives LED tip→insert face ≈ 15 mm and LED tip→back of fully screwed foot ≈ 27 mm (`measurement-basis.json:13-16, 44-47`), i.e. a foot protrudes ≈ 12 mm: **zero nominal clearance to the web**. The rib (z 4–10) starts 4.8–5.8 mm from those insert axes, so any magnet disc larger than ≈ Ø9.6 mm overlaps the rib zone. Reference feet in the related OEM family have Ø12.2/14.5 mm heads (`panel-mechanical-source-research.md`, Source 2). That only illustrates the risk; it is not data on the owner's feet. `trial-carrier-instructions.txt:21-24` (steps 2–5) mention feet only as screw candidates and never say to remove them.
- **Failure scenario:** the owner leaves feet in the intermediate (and/or corner) inserts. Their panels were last described with feet "fully screwed in". The carrier then stands proud or rocks on the feet, and the result is misreported as a pattern mismatch or a seating failure, wasting the print's main purpose.
- **Fix:** add an explicit step 0: "Unscrew and set aside **all eight** magnetic feet, corners and intermediates. Leave all inserts empty for the fit check." In R4, sight holes double as a visual check that the intermediates are free.
- **Test:** the instruction text is the change. No geometry change is required.

### R3 (S1, confidence: high): the seating test is not operationally defined

- **Evidence:** `trial-carrier-instructions.txt:22` says "Record whether all four boss faces can seat flat". The carrier is an open 159.6 mm frame with a 4 mm web and a 4 × 6 mm rib (`trial_carrier.py:17-30`), so it twists easily under hand pressure. Inserts on a glued 2×2 array may not be coplanar.
- **Failure scenario:** the owner presses the carrier on and sees all four bosses touch, then later tightens four screws and pulls a non-coplanar panel or a glued seam into a step that is visible at the LED face, or gets a false "fits".
- **Fix (method only):**
  1. Before the panel, place the carrier bosses-down on a flat reference (glass or stone) and record the largest feeler/paper gap under any boss. This separates print warp from panel non-coplanarity.
  2. On the panel (all feet removed, R2), rest the carrier under its own weight. Check each boss with a feeler gauge or a thin paper slip, and record the gaps.
  3. Mark the boss tops with a non-permanent marker, rest the carrier and lift it to see whether contact is on the insert face or on the surrounding frame (R4/R5).
  4. Pass a Ø3 mm rod or drill shank through each bore to the insert, to show concentric alignment without any screw.
  5. Do not tighten. If screws are later tried, finger-snug only after a thread is identified.
- **Test:** report per-corner gap values (TL/TR/BL/BR in a stated view), the contact pattern photos (kept private), and the pin-alignment result.

### R4 (S2, confidence: high for value, zero cost): add sight holes at the four intermediate inserts

- **Evidence:** the exact frame revision is unproven and only the four corners are confirmed (`measurement-basis.json:180-186`). The drawing's intermediate set {(±86, 0), (+7.5, +87), (−7.5, −87)} is **point-symmetric**, so it maps onto itself under the 180° flip, the only other way this carrier fits the rectangle. The positions lie on the web (z 0–4), clear of the rib (≥ 4.8 mm from the axis to the rib face).
- **Failure scenario without it:** after the trial, intermediate-insert positions are still unconfirmed, and a later 6- or 8-point carrier or a revision check needs another measurement or gauge.
- **Fix:** cut four Ø5 mm through-holes in the web at those coordinates. Ø5 leaves ≥ 2.3 mm to the rib face on the top/bottom ones. The owner can then look through each hole and see whether the insert is centred, confirming the full drawing pattern (and thus the drawing as a whole) at no material cost. The holes also make R2 visually checkable.
- **Test:** add `isInside` probes for the four sight holes at z = 2 to the generator.

### R5 (S2, confidence: high for geometry): seam holes make the carrier handed, so the four tiles would need two different parts

- **Evidence:** `trial_carrier.py:12-13` puts the seam holes only at (86, −54) and (−54, 86). The mounting rectangle is 174 × 158 mm (not square), so a carrier fits a given panel orientation only as-is or rotated 180°. With all four modules in the same orientation: BL needs seams on +X/+Y (as-is), TR needs −X/−Y (180°), but **BR and TL need a mirror image**, which no rotation provides.
- **Failure scenario:** the trial carrier can only be reused in two of the four positions, and a four-tile set needs two STLs/SKUs, doubling the parts to manage and the reprint risk.
- **Fix:** place seam holes symmetrically on all four edges at (±86, ±54) and (±54, ±86) (8 holes). One STL then fits all tiles. Unused holes are harmless and cost nothing to print. Also add a small orientation mark (e.g. a notch on the rib) so the HUB75 IN edge can be recorded consistently.
- **Test:** the generator asserts 8 seam bores. Check that the hole set is symmetric under X and Y mirror.

### R6 (S2, confidence: medium): the boss contact is wider than the drawing's insert boss

- **Evidence:** the carrier boss is Ø10 (`trial_carrier.py:10`). The drawing shows each insert as a Ø2.4 mm hole within a **Ø9.0 mm boss circle** (`panel-mechanical-source-research.md`, rear-frame view). Whether the insert face is the proudest feature, or sits in a recess or pocket, is unknown.
- **Failure scenario:** if the insert is recessed or flanked by frame ribs, a Ø10 face bears on the frame instead of the insert, giving a gap at the insert or a false seat. This is the same geometry a final carrier would inherit.
- **Fix:** either rely on R3's marker-transfer test, or (free) add a 1 mm tall Ø7–8 mm contact pad on top of each boss, keeping the Ø10 body below. Keep the 16 mm grip by subtracting the pad from the boss height.
- **Test:** contact-pattern photo per corner.

### R7 (S2, confidence: high for the method, medium for impact): record the real glued-array pitch and body size during the trial

- **Evidence:** the trial and v0.6 assume 191.6 mm carriers on a **192 mm** tile pitch with a 0.4 mm seam (`README.md:78`, `enclosure.py:226-229`). The owner's modules are joined/glued, and the actual body and pitch are unmeasured (`panel-mechanical-source-research.md`, uncertainty 4). An LED array of 64 × 3 mm puts outer LED centres 1.5 mm from a 192 mm edge, so butted 191.6 mm bodies would give a 2.6 mm pitch across the seam.
- **Failure scenario:** four 191.6 mm carriers on a < 192 mm glued pitch interfere at the seams, or the seam shows as a visible line in the LED grid. Neither is detectable with only one carrier fitted.
- **Fix/test (no print change):** with calipers, measure (a) the body size in both axes on one module, and (b) the distance spanning 10 LED pitches straddling each seam vs. 10 pitches inside one module, in both axes. Record whether bodies touch. Set the four-tile carrier side to `pitch − clearance` from these numbers.

### R8 (S2/S3, confidence: medium): cable and connector passage through the carrier perimeter is the main unknown for reuse

- **Evidence:** the web band blocks the outer 16 mm of the module footprint at 12–16 mm behind the insert faces, and the rib blocks the outer 4 mm at 6–12 mm. The connected cable bend reaches ≈ 17 mm behind the insert face (32 − 15, `measurement-basis.json:17-20`). Connector XY positions are not known (`enclosure.py:218-224` reserves an assumed 50 × 40 mm block).
- **Failure scenario:** inter-module HUB75 ribbons or power leads that cross a seam must squeeze into the ≤ 6 mm gap above the rib or route out through the open middle, around behind the base and back. A connector inside the 16 mm band collides with the web. In the final four-tile assembly this forces a carrier redesign after the trial print.
- **Fix:** none needed before printing. The trial is the right test. Add explicit recording to the instructions: for each connector and each cable that crosses the carrier footprint, note (a) the edge band it crosses (web, rib or open middle), and (b) the height above the insert-face plane where it crosses. Then design cable notches/pass-throughs in the rib/web for the four-tile carrier.

### R9 (S3, confidence: medium-high for math, low for impact): CAD frames disagree on handedness

- **Evidence:** `enclosure.py:3` defines XY as the display plane with **+Z from the viewer toward the wall** (right-handed). A front viewer then sees **+X on their left**, so `panel_left_*` (x < 0), the controller reservation (x = −65) and the halo pass-through (x = +170) are mirrored relative to the front view. `trial_carrier.py:1` uses +Z toward the panel (front-view consistent) but copies the enclosure's "bottom-left" seam-hole XY.
- **Failure scenario:** seam, cable, antenna or halo positions moved between the enclosure and the carrier frames without stating the view arrive mirrored.
- **Fix:** state one named view (e.g. "rear view, as assembled, X right, Y up") in both sources and in manifests. Label carrier edges in that view.

### R10 (S3, doc, confidence: high): the README "Current mechanical basis" still reads as six-hole current

`README.md:74-83` still describes M3 columns ±85, six-boss carriers and joiners as "current". They are marked superseded elsewhere, but a quick reader could mix them up. Retitle this section as "v0.6 enclosure basis (on hold, historical six-hole mounts)".

## Path from trial carrier to product (planned gaps, not defects of this print)

- **P-A Optics and fabric retention.** To match the reference (crisp dots, dark gaps, black look), test a **dark/black, thin, open** fabric or a smoked contrast sheet against white or grey. White fabric raises the black level and blurs the gaps. A specified 0.5 mm air gap over 384 mm (`enclosure.py:35`) cannot be held uniformly by tension alone. A more robust hypothesis to test: stretch the fabric "drum-like" so the LED field itself supports it (light contact, ~0 mm gap), with the fabric clamp plane slightly behind the LED tip plane, on a removable stretcher ring clamped between the bezel lip and the carrier stack from the rear. Gate: swatch photos on one powered panel at 0 / 0.5 / 1 / 2 mm spacing from the intended viewing distance. The approved trial carrier is a suitable fixture for this.
- **P-B Z-datum chain.** Make the LED tip plane the master datum: bezel lip → fabric → LED tips → (≈ 15 mm, per-module variation unknown) → insert face → 12 mm boss → web. Record the LED-tip-to-insert-face distance on all four modules during the trial, because fabric contact depends on their spread.
- **P-C Depth.** 85 mm off the wall is a choice to confirm against the reference's slim impression. It can probably shrink once the electronics are known: the open middle of each carrier (159.6 mm) already holds connectors and could host electronics. Decide with the electronics design (#172). Do not change it for this trial.
- **P-D Thermal and creep.** Each module is specified up to 20 W (80 W for four, `measurement-basis.json:37-43`). Prusament PETG lists HDT 68 °C at 0.45 MPa (ISO 75) ([TDS](https://prusament.com/wp-content/uploads/2022/10/PETG_Prusament_TDS_2021_10_EN.pdf)). A 16 mm PETG boss clamped under screw preload near warm module backs will relax over time. For the product: low preload with threadlocker or washers, or metal M3 standoffs/sleeves in the bosses. Gate: soak one module on the carrier with worst-case content for 1–2 h inside a closed mock-up with the cover, logging module-back, boss and cavity temperatures, then re-check screw tightness and boss height.
- **P-E Joiners and fasteners.** Joiner holes through a 4 mm web need a captive nut or heat-set insert on the panel side, because that side becomes inaccessible once the module is mounted. Optionally add M3 nut-trap and insert-pilot test features to the next coupon, sized from the chosen insert vendor's datasheet (not guessed).
- **P-F Wall load path.** Not tested by this trial and must not be (as the instructions already say). The hanger reservation (`enclosure.py:435-442`) should carry load through the bezel/rear frame, not through PETG bosses into module inserts.
- **P-G Serviceability.** A rear cover removable without disturbing the fabric. Screw heads stay reachable from the rear in the current concept, which is good. The controller must be swappable without removing modules.
- **P-H RF.** A Wi-Fi antenna behind four module PCBs and near any metal hanger is a real risk. Keep the antenna at the perimeter (the v0.6 reservation is at a corner), away from the hanger. Gate: an RSSI comparison of open board vs. closed mock-up on the wall.
- **P-I One-piece rear frame (alternative).** A single ≈ 384 mm rear frame would fit the 400 × 400 printer with ≈ 8 mm margins and avoid joiners. It costs more material per failed print, and the bezel would still need splitting. Keep the tile approach (it reuses this trial) and reconsider only if R7/R8 show the tile seams are a problem.

## Cost and iteration value

The carrier is 66.0 cm³ of CAD solid. At a typical PETG density of ≈ 1.27 g/cm³ that is ≈ 84 g as a solid-equivalent **upper bound**. The thin web and rib mean slicing will remove little. The coupon is ≈ 7 g. This is arithmetic, not a slicer quote. A skeletal corner-only gauge would save some material but lose the cable-band and reuse information, and the parent already decided against a separate gauge. One combined job is the economical choice. R4–R6 add information at essentially zero material cost.

## Known uncertainties (unchanged by this review)

- Thread identity, usable insert depth and screw length. The 16 mm grip is not a screw length, and 5.15–5.5 mm smooth-shaft readings are not thread data.
- Exact frame/PCB revision and connector registration. R4 partially addresses this.
- Insert coplanarity on the glued array, actual body size and pitch (R7).
- PETG print accuracy for 16 mm long Ø3.5 mm bores vs. the 3 mm coupon. The coupon informs diameter, not long-bore straightness.
- Optics, halo, thermal, RF and wall load: all untested.

## Next validation gates

1. **Before submission:** fix R1 (or drop the preplacement claim), and add R2 and R3 to the instructions. Optionally add R4–R6 geometry and re-run the generator checks.
2. **After printing, before the panel:** flatness and boss coplanarity on a reference surface; coupon clearance (smallest hole a known M3 clears freely, once the thread is identified).
3. **On the panel (no fastening):** all feet removed; four-pin alignment; per-boss gaps; contact pattern; intermediate sight-hole centring (if R4); HUB75 IN edge and seam-hole edge in a stated view; cable/connector crossing table (R8); LED-tip-to-insert-face distance per module; glued pitch and body size (R7).
4. **Thread gate:** identify the insert thread and usable depth with a known screw or gauge, then select screw length and head/washer. Then, and only then, finger-snug fastening and re-check of gaps and seam steps.
5. **Next prints:** a revised universal carrier (R5, R8 notches, R6 pad as needed); the one-panel fabric/stretcher trial (P-A); a thermal soak (P-D); then the four-tile set, bezel, rear cover and wall-mount design and tests.

## Sources

- Prusa: [Original Prusa i3 MK3S+](https://www.prusa3d.com/original-prusa-i3-mk3/) (build volume 25 × 21 × 21 cm).
- Prusament: [PETG technical data sheet](https://prusament.com/wp-content/uploads/2022/10/PETG_Prusament_TDS_2021_10_EN.pdf) (HDT 68 °C at 0.45 MPa, ISO 75).
- Waveshare drawing and OEM comparisons as recorded in `panel-mechanical-source-research.md`.
