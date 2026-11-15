#!/usr/bin/env python3
"""Build the power + halo board PCB from the KiCad netlist.

Stage 1 (``place <netlist>``): footprints, outline, mounting holes, pre-route
power zones, the In1 GND / In2 +5V_AUX planes, converter- and eFuse-local
wiring (MODULE_LINKS, EFUSE_LINKS), a few fixed signal links (GLOBAL_LINKS)
and a deterministic GND / +5V_AUX via fanout.
Stage 2 (``route``): Freerouting 1.9.0 (headless, FREEROUTING_JAR) routes every
remaining signal on F.Cu/B.Cu; zones are passed as fixed path wires.  Retries
until nothing is left unrouted; the session is saved in LEDPANEL_SCRATCH.
Stage 3 (``finish``): GND pours (F.Cu, In2 remainder, B.Cu), stitching vias,
silkscreen labels, stackup and title block.  Zones are saved unfilled.

Coordinates in this file are board millimetres, origin top-left, +y down.
"""

from __future__ import annotations

import math
import os
import re
import subprocess
import sys
from pathlib import Path

import pcbnew

sys.path.insert(0, str(Path(__file__).resolve().parent))
from sexpr import find, find_all, parse  # noqa: E402

HERE = Path(__file__).resolve().parent
BOARD_DIR = HERE.parent / "power-halo-board"
PCB = BOARD_DIR / "power-halo-board.kicad_pcb"
SYSFP = Path("/usr/share/kicad/footprints")

W, H = 136.0, 86.0
OX, OY = 50.0, 50.0          # page offset of the board origin
BAND = (2.0, 40.5, 113.4, 45.5)   # F.Cu +24V distribution band
IN2_AUX = [(1.0, 38.0, 135.0, 48.0), (1.0, 14.0, 31.0, 72.0), (100.0, 48.0, 135.0, 85.0)]
HOLES = [(4.0, 4.0), (132.0, 4.0), (4.0, 82.0), (132.0, 82.0)]


def mm(v):
    return pcbnew.FromMM(v)


def P(x, y):
    return pcbnew.VECTOR2I(mm(x + OX), mm(y + OY))


# --------------------------------------------------------------------------
# Placement tables
# --------------------------------------------------------------------------

# Panel / AUX power cell in native orientation: module VIN faces -y (towards the
# middle band), connector at +y (board edge).  (kind, index): (x, y, rot)
CELL = {
    ("U", 1): (6.5, 0.0, 0), ("C", 1): (4.1, -6.0, 0), ("C", 2): (8.9, -6.0, 180),
    ("C", 3): (4.1, 6.0, 0), ("C", 4): (4.1, 9.2, 0), ("C", 5): (8.9, 6.0, 180), ("C", 6): (8.9, 9.2, 180),
    ("R", 4): (-1.0, -1.0, 270), ("C", 7): (1.25, 0.0, 180), ("R", 1): (0.6, 2.4, 90),
    ("R", 2): (-3.3, 1.575, 180), ("C", 8): (-1.0, 2.35, 90), ("R", 3): (11.8, 1.3, 0), ("R", 7): (11.8, -0.2, 0),
    ("U", 2): (-9.0, 14.6, 0), ("R", 14): (-3.0, 14.6, 270), ("C", 11): (-6.0, 13.6, 0), ("C", 12): (-6.0, 15.6, 0),
    ("R", 11): (-12.7, 12.6, 90), ("R", 12): (-14.5, 12.6, 90), ("R", 13): (-16.3, 12.6, 90),
    ("D", 1): (-13.0, -7.0, 90), ("R", 15): (-14.8, -7.0, 90),
    ("C", 21): (-11.0, 24.0, 270), ("J", 1): (0.7, 18.0, 0), ("TP", 1): (-4.5, 21.5, 0),
}
# zones in native cell coordinates: (net-key, layer, [rects])
CELL_ZONES = [
    ("+24V", "F.Cu", [(0.0, -9.3, 4.6, -2.45)]),
    ("+24V", "F.Cu", [(8.4, -9.3, 13.0, -2.45)]),
    ("GND", "F.Cu", [(4.85, -8.1, 8.15, 10.65)]),
    ("VOUT", "F.Cu", [(-2.5, 2.25, 4.6, 11.0), (8.4, 2.25, 13.0, 11.0), (-10.5, 11.0, 13.0, 12.9),
                      (-9.42, 12.9, -9.08, 13.6)]),
    ("VBR", "F.Cu", [(-8.92, 15.6, -8.58, 16.5), (-16.5, 16.3, 6.5, 23.5)]),
]
THERMAL_VIAS = [(6.5 + dx, dy) for dy in (-2.25, -0.75, 0.75, 2.25) for dx in (-1.15, 0.0, 1.15)] + \
               [(6.5, -6.0), (6.5, 6.0), (6.5, 9.2), (6.5, -7.6), (6.5, 7.6)]

# panel n -> (cell centre x, orientation) ; top cells are rotated 180 deg about (cx, 32)
CELLS = {1: (49.5, "top"), 2: (84.5, "top"), 3: (49.5, "bot"), 4: (84.5, "bot")}
AUX_CELL = (16.0, "top")


def cell_xf(cx, orient, x, y, rot=0):
    if orient == "top":
        return cx - x, 32.0 - y, (rot + 180) % 360
    return cx + x, 54.0 + y, rot % 360


GLOBAL_PLACE = {
    # ---- input (top right) ----
    "J1": (121.0, 26.0, 270), "F1": (121.0, 17.0, 90), "Q1": (114.0, 12.0, 180),
    "U1": (120.5, 7.0, 0), "C1": (120.5, 9.6, 0), "D1": (106.0, 4.5, 90),
    "C2": (106.0, 24.0, 180), "C3": (106.0, 34.0, 180),
    "R1": (117.0, 37.0, 90), "D2": (119.0, 37.0, 90), "TP1": (112.6, 39.0, 0), "TP2": (123.5, 37.0, 0),
    # ---- AUX extras (top left, AUX cell occupies x 0..32, y 0..42) ----
    "R505": (1.6, 36.4, 90), "R506": (1.6, 33.4, 90),
    "R531": (29.5, 25.0, 90), "D531": (27.7, 25.0, 90), "TP502": (24.5, 24.5, 0),
    # ---- logic (bottom left) ----
    "J603": (10.2, 50.5, 270), "U603": (16.5, 49.5, 0), "C631": (16.5, 52.9, 0),
    "R631": (19.8, 49.8, 90), "R632": (21.6, 49.8, 90),
    "R633": (14.5, 59.6, 0), "R634": (9.3, 59.35, 0), "R635": (6.0, 59.35, 0),
    "Q601": (20.0, 54.6, 0), "Q602": (25.0, 54.6, 0), "R636": (28.8, 54.6, 90),
    "R637": (14.6, 56.6, 90), "C632": (16.4, 56.6, 90),
    "JP601": (11.08, 62.0, 270), "J602": (6.0, 66.5, 90),
    # ---- halo + MCU (bottom right) ----
    "J601": (121.5, 66.0, 90), "U602": (112.1, 63.0, 0),
    "R611": (109.1, 63.0, 0), "R613": (106.1, 63.0, 0), "R612": (103.1, 63.0, 0),
    "R614": (118.1, 63.0, 270), "C611": (115.1, 62.0, 0), "C612": (115.1, 64.0, 0),
    "R621": (100.6, 60.0, 90), "R616": (100.6, 64.0, 90),
    "D601": (107.5, 52.0, 90), "R615": (109.3, 52.0, 90),
    "C621": (109.4, 74.0, 270), "D621": (115.5, 66.6, 0), "TP601": (106.0, 67.0, 0),
    "R622": (116.5, 83.6, 180), "D622": (117.2, 60.1, 180),
    "U601": (121.5, 76.0, 90), "C601": (113.0, 81.0, 90), "C602": (111.0, 81.0, 90),
    "R602": (108.0, 81.5, 90), "D602": (106.2, 81.5, 90),
    # ---- mechanical ----
    "H1": (4.0, 4.0, 0), "H2": (132.0, 4.0, 0), "H3": (4.0, 82.0, 0), "H4": (132.0, 82.0, 0),
    "FID1": (127.0, 46.5, 0), "FID2": (2.5, 72.0, 0), "FID3": (66.5, 3.0, 0),
}

