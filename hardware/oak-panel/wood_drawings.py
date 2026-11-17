#!/usr/bin/env python3
"""Cut and drill sheets for the v0.7 wood rear (pine strips + HDF back), from v07.py.

Writes exports/v07/wood-cut-drill.pdf:
  1    overview (rear view), cut list, shopping list
  2    order of work and tools
  3    strip drilling tables
  4-5  strip templates at 1:1 (two halves that overlap by 10 mm)
  6    HDF back: drawing and hole table
  7-12 HDF templates at 1:1 (six tiles)

Every view is from the wall side: the display lies face down on the table with its top edge away
from you. The templates only work printed at 100 % ("actual size"); measure the 100 mm bar first.
"""

import argparse
from pathlib import Path

from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.units import mm
from reportlab.pdfgen import canvas

import electronics_plates as ep
import v07

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=ROOT / "exports" / "v07" / "wood-cut-drill.pdf")
args = parser.parse_args()
args.output.parent.mkdir(parents=True, exist_ok=True)

OUT, INN, T = v07.STRIP_OUT, v07.STRIP_IN, v07.STRIP_T
SIDE_LEN = 2 * INN
FULL = 2 * OUT
CB_D, CB_DEPTH = v07.STRIP_CBORE
GREY, WOOD, INK = (0.55, 0.55, 0.55), (0.93, 0.85, 0.72), (0, 0, 0)


def fmt(v):
    return f"{v:.1f}".rstrip("0").rstrip(".")


# --- Hole data per strip, in each template's page frame ------------------------------------
# Template frame: back face up (the face the HDF will touch), the strip's OUTER long edge at the
# top of the band. `along` runs from the page-left end; `inset` is from the outer edge.
def strip_holes():
    """{strip: (length, left_end_label, right_end_label, [(along, inset, what)])}."""
    s = {"top": [], "bottom": [], "left": [], "right": []}
    for x, y, what in v07.strip_screws():
        if y > INN:
            s["top"].append((x + OUT, OUT - y, what))
        elif y < -INN:
            # Outer edge on top means the strip is turned round: page-left is the Pi end.
            s["bottom"].append((OUT - x, y + OUT, what))
        elif x < -INN:
            s["left"].append((y + INN, x + OUT, what))  # page-left = bottom end
        else:
            s["right"].append((INN - y, OUT - x, what))  # page-left = top end
    for v in s.values():
        v.sort()
    return {
        "top": (FULL, "either end", "either end", s["top"]),
        "bottom": (FULL, "Pi end (right in rear view)", "power end (left in rear view)", s["bottom"]),
        "left": (SIDE_LEN, "BOTTOM end", "top end", s["left"]),
        "right": (SIDE_LEN, "TOP end", "bottom end", s["right"]),
    }


def socket_holes():
    """Bottom strip's OUTER face, seen from outside below the display: page-left = power end.
    [(along, from_edge, d, what)]; from_edge is from either long edge (centred)."""
    a = v07.DC_SOCKET_X + OUT
    s = ep.DC_SOCKET["screw_spacing"] / 2
    off = v07.DC_SOCKET_Z - v07.Z_REAR
    return [
        (a, off, ep.DC_SOCKET["body_cutout"], "socket Ø14 through"),
        (a - s, off, v07.DC_SOCKET_PILOT[0], "pilot Ø2, 10 deep"),
        (a + s, off, v07.DC_SOCKET_PILOT[0], "pilot Ø2, 10 deep"),
    ]


def hdf_holes():
    """[(X, Y, d, what)] from the HDF's bottom-left corner, seen from the wall side."""
    return [(x + OUT, y + OUT, d, what) for x, y, what, d in v07.hdf_holes()]


# --- Drawing helpers -------------------------------------------------------------------------
c = canvas.Canvas(str(args.output), pagesize=landscape(A4))
c.setTitle("v0.7 wood rear: cut and drill sheets")


def page(orientation="landscape"):
    c.showPage()
    c.setPageSize(landscape(A4) if orientation == "landscape" else A4)


def txt(x, y, s, size=9, bold=False, anchor="left", colour=INK):
    c.setFillColorRGB(*colour)
    c.setFont("Helvetica-Bold" if bold else "Helvetica", size)
    {"left": c.drawString, "centre": c.drawCentredString, "right": c.drawRightString}[anchor](x * mm, y * mm, s)


