#!/usr/bin/env python3
"""Cut and drill sheets for the v0.7 wood rear (pine strips + HDF back), from v07.py. German text
(the owner's build sheets); numbers come from v07.py.

Writes exports/v07/holz-zuschnitt-bohrplan.pdf:
  1     overview (rear view), cut list
  2     Bauhaus shopping list: wood, screws, paint, drill bits and tools
  3     order of work
  4     strip drilling tables
  5-6   strip templates at 1:1 (two halves that overlap by 10 mm)
  7     HDF back: drawing and hole table
  8-13  HDF templates at 1:1 (six tiles)

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
parser.add_argument("--output", type=Path, default=ROOT / "exports" / "v07" / "holz-zuschnitt-bohrplan.pdf")
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
        "top": (FULL, "beliebiges Ende", "beliebiges Ende", s["top"]),
        "bottom": (FULL, "Pi-Ende (rechts in Rückansicht)", "Netzteil-Ende (links in Rückansicht)", s["bottom"]),
        "left": (SIDE_LEN, "UNTERES Ende", "oberes Ende", s["left"]),
        "right": (SIDE_LEN, "OBERES Ende", "unteres Ende", s["right"]),
    }


def socket_holes():
    """Bottom strip's OUTER face, seen from outside below the display: page-left = power end.
    [(along, from_edge, d, what)]; from_edge is from either long edge (centred)."""
    a = v07.DC_SOCKET_X + OUT
    s = ep.DC_SOCKET["screw_spacing"] / 2
    off = v07.DC_SOCKET_Z - v07.Z_REAR
    return [
        (a, off, ep.DC_SOCKET["body_cutout"], "Buchse Ø14 durch"),
        (a - s, off, v07.DC_SOCKET_BOSS_POCKET[0], "Ø6, 7 tief; darin Ø2 bis 17 tief"),
        (a + s, off, v07.DC_SOCKET_BOSS_POCKET[0], "Ø6, 7 tief; darin Ø2 bis 17 tief"),
    ]


def hdf_holes():
    """[(X, Y, d, what)] from the HDF's bottom-left corner, seen from the wall side."""
    return [(x + OUT, y + OUT, d, what) for x, y, what, d in v07.hdf_holes()]


# --- Drawing helpers -------------------------------------------------------------------------
c = canvas.Canvas(str(args.output), pagesize=landscape(A4))
c.setTitle("v0.7 Holzrückseite: Zuschnitt- und Bohrplan")
HOLE_DE = {"corner": "Ecke", "rail": "Schiene", "bumper": "Puffer", "halo cable": "Halo-Kabel", "vent": "Lüftung"}
PLATE_DE = {"carrier": "Träger", "power plate": "Netzteilplatte", "controller plate": "Pi-Platte"}


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
    txt(x + 50, y + 4.5, "100 mm: vor Gebrauch nachmessen (drucken mit 100 %, nicht 'an Seite anpassen')", 7, anchor="centre")


def header(title, sub=None, w=297):
    txt(12, 210 - 12 if w == 297 else 297 - 12, title, 14, True)
    if sub:
        txt(12, (210 if w == 297 else 297) - 18, sub, 8.5, colour=(0.3, 0.3, 0.3))


# ============================================================================================
# Page 1: overview, cut list, shopping list
# ============================================================================================
header("v0.7 Holzrückseite: Übersicht", "Ansicht von der Wandseite: Display liegt mit den LEDs nach unten auf dem Tisch, Oberkante von dir weg. Maße in mm.")
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
txt(px + 6, py - 2.5, "DC-Buchse", 7)
txt(*P(0, OUT + 6), "OBEN", 8, True, anchor="centre")
txt(*P(0, INN - 10), "Leiste oben 390", 7, anchor="centre")
txt(*P(0, -INN + 5), "Leiste unten 390", 7, anchor="centre")
c.saveState()
for xs, label in ((-OUT + 6, "Leiste links 352"), (INN + 13, "Leiste rechts 352")):
    px, py = P(xs, 0)
    c.translate(px * mm, py * mm)
    c.rotate(90)
    txt(0, 0, label, 7, anchor="centre")
    c.rotate(-90)
    c.translate(-px * mm, -py * mm)