GLOBAL_ZONES = [
    # (net, layer, rects, priority)
    ("/Input/VIN_RAW", "F.Cu", [(119.0, 18.3, 123.0, 31.8)]),
    ("/Input/VIN_F", "F.Cu", [(116.1, 11.0, 123.0, 15.6)]),
    ("+24V", "F.Cu", [(1.0, 36.6, 3.0, 37.85),    # tab to the AUX EN divider top (R505)
                      (109.5, 8.0, 115.9, 21.0), (109.5, 21.0, 113.4, 40.6), (104.6, 5.0, 107.4, 8.0),
                      (107.0, 21.0, 109.5, 37.0), BAND]),
    ("+5V_AUX", "F.Cu", [(104.0, 56.5, 113.5, 61.3), (111.68, 61.2, 112.02, 62.0)]),
    ("/Halo/VHALO", "F.Cu", [(112.18, 64.0, 112.52, 64.8), (104.0, 64.7, 124.0, 69.0)]),
]


# --------------------------------------------------------------------------
# Netlist
# --------------------------------------------------------------------------

def load_netlist(path):
    tree = parse(Path(path).read_text())
    comps = []
    for c in find_all(find(tree, "components"), "comp"):
        ref = str(find(c, "ref")[1])
        fp = find(c, "footprint")
        props = {str(p[1][1]): str(p[2][1]) for p in find_all(c, "property") if len(p) > 2}
        sp = find(c, "sheetpath")
        comps.append({"ref": ref, "value": str(find(c, "value")[1]), "fp": str(fp[1]) if fp else "",
                      "tstamp": str(find(c, "tstamps")[1]), "sheet": str(find(sp, "tstamps")[1]),
                      "sheetname": str(find(sp, "names")[1]), "props": props})
    nets = {}
    for n in find_all(find(tree, "nets"), "net"):
        name = str(find(n, "name")[1])
        for node in find_all(n, "node"):
            nets[(str(find(node, "ref")[1]), str(find(node, "pin")[1]))] = name
    return comps, nets


def fp_load(fpid):
    lib, name = fpid.split(":", 1)
    path = BOARD_DIR / "lib" / "ledpanel.pretty" if lib == "ledpanel" else SYSFP / f"{lib}.pretty"
    fp = pcbnew.FootprintLoad(str(path), name)
    if fp is None:
        raise SystemExit(f"footprint {fpid} not found")
    return fp


def placement_for(ref):
    if ref in GLOBAL_PLACE:
        return GLOBAL_PLACE[ref]
    m = re.match(r"([A-Z]+)(\d)(\d\d)$", ref)
    if m:
        kind, grp, idx = m.group(1), int(m.group(2)), int(m.group(3))
        key = (kind, idx)
        if grp in CELLS and key in CELL:
            cx, orient = CELLS[grp]
            return cell_xf(cx, orient, *CELL[key])
        if grp == 5 and key in CELL:
            return cell_xf(*AUX_CELL, *CELL[key])
    raise SystemExit(f"no placement for {ref}")


# --------------------------------------------------------------------------
# Zones / helpers
# --------------------------------------------------------------------------

ZONE_RECTS = []   # (net, layer, rect) of every pre-route zone, used by the fanout clearance checks


def add_zone(board, net, layer, rects, priority=1, connect="full", name=None, clearance=None):
    for r in rects:
        ZONE_RECTS.append((net, layer, r))
    if board.FindNet(net) is None:
        raise SystemExit(f"zone net {net!r} does not exist")
    z = pcbnew.ZONE(board)
    z.SetLayer(board.GetLayerID(layer))
    z.SetNet(board.FindNet(net))
    poly = z.Outline()          # edit in place: SetOutline() would adopt a Python-owned object
    for (x1, y1, x2, y2) in rects:
        o = pcbnew.SHAPE_POLY_SET()
        o.NewOutline()
        for x, y in [(x1, y1), (x2, y1), (x2, y2), (x1, y2)]:
            o.Append(mm(x + OX), mm(y + OY))
        poly.BooleanAdd(o)
    poly.Simplify()
    z.SetAssignedPriority(priority)
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL if connect == "full" else pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetMinThickness(mm(0.2))
    z.SetThermalReliefGap(mm(0.3))
    z.SetThermalReliefSpokeWidth(mm(0.5))
    if clearance is not None:
        z.SetLocalClearance(mm(clearance))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    if name:
        z.SetZoneName(name)
    board.Add(z)
    return z


def touching_groups(rects):
    """Split rectangles into groups whose union is connected (overlap or shared edge)."""
    parent = list(range(len(rects)))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    for i, a in enumerate(rects):
        for j in range(i):
            b = rects[j]
            if a[0] <= b[2] and b[0] <= a[2] and a[1] <= b[3] and b[1] <= a[3]:
                parent[root(i)] = root(j)
    groups = {}
    for i, r in enumerate(rects):
        groups.setdefault(root(i), []).append(r)
    return list(groups.values())


def add_via(board, net, x, y, dia=0.6, drill=0.3, locked=True):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(P(x, y))
    v.SetWidth(mm(dia))
    v.SetDrill(mm(drill))
    v.SetNet(board.FindNet(net))
    v.SetLocked(locked)
    board.Add(v)
    return v


def outline(board):
    pts = [(0, 0), (W, 0), (W, H), (0, H)]
    for i in range(4):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(mm(0.1))
        s.SetStart(P(*pts[i]))
        s.SetEnd(P(*pts[(i + 1) % 4]))
        board.Add(s)


