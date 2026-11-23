#!/usr/bin/env python3
"""Drill sheet for the v0.8 plywood back (v08.py), in German like the v0.7 wood sheets.

Writes exports/v08/sperrholz-bohrplan.pdf:
  1  drawing (rear view) with the window and every hole
  2  hole table, measured from the bottom-left corner
  3  shopping list and order of work

View: from the wall side. The plywood lies on the table with its inner face (the side the panels
touch) DOWN and the top edge away from you; that is also how it lies on the face-down panels.
"""

import argparse
from collections import Counter
from pathlib import Path

from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.units import mm
from reportlab.pdfgen import canvas

import v08

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=ROOT / "exports" / "v08" / "sperrholz-bohrplan.pdf")
args = parser.parse_args()
args.output.parent.mkdir(parents=True, exist_ok=True)

E = v08.E
INK, GREY = (0, 0, 0), (0.45, 0.45, 0.45)
WHAT_DE = {"panel screw": "Panelschraube", "M3 halo": "Halo", "M3 pod": "Pi-Gehäuse", "M3 bumper": "Puffer",
           "M3 rail": "Schiene", "cable tie": "Kabelbinder", "vent": "Lüftung"}
MARK = {"panel screw": "P", "M3 halo": "H", "M3 pod": "G", "M3 bumper": "U", "M3 rail": "S", "cable tie": "K", "vent": "L"}

c = canvas.Canvas(str(args.output), pagesize=landscape(A4))
c.setTitle("v0.8 Sperrholz-Rückwand: Bohrplan")


def fmt(v):
    return f"{v:.1f}".rstrip("0").rstrip(".")


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


def header(title, sub):
    txt(12, 198, title, 14, True)
    txt(12, 192, sub, 8.5, colour=(0.3, 0.3, 0.3))


holes = []  # (from_left, from_bottom, what, d)
for x, y, what, d in v08.ply_holes():
    holes.append((round(x + E, 1), round(y + E, 1), what, d))
wx0, wx1, wy0, wy1 = (v + E for v in v08.pod_window())

# --- Page 1: drawing --------------------------------------------------------------------------
header(f"v0.8 Rückwand: Pappelsperrholz {fmt(2 * E)} x {fmt(2 * E)} x {fmt(v08.PLY_T)} mm",
       "Ansicht von der Wandseite: Innenseite (zu den Panels) liegt unten, Oberkante von dir weg. "
       "Maße ab der Ecke unten links, in mm.")
S = 0.43
ox, oy = 20, 18


def P(x, y):
    return ox + x * S, oy + y * S


c.setLineWidth(0.6)
c.setFillColorRGB(0.96, 0.9, 0.8)
c.rect(ox * mm, oy * mm, 2 * E * S * mm, 2 * E * S * mm, stroke=1, fill=1)
x0, y0 = P(wx0, wy0)
c.setFillColorRGB(1, 1, 1)
c.rect(x0 * mm, y0 * mm, (wx1 - wx0) * S * mm, (wy1 - wy0) * S * mm, stroke=1, fill=1)
txt(x0 + (wx1 - wx0) * S / 2, y0 + (wy1 - wy0) * S / 2, "Fenster (Stichsäge)", 6.5, anchor="centre")
for hx, hy, what, d in holes:
    px, py = P(hx, hy)
    c.setLineWidth(0.3)
    c.setStrokeColorRGB(*INK)
    c.setFillColorRGB(*((0.1, 0.1, 0.1) if what == "panel screw" else (1, 1, 1)))
    c.circle(px * mm, py * mm, (d if d >= 6 else 1.6 * max(d, 3)) / 2 * S * mm, stroke=1, fill=1)  # small holes enlarged
    if what != "cable tie":
        txt(px + 1.6, py + 1.2, MARK[what], 5.5, colour=GREY)
# dimension the window
for v, lbl in ((wx0, "x"), (wx1, "x")):
    px, _ = P(v, 0)
    txt(px, oy - 4, fmt(v), 6.5, anchor="centre")
for v in (wy0, wy1):
    _, py = P(0, v)
    txt(ox - 2, py - 1, fmt(v), 6.5, anchor="right")
txt(ox + E * S, oy + 2 * E * S + 3, "OBEN (zur Decke)", 8, True, anchor="centre")
lx = ox + 2 * E * S + 10
lines(lx, 170, [
    "## Löcher",
    f"P  Panelschraube: 16 x Ø {fmt(v08.PLY_HOLE)} (M3 x 30 mit Spiel ±1,3)",
    f"H  Halo-Teile: {sum(1 for h in holes if h[2] == 'M3 halo')} x Ø 3,5",
    f"G  Pi-Gehäuse: {sum(1 for h in holes if h[2] == 'M3 pod')} x Ø 3,5",
    f"S  Aufhängeschienen: {sum(1 for h in holes if h[2] == 'M3 rail')} x Ø 3,5",
    f"U  Puffer: {sum(1 for h in holes if h[2] == 'M3 bumper')} x Ø 3,5",
    f"o  Kabelbinder: {sum(1 for h in holes if h[2] == 'cable tie')} x Ø 4",
    f"L  Lüftung: {sum(1 for h in holes if h[2] == 'vent')} x Ø {fmt(v08.VENT_D)} (unten Luft rein, oben raus)",
    "",
    "## Fenster",
    f"{fmt(wx1 - wx0)} x {fmt(wy1 - wy0)} mm, links {fmt(wx0)}, unten {fmt(wy0)}.",
    "In den Ecken Ø 8 vorbohren, dann aussägen.",
    "",
    "## Wichtig",
    "Die Ø 6-Löcher geben ±1,3 mm Spiel: die Panels",
    "bestimmen die Lage, nicht das Holz. Panels erst",
    "fugenlos zusammenspannen, dann verschrauben.",
    "Alle Löcher senkrecht bohren (Bohrständer oder",
    "Winkel), Holz unterlegen gegen Ausreißen.",
    "",
    "Die Zeichnung ist NICHT maßstäblich zum",
    "Abnehmen: Maße aus der Tabelle (Seite 2).",
], 8.5, 4.3)

