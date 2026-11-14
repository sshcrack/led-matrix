# Panel mechanical source research (Waveshare RGB Matrix P3 64x64)

Research note, 2026-11-14. Tracks [issue #170](https://github.com/sshcrack/led-matrix/issues/170). Evidence only: no CAD, template or vendor file was changed or committed here. Downloaded vendor files were inspected temporarily and are not part of the repository.

## Findings

- **An official Waveshare mechanical drawing exists and its hole topology matches the owner's panel.** It is the "2D Drawing" on the [Waveshare wiki page](https://www.waveshare.com/wiki/RGB-Matrix-P3-64x64) (resource section 7.3), a DWG in [RGB-Matrix-P3-64x64-2D.zip](https://files.waveshare.com/upload/e/e1/RGB-Matrix-P3-64x64-2D.zip). The server reports `Last-Modified: 2024-01-11`; the DWG inside is dated 2024-01-11; SHA-256 of the zip is `48ab5ead9c8f301e23bc45c93662eef3d8d692897360c09ec85797032b004163`. It carries no title block text, revision letter, PCB marking or view labels.
- It shows **eight inserts: three top, two middle, three bottom, no centre. The intermediate inserts are staggered both along and normal to the edge, so they are not collinear with the corner inserts.** That reproduces the owner's recorded observation, which the earlier Adafruit 4732 drawing could not.
- **Assessment: strong candidate for an exact mechanical match, not a proven one.** Three clues are compatible: topology, owner span readings and owner corner-inset readings (below). They are not independent metrology validation. Still open: the drawing's view (front/rear) is unlabelled, it never mentions silkscreen `P3(2121)64X64-32S-M5.1`, and batch/back-frame revisions cannot be ruled out. Use it for provisional layout; confirm physically before any print.
- **Reseller hypothesis (unverified):** Waveshare's drawing is a generic LED-module back frame with an M3 magnetic foot, consistent with an OEM module sold under several brands. No source found names the OEM. The only OEM-style drawing found (Mightary/Furi M3, below) is mechanically incompatible.
- The thread is **not established for the owner's panel** (see "Thread").

## Source 1: Waveshare official 2D drawing (candidate exact match)

How read: DWG (AutoCAD 2007 format) converted with LibreDWG 0.13.3 `dwg2dxf`, then parsed with ezdxf. Entities are real geometry, not an image. The header `$INSUNITS` says inches, but every dimension is consistent with millimetres (frame 191.6, 12.00, 87.00, 79.00), so treat as **mm**.

Layers: `1` (geometry), `dim`, `screw` (8 inserts of block `screw_m3_g`), `图框`, `text`.

### Rear-frame view with the eight marked inserts (left view)

The drawing has two plan views, a side view and a top view. The left plan view is the only one that marks the eight `screw` points, and its two large pockets are separated by a central strip carrying two arrow marks. **Which face it shows is not labelled**; the numbers below are in that view as drawn (arrows pointing up). Datum: panel centre of the 191.6 mm square body outline, X right, Y up.

| Insert | X (mm) | Y (mm) | Source of value |
| --- | ---: | ---: | --- |
| Top-left corner | -87.0 | +79.0 | dims 87.00 / 79.00 |
| Top intermediate | +7.5 | +87.0 | dims 7.50 / 87.00, Y from geometry |
| Top-right corner | +87.0 | +79.0 | dims 87.00 / 79.00 |
| Middle left | -86.0 | 0 | geometry (1.0 mm inboard of the corner column) |
| Middle right | +86.0 | 0 | geometry (1.0 mm inboard of the corner column) |
| Bottom-left corner | -87.0 | -79.0 | dims 87.00 / 79.00 |
| Bottom intermediate | -7.5 | -87.0 | geometry (point-symmetric to the top one) |
| Bottom-right corner | +87.0 | -79.0 | dims 87.00 / 79.00 |

- Only 87.00, 7.50 and 79.00 are dimensioned. The 86.0 middle X and the ±87.0 Y of the intermediates are read from drawing geometry, not from a printed dimension.
- Body outline: **191.60 mm square** (dimensioned both ways), not 192 mm. Edge insets derived from it (half-body 95.8): corner inserts **8.8 mm** from the side edges and **16.8 mm** from the top/bottom edges; top/bottom intermediates 8.8 mm from their edge; middle inserts 9.8 mm from the side edges.
- Each insert has a Ø2.4 mm hole circle inside a Ø9.0 mm boss circle (middle-left also a Ø6.6 mm ring). The `screw` block is named `screw_m3_g` and draws Ø3.0 and Ø2.5 circles. That is a drawing symbol, not a thread callout.
- Top-view dimension **12.00 mm** is shown without a stated datum (not the 14 mm advertised depth).
- The right plan view is dimensioned differently: **12 holes at ±90 / ±30 mm** (Ø3.5 through, Ø7.0 counterbore). They are not marked as inserts, their function is unknown, and they do not appear in the owner's facts. Do not use them without clarification.
- Not in the drawing: HUB75 or VH4 connector position, any tolerance, thread depth, material.

### Comparison with the recorded facts

| Recorded fact | Drawing | Result |
| --- | --- | --- |
| Eight inserts, 3 / 2 / 3, no centre | Same | Match |
| Intermediates staggered along edges | Top +7.5, bottom -7.5 from centre | Match |
| Intermediates not collinear with corners | Top/bottom 8 mm outboard of corner line; middle 1 mm inboard | Match |
| Owner sketch: bottom D short (left), C long (right); top G long (left), H short (right) | Bottom 79.9 (left) and 94.8 (right); top 94.8 (left) and 79.9 (right) | Same pattern when read as the drawn view |
| Owner raw shaft-span calculations | see next table | Compatible clues; caliper-axis direction is unresolved |

The caliper measuring-axis direction was not recorded. The shaft formulas return a **projection onto the measuring axis**; that equals the Euclidean centre distance only if the axis follows the centre-to-centre line. Do not assume the owner's values are direct diagonal distances. For comparison, both drawing quantities are shown below. Neither comparison proves a physical match.

| Span | Drawing along-edge projection (mm) | Drawing Euclidean distance (mm) | Owner derived value (mm) |
| --- | ---: | ---: | ---: |
| A, upper right side | 79.000 | 79.006 | 78.787 |
| B, lower right side | 79.000 | 79.006 | 79.112 |
| C, bottom long | 94.500 | 94.838 | 94.395 |
| D, bottom short | 79.500 | 79.902 | 79.285 |
| G, top long | 94.500 | 94.838 | 94.445 |
| H, top short | 79.500 | 79.902 | 79.275 |

For the top/bottom pairs, the normal separation is 8 mm; for corner-to-middle side pairs it is 1 mm. If the owner's measuring axes were edge-parallel, their values differ from the drawing projections by about 0.06 to 0.23 mm. If the axes followed centre-to-centre lines, differences from Euclidean distances range from about -0.63 to +0.11 mm. Axis direction, reference-foot coaxiality and the real panel revision remain unresolved. These alternative interpretations prevent claiming a systematic dimensional discrepancy or an exact match. Sketch IDs remain unregistered to connector orientation.

### Owner corner-inset clue (2026-11-14, approximate)

The owner confirmed outer-plastic-edge to far-shaft-surface spans of **X = 11.0 mm** and **Y = 18.6 mm** at every corner, with a shaft of about **5.2 mm** (a rough measurement; the owner states they could not measure accurately). Provisional centre insets: **X = 8.4 mm, Y = 16.0 mm**. Nominal-192 mm layout would imply corner spans of 175.2 x 160.0 mm (not directly measured).

| Quantity | Owner (approx.) | Drawing (191.6 body) | Difference |
| --- | ---: | ---: | ---: |
| Inset from side edge (X) | 8.4 | 8.8 | -0.4 |
| Inset from top/bottom edge (Y) | 16.0 | 16.8 | -0.8 |
| Corner span X | 175.2 (nominal 192) | 174.0 | +1.2 |
| Corner span Y | 160.0 (nominal 192) | 158.0 | +2.0 |

Reading: the owner's larger-inset and smaller-inset axis agree with the drawing's pattern (about 8.8 versus 16.8 mm), which is a good sign. Sensitivities to keep in mind:

- The inset formula subtracts half the shaft diameter: a 5.5 mm shaft in place of 5.2 mm moves each inset by -0.15 mm (X 8.25, Y 15.85), so the gap to the drawing grows slightly.
- The "corner span" columns are not like-for-like: the owner's figures assume a 192 mm body and equal opposite insets, whereas the drawing's body is 191.6 mm. The body size is not measured.
- The owner described the measurements as rough and gave no uncertainty bound. Differences of 0.4 to 0.8 mm per edge are therefore inconclusive; the real panel could also differ. These numbers are clues, not a validation, and the OEM dimensions should not be adopted just because they are close.
- Which owner axis (X or Y) corresponds to the drawing's X (the 87 mm side) is unconfirmed. The similarity pattern suggests X is the short-inset side axis, but the connector orientation is still open.

## Source 2: Mightary / Furi Electronics M3 module spec (similar OEM family, incompatible)

[M3-192X192-2121-32S, version A1.0, issue date 2024-06-20](https://www.ledmary.com/Uploads/file/20250710/20250710154823_63775.pdf) (Chinese-language PDF, page 4 "产品尺寸", low-resolution drawing). Same headline: 192 x 192 mm, 64 x 64, SMD2121, 3 mm, 1/32 scan, VH4 power, HUB75.

- **Eight inserts, but in a regular 3 / 2 / 3 collinear layout**: dimensions 83.00 + 83.00 along the top and 86.00 + 86.00 down the side, i.e. corner spacing about 166 x 172 mm, middle inserts centred on the edges. Outer frame 191.70 ±0.15 mm, inner 175.50 mm, depth 12.00 ±0.1 mm. Rear shows a 3 x 3 window grid.
- Callout text reads approximately "8-Ø5 x M3x6 through-hole round nut", with two locating posts Ø3.70. Tolerances: outline ±0.1 mm, hole ±0.05 mm.
- Magnetic post drawn: M3 external thread, Ø5.5 shaft 12 mm, 17 mm overall, head Ø12.2 / Ø14.5. Fits the owner's 5.5 mm shaft but not the owner's second 5.15 to 5.2 mm foot.
- Signal pins are **A B C D only** (no E), a 16-pin interface that differs from the HUB75E panel in the recorded facts.
- **Classification: mechanically incompatible variant.** Collinear, different spacing, different rear frame, different address lines. Useful as evidence that "same resolution and size" does not imply "same mechanics", and that M3 magnetic posts are common in this family. It is **not** to be used for dimensions.

## Other sources checked (no usable mounting drawing)

| Source | What it gives | Class |
| --- | --- | --- |
| [Waveshare product page](https://www.waveshare.com/rgb-matrix-p3-64x64.htm) | 192 x 192 mm, 3 mm pitch, 1/32 scan, HUB75E, 5V/4A, links to the wiki drawing | Official, drawing only on wiki |
| [Adafruit 4732](https://www.adafruit.com/product/4732) | Six-hole drawing, M3 (recorded in the repo: columns ±85, rows -73/0/+73) | Different panel mechanics, withdrawn as an assumption |
| [DSPPA DP-N307GC](https://vi.dsppacs.com/uploads/file/data-sheet-dp-n307gc-indoor-p3-full-color-led-module.pdf) | 192 x 192 x 14.2 mm, SMD2121, 1/32, VH4, HUB75D (IDC16), 17.5 W | Plausible OEM family (specs only), no mechanics |
| [Dahua IFS-EIA3Z-D](https://material.dahuasecurity.com/uploads/soft/20221003/IFS-EIA3Z_D_Datasheet20220105.pdf) | 192 x 192 x 14 mm, SMD2121, 1/32 | Plausible OEM family (specs only) |
| [Eager LED P3 outdoor SMD1415](https://led-mall.com/specifications/P3_Outdoor_SMD1415_192x192mm_LED_Module_Specification.pdf) | 25 mm depth, 1/16 duty | Outdoor, incompatible |
| [Outdoor VG-P3-16scan](https://p.globalsources.com/IMAGES/PDT/SPEC/632/K1227414632.pdf) | 15.5 mm, 1/16 scan | Outdoor, incompatible |
| iPixel M6464 P3 on [Mouser](https://www.mouser.com/datasheet/2/813/M6464-P3-1393342.pdf) and the Hubtronics P3 indoor datasheet | Reported by search snippets: 192 mm module, 1/16 scan, 14.66 mm (26.55 mm with magnet); Hubtronics has a hole-position diagram | Could not fetch (access denied); unread leads, 1/16 scan variant likely incompatible |

- **Silkscreen string:** `P3(2121)64X64-32S-M5.1` appears on a [project page by "gasman"](https://gasman.zxdemo.org/pharmageddon/) (consumer AliExpress panels from two Shenzhen shops). That author warns that even the same product may have different mounting holes or addressing modes, hopefully the same if the PCB carries this marking. This is a hint that the marking identifies a generic panel; it gives no drawing. A search for the exact string found no manufacturer datasheet or CAD, and no source tying the marking to Waveshare, to an OEM name, or to a hole layout.
- **Chinese and OEM catalog searches** (module size, "结构图", "磁柱", "安装孔") found specification pages for generic P3 192 x 192 modules but no further dimensioned hole drawing. Candidates with specs only: [lekled](https://www.lekled.com/?page_id=20000), [minsonled](https://www.minsonled.com/pro/6.html), [177kd](https://www.177kd.com/scheme/1146.html). Not opened beyond search snippets.
- **Third-party meshes** (the existing STL/3MF reference names) were not used: they do not establish thread or accurate drilling coordinates.
- Nothing found names the manufacturer behind the Waveshare drawing, so the reseller hypothesis stays unverified.

## Thread and insert specification

- **Not established for the panel.** Evidence for M3 is indirect: a symbol named `screw_m3_g` in the Waveshare drawing and an M3 external-thread magnetic post (Ø5.5 shaft) with "M3x6" nuts in the Mightary family spec. The owner's shaft readings concern the foot's smooth shaft, not a thread. Do not infer a thread from shaft diameter. A direct check (M3 screw or a thread gauge into an insert) and an insert depth measurement are still needed.
- The drawing's Ø2.4 mm hole circle may be the moulded pilot, not the insert's inner thread. Do not use it for tapped-hole sizing.

## Mismatch and uncertainty list

1. View orientation of the drawing's marked-insert plan is unlabelled; the arrows pointing up are a lead for orientation, not established.
2. The drawing has no revision, PCB marking, tolerance or connector. A Waveshare batch or frame revision different from this DWG cannot be excluded.
3. The 12-hole ±90 / ±30 pattern in the second view is unexplained.
4. Body size: drawing 191.6 mm versus nominal 192 mm; the physical body is not measured.
5. Unknown caliper-axis directions: top/bottom owner spans are about 0.06 to 0.23 mm below drawing projections, or 0.39 to 0.63 mm below drawing Euclidean distances; these are different comparisons.
6. Owner inset readings are approximate and use a 5.2 mm shaft; the drawing differs by about 0.4 mm (X) and 0.8 mm (Y).
7. Connector orientation versus owner X/Y is unconfirmed.

## Minimum physical verification for the four-corner prototype

Start with **orientation and a four-corner candidate fit check**, rather than repeating the full measurement series:

1. In rear view, identify HUB75 IN and the visible arrow direction, and establish which pair of panel edges is the 174 mm corner-span axis versus the 158 mm axis. Do not mirror the complete eight-hole pattern without registering the view.
2. Check a four-corner candidate pattern against the panel, with print scale confirmed in BOTH axes. If it matches, follow with a small rigid gauge before printing carriers. Previous templates remain withdrawn; no replacement was generated by this research.

If that candidate fails to fit, collect targeted measurements to locate the mismatch: refine corner edge insets with the same measured foot, check the body envelope in both directions, and check corner spans. The drawing predicts side/top insets about 8.8/16.8 mm, body 191.6 mm square and corner spans 174/158 mm. A ruler reading within +/-1 mm may reject a large mismatch but cannot prove agreement at printed mounting-clearance tolerances or conclusively distinguish 174.0 from the approximate 175.2 mm estimate.

Thread fit and usable engagement must still be established when selecting screws, before final hardware or carrier approval. Do not infer M3 from the shaft diameter or use an unestablished screw length. No intermediate-insert measurement is needed for the four-corner design.

## Parent verification and next fit check

The parent independently parsed the converted official DXF and confirmed all eight insert centres and dimension entities 87, 79, 7.5, 191.6 and 12 mm, and inspected the full drawing rendering. The archive hash matches the research record above. The source wiki link was also fetched and checked; vendor originals remain outside Git. The comparison was corrected to distinguish axis projections from Euclidean distances.

The most useful physical next step is a **four-corner pattern check at 174 x 158 mm**, after identifying which pair of panel edges corresponds to each axis and verifying print scale in both axes. This can validate a candidate layout without requiring another full round of intermediate-hole measurements. It is not a carrier approval or evidence of thread depth. The previously withdrawn templates remain withdrawn; this research did not generate a replacement template or update enclosure solids.
