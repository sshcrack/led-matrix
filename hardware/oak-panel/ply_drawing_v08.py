#!/usr/bin/env python3
"""Drill sheet for the v0.8 plywood back (v08.py), in German like the v0.7 wood sheets.

Writes exports/v08/sperrholz-bohrplan.pdf:
  1  overview and page index
  2  shopping list
  3  order of work
  4  drill table, measured from the bottom-left corner
  5-10 full-size A4 templates, with 10 mm overlap

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
    txt(285, 198, f"Seite {c.getPageNumber()}", 8, anchor="right", colour=GREY)
    txt(12, 198, title, 14, True)
    txt(12, 192, sub, 8.5, colour=(0.3, 0.3, 0.3))


FULL = 2 * E
OVER = 10.0
COLS, ROWS = 2, 3
TW = (FULL + (COLS - 1) * OVER) / COLS  # 196.5 wide
TH = (FULL + (ROWS - 1) * OVER) / ROWS  # 134.3 high
MX, MY = 14.0, 40.0  # page position of the tile's bottom-left corner


def mline(x0, y0, x1, y1, w=0.25, col=INK, dash=None):
    c.setLineWidth(w)
    c.setStrokeColorRGB(*col)
    c.setDash(*dash) if dash else c.setDash()
    c.line(x0 * mm, y0 * mm, x1 * mm, y1 * mm)
    c.setDash()



FIRST_TILE_PAGE = 5


def tile_map(x, y, cell, active=None):
    """Wall-side sheet positions, reading from top left."""
    txt(x + COLS * cell / 2, y + ROWS * cell + 3, "OBEN", 7, True, anchor="centre")
    for row in range(ROWS):
        for col in range(COLS):
            name = f"{'ABC'[row]}{col + 1}"
            page = FIRST_TILE_PAGE + row * COLS + col
            px, py = x + col * cell, y + (ROWS - 1 - row) * cell
            c.setFillColorRGB(*((0.82, 0.90, 0.98) if name == active else (1, 1, 1)))
            c.setStrokeColorRGB(*GREY)
            c.setLineWidth(0.35)
            c.rect(px * mm, py * mm, cell * mm, cell * mm, stroke=1, fill=1)
            txt(px + cell / 2, py + cell * 0.55, name, 7, name == active, anchor="centre")
            txt(px + cell / 2, py + cell * 0.18, f"S. {page}", 6, anchor="centre")

holes = []  # (from_left, from_bottom, what, d)
for x, y, what, d in v08.ply_holes():
    holes.append((round(x + E, 1), round(y + E, 1), what, d))
wx0, wx1, wy0, wy1 = (v + E for v in v08.pod_window())

# --- Page 1: overview --------------------------------------------------------------------------
header(f"v0.8 Holzrückseite: Übersicht | Pappelsperrholz {fmt(2 * E)} x {fmt(2 * E)} x {fmt(v08.PLY_T)} mm",
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
    "## Seiten",
    "S. 2  Einkaufsliste Bauhaus",
    "S. 3  Arbeitsablauf und Kachelübersicht",
    "S. 4  Bohrtabelle und Fenstermaße",
    "S. 5-10  Schablonen A1 bis C2 (1:1)",
    "",
    "Die Übersicht ist verkleinert.",
    "Zum Übertragen die Schablonen verwenden.",
], 8.5, 4.3)
tile_map(lx + 2, 21, 15)

# --- Pages 2-3: shopping list and order of work ------------------------------------------------------
c.showPage()
c.setPageSize(landscape(A4))
n_m3_12 = sum(1 for *_, L, _ in v08.inside_screws() if L == 12.0)
n_m3_20 = sum(1 for *_, L, _ in v08.inside_screws() if L == 20.0)
n_ties = 2 * (len(v08.BOARDS) + 1)  # two ties per board and the fuse
header("Einkaufsliste Bauhaus", "Holzzuschnitt im Baumarkt. Bohrungen und Fenster machst du zu Hause.")
lines(12, 180, [
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
lines(156, 180, [
    "## Druckaufträge",
    "1 Auftrag: Halo-Teile",
    "1 Auftrag: Gehäuse, Schienen, Puffer und Röhrchen",
    "",
    "## Unterlagen",
    "Bohrtabelle S. 4, A4-Schablonen S. 5-10.",
], 9, 4.7)

c.showPage()
header("Arbeitsablauf", "Die Schablonen zeigen die Wandseite. Die Innenseite der Rückwand liegt beim Anzeichnen unten.")
lines(12, 180, [
    "## Rückwand vorbereiten",
    "1  Schablonen S. 5-10 in A4 quer bei 100 % drucken. Jeden 100-mm-Balken",
    "   nachmessen. Blätter nach dem Kachelplan anordnen, eine Papierkante je Stoß",
    "   am Kachelrahmen abschneiden und die 10-mm-Streifen deckungsgleich kleben.",
    "   Die Außenkanten der Schablone bündig an der Rückwand ausrichten.",
    "   Lochmitten durchstechen, Ø 2 vorbohren und nach Kennung aufbohren.",
    "   Fenster nach gestrichelter Kontur aussägen, Kanten schleifen.",
    "",
    "## Zusammenbau",
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
tile_map(221, 112, 23)
lines(199, 98, [
    "A1 beginnt oben links (S. 5).",
    "C2 liegt unten rechts (S. 10).",
    "Graue Streifen überlappen 10 mm.",
    "Nur Lochmarken mit Kennung bohren.",
], 8.5, 4.8)

# --- Page 4: drill table ----------------------------------------------------------------------------
c.showPage()
c.setPageSize(landscape(A4))
header("Rückwand: Bohrtabelle", "Wandansicht, Maße in mm ab Ecke unten links. Schablonen auf S. 5-10.")
txt(12, 10, f"Fenster: X {fmt(wx0)} bis {fmt(wx1)}, Y {fmt(wy0)} bis {fmt(wy1)}. Eckbohrungen Ø 8.", 8)
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

# --- Pages 5-10: templates at 1:1 ------------------------------------------------------------------
LABEL = {"panel screw": "P6", "M3 halo": "H3,5", "M3 pod": "G3,5", "M3 bumper": "U3,5", "M3 rail": "S3,5",
         "cable tie": "K4", "vent": "L10"}

for row in range(ROWS - 1, -1, -1):
    for col in range(COLS):
        c.showPage()
        c.setPageSize(landscape(A4))
        tx0 = col * (TW - OVER)
        ty0 = row * (TH - OVER)
        name = f"{'ABC'[ROWS - 1 - row]}{col + 1}"
        header(f"Schablone {name} (1:1)  -  links {fmt(tx0)} bis {fmt(tx0 + TW)}, unten {fmt(ty0)} bis {fmt(ty0 + TH)}",
               "Mit 100 % drucken (nicht 'an Seite anpassen'), Balken unten nachmessen. Grau: 10 mm Überlappung "
               "mit der Nachbarschablone. A oben, C unten; 1 links, 2 rechts.")
        tile_map(237, 113, 17, name)

        def T(x, y):
            return MX + x - tx0, MY + y - ty0

        def inside(x, y, m=0.0):
            return tx0 - m <= x <= tx0 + TW + m and ty0 - m <= y <= ty0 + TH + m
        # tile frame and overlap bands
        c.setFillColorRGB(0.9, 0.9, 0.9)
        if col > 0:
            c.rect(MX * mm, MY * mm, OVER * mm, TH * mm, stroke=0, fill=1)
        if col < COLS - 1:
            c.rect((MX + TW - OVER) * mm, MY * mm, OVER * mm, TH * mm, stroke=0, fill=1)
        if row > 0:
            c.rect(MX * mm, MY * mm, TW * mm, OVER * mm, stroke=0, fill=1)
        if row < ROWS - 1:
            c.rect(MX * mm, (MY + TH - OVER) * mm, TW * mm, OVER * mm, stroke=0, fill=1)
        c.setStrokeColorRGB(*GREY)
        c.setLineWidth(0.2)
        c.rect(MX * mm, MY * mm, TW * mm, TH * mm, stroke=1, fill=0)
        # board edges (thick) where they fall on this tile
        for x in (0.0, FULL):
            if tx0 <= x <= tx0 + TW:
                mline(*T(x, ty0), *T(x, ty0 + TH), 0.8)
                txt(T(x, 0)[0] + (2 if x == 0 else -2), MY + TH / 2, "Holzkante", 7, anchor="left" if x == 0 else "right")
        for y in (0.0, FULL):
            if ty0 <= y <= ty0 + TH:
                mline(*T(tx0, y), *T(tx0 + TW, y), 0.8)
                txt(MX + TW / 2, T(0, y)[1] + (2 if y == 0 else -4), "Holzkante", 7, anchor="centre")
        # 10 mm ticks along the bottom and left tile border with board coordinates
        for v in range(0, int(FULL) + 1, 10):
            if tx0 <= v <= tx0 + TW:
                x, _ = T(v, 0)
                mline(x, MY, x, MY - (3 if v % 50 == 0 else 1.5), 0.2)
                if v % 50 == 0:
                    txt(x, MY - 6, str(v), 6, anchor="centre")
            if ty0 <= v <= ty0 + TH:
                _, y = T(0, v)
                mline(MX, y, MX - (3 if v % 50 == 0 else 1.5), y, 0.2)
                if v % 50 == 0:
                    txt(MX - 4, y - 1, str(v), 6, anchor="right")
        # Clip the actual window outline so a tile seam never becomes a cut line.
        c.saveState()
        tile = c.beginPath()
        tile.rect(MX * mm, MY * mm, TW * mm, TH * mm)
        c.clipPath(tile, stroke=0)
        c.setDash(3, 2)
        c.setLineWidth(0.5)
        c.setStrokeColorRGB(*INK)
        x0, y0 = T(wx0, wy0)
        c.rect(x0 * mm, y0 * mm, (wx1 - wx0) * mm, (wy1 - wy0) * mm, stroke=1, fill=0)
        c.restoreState()
        if inside((wx0 + wx1) / 2, (wy0 + wy1) / 2):
            txt(*T((wx0 + wx1) / 2, (wy0 + wy1) / 2), "Fenster aussägen (Ecken Ø 8)", 8, anchor="centre")
        # Dashed lines show the inside edge of each grey overlap strip.
        if col > 0:
            mline(MX + OVER, MY, MX + OVER, MY + TH, dash=(2, 2), col=GREY)
        if col < COLS - 1:
            mline(MX + TW - OVER, MY, MX + TW - OVER, MY + TH, dash=(2, 2), col=GREY)
        if row > 0:
            mline(MX, MY + OVER, MX + TW, MY + OVER, dash=(2, 2), col=GREY)
        if row < ROWS - 1:
            mline(MX, MY + TH - OVER, MX + TW, MY + TH - OVER, dash=(2, 2), col=GREY)
        # holes: true-size circle, a cross through the centre, label
        for hx, hy, what, d in holes:
            if not inside(hx, hy):
                continue
            x, y = T(hx, hy)
            c.setLineWidth(0.25)
            c.setStrokeColorRGB(*INK)
            c.circle(x * mm, y * mm, d / 2 * mm, stroke=1, fill=0)
            r = d / 2 + 3
            mline(x - r, y, x + r, y, 0.15)
            mline(x, y - r, x, y + r, 0.15)
            c.setFillColorRGB(*INK)
            c.circle(x * mm, y * mm, 0.35 * mm, stroke=0, fill=1)
            txt(x + d / 2 + 1.2, y + 1.2, LABEL[what], 5.5, colour=GREY)
        # scale bar
        mline(MX + 120, 14, MX + 220, 14, 0.5)
        for i in range(11):
            mline(MX + 120 + 10 * i, 14, MX + 120 + 10 * i, 14 + (3 if i % 5 == 0 else 1.6), 0.3)
        txt(MX + 170, 18.5, "100 mm: nachmessen", 7, anchor="centre")
        txt(MX, 26, "P6 Panelschraube Ø6 · H/G/U/S Ø3,5 · K4 Kabelbinder Ø4 · L10 Lüftung Ø10", 7)

c.save()
print(args.output, Counter(h[2] for h in holes))
