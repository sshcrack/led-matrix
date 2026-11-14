#!/usr/bin/env python3
"""Create an uncorrected calibration PDF and a separately compensated fit pack."""

import argparse
import json
import math
from pathlib import Path

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.pdfgen import canvas

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=ROOT / "exports")
parser.add_argument(
    "--printed-x-mm",
    type=float,
    default=95,
    help="Physical horizontal length of the uncorrected 100 mm calibration bar",
)
parser.add_argument(
    "--printed-y-mm",
    type=float,
    default=95,
    help="Physical vertical length of the uncorrected 100 mm calibration bar",
)
args = parser.parse_args()
for value in [args.printed_x_mm, args.printed_y_mm]:
    if not math.isfinite(value) or value <= 0:
        parser.error("Calibration readings must be positive and finite")
sx, sy = 100 / args.printed_x_mm, 100 / args.printed_y_mm
basis = json.loads((ROOT / "measurement-basis.json").read_text())
raw = json.loads((ROOT / "mounting-measurements.json").read_text())
wave = basis["official_mechanical_drawing_candidate"]
owner = raw["corner_approximate_report"]["derived_centre_insets_mm"]
# These are corner patterns only. No intermediate-insert positions are inferred.
candidates = [
    {
        "id": "A",
        "title": "Waveshare official DWG",
        "status": "Strongest candidate; exact panel revision and view orientation unconfirmed.",
        "corners": wave["drawing_centred_corner_xy_mm"],
        "source": wave["source"],
        "source_label": "Waveshare wiki 2D Drawing / archive dated 2024-01-11",
        "basis": "Dimensioned corner offsets +/-87 X, +/-79 Y; drawing body 191.6 mm square.",
    },
    {
        "id": "B",
        "title": "Mightary / Furi M3 module",
        "status": "Different OEM frame; comparison only, not claimed to match your panel.",
        "corners": [[-83, -86], [-83, 86], [83, -86], [83, 86]],
        "source": "https://www.ledmary.com/Uploads/file/20250710/20250710154823_63775.pdf",
        "source_label": "M3-192X192-2121-32S, A1.0, page 4 (2024-06-20)",
        "basis": "Corner spans from 83 + 83 horizontal and 86 + 86 vertical dimensions.",
    },
    {
        "id": "C",
        "title": "Your approximate corner measurements",
        "status": "Owner estimate, not a supplier drawing; edge contacts confirmed, accuracy limited.",
        "corners": [
            [x * (96 - owner["x"]), y * (96 - owner["y"])]
            for x in [-1, 1]
            for y in [-1, 1]
        ],
        "source": "https://github.com/sshcrack/led-matrix/issues/170",
        "source_label": "Owner report / mounting-measurements.json / 2026-11-14",
        "basis": "Centre insets 8.4 / 16.0 mm in a nominal 192 mm square; equal opposite insets assumed.",
    },
]
for candidate in candidates:
    xs, ys = zip(*candidate["corners"])
    candidate["span_mm"] = [max(xs) - min(xs), max(ys) - min(ys)]
    if (
        candidate["span_mm"][0] * sx + 6 * sx > 194
        or candidate["span_mm"][1] * sy + 6 * sy > 190
    ):
        parser.error(
            "Compensated pattern exceeds this A4 layout. Change print route or paper size."
        )

out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)


def text(c, x, y, value, size=9, bold=False, centre=False):
    c.setFont("Helvetica-Bold" if bold else "Helvetica", size)
    method = c.drawCentredString if centre else c.drawString
    method(x * mm, y * mm, value)


def line(c, x1, y1, x2, y2):
    c.line(x1 * mm, y1 * mm, x2 * mm, y2 * mm)


def horizontal_bar(c, x, y, length, label):
    c.setLineWidth(0.7)
    line(c, x, y, x + length, y)
    for xx in [x, x + length]:
        line(c, xx, y - 3, xx, y + 3)
    text(c, x + length / 2, y + 7, label, 9, True, True)


def vertical_bar(c, x, y, length, label):
    c.setLineWidth(0.7)
    line(c, x, y, x, y + length)
    for yy in [y, y + length]:
        line(c, x - 3, yy, x + 3, yy)
    c.saveState()
    c.translate((x + 7) * mm, (y + length / 2) * mm)
    c.rotate(90)
    c.setFont("Helvetica-Bold", 9)
    c.drawCentredString(0, 0, label)
    c.restoreState()