c.restoreState()
txt(*P(-96, -60), "Netzteilmodule", 7, anchor="centre", colour=GREY)
txt(*P(74, -80), "Pi", 7, anchor="centre", colour=GREY)
txt(*P(-80, 128), "Aufhängeschiene", 7, anchor="centre", colour=(0.7, 0.1, 0.1))
txt(*P(80, 128), "Aufhängeschiene", 7, anchor="centre", colour=(0.7, 0.1, 0.1))
lx = 18
txt(lx, 8, "Braun: Kiefernleisten. Blau: gedruckte Halo-Kanalstücke (auf der HDF). Rot: Aufhängeschienen, Wandschrauben, Puffer.", 7)
txt(lx, 4, "Weiße Kreise: M3 x 40 durch die Leisten (Ansenkung auf der Rückseite). Grau: Löcher in der HDF.", 7)

rows = [
    "## Zuschnitt",
    "Kiefernleiste 19 x 30 mm:",
    f"   2 x {fmt(FULL)} mm (oben, unten, volle Breite)",
    f"   2 x {fmt(SIDE_LEN)} mm (links, rechts, zwischen oben und unten)",
    f"   zusammen {fmt(2 * FULL + 2 * SIDE_LEN)} mm + Sägeschnitte: eine 2,4-m-Leiste reicht.",
    "   30 mm ist die Tiefe (Richtung Wand), 19 mm liegen in der Panelebene.",
    "   Seitenteile = 390 - 2 x echte Leistendicke. Erst messen!",
    "   Jede Leiste von 15 bis 20 mm Dicke passt.",
    f"HDF 3 mm: 1 x {fmt(FULL)} x {fmt(FULL)} mm, im Baumarkt zuschneiden lassen.",
    "",
    "## Was bleibt gedruckt",
    "4 Träger, 8 Verbinder, 2 Elektronikplatten,",
    "2 Aufhängeschienen, 2 Puffer, 8 Halo-Kanalstücke (neu).",
    "Etwa 528 cm3 statt 900 cm3 Druck.",
    "",
    "## Seiten",
    "2 Einkaufsliste Bauhaus,  3 Arbeitsablauf,  4 Bohrtabellen Leisten,",
    "5-6 Schablonen Leisten 1:1,  7 HDF-Rückwand,  8-13 Schablonen HDF 1:1",
]
lines(190, 186, rows, 8.5, 4.3)
scale_bar(190, 20)

# ============================================================================================
# Page 2: Bauhaus shopping list
# ============================================================================================
page()
header("Einkaufsliste Bauhaus", "Alles für die Holzrückseite und die Schrauben für die ganze Rückseite. Produktnummern sind Hinweise aus bauhaus.info; Bestand im Markt prüfen.")
cols = (12, 66, 128, 148, 205)
heads = ("Artikel", "Maße / Ausführung", "Menge", "Wofür", "Hinweis")


def table(y, rows_, size=7.6, lead=3.7):
    for x, h in zip(cols, heads):
        txt(x, y, h, 8.5, True)
    y -= 1.8
    line(12, y, 287, y, 0.3)
    y -= 4
    for r in rows_:
        if isinstance(r, str):
            y -= 1.5
            txt(12, y, r, 8.8, True)
            y -= 4.6
            continue
        cells = [str(v).split("\n") for v in r]
        for x, cell in zip(cols, cells):
            for i, ln in enumerate(cell):
                txt(x, y - i * lead, ln, size)
        y -= lead * max(len(cl) for cl in cells) + 1.6
        line(12, y + 2.6, 287, y + 2.6, 0.1, (0.8, 0.8, 0.8))
    return y


