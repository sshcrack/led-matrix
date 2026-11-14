"""Provisional measured eight-hole check, printed at 1:1 on A4; no edge datum."""

from decimal import Decimal, ROUND_HALF_UP
import argparse
import json
import math
import os
from pathlib import Path

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.pdfgen import canvas

from mounting_measurements import measured_pattern

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--printed-bar-mm",
    type=float,
    help="Measured print length of an original 100 mm bar; assumes uniform scaling",
)
args = parser.parse_args()
scale = 1.0
if args.printed_bar_mm is not None:
    if not math.isfinite(args.printed_bar_mm) or args.printed_bar_mm <= 0:
        parser.error("Printed bar length must be positive and finite")
    scale = 100 / args.printed_bar_mm
out = Path(os.environ.get("OAK_PANEL_OUTPUT", root / "exports"))
out.mkdir(parents=True, exist_ok=True)
pattern = measured_pattern()
(out / "mounting-pattern-derived.json").write_text(json.dumps(pattern, indent=2) + "\n")
pdf = out / "single-panel-mounting-check.pdf"
if args.printed_bar_mm is not None:
    pdf = out / f"single-panel-mounting-check-compensated-{args.printed_bar_mm:g}mm.pdf"
if pattern["width_mm"] * scale + 6 * scale > 194 or pattern["height_mm"] * scale > 170:
    parser.error(
        "Compensated pattern will not fit this A4 layout; use a different print route"
    )
c = canvas.Canvas(str(pdf), pagesize=A4)
c.setTitle("P3 measured eight-hole mounting check - measured-v1 - PROVISIONAL")


def text(x, y, value, size=9, bold=False, centre=False):
    c.setFont("Helvetica-Bold" if bold else "Helvetica", size)
    method = c.drawCentredString if centre else c.drawString
    method(x * mm, y * mm, value)


def dimension(value):
    return str(Decimal(str(value)).quantize(Decimal("0.01"), rounding=ROUND_HALF_UP))


def line(x1, y1, x2, y2):
    c.line(x1 * mm, y1 * mm, x2 * mm, y2 * mm)


text(12, 284, "P3 measured eight-hole check / measured-v1", 12, True)
text(
    12, 277, "PROVISIONAL - PAPER FIT CHECK ONLY - replaces six-hole template", 9, True
)
text(
    12,
    270,
    (
        f"COMPENSATED for a {args.printed_bar_mm:g} mm print: use the SAME printer/app/settings."
        if args.printed_bar_mm is not None
        else "Print Actual size / 100%. Disable Fit/Shrink. Verify BOTH 100 mm bars first."
    ),
)
text(
    12,
    264,
    "Owner sketch orientation; connector orientation and panel edge offsets unconfirmed.",
    8,
)

x0, y0 = 18, 87
width, height = pattern["width_mm"], pattern["height_mm"]
c.saveState()
c.translate((x0 + width / 2) * mm, (y0 + height / 2) * mm)
c.scale(scale, scale)
c.translate(-(x0 + width / 2) * mm, -(y0 + height / 2) * mm)
c.setLineWidth(0.4)
c.setDash(2, 3)
# These lines join centres. They do not represent the panel perimeter.
for a, b in [("0,0", "2,0"), ("0,0", "0,2"), ("0,2", "2,2"), ("2,0", "2,2")]:
    x1, y1 = pattern["hole_centres_mm"][a]
    x2, y2 = pattern["hole_centres_mm"][b]
    line(x0 + x1, y0 + y1, x0 + x2, y0 + y2)
c.setDash()
for name, (x, y) in pattern["hole_centres_mm"].items():
    x, y = x0 + x, y0 + y
    c.circle(x * mm, y * mm, 1.75 * mm)
    line(x - 3, y, x + 3, y)
    line(x, y - 3, x, y + 3)
    text(
        x - 16 if name.startswith("2") else x + 4,
        y - 7 if name.endswith("2") else y + 5,
        f"({name})",
        8,
    )

s = pattern["provisional_template_spans_mm"]
text(x0 + s["G"] / 2, y0 + height + 7, f"G = {dimension(s['G'])}", 9, centre=True)
text(
    x0 + s["G"] + s["H"] / 2,
    y0 + height + 7,
    f"H = D = {dimension(s['H'])}",
    9,
    centre=True,
)
text(x0 + s["D"] / 2, y0 - 9, f"D = {dimension(s['D'])}", 9, centre=True)
text(x0 + s["D"] + s["C"] / 2, y0 - 9, f"C = G = {dimension(s['C'])}", 9, centre=True)
text(x0 + 5, y0 + s["B"] / 2, f"E = B = {dimension(s['E'])}", 9)
text(x0 + 5, y0 + s["B"] + s["A"] / 2, f"F = A = {dimension(s['F'])}", 9)
text(x0 + width - 31, y0 + s["B"] / 2, f"B = {dimension(s['B'])}", 9)
text(x0 + width - 31, y0 + s["B"] + s["A"] / 2, f"A = {dimension(s['A'])}", 9)
text(105, 186, "NO CENTRE HOLE", 9, True, centre=True)
text(105, 177, "Dashed lines join hole centres, NOT panel edges.", 8, centre=True)
text(105, 168, "Align by holes, not the paper or panel edges.", 8, centre=True)
text(105, 152, f"Outer centre spans: {width:.2f} x {height:.2f} mm", 9, centre=True)
text(
    105, 143, "Decimals describe calculations, not certified accuracy.", 8, centre=True
)
c.restoreState()

c.setLineWidth(0.8)
line(18, 56, 18 + 100 * scale, 56)
for x in [18, 18 + 100 * scale]:
    line(x, 53, x, 59)
text(68, 62, "100 mm HORIZONTAL", 9, True, centre=True)
bar_x = 7 if scale > 1 else 198
line(bar_x, 144 - 50 * scale, bar_x, 144 + 50 * scale)
for y in [144 - 50 * scale, 144 + 50 * scale]:
    line(bar_x - 3, y, bar_x + 3, y)
c.saveState()
c.translate((12 if scale > 1 else 204) * mm, 144 * mm)
c.rotate(90)
c.setFont("Helvetica-Bold", 8)
c.drawCentredString(0, 0, "100 mm VERTICAL")
c.restoreState()
text(
    12,
    44,
    "1. BOTH bars must print at 100 mm. If either differs, do not judge hole alignment yet.",
    8,
)
text(
    12,
    38,
    "2. On the unplugged panel, align the eight crosses with the insert centres; do not force screws.",
    8,
)
text(
    12,
    32,
    "3. Record matches/misses by hole ID and mark the HUB75 IN edge on this sheet.",
    8,
)
text(
    12,
    26,
    "4. Measure edge-to-centre offsets before transferring this relative pattern to enclosure CAD.",
    8,
)
text(
    12,
    18,
    (
        f"PDF geometry enlarged to {scale * 100:.3f}%; requires uniform {args.printed_bar_mm:g}% print scaling. Recheck bars."
        if args.printed_bar_mm is not None
        else "Reference circles are centre markers, not drilling sizes. No screw length specified."
    ),
    8,
)
text(
    12,
    12,
    "The six-hole STL gauge and full carrier are ON HOLD pending correction and physical fit.",
    8,
    True,
)
c.showPage()
c.save()
print(pdf)