def lines(x, y, rows, size=9, lead=4.6):
    for r in rows:
        bold = r.startswith("## ")
        txt(x, y, r[3:] if bold else r, size + (1 if bold else 0), bold)
        y -= lead * (1.25 if bold else 1)
    return y


def rect(x0, y0, w, h, fill=None, stroke=INK, width=0.3, dash=None):
    c.setLineWidth(width)
    c.setStrokeColorRGB(*stroke)
    c.setDash(*dash) if dash else c.setDash()
    if fill:
        c.setFillColorRGB(*fill)
    c.rect(x0 * mm, y0 * mm, w * mm, h * mm, stroke=1, fill=1 if fill else 0)
    c.setDash()


def line(x0, y0, x1, y1, width=0.25, colour=INK, dash=None):
    c.setLineWidth(width)
    c.setStrokeColorRGB(*colour)
    c.setDash(*dash) if dash else c.setDash()
    c.line(x0 * mm, y0 * mm, x1 * mm, y1 * mm)
    c.setDash()


def circle(x, y, d, width=0.3, fill=None, colour=INK):
    c.setLineWidth(width)
    c.setStrokeColorRGB(*colour)
    if fill:
        c.setFillColorRGB(*fill)
    c.circle(x * mm, y * mm, d / 2 * mm, stroke=1, fill=1 if fill else 0)


def target(x, y, d, arm=4.0):
    """1:1 drill mark: hole circle plus a cross that runs past it."""
    circle(x, y, d, 0.25)
    r = d / 2 + arm
    line(x - r, y, x + r, y, 0.15)
    line(x, y - r, x, y + r, 0.15)
    circle(x, y, 0.6, 0.15, fill=INK)


def scale_bar(x, y):
    line(x, y, x + 100, y, 0.5)
    for i in range(11):
        line(x + 10 * i, y, x + 10 * i, y + (3 if i % 5 == 0 else 1.6), 0.3)
    txt(x + 50, y + 4.5, "100 mm: measure this before you use the templates (print at 100 %, not 'fit to page')", 7, anchor="centre")


def header(title, sub=None, w=297):
    txt(12, 210 - 12 if w == 297 else 297 - 12, title, 14, True)
    if sub:
        txt(12, (210 if w == 297 else 297) - 18, sub, 8.5, colour=(0.3, 0.3, 0.3))


# ============================================================================================
# Page 1: overview, cut list, shopping list
# ============================================================================================
header("v0.7 wood rear: overview", "Seen from the wall side: display face down on the table, top edge away from you. Numbers in mm.")
S = 0.4  # overview scale
ox, oy = 18 + OUT * S, 22 + OUT * S  # centre of the drawing on the page


def P(x, y):
    return ox + x * S, oy + y * S


x0, y0 = P(-OUT, -OUT)
rect(x0, y0, FULL * S, FULL * S, stroke=GREY, width=0.3)
for name, (bx0, bx1, by0, by1) in v07.strip_boxes().items():
    px, py = P(bx0, by0)
    rect(px, py, (bx1 - bx0) * S, (by1 - by0) * S, fill=WOOD, width=0.4)
for _, e, h in v07.halo_piece_placements():
    a0, a1 = sorted((h * v07.HALO_SPAN[0], h * v07.HALO_SPAN[1]))
    w = v07.HALO_BAND_W / 2
    if e in ("top", "bottom"):
        yc = v07.HALO_C * (1 if e == "top" else -1)
        px, py = P(a0, yc - w)
        rect(px, py, (a1 - a0) * S, 2 * w * S, stroke=(0.1, 0.3, 0.8), width=0.3)
    else:
        xc = v07.HALO_C * (1 if e == "right" else -1)
        px, py = P(xc - w, a0)
        rect(px, py, 2 * w * S, (a1 - a0) * S, stroke=(0.1, 0.3, 0.8), width=0.3)
for x, y, what in v07.strip_screws():
    circle(*P(x, y), CB_D * S, 0.25, fill=(1, 1, 1))