shop = [
    "Holz",
    ("Kiefernleiste, gehobelt\n(Rechteckleiste)", "19 x 30 mm, 2,4 m lang\ngerade aussuchen (über die Länge peilen)", "1",
     "4 Seitenteile\n(Zuschnitt S. 1)", "z. B. Rechteckleiste 240 x 3 x 1,9 cm (Nr. 30735017,\nlackiert) oder unbehandelt; 15-20 mm Dicke geht auch"),
    ("HDF-Platte (Hartfaser)", f"3 mm, Zuschnitt {fmt(FULL)} x {fmt(FULL)} mm\nwinklig, Maß genau angeben", "1",
     "Rückwand", "Hartfaserplatte nach Maß; schwarz (Nr. 14512381)\nspart das Lackieren"),
    "Schrauben und Scheiben (für die ganze Rückseite)",
    ("Gewindeschraube M3 x 40", "Linsen-, Flach- oder Zylinderkopf, verzinkt\nKopf muss in ein 10-mm-Loch passen", "22 (+2)",
     "Leisten an Träger (16)\nund Plattenpfosten (6)", "z. B. Profi Depot Gewindeschraube VZ 3 x 40\n(100er-Packung)"),
    ("Unterlegscheibe M3, groß", "DIN 9021 / ISO 7093: 3,2 x 9 mm\n(Notlösung: DIN 125, 3,2 x 7 mm)", "22 (+2)",
     "unter die M3 x 40", "online nicht gefunden: im Markt nach 'Scheibe 3,2'\nfragen; bei 7-mm-Scheiben Löcher nicht auf 6 aufbohren"),
    ("Spanplattenschraube\nSenkkopf", "3 x 20 mm, Kreuzschlitz PZ1", "28 (+4)",
     "Halo-Stücke (24),\nHDF-Ecken (4)", "z. B. Nr. 10800509 (20 Stück, also 2 Packungen)\noder SWG 3 x 20 (Nr. 32882140, 200 Stück)"),
    ("Gewindeschraube M3 x 12", "Linsenkopf ISO 7380", "26 (+4)",
     "Verbinder 16, Schienen 6,\nPuffer 2, innere Pfosten 2", "Marinetech (Nr. 23678981), 0,13 EUR/Stück:\n30 Stück = 3,90 EUR"),
    ("Gewindeschraube M3 x 20", "Linsenkopf, verzinkt", "16",
     "Träger an Panel-Gewinde", "Profi Depot VZ (Nr. 10807533), 100er = 3,95 EUR"),
    ("Zylinderschraube M2,5 x 10", "DIN 912, Innensechskant", "4",
     "Pi", "Suki (Nr. 21307474), 10er = 3,90 EUR"),
    ("Holzschraube Halbrundkopf", "3 x 16 mm (z. B. Spax)", "2 (+2)",
     "DC-Buchse", "Linsen-/Halbrundkopf, kein Senkkopf"),
    "Farbe und Kleinkram",
    ("Lack schwarz matt", "Sprühdose oder Acryllack, kleine Dose", "1",
     "Leisten (außen, Enden)", "HDF nur, wenn sie nicht schwarz ist"),
    ("Holzleim (optional)", "z. B. Ponal Express, kleine Flasche", "1",
     "Eckstöße der Leisten", ""),
    ("Schleifpapier", "Körnung 120", "1-2 Bogen", "Schnittkanten", ""),
    ("Kreppband", "", "1", "Schablonen, Tiefenmarke", "falls nicht vorhanden"),
    "Bohrer und Werkzeug (falls nicht vorhanden)",
    ("Holzspiralbohrer\nmit Zentrierspitze", "2, 3, 5, 6, 8 und 10 mm\n+ 3,5 mm Metallbohrer (HDF, Buchse)", "je 1",
     "alle Löcher; 10 mm für\ndie Ansenkungen", "Satz ist oft günstiger als einzelne Bohrer"),
    ("Forstnerbohrer oder\nFlachfräsbohrer", "14 mm", "1", "DC-Buchse", "Forstner reißt weniger aus"),
    ("Kegelsenker", "für 3-mm-Senkkopf (Ø6-10)", "1", "HDF-Ecken", "optional; die Halo-Stücke sind angesenkt"),
    ("Feinsäge + Gehrungslade", "", "1", "gerade Schnitte", "oder Zuschnitt im Markt (Leiste auf Länge)"),
    ("Anschlagwinkel, Stahllineal\noder Maßband, Vorstecher", "", "", "anreißen, ankörnen", ""),
    ("Schraubzwingen", "", "2", "beim Bohren festhalten", ""),
]
y = table(186, shop)
txt(12, max(y - 2, 8), "Bereits geplante Schrauben (M3 x 12, M3 x 20, M2,5 x 10) stehen mit drin, falls sie noch nicht gekauft sind. Muttern sind nicht nötig.", 7.5, colour=(0.3, 0.3, 0.3))