def outer_zone_specs():
    """All pre-route F.Cu zones as (net, layer, rects, name): one zone per connected group of
    same-net rectangles (overlapping same-net zones would need distinct priorities, and one zone
    must not hold disjoint outlines)."""
    merged = {}
    for n, (cx, orient) in list(CELLS.items()) + [(5, AUX_CELL)]:
        for key, layer, rects in CELL_ZONES:
            if n == 5:
                net = {"+24V": "+24V", "GND": "GND", "VOUT": "+5V_AUX", "VBR": "/Aux/VPI"}[key]
                if key == "VBR":
                    rects = [rects[0], (-15.5, 16.3, 2.6, 23.5)]
            else:
                net = {"+24V": "+24V", "GND": "GND", "VOUT": f"/Panel{n}/VOUT", "VBR": f"/Panel{n}/VPANEL"}[key]
            for (x1, y1, x2, y2) in rects:
                a = cell_xf(cx, orient, x1, y1)
                b = cell_xf(cx, orient, x2, y2)
                merged.setdefault((net, layer), []).append(
                    (min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1])))
    for net, layer, rects in GLOBAL_ZONES:
        merged.setdefault((net, layer), []).extend(rects)
    out = []
    for (net, layer), rects in merged.items():
        for i, group in enumerate(touching_groups(rects)):
            out.append((net, layer, group, f"{net}-{layer}-{i}"))
    return out


def place(netlist_path):
    comps, nets = load_netlist(netlist_path)
    board = pcbnew.NewBoard(str(PCB))
    board.SetCopperLayerCount(4)
    board.SetLayerName(pcbnew.In1_Cu, "In1.GND")
    board.SetLayerName(pcbnew.In2_Cu, "In2.P5V")
    board.SetLayerType(pcbnew.In1_Cu, pcbnew.LT_POWER)
    board.SetLayerType(pcbnew.In2_Cu, pcbnew.LT_POWER)
    ds = board.GetDesignSettings()
    ds.SetBoardThickness(mm(1.6))
    outline(board)

    for name in sorted(set(nets.values())):
        if board.FindNet(name) is None:
            board.Add(pcbnew.NETINFO_ITEM(board, name))

    for c in comps:
        if not c["fp"]:
            continue
        fp = fp_load(c["fp"])
        fp.SetReference(c["ref"])
        fp.SetValue(c["value"])
        lib, name = c["fp"].split(":", 1)
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        x, y, rot = placement_for(c["ref"])
        fp.SetPosition(P(x, y))
        fp.SetOrientationDegrees(rot)
        fp.SetPath(pcbnew.KIID_PATH(c["sheet"] + c["tstamp"]))
        fp.SetSheetname(c["sheetname"])
        fp.SetSheetfile(c["props"].get("Sheetfile", ""))
        if c["props"].get("exclude_from_bom") is not None or "TestPoint" in c["fp"]:
            fp.SetExcludedFromBOM(True)
        for pad in fp.Pads():
            n = nets.get((c["ref"], pad.GetNumber()))
            if n:
                pad.SetNet(board.FindNet(n))
        fp.Reference().SetVisible(True)
        board.Add(fp)

    # pre-route zones --------------------------------------------------------
    for net, layer, group, name in outer_zone_specs():
        add_zone(board, net, layer, group, priority=10, name=name)
    for n, (cx, orient) in list(CELLS.items()) + [(5, AUX_CELL)]:
        for (x, y) in THERMAL_VIAS:
            gx, gy, _ = cell_xf(cx, orient, x, y)
            add_via(board, "GND", gx, gy)
    # In1: solid GND; In2: +5V_AUX distribution
    add_zone(board, "GND", "In1.Cu", [(0.3, 0.3, W - 0.3, H - 0.3)], priority=1, connect="thermal", name="In1-GND")
    add_zone(board, "+5V_AUX", "In2.Cu", IN2_AUX, priority=10, name="In2-5V_AUX")
    # +5V_AUX vias: AUX output zone -> In2, In2 -> halo eFuse input zone
    for x in (15.5, 17.0, 18.5):
        for y in (19.4, 20.6):
            add_via(board, "+5V_AUX", x, y, 0.8, 0.4)
    for x in (105.0, 106.5, 108.0, 109.5):
        for y in (57.3, 58.8):
            add_via(board, "+5V_AUX", x, y, 0.8, 0.4)

    edge_keepout(board)
    made, failed = fanout(board)
    print(f"fanout: {made} vias placed, {len(failed)} pads without a free via site: {failed}")
    pcbnew.SaveBoard(str(PCB), board)
    return board


def edge_keepout(board, width=0.85):
    """Rule areas along the outline: no tracks or vias within ``width`` of the edge (the
    autorouter does not know the 0.5 mm copper-to-edge rule; pads and pours are unaffected)."""
    for i, (x1, y1, x2, y2) in enumerate([(0, 0, W, width), (0, H - width, W, H),
                                          (0, 0, width, H), (W - width, 0, W, H)]):
        z = pcbnew.ZONE(board)
        z.SetIsRuleArea(True)
        z.SetDoNotAllowTracks(True)
        z.SetDoNotAllowVias(True)
        z.SetDoNotAllowPads(False)
        z.SetDoNotAllowCopperPour(False)
        z.SetDoNotAllowFootprints(False)
        z.SetLayerSet(pcbnew.LSET.AllCuMask())
        poly = z.Outline()
        poly.NewOutline()
        for x, y in [(x1, y1), (x2, y1), (x2, y2), (x1, y2)]:
            poly.Append(mm(x + OX), mm(y + OY))
        z.SetZoneName(f"edge-keepout-{i}")
        board.Add(z)


def courtyard_check(board):
    """Report overlapping front courtyards (KiCad DRC also checks this)."""
    items = []
    for fp in board.GetFootprints():
        cy = fp.GetCourtyard(pcbnew.F_CrtYd)
        if cy.OutlineCount():
            items.append((fp.GetReference(), cy))
    bad = []
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            a = items[i][1]
            b = items[j][1]
            if a.BBox().Intersects(b.BBox()):
                inter = pcbnew.SHAPE_POLY_SET(a)
                inter.BooleanIntersection(b)
                if inter.OutlineCount() and inter.Area() > 0:
                    bad.append((items[i][0], items[j][0], pcbnew.ToMM(pcbnew.ToMM(inter.Area()))))
    return bad


def outside_board(board):
    out = []
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            bb = pad.GetBoundingBox()
            x1, y1 = pcbnew.ToMM(bb.GetX()) - OX, pcbnew.ToMM(bb.GetY()) - OY
            x2, y2 = pcbnew.ToMM(bb.GetRight()) - OX, pcbnew.ToMM(bb.GetBottom()) - OY
            if x1 < 0.5 or y1 < 0.5 or x2 > W - 0.5 or y2 > H - 0.5:
                out.append(fp.GetReference())
                break
    return out


# --------------------------------------------------------------------------
# Deterministic fanout: GND -> In1 plane, +5V_AUX -> In2 plane
# --------------------------------------------------------------------------

FANOUT_NETS = {"GND", "+5V_AUX"}
CLR = 0.22                      # fanout clearance (netclass rules: 0.20)
CLR_24 = 0.32                   # clearance to +24V copper
VIA_R = 0.3
TRACK_W = 0.3
GND_W = 0.4