calibration = out / "panel-print-calibration.pdf"
c = canvas.Canvas(str(calibration), pagesize=A4)
c.setTitle("Panel print calibration - UNCORRECTED 100 mm bars")
text(c, 16, 280, "PRINT THIS CALIBRATION SHEET FIRST", 15, True)
text(
    c,
    16,
    270,
    "A4 / one page per sheet / same printer, app and settings as the comparison pack.",
)
text(
    c,
    16,
    263,
    "This page is UNCORRECTED: both bars are exactly 100 mm in the PDF.",
    10,
    True,
)
text(
    c,
    16,
    253,
    "If possible, ask the copy shop for Actual size / 100%, with Fit/Shrink disabled.",
)
text(
    c,
    16,
    246,
    "If scaling cannot be selected, leave it as it is and report the two physical readings.",
)
text(
    c,
    16,
    239,
    "Measure between the centres of the end ticks. Do not adjust this PDF yourself.",
)
horizontal_bar(c, 40, 75, 100, "X / HORIZONTAL / 100 mm in PDF")
vertical_bar(c, 40, 75, 100, "Y / VERTICAL / 100 mm in PDF")
text(c, 105, 205, "No mounting-hole pattern on this sheet", 10, False, True)
text(c, 95, 152, "Send these two readings:", 11, True)
text(c, 95, 139, "X printed length: __________ mm", 10)
text(c, 95, 126, "Y printed length: __________ mm", 10)
text(
    c,
    16,
    52,
    "Correction for the other PDF: X scale = 100 / your X reading; Y scale = 100 / your Y reading.",
)
text(
    c,
    16,
    44,
    "Example: 95 mm in both directions requires 105.263% enlargement of the fit patterns.",
)
text(
    c,
    16,
    31,
    "Calibration bars remain uncorrected even when the comparison pack is regenerated.",
)
text(
    c,
    16,
    23,
    "A change of printer, print app, paper size or settings requires another calibration check.",
)
c.showPage()
c.save()

pack = out / "panel-mounting-source-comparison.pdf"
c = canvas.Canvas(str(pack), pagesize=A4)
c.setTitle("Four-corner mounting comparisons - printer-compensated candidates")
for page, candidate in enumerate(candidates, 1):
    span_x, span_y = candidate["span_mm"]
    text(c, 12, 285, f"{candidate['id']} / {candidate['title']}", 13, True)
    text(c, 12, 278, candidate["status"], 8)
    text(
        c,
        12,
        271,
        f"Intended PHYSICAL corner spans: {span_x:.2f} x {span_y:.2f} mm.",
        10,
        True,
    )
    text(
        c,
        12,
        264,
        f"Printer correction: X {sx * 100:.3f}% / Y {sy * 100:.3f}% (original 100 mm bars printed {args.printed_x_mm:g} / {args.printed_y_mm:g} mm).",
        8,
    )
    text(c, 12, 257, "Source: " + candidate["source_label"], 8)
    c.linkURL(candidate["source"], (12 * mm, 255 * mm, 198 * mm, 261 * mm), relative=0)
    # Scale geometry only. Source/physical dimensions in text remain the intended values.
    c.saveState()
    c.translate(105 * mm, 158 * mm)
    c.scale(sx, sy)
    c.setLineWidth(0.3)
    c.setDash(2, 3)
    c.rect(-span_x / 2 * mm, -span_y / 2 * mm, span_x * mm, span_y * mm)
    c.setDash()
    for x, y in candidate["corners"]:
        c.setLineWidth(0.6)
        c.circle(x * mm, y * mm, 1.75 * mm)
        line(c, x - 3, y, x + 3, y)
        line(c, x, y - 3, x, y + 3)
    c.restoreState()
    for x, y in candidate["corners"]:
        xx, yy = 105 + x * sx, 158 + y * sy
        label = ("T" if y > 0 else "B") + ("L" if x < 0 else "R")
        text(c, xx + (5 if x < 0 else -10), yy + (-7 if y > 0 else 5), label, 8)
    text(c, 105, 169, "FOUR CORNER INSERTS ONLY", 10, True, True)
    text(c, 105, 161, "Intermediate holes are deliberately omitted.", 8, False, True)
    text(c, 105, 153, "Dashed rectangle joins hole centres.", 8, False, True)
    text(c, 105, 145, "It is NOT the panel outline.", 8, False, True)
    vertical_bar(c, 177, 103, 100 * sy, "Y / MUST PRINT 100 mm")
    horizontal_bar(c, 30, 35, 100 * sx, "X / MUST PRINT 100 mm")
    text(
        c,
        12,
        57,
        "Both scale bars MUST measure 100 mm on paper before judging hole alignment.",
        8,
        True,
    )
    text(
        c,
        12,
        51,
        "Rotate to find the matching axis; mark HUB75 IN. Align centres, not paper edges.",
        8,
    )
    text(c, 12, 23, candidate["basis"], 7.5)
    text(
        c,
        12,
        17,
        "Diagnostic only. Marker circles are not drilling sizes; no screw/thread/length approval.",
        8,
    )
    text(
        c,
        12,
        11,
        f"Result: TL ___ TR ___ BL ___ BR ___   IN edge: __________   Page {page}/{len(candidates)}",
        8,
    )
    c.showPage()
c.save()
(out / "comparison-print-basis.json").write_text(
    json.dumps(
        {
            "calibration_pdf": "Uncorrected 100 mm bars, always; report actual printed X and Y.",
            "assumed_original_printed_bars_mm": [args.printed_x_mm, args.printed_y_mm],
            "comparison_geometry_scale": [sx, sy],
            "comparison_page_mapping": candidates,
            "status": "Alternative diagnostic patterns only; no geometry adopted into the enclosure.",
        },
        indent=2,
    )
    + "\n"
)
print(calibration)
print(pack)