# ============================================================================================
# Page 3: order of work
# ============================================================================================
page()
header("Arbeitsablauf")
rows = [
    "## 1  Leisten zuschneiden",
    f"2 x {fmt(FULL)} und 2 x {fmt(SIDE_LEN)} mm, Enden rechtwinklig (Gehrungslade). Mit dem Winkel prüfen, Enden schleifen.",
    "Auf jedes Stück schreiben: welches Teil, welche Fläche die RÜCKSEITE ist (dort liegt die HDF auf) und welche",
    "Längskante AUSSEN ist. Seitenleisten: das UNTERE Ende markieren. Untere Leiste: das NETZTEIL-Ende markieren.",
    "",
    "## 2  Leisten bohren (Tabelle S. 4, Schablonen S. 5-6)",
    "Schablone auf die RÜCKSEITE kleben: Schablonen-Außenkante auf die Außenkante des Holzes, Ende auf Ende.",
    "Jedes Kreuz mit dem Vorstecher durchstechen. Dann für jedes M3-Loch:",
    f"   a) zuerst 10-mm-Ansenkung, {fmt(CB_DEPTH)} mm tief, auf der Rückseite (die Zentrierspitze findet die Markierung;",
    "      Kreppband als Tiefenmarke auf den Bohrer).",
    "   b) dann 5 mm ganz durch die 30 mm. Bohrer senkrecht halten; mit Bohrständer ist das einfach.",
    "Nur untere Leiste, AUSSENFLÄCHE (Schablone S. 6): erst zwei 6-mm-Taschen, 7 mm tief, darin 2 mm bis 17 mm tief,",
    "dann 14 mm ganz durch für die DC-Buchse. Vorher an einem Reststück testen. Kabel vor dem Einbau anlöten.",
    "",
    "## 3  Probe-Aufbau vor dem Lackieren",
    "Die vier Panels mit Trägern und Verbindern mit den LEDs nach unten hinlegen. Leisten auf die Außenränder der",
    "Träger stellen: oben und unten über die volle Breite, die Seiten dazwischen, Außenflächen 3 mm über die Panelkante.",
    "Durch jedes Loch eine M3 x 40 stecken: sie muss das Vorbohrloch darunter treffen. Wenn eine danebenliegt,",
    "das Loch auf 6 mm aufbohren (die 9-mm-Scheibe deckt das noch ab).",
    "",
    "## 4  Lackieren",
    "Leisten schwarz matt (vor allem Außenflächen und Enden). Die HDF auch, wenn sie nicht schwarz gekauft ist.",
    "",
    "## 5  HDF bohren (Tabelle S. 7, Schablonen S. 8-13)",
    "Die sechs Kacheln an den gestrichelten Linien zusammenkleben, die 390-mm-Außenkante mit der Platte vergleichen,",
    "Markierungen durchstechen, bohren. Die Seite, von der du bohrst, ist die WANDSEITE (außen). Ein Reststück",
    "Holz unterlegen und festzwingen, damit die Löcher nicht ausreißen.",
    "",
    "## 6  Zusammenbau",
    "Elektronikplatten und Träger wie geplant. Leisten mit M3 x 40 + Scheibe anschrauben (Eckstöße dürfen geleimt werden).",
    "Aufhängeschienen und Puffer an die HDF: M3 x 12 von innen. HDF auflegen, Außenkanten bündig mit den Leisten.",
    "Ecken: 2-mm-Vorbohrung durch die Eckbohrung in die Leiste, Spanplattenschraube 3 x 20.",
    "Halo-Stücke auf die HDF über den Leisten (mittig auf der Leiste, Stoß jeweils in der Kantenmitte): durch jedes",
    "Loch 3 mm durch Stück und HDF bohren, dann 2 mm, 15 mm tief in die Leiste vorbohren; 3 x 20 eindrehen.",
    "Den Halo-Streifen von den Ecken her einschieben; das Kabel kommt durch das 8-mm-Loch unten rechts.",
]
lines(14, 186, rows, 8.5, 4.3)