for x, y, what, d in v07.hdf_holes():
    circle(*P(x, y), max(d, 3) * S, 0.2, colour=(0.3, 0.3, 0.3))
for sx in (-1, 1):
    a, b = sorted((sx * (v07.PITCH / 2 + v07.RAIL_X_IN), sx * (v07.PITCH / 2 + v07.RAIL_X_OUT)))
    px, py = P(a, v07.PITCH / 2 + v07.RAIL_LIP_Y0)
    rect(px, py, (b - a) * S, (v07.RAIL_WEB_Y[1] - v07.RAIL_LIP_Y0) * S, stroke=(0.7, 0.1, 0.1), width=0.4)
    circle(*P(sx * v07.WALL_SCREW["spacing"] / 2, v07.PITCH / 2 + v07.RAIL_LIP_Y0), 10 * S, 0.4, colour=(0.7, 0.1, 0.1))
    bx, by = sx * (v07.PITCH / 2 + v07.BUMPER["x"]), -v07.PITCH / 2 + v07.BUMPER["y"]
    circle(*P(bx, by), v07.BUMPER["d"] * S, 0.4, colour=(0.7, 0.1, 0.1))
px, py = P(v07.DC_SOCKET_X, -OUT)
rect(px - 7 * S, py - 3, 14 * S, 3, fill=(0.2, 0.2, 0.2))
txt(px + 6, py - 2.5, "DC socket", 7)
txt(*P(0, OUT + 6), "TOP", 8, True, anchor="centre")
txt(*P(0, INN - 10), "strip-top 390", 7, anchor="centre")
txt(*P(0, -INN + 5), "strip-bottom 390", 7, anchor="centre")
c.saveState()
for xs, label in ((-OUT + 6, "strip-left 352"), (INN + 13, "strip-right 352")):
    px, py = P(xs, 0)
    c.translate(px * mm, py * mm)
    c.rotate(90)
    txt(0, 0, label, 7, anchor="centre")
    c.rotate(-90)
    c.translate(-px * mm, -py * mm)
c.restoreState()
txt(*P(-96, -60), "power modules", 7, anchor="centre", colour=GREY)
txt(*P(74, -80), "Pi", 7, anchor="centre", colour=GREY)
txt(*P(-80, 128), "hanging rail", 7, anchor="centre", colour=(0.7, 0.1, 0.1))
txt(*P(80, 128), "hanging rail", 7, anchor="centre", colour=(0.7, 0.1, 0.1))
lx = 18
txt(lx, 8, "Brown: pine strips. Blue: printed halo channel pieces (on the HDF). Red: rails, wall screws, bumpers.", 7)
txt(lx, 4, "White circles: M3 x 40 holes through the strips (counterbore on the back face). Grey: holes in the HDF.", 7)

rows = [
    "## Cut list",
    f"Pine strip 19 x 30 mm: 2 x {fmt(FULL)} (top, bottom), 2 x {fmt(SIDE_LEN)} (sides)",
    f"   = {fmt(2 * FULL + 2 * SIDE_LEN)} mm + saw cuts: one 2.4 m strip is enough.",
    "   The 30 mm side is the depth (toward the wall); 19 mm is in the panel plane.",
    "   Sides = 390 - 2 x the real strip thickness. Measure it; a 15-20 mm strip works.",
    f"HDF 3 mm: 1 x {fmt(FULL)} x {fmt(FULL)}. Let the store cut it (Zuschnitt).",
    "",
    "## Shopping (new for the wood version)",
    "1 x pine strip 30 x 19 mm, 2.4 m (Bauhaus 'Rechteckleiste' 240 x 3 x 1.9 cm)",
    "1 x HDF 3 mm cut to 390 x 390 (Bauhaus Hartfaserplatte nach Mass;",
    "   the black one saves painting it)",
    "22 x M3 x 40 machine screws (+2 spare), any head that fits a 10 mm hole",
    "22 x M3 washers DIN 9021 (3.2 x 9 mm) (+2 spare)",
    "28 x wood screws 3 x 20, countersunk (Senkkopf) (+spare)",
    "Black matte paint for the strips (spray or acrylic), wood glue (optional)",
    "",
    "## Screws for the whole rear (replaces the tray plan in #172)",
    "M3 x 20: 16 (carriers to panel inserts, unchanged)",
    "M3 x 12: 26 (joiners 16, rails 6, bumpers 2, inner plate posts 2)",
    "M3 x 40: 22 (strips to carriers 16, outer plate posts 6)",
    "M2.5 x 10: 6 (Pi 4, DC socket 2)",
    "Wood 3 x 20 countersunk: 28 (halo pieces 24, HDF corners 4)",
    "",
    "## Printed parts that stay",
    "4 carriers, 8 joiners, 2 electronics plates, 2 rails, 2 bumpers,",
    "8 halo channel pieces (new). About 528 cm3 instead of 900 cm3.",
]
lines(190, 186, rows, 8.5, 4.3)
scale_bar(190, 20)