# Paths start and end on pad centres so the router adds no stubs.
# Converter-local wiring, identical in every cell, in module-native coordinates (module centre =
# (0, 0)): (pin, path, layer, width, via at the path end).  AGND 7/11 and MODE 15 go straight to
# the exposed GND pads (TI layout example); the rest feed the cell's passives (CELL table).
MODULE_LINKS = [
    ("7", [(-2.853, 1.3), (-1.9, 1.3), (-1.5, 1.0)], "F.Cu", 0.25, False),
    ("11", [(2.853, 1.95), (1.5, 1.95)], "F.Cu", 0.25, False),
    ("15", [(2.853, -0.65), (1.5, -0.65)], "F.Cu", 0.25, False),
    ("2", [(-2.853, -1.95), (-7.375, -1.95), (-7.5, -1.825)], "F.Cu", 0.2, False),                  # RBOOT -> R4
    ("3", [(-2.853, -1.3), (-6.375, -1.3), (-7.5, -0.175)], "F.Cu", 0.2, False),     # CBOOT -> R4
    ("5", [(-2.853, 0.0), (-4.475, 0.0), (-4.3, 0.9)], "F.Cu", 0.4, True),      # VLDOIN -> C7, via
    ("5", [(-4.3, 0.9), (-4.3, 3.0)], "B.Cu", 0.4, True),                     # ... to the VOUT zone
    ("8", [(-2.853, 1.95), (-3.4, 1.95), (-3.775, 1.575), (-8.975, 1.575)], "F.Cu", 0.2, False),    # FB -> R1, C8, R2
    ("12", [(2.853, 1.3), (4.475, 1.3)], "F.Cu", 0.2, False),                    # RT -> R3
    ("14", [(2.853, 0.0), (3.4, 0.0), (3.9, -0.2), (4.475, -0.2)], "F.Cu", 0.2, False),      # SPSP -> R7
    ("17", [(2.853, -1.95), (3.4, -1.95), (4.5, -1.75)], "F.Cu", 0.2, True),                   # EN escape via
]


def _seg_rect_dist(ax, ay, bx, by, r):
    x1, y1, x2, y2 = r
    if _seg_hits_rect(ax, ay, bx, by, r):
        return 0.0
    d = min(_pt_rect_dist(ax, ay, r), _pt_rect_dist(bx, by, r))
    for cx, cy in ((x1, y1), (x2, y1), (x2, y2), (x1, y2)):
        d = min(d, _pt_seg_dist(cx, cy, ax, ay, bx, by))
    return d


def _seg_hits_rect(ax, ay, bx, by, r):
    x1, y1, x2, y2 = r
    if x1 <= ax <= x2 and y1 <= ay <= y2:
        return True
    if x1 <= bx <= x2 and y1 <= by <= y2:
        return True
    edges = [((x1, y1), (x2, y1)), ((x2, y1), (x2, y2)), ((x2, y2), (x1, y2)), ((x1, y2), (x1, y1))]
    return any(_segs_cross((ax, ay), (bx, by), e[0], e[1]) for e in edges)


def _segs_cross(p1, p2, p3, p4):
    def o(a, b, c):
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    d1, d2, d3, d4 = o(p3, p4, p1), o(p3, p4, p2), o(p1, p2, p3), o(p1, p2, p4)
    return (d1 * d2 < 0) and (d3 * d4 < 0)


def _pt_rect_dist(px, py, r):
    x1, y1, x2, y2 = r
    dx = max(x1 - px, 0, px - x2)
    dy = max(y1 - py, 0, py - y2)
    return math.hypot(dx, dy)


def _pt_seg_dist(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    L2 = vx * vx + vy * vy
    t = 0 if L2 == 0 else max(0, min(1, ((px - ax) * vx + (py - ay) * vy) / L2))
    return math.hypot(px - ax - t * vx, py - ay - t * vy)


def _seg_seg_dist(a, b, c, d):
    if _segs_cross(a, b, c, d):
        return 0.0
    return min(_pt_seg_dist(*a, *c, *d), _pt_seg_dist(*b, *c, *d),
               _pt_seg_dist(*c, *a, *b), _pt_seg_dist(*d, *a, *b))


class Obstacles:
    def __init__(self, board):
        self.pads = []      # (net, layerset, rect, owner)
        self.circles = []   # (net, x, y, r)  vias and holes, all layers
        self.segs = []      # (net, layer, a, b, half_width)
        for fp in board.GetFootprints():
            for pad in fp.Pads():
                bb = pad.GetBoundingBox()
                rect = (pcbnew.ToMM(bb.GetX()) - OX, pcbnew.ToMM(bb.GetY()) - OY,
                        pcbnew.ToMM(bb.GetRight()) - OX, pcbnew.ToMM(bb.GetBottom()) - OY)
                tht = pad.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH)
                layers = {"F.Cu", "B.Cu", "In1.Cu", "In2.Cu"} if tht else {"F.Cu"}
                self.pads.append((pad.GetNetname(), layers, rect, (fp.GetReference(), pad.GetNumber())))
                if tht and pad.GetDrillSize().x > 0:
                    p = pad.GetPosition()
                    self.circles.append(("__hole__" + pad.GetNetname(), pcbnew.ToMM(p.x) - OX,
                                         pcbnew.ToMM(p.y) - OY, pcbnew.ToMM(pad.GetDrillSize().x) / 2))
        for t in board.GetTracks():
            if t.GetClass() == "PCB_VIA":
                p = t.GetPosition()
                self.circles.append((t.GetNetname(), pcbnew.ToMM(p.x) - OX, pcbnew.ToMM(p.y) - OY,
                                     pcbnew.ToMM(t.GetWidth(pcbnew.F_Cu)) / 2))
            else:
                a, b = t.GetStart(), t.GetEnd()
                self.segs.append((t.GetNetname(), board.GetLayerName(t.GetLayer()),
                                  (pcbnew.ToMM(a.x) - OX, pcbnew.ToMM(a.y) - OY),
                                  (pcbnew.ToMM(b.x) - OX, pcbnew.ToMM(b.y) - OY), pcbnew.ToMM(t.GetWidth()) / 2))

    @staticmethod
    def clr(net_a, net_b):
        return CLR_24 if "+24V" in (net_a, net_b) or "VIN" in net_a + net_b else CLR

    def via_ok(self, net, x, y, r=VIA_R):
        if x - r < 0.6 or y - r < 0.6 or x + r > W - 0.6 or y + r > H - 0.6:
            return False
        for hx, hy in HOLES:
            if math.hypot(x - hx, y - hy) < 3.8 + r:
                return False
        for n, layers, rect, _ in self.pads:
            if n != net and _pt_rect_dist(x, y, rect) < r + self.clr(net, n):
                return False
            if n == net and _pt_rect_dist(x, y, rect) < r + 0.1:     # no via-in-pad, even same net
                return False
        for n, cx, cy, cr in self.circles:
            need = cr + r + (0.25 if n == net else self.clr(net, n.replace("__hole__", "")))
            if math.hypot(x - cx, y - cy) < need:
                return False
        for n, layer, a, b, hw in self.segs:
            if n != net and _pt_seg_dist(x, y, *a, *b) < r + hw + self.clr(net, n):
                return False
        for n, layer, rect in ZONE_RECTS:
            if layer in ("F.Cu", "B.Cu") and n != net and _pt_rect_dist(x, y, rect) < r + self.clr(net, n):
                return False
        if net == "+5V_AUX":
            if not any(r_[0] + 0.7 <= x <= r_[2] - 0.7 and r_[1] + 0.7 <= y <= r_[3] - 0.7 for r_ in IN2_AUX):
                return False
        return True

    def seg_ok(self, net, a, b, hw, owner):
        for n, layers, rect, own in self.pads:
            if "F.Cu" in layers and n != net and _seg_rect_dist(*a, *b, rect) < hw + self.clr(net, n):
                return False
        for n, cx, cy, cr in self.circles:
            if n != net and _pt_seg_dist(cx, cy, *a, *b) < cr + hw + self.clr(net, n.replace("__hole__", "")):
                return False
        for n, layer, p1, p2, w in self.segs:
            if layer == "F.Cu" and n != net and _seg_seg_dist(a, b, p1, p2) < hw + w + self.clr(net, n):
                return False
        for n, layer, rect in ZONE_RECTS:
            if layer == "F.Cu" and n != net and _seg_rect_dist(*a, *b, rect) < hw + self.clr(net, n):
                return False
        return True