# ============================================================================================
# Page 3: strip drilling tables
# ============================================================================================
page()
header("Leisten: Bohrtabellen", "Lage wie auf den Schablonen: Rückseite oben, Außenkante oben, Maße vom linken Ende. Durchgangslöcher 5 mm, Ansenkung 10 mm x 3 mm auf der Rückseite.")
data = strip_holes()
y = 180
names = {"top": "Leiste oben", "bottom": "Leiste unten", "left": "Leiste links (Netzteil-Seite)", "right": "Leiste rechts (Pi-Seite)"}
for key in ("top", "bottom", "left", "right"):
    length, lend, rend, holes = data[key]
    txt(14, y, f"{names[key]}  ({fmt(length)} mm lang; links = {lend}, rechts = {rend})", 9.5, True)
    y -= 5
    extra = ""
    if key == "right":
        extra = "   (= " + ", ".join(fmt(length - a) for a, _, _ in reversed(holes)) + " vom unteren Ende, wie links)"
    txt(18, y, "vom linken Ende:  " + ",  ".join(fmt(a) for a, _, _ in holes) + extra, 9)
    y -= 4.5
    txt(18, y, f"alle {fmt(holes[0][1])} mm von der Außenkante.  " + "  ".join(
        f"{fmt(a)}: {PLATE_DE[w]}" for a, _, w in holes if w != "carrier") or "", 8, colour=(0.3, 0.3, 0.3))
    # Sketch at 0.6 scale.
    y -= 13
    sk = 0.6
    rect(18, y, length * sk, T * sk + 0.01, fill=WOOD)
    for a, inset, _ in holes:
        circle(18 + a * sk, y + (T - inset) * sk, CB_D * sk, 0.25, fill=(1, 1, 1))
        circle(18 + a * sk, y + (T - inset) * sk, v07.STRIP_HOLE * sk, 0.2)
    txt(18 + length * sk + 3, y + 3, "Außenkante oben", 7, colour=GREY)
    y -= 12
if socket_holes():
    txt(14, y, "Leiste unten, AUSSENFLÄCHE (von unten gesehen; links = Netzteil-Ende): DC-Buchse", 9.5, True)
    y -= 5
    for a, e, d, w in socket_holes():
        txt(18, y, f"{fmt(a)} mm vom Netzteil-Ende ({fmt(FULL - a)} vom Pi-Ende), {fmt(e)} mm von beiden Längskanten: {w}", 9)
        y -= 4.5
    txt(18, y, "Erst die zwei Ø6-Taschen (für die Kunststoffdome hinter dem Flansch), dann Ø14 ganz durch. Flansch außen aufs Holz.", 8, colour=(0.3, 0.3, 0.3))
    y -= 4.5
    txt(18, y, "Die zwei Löcher der Buchse (Flansch + Dom, 2,3 mm) auf 3,5 mm aufbohren; Halbrundkopf-Holzschrauben 3 x 16 hinein.", 8, colour=(0.3, 0.3, 0.3))
    y -= 4
    txt(18, y, "Kabel vorher anlöten: die Lötösen enden ca. 12 mm tief im Ø14-Loch. Körper max. Ø12,6, Dome Ø4,8 (Kycon-CAD).", 8, colour=(0.3, 0.3, 0.3))


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
        txt(13, y + width + 2.5, f"{lend} (Schnittkante)", 6.5, colour=GREY)
        line(12 + SPLIT, y - 3, 12 + SPLIT, y + width + 3, 0.3, dash=(2, 2))
        txt(12 + SPLIT - 1, y - 4.5, f"Überlappung: mit Teil 2 bei {fmt(SPLIT - OVER)}-{fmt(SPLIT)} ausrichten", 6.5, anchor="right", colour=GREY)
    else:
        line(12, y - 3, 12, y + width + 3, 0.3, dash=(2, 2))
        line(12 + OVER, y - 3, 12 + OVER, y + width + 3, 0.3, dash=(2, 2))
        line(x_left + length, y - 3, x_left + length, y + width + 3, 0.6)
        txt(x_left + length - 1, y + width + 2.5, f"{rend} (Schnittkante)", 6.5, anchor="right", colour=GREY)
    for a, inset, d in holes:
        if a0 <= a <= a1:
            txt(x_left + a, y - 4.5, fmt(a), 6.5, anchor="centre")
    txt(12, y + width + 6, f"{label}  (Teil {part + 1} von 2)", 8.5, True)
    if not socket:
        txt(12 + (a1 - a0) + 3, y + width - 2, "Außenkante", 6.5, colour=GREY)