# --- Page 2: table ----------------------------------------------------------------------------
c.showPage()
c.setPageSize(landscape(A4))
header("Bohrtabelle", "Ab Ecke unten links (Wandansicht). Erst alle Mittelpunkte mit Winkel und Bleistift anreißen, "
       "dann mit 2 mm vorbohren, dann aufbohren.")
order = ["panel screw", "M3 rail", "M3 pod", "M3 halo", "M3 bumper", "cable tie", "vent"]
rows = sorted(holes, key=lambda h: (order.index(h[2]), h[1], h[0]))
col_w, per_col = 68, 34
for i, (hx, hy, what, d) in enumerate(rows):
    col, row = divmod(i, per_col)
    x = 12 + col * col_w
    y = 180 - row * 4.8
    if row == 0:
        txt(x, y + 5.5, "Nr  Zweck            Ø     links   unten", 7.5, True)
    txt(x, y, f"{i + 1:>2}", 7.5)
    txt(x + 7, y, WHAT_DE[what], 7.5)
    txt(x + 33, y, fmt(d if d != v08.M3_CLEAR else 3.5), 7.5, anchor="right")
    txt(x + 47, y, fmt(hx), 7.5, anchor="right")
    txt(x + 60, y, fmt(hy), 7.5, anchor="right")

# --- Page 3: shopping list and order of work ------------------------------------------------------
c.showPage()
c.setPageSize(landscape(A4))
n_m3_12 = sum(1 for *_, L, _ in v08.inside_screws() if L == 12.0)
n_m3_20 = sum(1 for *_, L, _ in v08.inside_screws() if L == 20.0)
n_ties = 2 * (len(v08.BOARDS) + 1)  # two ties per board and the fuse
header("Einkauf und Reihenfolge", "Ein Druckauftrag für die Halo-Teile, einer für Gehäuse, Schienen, Puffer und Röhrchen.")
y = lines(12, 180, [
    "## Bauhaus / Baumarkt",
    f"Pappelsperrholz {fmt(v08.PLY_T)} mm, Zuschnitt {fmt(2 * E)} x {fmt(2 * E)} mm (Toleranz lieber -1 als +1)",
    "Bohrer Ø 2, 3,5, 4, 6, 8 und 10 mm (Holz), Stichsäge mit feinem Blatt",
    f"Kabelbinder 2,5 mm breit, ca. 200 mm lang: {n_ties} Stück",
    "Doppelseitiges Schaumklebeband ca. 1 mm (Montageband)",
    "",
    "## Schrauben",
    "16 x M3 x 30 Linsenkopf ISO 7380 (neu kaufen; Zylinderkopf DIN 912 geht auch)",
    "8 x Unterlegscheibe DIN 9021 3,2 x 9 (vorhanden)",
    f"{n_m3_12} x M3 x 12 Linsenkopf (vorhanden: Halo, Gehäuse, Puffer)",
    f"{n_m3_20} x M3 x 20 (vorhanden: Schienen)",
    "4 x M2,5 x 10 (vorhanden: Pi ans Gehäusedach)",
    "2 x M5 x 45 + Mutter (Sicherung an den Wandschrauben, wie v0.7)",
], 9, 4.7)
lines(150, 180, [
    "## Reihenfolge",
    "1  Holz anreißen, bohren, Fenster aussägen, Kanten schleifen.",
    "2  Auf dem Tisch, Innenseite nach oben: Halo-Teile, Schienen, Puffer und",
    "   Pi-Gehäuse von der Innenseite mit M3 x 12 / M3 x 20 anschrauben.",
    "3  Pi kopfüber mit 4 x M2,5 x 10 ans Gehäusedach, Pro Micro mit",
    "   Klebeband daneben. DC-Buchse ins Gehäuse.",
    "4  Wandler, Sicherung und WAGOs mit Schaumband auf die Innenseite,",
    "   Wandler und Sicherung zusätzlich mit Kabelbindern. Komplett verdrahten,",
    "   solange das Holz frei liegt (die WAGO-Hebel brauchen Platz zum Öffnen).",
    "5  Panels mit den LEDs nach unten auf ein Tuch, fugenlos zusammenschieben",
    "   und mit einem Spanngurt rundum zusammenziehen.",
    "6  Flachbandkabel und Stromkabel an den Panels einstecken, Röhrchen auf",
    "   die 16 Gewindehülsen stellen.",
    "7  Holz auflegen, Röhrchen unter die Löcher schieben. 8 x M3 x 30 mit",
    "   Scheibe, 8 x M3 x 30 durch die Halo-Teile (Inbus durch die Bohrung).",
    "   Erst alle locker, Fugen prüfen, dann über Kreuz festziehen.",
    "8  LED-Streifen zuletzt in den Halo-Kanal legen (deckt die 8 Schrauben).",
    "",
    "Zum Abnehmen später: Streifen an den 8 Stellen anheben, 16 Schrauben raus.",
], 9, 4.7)
c.save()
print(args.output, Counter(h[2] for h in holes))