def add_track(board, net, layer, pts, width):
    for a, b in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(P(*a))
        t.SetEnd(P(*b))
        t.SetWidth(mm(width))
        t.SetLayer(board.GetLayerID(layer))
        t.SetNet(board.FindNet(net))
        t.SetLocked(True)
        board.Add(t)


# eFuse right-hand pins escape as four parallel tracks into C11 (ITIMER), C12 (dVdt) and the
# vertical RILM resistor; GND pin 8 reaches ground through RILM's grounded pad (which gets a via).
# Gaps between these tracks and the neighbouring pads are >= 0.20 mm (checked by DRC).
EFUSE_LINKS = [
    ("10", [(0.9, -0.7), (1.4, -0.7), (1.7, -1.0), (2.225, -1.0)]),
    ("9", [(0.9, -0.225), (5.4, -0.225), (6.0, -0.825)]),
    ("8", [(0.9, 0.225), (5.4, 0.225), (6.0, 0.825)]),
    ("7", [(0.9, 0.7), (1.4, 0.7), (1.7, 1.0), (2.225, 1.0)]),
]


def _fp_xf(fp):
    c = fp.GetPosition()
    cx, cy = pcbnew.ToMM(c.x) - OX, pcbnew.ToMM(c.y) - OY
    th = math.radians(fp.GetOrientationDegrees())
    # KiCad rotation: (x, y) -> (x cos + y sin, -x sin + y cos)
    return lambda x, y: (cx + x * math.cos(th) + y * math.sin(th), cy - x * math.sin(th) + y * math.cos(th))


# cell-local GND exits that the generic fanout cannot find: (part key, pad, [path in cell coords])
CELL_GND_LINKS = [
    (("C", 7), "2", [(0.475, 0.0), (-0.1, 0.6)]),     # VLDOIN cap: between R4, C8 and R1
]
LINKED_PADS = {("D621", "2")}      # pads whose GND via is a GLOBAL_LINKS entry, not the fanout
F_CU_ONLY_NETS = {"/Aux/EN_AUX"}   # module EN escape needs no via: GLOBAL_LINKS reach it on F.Cu
# fixed signal links in global coordinates: (net, [path], width, via at end[, layer])
GLOBAL_LINKS = [
    ("/Aux/EN_AUX", [(1.6, 34.225), (2.075, 33.75), (5.0, 33.75)], 0.2, False),   # R506 -> AUX EN via
    ("/Aux/EN_AUX", [(1.6, 34.225), (1.6, 35.575)], 0.2, False),                  # R506 <-> R505
    # panel-mode jumper chain: J603.3 (GPIO26) -> R633 -> JP601.1; JP601.2 -> R634; JP601.3 -> R635
    ("/Halo/PI_PANEL_REQ", [(10.2, 55.5), (10.2, 57.4), (12.4, 59.6), (13.675, 59.6)], 0.25, False),
    ("/Halo/PANEL_REQ_R", [(15.325, 59.6), (15.325, 60.6), (13.925, 62.0), (11.08, 62.0)], 0.25, False),
    ("/Halo/PANEL_ON", [(8.475, 59.35), (8.54, 62.0)], 0.25, False),
    ("/Halo/FORCE_ON", [(5.175, 59.35), (6.0, 60.175), (6.0, 62.0)], 0.25, False),
    ("/Halo/PI_TXD", [(10.2, 53.0), (11.5, 53.0), (12.9, 51.6), (19.0, 51.6), (19.8, 50.8), (19.8, 50.625)],
     0.25, False),                                                               # J603.2 -> R631
    # JP601.2 / R634 -> Q601 gate: under the GPIO26 link on B.Cu
    ("/Halo/PANEL_ON", [(8.475, 59.35), (7.6, 58.0)], 0.25, True),
    ("/Halo/PANEL_ON", [(7.6, 58.0), (12.6, 58.0), (16.5, 54.1), (17.5, 54.1)], 0.25, True, "B.Cu"),
    ("/Halo/PANEL_ON", [(17.5, 54.1), (18.0, 53.65), (19.06, 53.65)], 0.25, False),
    # halo TVS anode: GND via beside the pad, inside the VHALO zone (the fill flows around it)
    ("GND", [(117.5, 66.6), (117.5, 68.4)], 0.5, True),
    # buffer output escapes to B.Cu before the PI_TXD link closes the pocket around U603 pin 4
    ("/Halo/UART_RX", [(17.64, 50.45), (18.6, 50.9)], 0.25, True),
]


def cell_links(board):
    done = set()
    for n, (cx, orient) in list(CELLS.items()) + [(5, AUX_CELL)]:
        for (kind, idx), pad, path in CELL_GND_LINKS:
            ref = f"{kind}{n}{idx:02d}"
            pts = [cell_xf(cx, orient, x, y)[:2] for x, y in path]
            add_track(board, "GND", "F.Cu", pts, 0.3)
            add_via(board, "GND", *pts[-1])
            done.add((ref, pad))
    done |= LINKED_PADS
    for net, path, width, via, *layer in GLOBAL_LINKS:
        add_track(board, net, layer[0] if layer else "F.Cu", path, width)
        if via:
            add_via(board, net, *path[-1])
    return done


def module_links(board):
    done = cell_links(board)
    for fp in board.GetFootprints():
        fpid = fp.GetFPIDAsString()
        if fpid.endswith("TI_RDF0022A_B3QFN-22_6.5x7.5mm"):
            links = MODULE_LINKS
        elif fpid.endswith("TI_RPW0010A_VQFN-HR-10_2x2mm"):
            links = [(pin, path, "F.Cu", 0.2, False) for pin, path in EFUSE_LINKS]
        else:
            continue
        xf = _fp_xf(fp)
        nets = {p.GetNumber(): p.GetNetname() for p in fp.Pads()}
        for pin, path, layer, width, via in links:
            pts = [xf(x, y) for x, y in path]
            add_track(board, nets[pin], layer, pts, width)
            if via and nets[pin] not in F_CU_ONLY_NETS:
                add_via(board, nets[pin], *pts[-1])
            done.add((fp.GetReference(), pin))
    return done