# ============================================================================================
# Page 2: order of work, tools
# ============================================================================================
page()
header("Order of work and tools")
rows = [
    "## Tools",
    "Fine saw + mitre box (Gehrungslade) for square cuts, drill (a drill stand helps a lot), try square,",
    "steel ruler or tape, pencil, awl or nail to prick the marks, clamps, masking tape as depth flag.",
    "Bits: 2, 3, 3.5, 5, 8 mm, a 10 mm brad-point (Holzspiralbohrer mit Zentrierspitze), 14 mm Forstner",
    "or spade bit, countersink. Sandpaper 120.",
    "",
    "## 1  Cut the strips",
    f"Cut 2 x {fmt(FULL)} and 2 x {fmt(SIDE_LEN)} mm, ends square. Check with the try square; sand the ends.",
    "Write on each piece which one it is, which face is the BACK (the face the HDF touches) and which long",
    "edge is OUTER. On the side strips also mark the BOTTOM end; on the bottom strip the POWER end.",
    "",
    "## 2  Drill the strips (page 3 table, templates on pages 4-5)",
    "Tape the template onto the BACK face, outer edge of the template on the outer edge of the wood, end on",
    "end. Prick every cross. Then, for every M3 hole:",
    f"   a) 10 mm counterbore, {fmt(CB_DEPTH)} mm deep, on the back face (drill this first: the brad point centres",
    "      itself on the mark; tape a depth flag on the bit).",
    "   b) 5 mm straight through the 30 mm. Keep the drill square; with a drill stand this is easy.",
    "Bottom strip only, on its OUTER face (template on page 5): 14 mm hole through for the DC socket, then",
    "two 2 mm pilots 10 mm deep for the socket screws. Solder the socket wires before you mount it.",
    "",
    "## 3  Test fit before painting",
    "Lay the four panels face down with carriers on, joiners screwed. Stand the strips on the carriers'",
    "outer bands: top and bottom over the full width, the sides between them, outer faces 3 mm outside the",
    "panel edges. Push an M3 x 40 through each hole: it must find the pilot hole below. If one misses, open",
    "that hole to 6 mm (the 9 mm washer still covers it).",
    "",
    "## 4  Paint",
    "Strips matte black (outer faces and ends matter most). HDF too, unless you bought black HDF.",
    "",
    "## 5  Drill the HDF (page 6 table, templates on pages 7-12)",
    "Tape the six tiles together on the dashed lines, check the 390 mm outline against the board, prick",
    "the marks, drill. Mark the side you drilled from as the WALL SIDE (outside); clamp a scrap board",
    "under the HDF so the holes do not splinter.",
    "",
    "## 6  Assemble",
    "Electronics plates and carriers as before. Strips on with M3 x 40 + washer (corner joints may be glued).",
    "Rails and bumpers onto the HDF: M3 x 12 from the inside face. Lay the HDF on, outer edges flush with",
    "the strips. Corners: 2 mm pilot through the corner holes into the strip, 3 x 20 wood screw.",
    "Halo pieces on the HDF over the strips (centred on the strip, butting at each edge's middle): drill",
    "3 mm through each piece hole and the HDF, then a 2 mm pilot 15 mm deep into the strip; 3 x 20 screws.",
    "Slide the halo strip in from the corner ends; the cable enters through the 8 mm hole bottom right.",
]
lines(14, 186, rows, 8.5, 4.05)

