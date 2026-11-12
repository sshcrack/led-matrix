"""One nominal panel's drawing-based hole positions, printed at 1:1 on A4."""

from pathlib import Path
import os
from reportlab.pdfgen import canvas
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from enclosure import Parameters

p = Parameters()
root = Path(__file__).resolve().parent
out = (
    Path(os.environ.get("OAK_PANEL_OUTPUT", root / "exports"))
    / "single-panel-mounting-check.pdf"
)
c = canvas.Canvas(str(out), pagesize=A4)
c.setTitle("P3 panel mounting-pattern check - v0.5")
c.setFont("Helvetica-Bold", 12)
c.drawString(9 * mm, 284 * mm, "P3 panel mounting-pattern check")
c.setFont("Helvetica", 9)
c.drawString(
    9 * mm,
    277 * mm,
    "Print at 100% / Actual size. Disable Fit to page. Check scale bar first.",
)
c.drawString(
    9 * mm,
    271 * mm,
    "Drawing-based assumption adopted for this design; verify before drilling.",
)
x0 = 9 * mm
y0 = 65 * mm
c.setLineWidth(0.5)
c.rect(x0, y0, p.panel_side * mm, p.panel_side * mm)
c.setDash(3, 3)
c.line(
    x0 + p.panel_side / 2 * mm, y0, x0 + p.panel_side / 2 * mm, y0 + p.panel_side * mm
)
c.line(
    x0, y0 + p.panel_side / 2 * mm, x0 + p.panel_side * mm, y0 + p.panel_side / 2 * mm
)
c.setDash()
for dx in [-p.mounting_half_width, p.mounting_half_width]:
    for dy in [-p.mounting_row_pitch, 0, p.mounting_row_pitch]:
        x = p.panel_side / 2 + dx
        y = p.panel_side / 2 + dy
        xx = x0 + x * mm
        yy = y0 + y * mm
        c.circle(xx, yy, p.mounting_clearance_diameter / 2 * mm)
        c.line(xx - 3 * mm, yy, xx + 3 * mm, yy)
        c.line(xx, yy - 3 * mm, xx, yy + 3 * mm)
        c.setFont("Helvetica", 8)
        c.drawString(
            xx + 4 * mm if dx < 0 else xx - 24 * mm, yy + 4 * mm, f"({x:g}, {y:g}) mm"
        )
c.setFont("Helvetica", 9)
c.drawString(
    9 * mm,
    57 * mm,
    "Rear view. Coordinates from bottom-left of nominal 192 x 192 mm outline.",
)
c.drawString(
    9 * mm, 51 * mm, "Pattern: x = +/-85; y = -73, 0, +73 mm relative to panel centre."
)
c.drawString(
    9 * mm,
    45 * mm,
    "M3 candidate, 3.5 mm chassis clearance holes. No screw length specified.",
)
c.drawString(
    9 * mm,
    39 * mm,
    "Supplier drawing frame is 191.70 mm square; align by centre, not one edge.",
)
c.setLineWidth(1)
c.line(25 * mm, 22 * mm, 125 * mm, 22 * mm)
for x in [25, 125]:
    c.line(x * mm, 19 * mm, x * mm, 25 * mm)
c.setFont("Helvetica-Bold", 9)
c.drawCentredString(75 * mm, 28 * mm, "100 mm calibration bar")
c.setFont("Helvetica", 8)
c.drawString(140 * mm, 21 * mm, "NOT A FABRICATION RELEASE")
c.showPage()
c.save()
print(out)