def in_same_net_zone(net, bbox):
    """True when the pad overlaps a same-net F.Cu pre-route zone (the fill then connects it)."""
    x1, y1, x2, y2 = bbox
    return any(n == net and layer == "F.Cu" and r[0] < x2 and x1 < r[2] and r[1] < y2 and y1 < r[3]
               for n, layer, r in ZONE_RECTS)


def fanout(board):
    done = module_links(board)
    obs = Obstacles(board)
    made, failed = 0, []
    pads = []
    for fp in board.GetFootprints():
        fc = fp.GetPosition()
        fcx, fcy = pcbnew.ToMM(fc.x) - OX, pcbnew.ToMM(fc.y) - OY
        for pad in fp.Pads():
            net = pad.GetNetname()
            if net not in FANOUT_NETS or pad.GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
                continue
            if (fp.GetReference(), pad.GetNumber()) in done:
                continue
            p = pad.GetPosition()
            x, y = pcbnew.ToMM(p.x) - OX, pcbnew.ToMM(p.y) - OY
            bb = pad.GetBoundingBox()
            if in_same_net_zone(net, (pcbnew.ToMM(bb.GetX()) - OX, pcbnew.ToMM(bb.GetY()) - OY,
                                      pcbnew.ToMM(bb.GetRight()) - OX, pcbnew.ToMM(bb.GetBottom()) - OY)):
                continue
            hw = pcbnew.ToMM(bb.GetWidth()) / 2
            hh = pcbnew.ToMM(bb.GetHeight()) / 2
            pads.append((fp.GetReference(), pad.GetNumber(), net, x, y, hw, hh, fcx, fcy))
    # several pads of one footprint on the same net share one via when they are close
    for ref, num, net, x, y, hw, hh, fcx, fcy in pads:
        best = None
        for gap in (0.25, 0.45, 0.7, 1.0, 1.4, 1.9, 2.5, 3.2):
            cands = []
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (0.7071, 0.7071), (-0.7071, 0.7071),
                           (0.7071, -0.7071), (-0.7071, -0.7071)):
                ext = abs(dx) * hw + abs(dy) * hh
                d = ext + gap + VIA_R
                vx, vy = x + dx * d, y + dy * d
                away = (vx - fcx) * dx + (vy - fcy) * dy
                cands.append((-(away > 0), gap, vx, vy, dx, dy, ext))
            cands.sort()
            for _, g, vx, vy, dx, dy, ext in cands:
                if not obs.via_ok(net, vx, vy):
                    continue
                a = (x, y)          # own pad is same-net, so the whole segment can be checked
                w = GND_W if net == "GND" else TRACK_W
                if not obs.seg_ok(net, a, (vx, vy), w / 2, (ref, num)):
                    w = TRACK_W - 0.05
                    if not obs.seg_ok(net, a, (vx, vy), w / 2, (ref, num)):
                        continue
                best = (vx, vy, a, w)
                break
            if best:
                break
        if not best:
            failed.append(f"{ref}.{num}")
            continue
        vx, vy, a, w = best
        add_track(board, net, "F.Cu", [(x, y), (vx, vy)], w)
        add_via(board, net, vx, vy)
        obs.circles.append((net, vx, vy, VIA_R))
        obs.segs.append((net, "F.Cu", (x, y), (vx, vy), w / 2))
        made += 1
    return made, failed


# --------------------------------------------------------------------------
# Routing (Freerouting 1.9.0) and finishing
# --------------------------------------------------------------------------

SCRATCH = Path(os.environ.get("LEDPANEL_SCRATCH", str(BOARD_DIR / ".route")))
FREEROUTING = Path(os.environ.get("FREEROUTING_JAR", str(HERE / "freerouting-1.9.0.jar")))
ROUTED = SCRATCH / "routed.kicad_pcb"


def _dsn(x, y):
    return f"{(x + OX) * 1000:.1f} {-(y + OY) * 1000:.1f}"


def zone_strips(rect, pitch=0.8):
    """Cover a rectangle with parallel fixed path wires (Freerouting 1.9 does not treat polygon
    wiring or planes as obstacles, but it does respect fixed path wires)."""
    x1, y1, x2, y2 = rect
    horiz = (x2 - x1) >= (y2 - y1)
    span = (y2 - y1) if horiz else (x2 - x1)
    n = max(1, math.ceil(span / pitch))
    w = span / n
    out = []
    for i in range(n):
        if horiz:
            c = y1 + w * (i + 0.5)
            a, b = (x1 + w / 2, c), (max(x2 - w / 2, x1 + w / 2), c)
        else:
            c = x1 + w * (i + 0.5)
            a, b = (c, y1 + w / 2), (c, max(y2 - w / 2, y1 + w / 2))
        out.append((w, a, b))
    return out


def dsn_postprocess(src, dst):
    """Zones -> fixed path wires on their outer layer (obstacles for other nets, connection
    targets for their own net).  Planes are removed; the router never uses In1/In2."""
    text = Path(src).read_text()
    text = re.sub(r"\s*\(plane \S+ \(polygon \S+ 0 [^)]*\)\)", "", text)
    wires = []
    for net, layer, group, _ in outer_zone_specs():
        q = f'"{net}"' if any(ch in net for ch in "() ") else net
        for w, a, b in (s for rect in group for s in zone_strips(rect)):
            wires.append(f"    (wire (path {layer} {w * 1000:.1f} {_dsn(*a)} {_dsn(*b)}) (net {q}) (type fix))")
    if "(wiring" in text:
        text = text.replace("(wiring", "(wiring\n" + "\n".join(wires), 1)
    else:
        idx = text.rstrip().rfind(")")
        text = text[:idx] + "  (wiring\n" + "\n".join(wires) + "\n  )\n)\n"
    Path(dst).write_text(text)
    return len(wires)


def unconnected(board):
    board.BuildConnectivity()
    return board.GetConnectivity().GetUnconnectedCount(False)


