"""Provisional measured eight-hole check, printed at 1:1 on A4; no edge datum."""

from decimal import Decimal, ROUND_HALF_UP
import json
import os
from pathlib import Path

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.pdfgen import canvas

from mounting_measurements import measured_pattern

root = Path(__file__).resolve().parent
out = Path(os.environ.get("OAK_PANEL_OUTPUT", root / "exports"))
out.mkdir(parents=True, exist_ok=True)
pattern = measured_pattern()
(out / "mounting-pattern-derived.json").write_text(json.dumps(pattern, indent=2) + "\n")
pdf = out / "single-panel-mounting-check.pdf"
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
    "Print Actual size / 100%. Disable Fit/Shrink. Verify BOTH 100 mm bars first.",
)
text(
    12,
    264,
    "Owner sketch orientation; connector orientation and panel edge offsets unconfirmed.",
    8,
)

x0, y0 = 18, 87
width, height = pattern["width_mm"], pattern["height_mm"]
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

c.setLineWidth(0.8)
line(18, 56, 118, 56)
for x in [18, 118]:
    line(x, 53, x, 59)
text(68, 62, "100 mm HORIZONTAL", 9, True, centre=True)
line(198, 94, 198, 194)
for y in [94, 194]:
    line(195, y, 201, y)
c.saveState()
c.translate(204 * mm, 144 * mm)
c.rotate(90)
c.setFont("Helvetica-Bold", 8)
c.drawCentredString(0, 0, "100 mm VERTICAL")
c.restoreState()
text(
    12,
    44,
    "1. Both bars must measure 100 mm; a 96 mm bar means the print is still scaled.",
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
    "Reference circles are centre markers, not drilling sizes. No screw length specified.",
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