# ============================================================================================
# Page 3: strip drilling tables
# ============================================================================================
page()
header("Strips: drilling tables", "Template frame: back face up, outer long edge at the top, distances from the page-left end. Through holes 5 mm, counterbore 10 mm x 3 mm on the back face.")
data = strip_holes()
y = 180
names = {"top": "strip-top", "bottom": "strip-bottom", "left": "strip-left (power side)", "right": "strip-right (Pi side)"}
for key in ("top", "bottom", "left", "right"):
    length, lend, rend, holes = data[key]
    txt(14, y, f"{names[key]}  ({fmt(length)} mm long; page-left = {lend}, page-right = {rend})", 9.5, True)
    y -= 5
    extra = ""
    if key == "right":
        extra = "   (= " + ", ".join(fmt(length - a) for a, _, _ in reversed(holes)) + " from the bottom end, as on strip-left)"
    txt(18, y, "from page-left end:  " + ",  ".join(fmt(a) for a, _, _ in holes) + extra, 9)
    y -= 4.5
    txt(18, y, f"all {fmt(holes[0][1])} mm in from the outer edge.  " + "  ".join(
        f"{fmt(a)}: {w}" for a, _, w in holes if w != "carrier") or "", 8, colour=(0.3, 0.3, 0.3))
    # Sketch at 0.6 scale.
    y -= 13
    sk = 0.6
    rect(18, y, length * sk, T * sk + 0.01, fill=WOOD)
    for a, inset, _ in holes:
        circle(18 + a * sk, y + (T - inset) * sk, CB_D * sk, 0.25, fill=(1, 1, 1))
        circle(18 + a * sk, y + (T - inset) * sk, v07.STRIP_HOLE * sk, 0.2)
    txt(18 + length * sk + 3, y + 3, "outer edge on top", 7, colour=GREY)
    y -= 12
if socket_holes():
    txt(14, y, "strip-bottom, OUTER face (seen from below; page-left = power end): DC socket", 9.5, True)
    y -= 5
    for a, e, d, w in socket_holes():
        txt(18, y, f"{fmt(a)} mm from the power end ({fmt(FULL - a)} from the Pi end), {fmt(e)} mm from either long edge: {w}", 9)
        y -= 4.5
    txt(18, y, "Socket flange outside, M2.5 x 10 through the flange into the pilots. Solder the wires before mounting.", 8, colour=(0.3, 0.3, 0.3))
    y -= 4.5
    txt(18, y, "Body behind the flange about 20 mm, the wood is 19 mm: if the solder cups sit inside the hole, widen it from", 8, colour=(0.3, 0.3, 0.3))
    y -= 4
    txt(18, y, "the inside to 20 mm, 8 mm deep.", 8, colour=(0.3, 0.3, 0.3))


# ============================================================================================
# Pages 4-5: strip templates 1:1
# ============================================================================================
SPLIT, OVER = 200.0, 10.0


def band(y, length, holes, label, lend, rend, width, part, socket=False):
    """Draw one strip face 1:1; part 0 shows 0..SPLIT, part 1 shows SPLIT-OVER..length."""
    a0, a1 = (0.0, SPLIT) if part == 0 else (SPLIT - OVER, length)
    x_left = 12.0 - a0  # page x of along = 0
    c.saveState()
    p = c.beginPath()
    p.rect(12 * mm, (y - 1) * mm, (a1 - a0) * mm, (width + 2) * mm)
    c.clipPath(p, stroke=0)
    rect(x_left, y, length, width, fill=WOOD, width=0.4)
    for a, inset, d in holes:
        target(x_left + a, y + width - inset, d)
        if d == v07.STRIP_HOLE:
            circle(x_left + a, y + width - inset, CB_D, 0.15, colour=GREY)
    c.restoreState()
    if part == 0:
        line(12, y - 3, 12, y + width + 3, 0.6)
        txt(13, y + width + 2.5, f"{lend} (cut end)", 6.5, colour=GREY)
        line(12 + SPLIT, y - 3, 12 + SPLIT, y + width + 3, 0.3, dash=(2, 2))
        txt(12 + SPLIT - 1, y - 4.5, f"overlap: align with part 2 at {fmt(SPLIT - OVER)}-{fmt(SPLIT)}", 6.5, anchor="right", colour=GREY)
    else:
        line(12, y - 3, 12, y + width + 3, 0.3, dash=(2, 2))
        line(12 + OVER, y - 3, 12 + OVER, y + width + 3, 0.3, dash=(2, 2))
        line(x_left + length, y - 3, x_left + length, y + width + 3, 0.6)
        txt(x_left + length - 1, y + width + 2.5, f"{rend} (cut end)", 6.5, anchor="right", colour=GREY)
    for a, inset, d in holes:
        if a0 <= a <= a1:
            txt(x_left + a, y - 4.5, fmt(a), 6.5, anchor="centre")
    txt(12, y + width + 6, f"{label}  (part {part + 1} of 2)", 8.5, True)
    if not socket:
        txt(12 + (a1 - a0) + 3, y + width - 2, "outer edge", 6.5, colour=GREY)