def route(passes=20, attempts=4):
    SCRATCH.mkdir(parents=True, exist_ok=True)
    board = pcbnew.LoadBoard(str(PCB))
    raw, dsn, ses = SCRATCH / "raw.dsn", SCRATCH / "board.dsn", SCRATCH / "board.ses"
    pcbnew.ExportSpecctraDSN(board, str(raw))
    n = dsn_postprocess(raw, dsn)
    print(f"DSN: zones passed as {n} fixed path wires")
    # Freerouting is not deterministic between runs: retry until every connection is routed
    best = None
    for attempt in range(1, attempts + 1):
        if ses.exists():
            ses.unlink()
        cmd = ["xvfb-run", "-a", "java", "-jar", str(FREEROUTING), "-de", str(dsn), "-do", str(ses),
               "-mp", str(passes + 10 * (attempt - 1)), "-mt", "1", "-da", "-dct", "0",
               "-inc", "GND,Power5AUX"]
        with open(SCRATCH / "freerouting.log", "w") as log:
            subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, timeout=1500, check=False, cwd=SCRATCH)
        if not ses.exists():
            raise SystemExit("Freerouting produced no session file; see freerouting.log")
        trial = pcbnew.LoadBoard(str(PCB))
        ok = pcbnew.ImportSpecctraSES(trial, str(ses))
        trial_fill = pcbnew.LoadBoard(str(PCB))       # connectivity needs filled zones
        ok = pcbnew.ImportSpecctraSES(trial_fill, str(ses)) and ok
        pcbnew.ZONE_FILLER(trial_fill).Fill(trial_fill.Zones())
        left = unconnected(trial_fill)
        print(f"route attempt {attempt}: session imported {ok}, {left} unrouted")
        if best is None or left < best[0]:
            best = (left, trial)
        if left == 0:
            break
    board = best[1]
    pcbnew.SaveBoard(str(PCB), board)
    pcbnew.SaveBoard(str(ROUTED), board)     # finish() starts from this, so it can be re-run


# --------------------------------------------------------------------------
# Finishing: ground pours, stitching, silkscreen, stackup, fill
# --------------------------------------------------------------------------

STACKUP = """		(stackup
			(layer "F.SilkS" (type "Top Silk Screen"))
			(layer "F.Paste" (type "Top Solder Paste"))
			(layer "F.Mask" (type "Top Solder Mask") (thickness 0.01))
			(layer "F.Cu" (type "copper") (thickness 0.07))
			(layer "dielectric 1" (type "prepreg") (thickness 0.2) (material "FR4") (epsilon_r 4.4) (loss_tangent 0.02))
			(layer "In1.Cu" (type "copper") (thickness 0.035))
			(layer "dielectric 2" (type "core") (thickness 0.99) (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))
			(layer "In2.Cu" (type "copper") (thickness 0.035))
			(layer "dielectric 3" (type "prepreg") (thickness 0.2) (material "FR4") (epsilon_r 4.4) (loss_tangent 0.02))
			(layer "B.Cu" (type "copper") (thickness 0.07))
			(layer "B.Mask" (type "Bottom Solder Mask") (thickness 0.01))
			(layer "B.Paste" (type "Bottom Solder Paste"))
			(layer "B.SilkS" (type "Bottom Silk Screen"))
			(copper_finish "ENIG")
			(dielectric_constraints no)
		)
"""

# (ref, offset direction, {pin: label}, connector caption)
CONNECTOR_LABELS = [
    ("J1", (0, -1), {"1": "+24V", "3": "GND"}, "24V IN"),
    ("J101", (0, 1), {"1": "+5V", "2": "+5V", "3": "GND", "4": "GND"}, None),
    ("J201", (0, 1), {"1": "+5V", "2": "+5V", "3": "GND", "4": "GND"}, None),
    ("J301", (0, -1), {"1": "+5V", "2": "+5V", "3": "GND", "4": "GND"}, None),
    ("J401", (0, -1), {"1": "+5V", "2": "+5V", "3": "GND", "4": "GND"}, None),
    ("J501", (0, 1), {"1": "+5V", "2": "GND"}, None),
    ("J601", (-1, 0), {"1": "+", "2": "D", "3": "-"}, None),          # +5V / DIN / GND
    ("J603", (1, 0), {"1": "-", "2": "TX", "3": "26"}, None),         # GND / TXD / GPIO26
    ("J602", (0, 1), {"1": "UPDI", "2": "+5V", "3": "GND"}, None),
    ("JP601", (0, -1), {"1": "PI", "3": "ON"}, None),
]
REF_POS = {"J1": (114.0, 33.6), "J601": (127.0, 69.6)}
LED_LABELS = {"D101": "FLT1", "D201": "FLT2", "D301": "FLT3", "D401": "FLT4", "D501": "FLT PI",
              "D601": "FLT HALO", "D2": "24V", "D531": "5V AUX", "D602": "STAT"}
BOARD_TEXT = [
    (17.0, 75.6, 1.0, "LED PANEL POWER + HALO  REV A"),
    (17.0, 77.4, 0.9, "PROTOTYPE - UNVALIDATED"),
    (17.0, 79.0, 0.8, "24V/160W IN - 4x5V/5A PANEL - 5V PI - 5V HALO"),
]


def add_text(board, text, x, y, size=1.0, layer=pcbnew.F_SilkS, just=None):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(P(x, y))
    t.SetLayer(layer)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(max(0.12, size * 0.15)))
    if just:
        t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT if just == "left" else pcbnew.GR_TEXT_H_ALIGN_RIGHT)
    board.Add(t)
    return t


class SilkPlanner:
    """Places labels at the first candidate spot whose text box clears every pad (mask opening),
    the board edge and the labels placed before it."""

    def __init__(self, board):
        self.board = board
        self.blocked = []
        for fp in board.GetFootprints():
            for g in fp.GraphicalItems():
                if g.GetLayer() == pcbnew.F_SilkS and g.GetClass() != "PCB_FIELD":
                    bb = g.GetBoundingBox()
                    self.blocked.append((pcbnew.ToMM(bb.GetX()) - OX - 0.15, pcbnew.ToMM(bb.GetY()) - OY - 0.15,
                                         pcbnew.ToMM(bb.GetRight()) - OX + 0.15,
                                         pcbnew.ToMM(bb.GetBottom()) - OY + 0.15))
            for pad in fp.Pads():
                bb = pad.GetBoundingBox()
                self.blocked.append((pcbnew.ToMM(bb.GetX()) - OX - 0.2, pcbnew.ToMM(bb.GetY()) - OY - 0.2,
                                     pcbnew.ToMM(bb.GetRight()) - OX + 0.2, pcbnew.ToMM(bb.GetBottom()) - OY + 0.2))

    @staticmethod
    def box(text, x, y, size, just):
        w = 0.8 * size * len(text) + 0.2
        h = size + 0.3
        x1 = x - w / 2 if just is None else (x if just == "left" else x - w)
        return (x1, y - h / 2, x1 + w, y + h / 2)

    def free(self, r):
        if r[0] < 0.4 or r[1] < 0.4 or r[2] > W - 0.4 or r[3] > H - 0.4:
            return False
        return not any(r[0] < q[2] and q[0] < r[2] and r[1] < q[3] and q[1] < r[3] for q in self.blocked)

    def place(self, text, cands, size, just=None, radius=2.5):
        """Try the candidates, then a spiral (0.25 mm steps) around the first one."""
        x0, y0 = cands[0]
        spiral = []
        r = 0.25
        while r <= radius:
            n = max(8, int(2 * math.pi * r / 0.25))
            spiral += [(x0 + r * math.cos(2 * math.pi * k / n), y0 + r * math.sin(2 * math.pi * k / n))
                       for k in range(n)]
            r += 0.25
        for x, y in list(cands) + spiral:
            b = self.box(text, x, y, size, just)
            if self.free(b):
                self.blocked.append(b)
                return add_text(self.board, text, round(x, 2), round(y, 2), size, just=just)
        print(f"silk: no free spot for {text!r} near {cands[0]}")
        return None


