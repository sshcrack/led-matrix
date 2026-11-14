"""Dimensioned schematic layout generated from the CAD parameters."""

from pathlib import Path
import os
from enclosure import Parameters

p = Parameters()
root = Path(__file__).resolve().parent
# SVG is a concept drawing in millimetres; on-screen scale is not a cutting template.
svg = [
    '<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="920" viewBox="0 0 1000 920">',
    '<rect width="1000" height="920" fill="#fff"/>',
    "<style>text{font-family:Arial,sans-serif;fill:#242424;font-size:15px}.small{font-size:12px}.dim{stroke:#444;fill:none;stroke-width:1}.dash{stroke:#666;stroke-dasharray:5 4;fill:none}</style>",
    '<defs><marker id="arrow" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M 10 0 L 0 5 L 10 10" fill="none" stroke="#444"/></marker></defs>',
]


def text(x, y, t, anchor="start", cls=""):
    svg.append(f'<text x="{x}" y="{y}" text-anchor="{anchor}" class="{cls}">{t}</text>')


def rect(x, y, w, h, fill, stroke="#555"):
    svg.append(
        f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" stroke="{stroke}"/>'
    )


def line(x1, y1, x2, y2, dim=False):
    extra = ' marker-start="url(#arrow)" marker-end="url(#arrow)"' if dim else ""
    svg.append(f'<path d="M{x1},{y1} L{x2},{y2}" class="dim"{extra}/>')


text(44, 36, "Black-bezel P3 panel — FDM concept v0.6")
text(
    44,
    59,
    "2026-11-14 · all dimensions mm · nominal module size; all packaging dimensions provisional",
    cls="small",
)
# Front elevation, 1 SVG unit per mm.
x = 70
y = 124
rect(x, y, p.outer, p.outer, "#202023")
rect(x + p.border, y + p.border, p.display, p.display, "#e1e1d9")
for a in (0, p.outer):
    line(x + a, y - 6, x + a, y - 31)
line(x, y - 22, x + p.outer, y - 22, True)
text(x + p.outer / 2, y - 30, f"{p.outer:g} overall", "middle")
line(x + p.border, y + p.outer + 8, x + p.border, y + p.outer + 34)
line(
    x + p.border + p.display,
    y + p.outer + 8,
    x + p.border + p.display,
    y + p.outer + 34,
)
line(x + p.border, y + p.outer + 25, x + p.border + p.display, y + p.outer + 25, True)
text(
    x + p.outer / 2,
    y + p.outer + 48,
    f"{p.display:g} aperture / module footprint",
    "middle",
)
svg.append(
    f'<path d="M{x + p.outer / 2},{y + p.border} V{y + p.outer - p.border} M{x + p.border},{y + p.outer / 2} H{x + p.outer - p.border}" class="dash"/>'
)
text(x + p.outer / 2, y + p.outer / 2 - 15, "128 × 128 pixels", "middle")
text(x + p.outer / 2, y + p.outer / 2 + 17, "4 × 64 × 64 P3 modules", "middle")
text(550, 150, "Front / enclosure envelope")
for i, t in enumerate(
    [
        f"{p.border:g} mm visible black bezel",
        f"{p.depth:g} mm frame depth",
        f"{p.wall_clearance:g} mm wall clearance reservation",
        f"{p.depth + p.wall_clearance:g} mm front-to-wall depth",
        "384 mm nominal module footprint",
        "24 drawing-based M3 mounting locations",
        "No rated wall hanger modeled",
    ]
):
    text(550, 184 + i * 29, t)
text(550, 427, "Front lip: 1.2 mm deep")
text(550, 456, "Rear cavity: 388 × 388 mm")
text(550, 485, "Diffuser carrier: 387.2 mm outside")
text(550, 514, "Carrier aperture: 384.4 mm")
# Side stack magnified in Z only, with an explicit axis and exact layer locations.
text(
    70,
    631,
    "Depth stack — magnified Z axis; cables and electronics shown in separate lanes",
)
x0 = 100
scale = 9
y0 = 662
h = 78
layers = [
    (p.fabric_front, p.fabric_thickness, "#c9c9bb"),
    (p.led_front, p.panel_envelope_depth, "#47745c"),
    (p.panel_back, p.connector_envelope_depth, "#e5c16b"),
    (p.chassis_front, p.chassis_thickness, "#9ca7af"),
    (p.electronics_front, p.electronics_depth, "#8eafd1"),
    (p.cover_front, p.cover_thickness, "#626e77"),
]
for index, (z, d, color) in enumerate(layers):
    # Cable and electronics occupy different XY areas behind the apertured plate.
    yy = y0 if index in [0, 1, 2, 5] else y0 + 42
    hh = 34 if index in [2, 4] else h if index in [0, 1, 5] else 34
    rect(x0 + z * scale, yy, d * scale, hh, color)
line(x0, y0 - 5, x0 + p.depth * scale, y0 - 5, True)
text(x0 + p.depth * scale / 2, y0 - 15, f"{p.depth:g} frame", "middle")
svg.append(
    f'<path d="M{x0 + (p.depth + p.wall_clearance) * scale},{y0 - 12} V{y0 + h + 28}" class="dash"/>'
)
text(x0 + (p.depth + p.wall_clearance) * scale, y0 + h + 48, "wall", "middle")
for z in [0, 10, 20, 30, 40, 50, 60, 70, 85]:
    line(x0 + z * scale, y0 + h + 5, x0 + z * scale, y0 + h + 12)
    text(x0 + z * scale, y0 + h + 29, str(z), "middle", "small")
text(
    70,
    833,
    "Fabric 1.3–1.6 · LED tips 2.1 · mounting plane 17.1 · cable reserve to 34.1",
    cls="small",
)
text(
    70,
    854,
    f"Carrier {p.chassis_front:g}–{p.chassis_front + p.chassis_thickness:g} · electronics {p.electronics_front:g}–{p.electronics_front + p.electronics_depth:g} · cover {p.cover_front:g}–{p.cover_front + p.cover_thickness:g} · wall {p.depth + p.wall_clearance:g}",
    cls="small",
)
text(
    70,
    888,
    "CONCEPT ONLY — no fabrication, structural, electrical, optical or thermal release",
    cls="small",
)
svg.append("</svg>")
(
    Path(os.environ.get("OAK_PANEL_OUTPUT", root / "exports")) / "layout-dimensions.svg"
).write_text("\n".join(svg))
print("Saved dimensioned concept drawing")