for part in (0, 1):
    page()
    header(f"Strip templates, 1:1, part {part + 1} of 2",
           "Cut along the band edges, tape onto the wood (end on the cut end, top line on the outer edge), prick every centre.")
    y = 158
    for key in ("top", "bottom", "left", "right"):
        length, lend, rend, holes = data[key]
        hs = [(a, inset, v07.STRIP_HOLE) for a, inset, _ in holes]
        band(y, length, hs, f"{names[key]}, BACK face", lend, rend, T, part)
        y -= T + (21 if part == 0 else 14)
    if part == 1:  # the socket sits at 213 mm: only this half is needed
        sh = [(a, e, d) for a, e, d, _ in socket_holes()]
        band(y - 14, FULL, sh, "strip-bottom, OUTER face (DC socket)", "power end", "Pi end", v07.STRIP_D, part, socket=True)
    scale_bar(185, 186)


# ============================================================================================
# Page 6: HDF drawing and table
# ============================================================================================
page()
header("HDF back, 390 x 390 x 3 mm", "Seen from the wall side. X from the LEFT edge, Y from the BOTTOM edge. Halo-piece screw holes are drilled on assembly through the pieces.")
S = 0.42
bx, by = 14, 22
rect(bx, by, FULL * S, FULL * S, fill=(0.96, 0.96, 0.96), width=0.4)
for X, Y, d, what in hdf_holes():
    circle(bx + X * S, by + Y * S, max(d, 3) * S, 0.3)
for x, y in v07.halo_screws():
    circle(bx + (x + OUT) * S, by + (y + OUT) * S, 1.2, 0.15, colour=(0.1, 0.3, 0.8))
txt(bx + FULL * S / 2, by + FULL * S + 2, "TOP", 8, True, anchor="centre")
txt(bx, by - 5, "origin: bottom-left corner (wall side)", 7, colour=GREY)
txt(bx + FULL * S, by - 5, "blue dots: halo-piece screws (drill on assembly)", 7, anchor="right", colour=(0.1, 0.3, 0.8))

groups = {}
for X, Y, d, what in hdf_holes():
    groups.setdefault((what, d), []).append((X, Y))
rows = ["## Holes (X, Y)"]
for (what, d), pts in groups.items():
    if what == "vent":
        continue
    rows.append(f"{what}, Ø{fmt(d)}{' countersunk' if what == 'corner' else ''}: " + "  ".join(f"({fmt(X)}, {fmt(Y)})" for X, Y in pts))
vents = sorted(groups[("vent", v07.VENT_D)], key=lambda p: (-p[1], p[0]))
rows += ["", f"## Vents Ø{fmt(v07.VENT_D)} ({len(vents)})"]
by_row = {}
for X, Y in vents:
    by_row.setdefault(Y, []).append(X)
for Y in sorted(by_row, reverse=True):
    rows.append(f"Y {fmt(Y)}:  X " + ", ".join(fmt(X) for X in sorted(by_row[Y])))