def silkscreen(board):
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        if re.match(r"(J|TP)\d", ref):
            if ref in REF_POS:
                fp.Reference().SetPosition(P(*REF_POS[ref]))
                fp.Reference().SetTextAngleDegrees(0)
        else:
            fp.Reference().SetLayer(pcbnew.F_Fab)
    sp = SilkPlanner(board)
    for fp in board.GetFootprints():          # kept references block their own area
        if fp.Reference().GetLayer() == pcbnew.F_SilkS:
            bb = fp.Reference().GetBoundingBox()
            sp.blocked.append((pcbnew.ToMM(bb.GetX()) - OX, pcbnew.ToMM(bb.GetY()) - OY,
                               pcbnew.ToMM(bb.GetRight()) - OX, pcbnew.ToMM(bb.GetBottom()) - OY))
    for ref, (dx, dy), pins, caption in CONNECTOR_LABELS:
        fp = board.FindFootprintByReference(ref)
        pads = {p.GetNumber(): p for p in fp.Pads()}
        size = 0.8 if ref in ("J603", "J602", "JP601") else 1.0
        far = 0.0
        for num, label in pins.items():
            pad = pads[num]
            c = pad.GetPosition()
            bb = pad.GetBoundingBox()
            half = pcbnew.ToMM(bb.GetHeight() if dy else bb.GetWidth()) / 2
            x, y = pcbnew.ToMM(c.x) - OX, pcbnew.ToMM(c.y) - OY
            steps = [0.0, 0.5, 1.0, 1.5, 2.0, 3.0]
            if dx:
                sp.place(label, [(x + dx * (half + 0.45 + k), y) for k in steps], size,
                         just="left" if dx > 0 else "right")
            else:
                off = half + 0.45 + size / 2
                sp.place(label, [(x, y + dy * (off + k)) for k in steps], size)
                far = max(far, off)
        if caption:
            xs = [pcbnew.ToMM(pads[n].GetPosition().x) - OX for n in pins]
            ys = [pcbnew.ToMM(pads[n].GetPosition().y) - OY for n in pins]
            if dx:
                cands = [(min(xs) - 4.5 - k, max(ys) + 2.6) for k in (0, 1, 2)]
            else:
                cands = [(sum(xs) / len(xs), ys[0] + dy * (far + 1.4 + k)) for k in (0, 0.6, 1.2, 2.0)]
            sp.place(caption, cands, 1.0, radius=6.0)
    for ref, label in LED_LABELS.items():
        fp = board.FindFootprintByReference(ref)
        bb = fp.GetCourtyard(pcbnew.F_CrtYd).BBox()
        x1, y1 = pcbnew.ToMM(bb.GetX()) - OX, pcbnew.ToMM(bb.GetY()) - OY
        x2, y2 = pcbnew.ToMM(bb.GetRight()) - OX, pcbnew.ToMM(bb.GetBottom()) - OY
        cx, cy = (x1 + x2) / 2, (y1 + y2) / 2
        cands = [(cx, y2 + 0.6), (cx, y1 - 0.6), (cx, y2 + 1.4), (cx, y1 - 1.4)]
        if not sp.place(label, cands, 0.8):
            sp.place(label, [(x2 + 0.2, cy), (x1 - 0.2, cy)][:1], 0.8, just="left")
    for x, y, size, text in BOARD_TEXT:
        sp.place(text, [(x, y)], size, radius=4.0)


def stitching(board, pitch=4.0):
    """GND vias tying the B.Cu / In2 / F.Cu ground pours to the In1 plane, outside the In2 +5V
    region, component courtyards and every other-net object."""
    obs = Obstacles(board)
    yards = []
    for fp in board.GetFootprints():
        bb = fp.GetCourtyard(pcbnew.F_CrtYd).BBox()
        yards.append((pcbnew.ToMM(bb.GetX()) - OX, pcbnew.ToMM(bb.GetY()) - OY,
                      pcbnew.ToMM(bb.GetRight()) - OX, pcbnew.ToMM(bb.GetBottom()) - OY))
    n = 0
    y = 2.0
    while y < H - 1.5:
        x = 2.0
        while x < W - 1.5:
            ok = not any(r[0] - 0.8 <= x <= r[2] + 0.8 and r[1] - 0.8 <= y <= r[3] + 0.8 for r in IN2_AUX)
            ok = ok and not any(r[0] - 0.3 <= x <= r[2] + 0.3 and r[1] - 0.3 <= y <= r[3] + 0.3 for r in yards)
            if ok and obs.via_ok("GND", x, y):
                add_via(board, "GND", x, y)
                obs.circles.append(("GND", x, y, VIA_R))
                n += 1
            x += pitch
        y += pitch
    return n


def finish():
    if not ROUTED.exists():
        raise SystemExit(f"{ROUTED} missing: run the route stage first")
    board = pcbnew.LoadBoard(str(ROUTED))
    for net, layer, group, _ in outer_zone_specs():
        for r in group:
            ZONE_RECTS.append((net, layer, r))
    full = [(0.3, 0.3, W - 0.3, H - 0.3)]
    # low-priority ground pours fill whatever the power zones and tracks leave free
    for layer in ("F.Cu", "B.Cu", "In2.Cu"):
        z = add_zone(board, "GND", layer, full, priority=0, connect="thermal", name=f"GND-pour-{layer}")
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)
    print("stitching vias:", stitching(board))
    silkscreen(board)
    tb = board.GetTitleBlock()
    tb.SetTitle("LED panel power + halo board")
    tb.SetRevision("A (prototype, unvalidated)")
    tb.SetDate("2026-11-15")
    tb.SetCompany("sshcrack/led-matrix")
    tb.SetComment(0, "4 layers, 1.6 mm, 2 oz outer / 1 oz inner, ENIG")
    tb.SetComment(1, "Generated by hardware/electronics/tools/gen_pcb.py - not approved for fabrication")
    # Zones are stored unfilled: the filled board is ~1.9 MB against the repo's 2 MiB per-file
    # guard.  KiCad refills on DRC/plot; tools/check_board.py fills a copy before DRC and exports.
    for z in board.Zones():
        z.UnFill()
    pcbnew.SaveBoard(str(PCB), board)
    text = PCB.read_text()
    if "(stackup" not in text:
        text = text.replace("\t(setup\n", "\t(setup\n" + STACKUP, 1)
        PCB.write_text(text)
    print("finished:", PCB)


if __name__ == "__main__":
    stage = sys.argv[1]
    if stage == "place":
        b = place(sys.argv[2])
        b = pcbnew.LoadBoard(str(PCB))
        for a, bb, area in courtyard_check(b):
            print(f"COURTYARD {a} x {bb}: {area:.2f} mm2")
        print("pads near/outside edge:", outside_board(b))
    elif stage == "route":
        route()
    elif stage == "finish":
        finish()