for part in (0, 1):
    page()
    header(f"Schablonen Leisten 1:1, Teil {part + 1} von 2",
           "An den Bandkanten ausschneiden, aufkleben (Ende auf Schnittkante, obere Linie auf Außenkante), jede Mitte durchstechen.")
    y = 158
    for key in ("top", "bottom", "left", "right"):
        length, lend, rend, holes = data[key]
        hs = [(a, inset, v07.STRIP_HOLE) for a, inset, _ in holes]
        band(y, length, hs, f"{names[key]}, RÜCKSEITE", lend, rend, T, part)
        y -= T + (21 if part == 0 else 14)
    if part == 1:  # the socket sits at 213 mm: only this half is needed
        sh = [(a, e, d) for a, e, d, _ in socket_holes()]
        band(y - 14, FULL, sh, "Leiste unten, AUSSENFLÄCHE (DC-Buchse)", "Netzteil-Ende", "Pi-Ende", v07.STRIP_D, part, socket=True)
    scale_bar(185, 186)


# ============================================================================================
# Page 6: HDF drawing and table
# ============================================================================================
page()
header("HDF-Rückwand, 390 x 390 x 3 mm", "Ansicht von der Wandseite. X von der LINKEN Kante, Y von der UNTEREN Kante. Die Löcher der Halo-Stücke werden beim Zusammenbau durch die Stücke gebohrt.")
S = 0.42
bx, by = 14, 22
rect(bx, by, FULL * S, FULL * S, fill=(0.96, 0.96, 0.96), width=0.4)
for X, Y, d, what in hdf_holes():
    circle(bx + X * S, by + Y * S, max(d, 3) * S, 0.3)
for x, y in v07.halo_screws():
    circle(bx + (x + OUT) * S, by + (y + OUT) * S, 1.2, 0.15, colour=(0.1, 0.3, 0.8))
txt(bx + FULL * S / 2, by + FULL * S + 2, "OBEN", 8, True, anchor="centre")
txt(bx, by - 5, "Nullpunkt: Ecke unten links (Wandseite)", 7, colour=GREY)
txt(bx + FULL * S, by - 5, "blaue Punkte: Schrauben der Halo-Stücke (beim Zusammenbau)", 7, anchor="right", colour=(0.1, 0.3, 0.8))

groups = {}
for X, Y, d, what in hdf_holes():
    groups.setdefault((what, d), []).append((X, Y))
rows = ["## Löcher (X, Y)"]
for (what, d), pts in groups.items():
    if what == "vent":
        continue
    rows.append(f"{HOLE_DE[what]}, Ø{fmt(d)}{' angesenkt' if what == 'corner' else ''}: " + "  ".join(f"({fmt(X)}, {fmt(Y)})" for X, Y in pts))
vents = sorted(groups[("vent", v07.VENT_D)], key=lambda p: (-p[1], p[0]))
rows += ["", f"## Lüftungslöcher Ø{fmt(v07.VENT_D)} ({len(vents)})"]
by_row = {}
for X, Y in vents:
    by_row.setdefault(Y, []).append(X)
for Y in sorted(by_row, reverse=True):
    rows.append(f"Y {fmt(Y)}:  X " + ", ".join(fmt(X) for X in sorted(by_row[Y])))
rows += [
    "",
    "## Hinweise",
    "Schienen und Puffer: M3 x 12 von innen in das Druckteil.",
    "Ecklöcher: auf der Wandseite ansenken, 3 x 20 in die Leiste.",
    "Halo-Kabel (unten rechts): Kabel vom Pro Micro zum Halo-Streifen.",
    "Lüftung über den Netzteilmodulen (unten links), dem Pi (unten",
    "rechts) und oben. Auf ein Reststück bohren, damit nichts ausreißt.",
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
        header(f"Schablone HDF, Kachel {n} von 6, 1:1  (Reihe {ri + 1} von oben, Spalte {ci + 1} von links)",
               "Wandseite. Kacheln so zusammenkleben, dass die gestrichelten Überlappungslinien und die Lochmarken übereinanderliegen.")
        px0, py0 = 14.0, 16.0
        c.saveState()
        p = c.beginPath()
        p.rect(px0 * mm, py0 * mm, (X1 - X0) * mm, (Y1 - Y0) * mm)
        c.clipPath(p, stroke=0)
        rect(px0 - X0, py0 - Y0, FULL, FULL, width=0.6)
        for X, Y, d, what in hdf_holes():
            target(px0 + X - X0, py0 + Y - Y0, d, arm=3)
            txt(px0 + X - X0 + d / 2 + 1, py0 + Y - Y0 + d / 2 + 0.6, f"{HOLE_DE[what]} Ø{fmt(d)}", 5.5, colour=GREY)
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