rows += [
    "",
    "## Notes",
    "Rails and bumpers: M3 x 12 from the inside face into the printed part.",
    "Corner holes: countersink on the wall side, 3 x 20 wood screw into the strip.",
    "Halo cable hole (bottom right): the Pro Micro's cable to the halo strip.",
    "Vents sit over the two power modules (bottom left), the Pi (bottom right)",
    "and along the top. Drill onto a scrap board so the HDF does not splinter.",
]
y = 186
for r in rows:
    if r.startswith("## "):
        txt(186, y, r[3:], 9.5, True)
        y -= 5.5
    else:
        # wrap long rows
        words, cur = r.split("  "), ""
        for wd in words:
            if cur and len(cur) + len(wd) > 62:
                txt(186, y, cur, 7.8)
                y -= 3.9
                cur = "   " + wd
            else:
                cur = (cur + "  " + wd) if cur else wd
        txt(186, y, cur, 7.8)
        y -= 3.9
scale_bar(186, 12)


# ============================================================================================
# Pages 7-12: HDF tiles 1:1 (landscape, 2 columns x 3 rows)
# ============================================================================================
OVERLAP = 10.0
TW, TH = (FULL + OVERLAP) / 2, (FULL + 2 * OVERLAP) / 3
COLS = [(i * (TW - OVERLAP), i * (TW - OVERLAP) + TW) for i in range(2)]
ROWS = [(i * (TH - OVERLAP), i * (TH - OVERLAP) + TH) for i in range(3)]
n = 0
for ri, (Y0, Y1) in enumerate(reversed(ROWS)):
    for ci, (X0, X1) in enumerate(COLS):
        n += 1
        page()
        header(f"HDF template tile {n} of 6, 1:1  (row {ri + 1} from the top, column {ci + 1} from the left)",
               "Wall side. Tape the tiles together so the dashed overlap lines and the hole marks line up.")
        px0, py0 = 14.0, 16.0
        c.saveState()
        p = c.beginPath()
        p.rect(px0 * mm, py0 * mm, (X1 - X0) * mm, (Y1 - Y0) * mm)
        c.clipPath(p, stroke=0)
        rect(px0 - X0, py0 - Y0, FULL, FULL, width=0.6)
        for X, Y, d, what in hdf_holes():
            target(px0 + X - X0, py0 + Y - Y0, d, arm=3)
            txt(px0 + X - X0 + d / 2 + 1, py0 + Y - Y0 + d / 2 + 0.6, f"{what} Ø{fmt(d)}", 5.5, colour=GREY)
        for x, y in v07.halo_screws():
            circle(px0 + x + OUT - X0, py0 + y + OUT - Y0, 1.0, 0.15, colour=(0.1, 0.3, 0.8))
        # 50 mm reference grid, light
        for g in range(0, int(FULL) + 1, 50):
            line(px0 + g - X0, py0 - Y0, px0 + g - X0, py0 - Y0 + FULL, 0.1, (0.8, 0.8, 0.8))
            line(px0 - X0, py0 + g - Y0, px0 - X0 + FULL, py0 + g - Y0, 0.1, (0.8, 0.8, 0.8))
            txt(px0 + g - X0 + 0.8, py0 + 1, f"X{g}", 5.5, colour=(0.6, 0.6, 0.6))
            txt(px0 + 0.8, py0 + g - Y0 + 0.8, f"Y{g}", 5.5, colour=(0.6, 0.6, 0.6))
        c.restoreState()
        rect(px0, py0, X1 - X0, Y1 - Y0, stroke=GREY, width=0.2)
        # Overlap lines with the neighbours.
        for cj, (A0, A1) in enumerate(COLS):
            if cj != ci:
                for v in (A0, A1):
                    if X0 < v < X1:
                        line(px0 + v - X0, py0, px0 + v - X0, py0 + Y1 - Y0, 0.3, dash=(2, 2))
        for rj, (B0, B1) in enumerate(ROWS):
            for v in (B0, B1):
                if Y0 < v < Y1:
                    line(px0, py0 + v - Y0, px0 + X1 - X0, py0 + v - Y0, 0.3, dash=(2, 2))
        scale_bar(120, 6)
        # locator
        lx, ly, ls = 262.0, 150.0, 0.07
        rect(lx, ly, FULL * ls, FULL * ls, width=0.3)
        rect(lx + X0 * ls, ly + Y0 * ls, (X1 - X0) * ls, (Y1 - Y0) * ls, fill=(0.6, 0.6, 0.6), width=0.2)

c.save()
print(args.output)
