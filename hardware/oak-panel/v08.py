"""v0.8 rear: one plywood back on 16 printed spacer tubes, five buck boards taped under it, a
printed pod over the Pi. Replaces the v0.7 pine box (95 mm deep) and the printed carriers.

Millimetres, same ASSEMBLY FRAME as v07.py (X right and Y up seen from the wall, Z from the LED
tips toward the wall, quadrant (sx, sy) centred at (sx, sy) * PITCH / 2).

Stack along Z:
  0      LED tips
  15     insert faces / panel frame top; spacer tubes 15-34 on all 16 inserts
  16-34  room under the plywood: buck boards, fuse, WAGOs, ribbons and wires (panel plugs to 23,
         power cable bend to 32)
  34-40  plywood back (one 383 mm square sheet, 6 mm), M3 x 30 through it into the inserts
  40-58  Pi pod; the Pi hangs upside down from its roof through a window in the plywood
  40-52  halo pieces on the plywood edge, strip tilted 20 deg outward
  40-~56 hanging rails on the two existing wall screws (#171), bumpers 40-62
  62     wall

Assembly: halo, rails, bumpers and pod are screwed to the plywood from its inner face (M3 x 12 /
M3 x 20 into printed pilots) and the electronics are taped and tied to the inner face, all on the
bench. Then the panels lie face down, pushed together and strapped so the seams close; the tubes
go on the inserts, the plywood on top, and 16 M3 x 30 go in from the back. The 6 mm holes give
+-1.3 mm, so the plywood follows the panels, not the other way round. Eight of the screws pass
through the halo pieces (access bores under the strip), so the strip goes in last.

Open, not designed here: front frame retention (waits for the frame and diffuser choice).
Estimates to confirm before printing are listed in ESTIMATES.
"""

import math

import cadquery as cq

import electronics_plates as ep
import v07
from v07 import M3_CLEAR, M3_PILOT, M25_PILOT, PATTERN, PITCH, box, cyl, cyl_x, cyl_y, quadrant_centre

QUADRANTS = [(-1, 1), (1, 1), (-1, -1), (1, -1)]
QNAME = {(-1, 1): "top-left", (1, 1): "top-right", (-1, -1): "bottom-left", (1, -1): "bottom-right"}

# --- Z stack ---------------------------------------------------------------------------------
Z_INSERT = v07.Z_INSERT  # 15
E = 191.5  # plywood half size: 383 mm square, 0.3 mm inside the panel block edges
TUBE = (9.0, 19.0)  # spacer tube outer diameter, length (M3 clearance bore)
Z_CEIL = Z_INSERT + TUBE[1]  # 34: plywood inner face, ceiling for everything under it
PLY_T = 6.0  # poplar plywood
Z_PLY = Z_CEIL + PLY_T  # 40: plywood rear face
WALL_GAP = 22.0  # plywood to wall; set by the rails and bumpers, the wall screws are adjustable
Z_WALL = Z_PLY + WALL_GAP  # 62

ESTIMATES = {
    "buck_board_mm": "Hailege H-1-1543 (amazon.de B07XFMMY1F) listing: 63 x 27 x 10; envelope 66 x 27 x 10 for the "
                     "jack/USB overhang; screw terminals at both short ends. Measure the real boards.",
    "pi_stack_mm": "27 from the Pi underside to the highest wired fold (owner, rough)",
    "dc_plug_mm": "16 diameter, 40 long envelope for the brick's 4-pin plug; not measured",
    "pro_micro_mm": "33 x 18, 23 tall with the Dupont plugs on top",
    "panel_insert_thread_mm": "4 mm of thread assumed usable in the panel inserts (same as the v0.7 carriers)",
}

# --- Panel screws: M3 x 30 through the plywood and a tube into each insert ---------------------
PLY_HOLE = 6.0  # +-1.3 mm play: the panels set the positions
WASHER = (9.0, 0.8)  # DIN 9021 3.2 x 9
PANEL_SCREW_L = 30.0
M3_BUTTON = (5.7, 1.7)  # ISO 7380 head
M3_PAN = (5.6, 3.0)  # pan / cylinder head of the M3 x 20 the owner has


def inserts():
    """[(x, y)] global: the 16 panel inserts (the 174 x 158 corner pattern of each panel)."""
    out = []
    for sx, sy in QUADRANTS:
        cx, cy = quadrant_centre(sx, sy)
        out += [(cx + x, cy + y) for x, y in PATTERN]
    return out


def build_tube():
    return cyl(0, 0, TUBE[0], 0, TUBE[1]).cut(cyl(0, 0, M3_CLEAR, 0, TUBE[1]))


# --- Electronics on the plywood's inner face (foam tape + cable ties) -------------------------
BOARD = (66.0, 27.0, 10.0)  # envelope, long axis along X; terminals at both short ends (ESTIMATES)
BOARD_BEND = 20.0  # wire room beyond each short end: straight run + bend toward the corridors
FOAM_T = 1.0
WAGO_5 = v07.WAGO_5  # (30.1 across the 5 levers, 21.1 along the wires, 8.4)
WAGO_ENTRY = 20.0  # wires straight into the clamps
WAGO_LEVER = 12.0  # levers swing toward the panels: wire them with the back off the panels
FUSE_BODY = v07.FUSE_BODY  # measured inline holder on its side (34.8 wide, 46 long, 14 high)
FUSE_LEAD = (12.0 + 30.0, 15.0)  # length beyond each end (12 straight + 30 for the 90 deg bend), width
TIE_HOLE = 4.0
# name: (quadrant, x, y) quadrant-local centre, long axis along X.
BOARDS = {
    "panel top-right": ((1, 1), 0.0, -76.0),
    "panel top-left": ((1, 1), 0.0, -40.0),
    "pi + halo": ((1, 1), 0.0, -4.0),
    "panel bottom-left": ((-1, -1), 0.0, -20.0),
    "panel bottom-right": ((1, -1), 0.0, -20.0),
}
FUSE_AT = ((-1, -1), 8.0, -62.0)  # leads along X
# name: (quadrant, x, y, entry direction (dx, dy)); body turned so the wires come in along X.
WAGOS = {
    "+24 V": ((1, -1), -35.0, -58.0, (-1, 0)),
    "24 V return": ((1, -1), 8.0, -58.0, (-1, 0)),
}


def _qbox(quad, x0, x1, y0, y1, z0, z1):
    cx, cy = quadrant_centre(*quad)
    return box(cx + x0, cx + x1, cy + y0, cy + y1, z0, z1)


def board_rects():
    """{name: (x0, x1, y0, y1) global} of the board envelopes."""
    w, d, _ = BOARD
    out = {}
    for name, (quad, x, y) in BOARDS.items():
        cx, cy = quadrant_centre(*quad)
        out[name] = (cx + x - w / 2, cx + x + w / 2, cy + y - d / 2, cy + y + d / 2)
    return out


def wago_rect(quad, x, y, entry):
    ww, wd, _ = WAGO_5
    a, b = (wd, ww) if entry[0] else (ww, wd)  # along X, along Y
    cx, cy = quadrant_centre(*quad)
    return cx + x - a / 2, cx + x + a / 2, cy + y - b / 2, cy + y + b / 2


def fuse_rect():
    quad, x, y = FUSE_AT
    fw, fl, _ = FUSE_BODY
    cx, cy = quadrant_centre(*quad)
    return cx + x - fl / 2, cx + x + fl / 2, cy + y - fw / 2, cy + y + fw / 2


def component_envelopes():
    """[(name, kind, shape)] global, assembly Z: parts on the plywood's inner face."""
    out = []
    zf = Z_CEIL - FOAM_T
    for name, (x0, x1, y0, y1) in board_rects().items():
        out.append((f"buck board {name}", "board", box(x0, x1, y0, y1, zf - BOARD[2], zf)))
    for name, (quad, x, y, entry) in WAGOS.items():
        x0, x1, y0, y1 = wago_rect(quad, x, y, entry)
        out.append((f"WAGO {name}", "wago", box(x0, x1, y0, y1, zf - WAGO_5[2], zf)))
    x0, x1, y0, y1 = fuse_rect()
    out.append(("input fuse 7.5 A", "fuse", box(x0, x1, y0, y1, zf - FUSE_BODY[2], zf)))
    return out


def cable_zones():
    """[(name, kind, shape)]: room the wires need where they leave a part ('zone'), lever swing
    ('lever', only checked against parts on the plywood) and the paths between quadrants
    ('corridor')."""
    out = []
    zf = Z_CEIL - FOAM_T
    for name, (x0, x1, y0, y1) in board_rects().items():
        z0 = zf - BOARD[2]
        out.append((f"board wires in {name}", "zone", box(x0 - BOARD_BEND, x0, y0, y1, z0, Z_CEIL)))
        out.append((f"board wires out {name}", "zone", box(x1, x1 + BOARD_BEND, y0, y1, z0, Z_CEIL)))
    for name, (quad, x, y, entry) in WAGOS.items():
        x0, x1, y0, y1 = wago_rect(quad, x, y, entry)
        z0 = zf - WAGO_5[2]
        dx, dy = entry
        if dx:
            ex0, ex1 = (x0 - WAGO_ENTRY, x0) if dx < 0 else (x1, x1 + WAGO_ENTRY)
            out.append((f"WAGO wires {name}", "zone", box(ex0, ex1, y0, y1, z0, Z_CEIL)))
        else:
            ey0, ey1 = (y0 - WAGO_ENTRY, y0) if dy < 0 else (y1, y1 + WAGO_ENTRY)
            out.append((f"WAGO wires {name}", "zone", box(x0, x1, ey0, ey1, z0, Z_CEIL)))
        out.append((f"WAGO levers {name}", "lever", box(x0, x1, y0, y1, z0 - WAGO_LEVER, z0)))
    x0, x1, y0, y1 = fuse_rect()
    ym = (y0 + y1) / 2
    ll, lw = FUSE_LEAD
    z0 = zf - FUSE_BODY[2]
    out.append(("fuse lead in", "zone", box(x0 - ll, x0, ym - lw / 2, ym + lw / 2, z0, Z_CEIL)))
    out.append(("fuse lead out", "zone", box(x1, x1 + ll, ym - lw / 2, ym + lw / 2, z0, Z_CEIL)))
    # DC socket pigtail inside the pod: solder cups + 25 mm before the wires turn down
    sx, sz = dc_socket_x(), DC_SOCKET_Z
    y = POD["y0"] + ep.DC_SOCKET["body_cutout"]
    out.append(("DC socket pigtail", "zone", cyl_y(sx, sz, 14.0, y, y + 25.0)))
    # Paths between quadrants under the plywood (16 mm up from the panel frames)
    zc0 = Z_INSERT + 1.0
    out.append(("corridor horizontal seam", "corridor", box(-E, E, -6.0, 6.0, zc0, Z_CEIL)))
    out.append(("corridor vertical seam", "corridor", box(-4.0, 4.0, -E, E, zc0, Z_CEIL)))
    cin, cout = ep.PANEL_CONNECTORS["hub75_in"], ep.PANEL_CONNECTORS["hub75_out"]
    for sy, n in ((1, "top"), (-1, "bottom")):
        lx, ly = quadrant_centre(-1, sy)
        rx, _ = quadrant_centre(1, sy)
        out.append((f"ribbon {n} left->right", "corridor",
                    box(lx + cout[1], rx + cin[0], ly + cin[2], ly + cin[3], zc0, Z_CEIL)))
    wx0, wx1, wy0, wy1 = pod_window()
    lx, ty = quadrant_centre(-1, 1)
    _, by = quadrant_centre(-1, -1)
    rx0, rx1 = lx + cin[0], lx + cin[0] + 22.0
    out.append(("ribbon Pi->top-left", "corridor", box(rx0, rx1, wy1, ty + cin[2], zc0, Z_CEIL)))
    out.append(("ribbon Pi->bottom-left", "corridor", box(rx0, rx1, by + cin[3], wy0, zc0, Z_CEIL)))
    return out


def tie_holes():
    """[(x, y)] global: pairs of tie holes across each board and the fuse."""
    out = []
    for x0, x1, y0, y1 in list(board_rects().values()) + [fuse_rect()]:
        xm = (x0 + x1) / 2
        for dx in (-(x1 - x0) / 4, (x1 - x0) / 4):
            out += [(xm + dx, y0 - 3.0), (xm + dx, y1 + 3.0)]
    merged = []  # stacked boards share the hole between them
    for x, y in out:
        near = [i for i, (mx, my) in enumerate(merged) if abs(mx - x) < 1 and abs(my - y) < 6.0]
        if near:
            mx, my = merged[near[0]]
            merged[near[0]] = (mx, (my + y) / 2)
        else:
            merged.append((x, y))
    return merged


def panel_connector_envelopes():
    """HUB75 and power plugs of every panel, from the panel frame top up to their measured tops."""
    out = []
    for quad in QUADRANTS:
        cx, cy = quadrant_centre(*quad)
        for name, (x0, x1, y0, y1) in ep.PANEL_CONNECTORS.items():
            out.append((f"{QNAME[quad]} {name}", box(cx + x0, cx + x1, cy + y0, cy + y1, Z_INSERT, ep.PLUG_TOP[name])))
    return out


# --- Pi pod: printed box on the plywood, Pi hangs upside down from its roof ---------------------
PI_STACK_H = 27.0  # Pi underside to the highest wired fold (ESTIMATES)
PI_STANDOFF = (6.0, 6.5)  # M2.5 x 10 through the 1.6 mm board needs 8.4 mm: 6.5 + 2 mm roof
PRO_MICRO = (33.0, 18.0, 23.0)
POD_WALL, POD_LEDGE, POD_ROOF, POD_BOSS_H = 2.5, 8.0, 2.0, 7.0
POD_TOP = 58.0
# Pi board (85 x 56, hole pattern 10 mm toward -X of the board centre... see ep.PI_HOLES) and Pro
# Micro, global rectangles. Both reach through the plywood window down to >= 7 mm above the panels.
PI_BOARD = (-158.0, -73.0, 30.0, 86.0)
PRO_MICRO_RECT = (-64.0, -31.0, 62.0, 80.0)
WINDOW_MARGIN = 3.0
DC_SOCKET_X = -45.0
DC_SOCKET_Z = Z_PLY + 8.5  # 48.5: plug envelope clears the plywood, body clears the roof
DC_SOCKET_PAD = (34.0, 15.0)  # width along X, thickness from the outer face
DC_PLUG = (16.0, 40.0)  # envelope: diameter, length outside the socket (ESTIMATES)
HALO_EXIT = (60.0, 6.0)  # y and diameter: halo cable leaves through the pod's -X wall


def pod_window():
    """Global (x0, x1, y0, y1) of the plywood window under the pod (Pi + Pro Micro + margin)."""
    m = WINDOW_MARGIN
    return (PI_BOARD[0] - m, PRO_MICRO_RECT[1] + m, PI_BOARD[2] - m, PI_BOARD[3] + m)


def pod_outline():
    x0, x1, y0, y1 = pod_window()
    k = POD_WALL + POD_LEDGE
    return x0 - k, x1 + k, y0 - k, y1 + k


POD = dict(zip(("x0", "x1", "y0", "y1"), pod_outline()))


def pod_screws():
    x0, x1, y0, y1 = POD["x0"], POD["x1"], POD["y0"], POD["y1"]
    c = POD_WALL + POD_LEDGE / 2
    xs = (x0 + c, (x0 + x1) / 2 - 20.0, x1 - c)
    return [(x, y) for x in xs for y in (y0 + c, y1 - c)]


def roof_inner():
    return POD_TOP - POD_ROOF


def pi_board_z():
    """(underside facing the roof, z of the stack's lowest point)."""
    z = roof_inner() - PI_STANDOFF[1]
    return z, z - PI_STACK_H


def pi_holes():
    """Global M2.5 holes; the pattern sits 10 mm toward +X of the board centre (SD side +X)."""
    x0, x1, y0, y1 = PI_BOARD
    cx, cy = (x0 + x1) / 2 + 10.0, (y0 + y1) / 2
    return [(cx + hx, cy + hy) for hx, hy in ep.PI_HOLES]


def pod_envelopes():
    zb, zl = pi_board_z()
    x0, x1, y0, y1 = PI_BOARD
    out = [("Pi 3B + Electrodragon (27 mm wired)", "pi", box(x0, x1, y0, y1, zl, zb))]
    x0, x1, y0, y1 = PRO_MICRO_RECT
    z = roof_inner() - FOAM_T
    out.append(("Pro Micro with Dupont plugs", "pro_micro", box(x0, x1, y0, y1, z - PRO_MICRO[2], z)))
    return out


def dc_socket_x():
    return DC_SOCKET_X


def build_pod():
    x0, x1, y0, y1, w = POD["x0"], POD["x1"], POD["y0"], POD["y1"], POD_WALL
    top = POD_TOP
    pod = box(x0, x1, y0, y1, Z_PLY, top)
    pod = pod.cut(box(x0 + w, x1 - w, y0 + w, y1 - w, Z_PLY, top - POD_ROOF))
    wx0, wx1, wy0, wy1 = pod_window()
    ring = box(x0 + w, x1 - w, y0 + w, y1 - w, Z_PLY, Z_PLY + POD_BOSS_H)
    pod = pod.union(ring.cut(box(wx0, wx1, wy0, wy1, Z_PLY, Z_PLY + POD_BOSS_H)))
    # 45 deg slope above the ledge: the pod prints roof-down, so the ledge needs no support
    z = Z_PLY + POD_BOSS_H
    slope = box(x0 + w, x1 - w, y0 + w, y1 - w, z, z + POD_LEDGE)
    funnel = (cq.Workplane("XY", origin=(0, 0, z)).center((wx0 + wx1) / 2, (wy0 + wy1) / 2)
              .rect(wx1 - wx0, wy1 - wy0).workplane(offset=POD_LEDGE).rect(x1 - x0 - 2 * w, y1 - y0 - 2 * w).loft())
    pod = pod.union(slope.cut(funnel))
    for x, y in pod_screws():
        pod = pod.cut(cyl(x, y, M3_PILOT, Z_PLY, Z_PLY + POD_BOSS_H - 0.5))
    # Pi standoffs from the roof
    od, h = PI_STANDOFF
    for x, y in pi_holes():
        pod = pod.union(cyl(x, y, od, roof_inner() - h, roof_inner()))
        pod = pod.cut(cyl(x, y, M25_PILOT, roof_inner() - h, top))
    # DC socket pad (Kycon KPJX-PM-4S-S from outside, as in v0.7)
    sx, sz = DC_SOCKET_X, DC_SOCKET_Z
    pw, pt = DC_SOCKET_PAD
    pod = pod.union(box(sx - pw / 2, sx + pw / 2, y0, y0 + pt, Z_PLY, top - POD_ROOF))
    pod = pod.cut(cyl_y(sx, sz, ep.DC_SOCKET["body_cutout"], y0, y0 + pt))
    s = ep.DC_SOCKET["screw_spacing"] / 2
    bd, bdepth = v07.DC_SOCKET_BOSS_POCKET
    for dx in (-s, s):
        pod = pod.cut(cyl_y(sx + dx, sz, bd, y0, y0 + bdepth))
        pod = pod.cut(cyl_y(sx + dx, sz, M3_PILOT, y0, y0 + bdepth + 7.0))
    # roof vents over the Pi, halo cable exit
    px0, px1, py0, py1 = PI_BOARD
    for i in range(7):
        x = px0 + 8 + i * 11
        pod = pod.cut(box(x, x + 5, py0 + 8, py1 - 8, top - POD_ROOF, top))
    pod = pod.cut(v07.cyl_x(HALO_EXIT[0], Z_PLY + POD_BOSS_H + 4.0, HALO_EXIT[1], x0, x0 + w))
    return pod


# --- Halo pieces (printed): channel tilted 20 deg outward with a shield above the outer wall --
HALO = v07.HALO  # channel profile from the v0.7 coupon (10.7 inner width fits the strip)
HALO_W = v07.HALO_BAND_W  # 15.5
HALO_D = v07.HALO_DEPTH  # 6.1
HALO_TILT = 20.0
HALO_SHIELD = 4.5  # outer wall extended past the opening: hides the LEDs from the side
HALO_LIFT = 2.0  # outer floor corner above the plywood
HALO_SPAN = (0.25, 169.5)  # from the edge centre; the corner washers stay clear
HALO_SEAT = 1.2  # panel-screw head seat above the plywood (head under the strip)
HALO_BORE = 6.6  # access for the ISO 7380 head and a 2 mm hex key, through the channel floor
HALO_FIX = (50.0, 140.0)  # along the edge: M3 x 12 from the plywood's inner face
HALO_FIX_U = 176.5  # outward position of those pilots: thickest part under the channel


def _halo_map(s, n):
    """Channel coordinates (s across, + outward; n along the channel normal) -> (u outward, z)."""
    t = math.radians(HALO_TILT)
    uc = E - (HALO_W / 2 * math.cos(t) + (HALO_D + HALO_SHIELD) * math.sin(t))
    zc = Z_PLY + HALO_LIFT + HALO_W / 2 * math.sin(t)
    return uc + s * math.cos(t) + n * math.sin(t), zc - s * math.sin(t) + n * math.cos(t)


def _uz_prism(pts, x0, x1):
    """Prism along X from x0 to x1 with an outline in (u = y, z)."""
    return cq.Workplane("YZ").polyline(pts).close().extrude(x1 - x0).translate((x0, 0, 0))


def halo_profile_pts():
    W, D, f, ih, lip, wall = HALO_W, HALO_D, HALO["floor"], HALO["inner_h"], HALO["lip"], HALO["wall"]
    m = _halo_map
    outer = [m(-W / 2, 0), m(W / 2, 0), m(W / 2, D + HALO_SHIELD), m(W / 2 - wall, D + HALO_SHIELD),
             m(W / 2 - wall, D), m(-W / 2, D)]
    ulo, zlo = m(W / 2, 0)
    uil, zil = m(-W / 2, 0)
    utop, ztop = m(W / 2, D + HALO_SHIELD)
    filler = [(uil, Z_PLY), (E, Z_PLY), (E, ztop), (ulo, zlo), (uil, zil)]
    iw, ow = HALO["inner_w"], HALO["inner_w"] - 2 * lip
    cavity = [m(-iw / 2, f), m(iw / 2, f), m(iw / 2, f + ih), m(-iw / 2, f + ih)]
    opening = [m(-ow / 2, f + ih - 0.01), m(ow / 2, f + ih - 0.01), m(ow / 2, D + 0.01), m(-ow / 2, D + 0.01)]
    return outer, filler, cavity, opening


def build_halo_piece(bore):
    """Top-edge piece from x = HALO_SPAN[0] to HALO_SPAN[1], assembly position. `bore` is the
    (along, u) of the panel screw that passes through it."""
    x0, x1 = HALO_SPAN
    outer, filler, cavity, opening = halo_profile_pts()
    p = _uz_prism(outer, x0, x1).union(_uz_prism(filler, x0, x1))
    p = p.cut(_uz_prism(cavity, x0, x1)).cut(_uz_prism(opening, x0, x1))
    a, u = bore
    p = p.cut(cyl(a, u, M3_CLEAR, Z_PLY, Z_PLY + HALO_SEAT))
    p = p.cut(cyl(a, u, HALO_BORE, Z_PLY + HALO_SEAT, Z_PLY + 20))
    for a in HALO_FIX:
        p = p.cut(cyl(a, HALO_FIX_U, M3_PILOT, Z_PLY, Z_PLY + 12.0 - PLY_T + 0.5))
    return p


def _to_edge(shape, rot, mirror):
    if mirror:
        shape = shape.mirror("YZ")
    return shape.rotate((0, 0, 0), (0, 0, 1), rot)


def _edge_point(a, u, rot, mirror):
    x = -a if mirror else a
    t = math.radians(rot)
    return (round(x * math.cos(t) - u * math.sin(t), 3), round(x * math.sin(t) + u * math.cos(t), 3))


def halo_layout():
    """[(name, bore, rot, mirror)]. The panel screw under each piece: top/bottom pieces cross the
    inner insert column (x = 9, y = 175), left/right pieces the outer column (x = 183, y = 17)."""
    res = []
    ins = inserts()
    for edge, rot in (("top", 0), ("left", 90), ("bottom", 180), ("right", 270)):
        for mirror in (False, True):
            hits = []
            for x, y in ins:  # back to piece coordinates
                for a in range(1, 170):
                    for u in (PITCH / 2 + 79.0, PITCH / 2 + 87.0):
                        if _edge_point(a, u, rot, mirror) == (round(x, 3), round(y, 3)):
                            hits.append((float(a), u))
            assert len(hits) == 1, (edge, mirror, hits)
            res.append((f"{edge}-{'b' if mirror else 'a'}", hits[0], rot, mirror))
    return res


def halo_variants():
    return sorted({bore for _, bore, _, _ in halo_layout()})


def halo_fix_points():
    return [_edge_point(a, HALO_FIX_U, rot, mir) for _, _, rot, mir in halo_layout() for a in HALO_FIX]


def halo_strip_envelope():
    """Strip in the top-right channel (10 x 2.63 mm on the channel floor)."""
    w, t = 10.0, 2.63
    f = HALO["floor"]
    m = _halo_map
    pts = [m(-w / 2, f), m(w / 2, f), m(w / 2, f + t), m(-w / 2, f + t)]
    return _uz_prism(pts, HALO_SPAN[0] + 2, HALO_SPAN[1] - 2)


def halo_bores():
    return {(round(x, 3), round(y, 3)) for _, (a, u), rot, mir in halo_layout() for x, y in [_edge_point(a, u, rot, mir)]}


# --- Hanging rails on the two existing wall screws (#171) -------------------------------------
# The wall already has two level countersunk screws in plugs, about 260 mm apart. The owner set
# their heads 10 mm off the wall (8.8 mm head, 5.2 mm thick, 4.37 mm shank, measured; #170). Two
# printed rails with a lip drop over the heads, picture-frame style, as in v0.7; a side-inserted
# M5 x 45 steel keeper under each head stops the display lifting off. Two bumpers at the bottom
# hold the display parallel to the wall. Rails: M3 x 20 from the plywood's inner face; bumpers M3 x 12.
WS = v07.WALL_SCREW
RAIL_X = v07.RAIL_X_IN, v07.RAIL_X_OUT  # quadrant-local along +sx: -88..60
RAIL_WEB_Y = v07.RAIL_WEB_Y  # quadrant-local, 40..48
RAIL_LIP_Y0 = v07.RAIL_LIP_Y0
RAIL_LIP_T = v07.RAIL_LIP_T
RAIL_SCREWS_X = (-55.0, -30.0, 42.0)  # rail-local; heads clear the ribbon corridor and power plugs
RAIL_SCREW_L = 20.0
KEEPER_X, KEEPER_Y, KEEPER_Z_OFFSET, KEEPER_LENGTH = v07.KEEPER_X, v07.KEEPER_Y, v07.KEEPER_Z_OFFSET, v07.KEEPER_LENGTH
BUMPERS = [(-160.0, -150.0), (160.0, -150.0)]
BUMPER_D = v07.BUMPER["d"]


def rail_z():
    lip_front = Z_WALL - WS["head_gap"]  # bears on the head's underside
    return lip_front, lip_front + RAIL_LIP_T


def build_rail(sx):
    """Rail for the top quadrant sx, quadrant-local XY, assembly Z."""
    lip_front, back = rail_z()
    x0, x1 = sx * RAIL_X[0], sx * RAIL_X[1]
    rail = box(x0, x1, RAIL_WEB_Y[0], RAIL_WEB_Y[1], Z_PLY, back)
    rail = rail.union(box(x0, x1, RAIL_LIP_Y0, RAIL_WEB_Y[1], lip_front, back))
    ym = sum(RAIL_WEB_Y) / 2
    for x in RAIL_SCREWS_X:
        rail = rail.cut(cyl(sx * x, ym, M3_PILOT, Z_PLY, Z_PLY + RAIL_SCREW_L - PLY_T + 0.5))
    inner, outer = KEEPER_X
    kz = lip_front - KEEPER_Z_OFFSET
    rail = rail.union(box(sx * inner, sx * outer, 9.0, 34.0, kz - 6.0, back))
    rail = rail.cut(cyl_x(KEEPER_Y, kz, 5.5, sx * inner, sx * outer))
    rail = rail.cut(box(sx * (inner + 4), sx * (inner + 8.5), 8.0, KEEPER_Y + 4.15, kz - 4.15, kz + 4.15))
    return rail


def rail_screws():
    y = PITCH / 2 + sum(RAIL_WEB_Y) / 2
    return [(sx * (PITCH / 2 + x), y) for sx in (-1, 1) for x in RAIL_SCREWS_X]


def wall_screw_y():
    return PITCH / 2 + RAIL_LIP_Y0 - WS["shank_d"] / 2  # shank top touches the lip


def wall_screws(shift=0.0):
    """[(name, shape)] measured countersunk wall screws at their set 10 mm gap."""
    out = []
    y = wall_screw_y()
    for s in (-1, 1):
        x = s * WS["spacing"] / 2 + shift
        out.append((f"wall screw shank {s}", cyl(x, y, WS["shank_d"], Z_WALL - WS["head_gap"], Z_WALL)))
        out.append((f"wall screw head {s}", cq.Workplane(obj=cq.Solid.makeCone(
            WS["head_d"] / 2, WS["shank_d"] / 2, WS["head_t"],
            cq.Vector(x, y, Z_WALL - WS["head_gap"] - WS["head_t"])))))
    return out


def keeper_shank(sx):
    cx, cy = quadrant_centre(sx, 1)
    x = cx + sx * KEEPER_X[1]
    return cyl_x(cy + KEEPER_Y, rail_z()[0] - KEEPER_Z_OFFSET, 5.0, x, x - sx * KEEPER_LENGTH)


def build_bumper(x, y):
    b = cyl(x, y, BUMPER_D, Z_PLY, Z_WALL).edges(">Z").chamfer(1.0)
    return b.cut(cyl(x, y, M3_PILOT, Z_PLY, Z_PLY + 12.0 - PLY_T + 0.5))


# --- Screws -----------------------------------------------------------------------------------
def inside_screws():
    """[(name, x, y, length, head)] M3 from the plywood's inner face into printed pilots."""
    out = [(f"halo fix {x:.0f},{y:.0f}", x, y, 12.0, M3_BUTTON) for x, y in halo_fix_points()]
    out += [(f"pod {x:.0f},{y:.0f}", x, y, 12.0, M3_BUTTON) for x, y in pod_screws()]
    out += [(f"bumper {x:.0f},{y:.0f}", x, y, 12.0, M3_BUTTON) for x, y in BUMPERS]
    out += [(f"rail {x:.0f},{y:.0f}", x, y, RAIL_SCREW_L, M3_PAN) for x, y in rail_screws()]
    return out


def panel_screws():
    """[(name, x, y, head seat z, washer?)]: M3 x 30, ISO 7380 head."""
    bores = halo_bores()
    out = []
    for x, y in inserts():
        if (round(x, 3), round(y, 3)) in bores:
            out.append((f"panel screw {x:.0f},{y:.0f} (through halo)", x, y, Z_PLY + HALO_SEAT, False))
        else:
            out.append((f"panel screw {x:.0f},{y:.0f}", x, y, Z_PLY + WASHER[1], True))
    return out


def screw_shapes():
    """[(name, shape)]: everything of the screws outside the printed pilots and the inserts."""
    out = []
    for name, x, y, seat, washer in panel_screws():
        out.append((name + " shank", cyl(x, y, 2.9, Z_INSERT, seat)))
        out.append((name + " head", cyl(x, y, M3_BUTTON[0], seat, seat + M3_BUTTON[1])))
        if washer:
            out.append((name + " washer", cyl(x, y, WASHER[0], Z_PLY, seat)))
    for name, x, y, length, (hd, ht) in inside_screws():
        out.append((f"inside screw {name} head", cyl(x, y, hd, Z_CEIL - ht, Z_CEIL)))
        out.append((f"inside screw {name} shank", cyl(x, y, 2.9, Z_CEIL, Z_PLY)))
    return out


def screw_checks():
    """{what: (tip z, thread engagement mm)}."""
    out = {}
    for washer, seat in ((True, Z_PLY + WASHER[1]), (False, Z_PLY + HALO_SEAT)):
        tip = seat - PANEL_SCREW_L
        out[f"panel M3 x 30 ({'washer' if washer else 'through halo'})"] = (tip, Z_INSERT - tip)
    out["inside M3 x 12 (halo, pod, bumpers)"] = (Z_PLY + 12.0 - PLY_T, 12.0 - PLY_T)
    out["inside M3 x 20 (rails)"] = (Z_PLY + RAIL_SCREW_L - PLY_T, RAIL_SCREW_L - PLY_T)
    return out


# --- Plywood back -----------------------------------------------------------------------------
# --- Vents: a chimney through the gap under the plywood ---------------------------------------
# The boards' PCBs lie flat on the plywood, so holes over them would not move air. Instead a row
# near the bottom edge lets cool air in and a row near the top lets the warm air out into the gap
# behind the display; it rises past the boards on the way. Holes go wherever nothing sits on
# either face of the plywood (VENT_CLEAR around each).
VENT_D = 10.0
VENT_ROWS = (-165.0, 160.0)
VENT_STEP = 20.0  # leaves >= 10 mm of wood between holes
VENT_CLEAR = 2.0
HALO_INNER = 173.6  # inner edge of the halo pieces, from the centre


def _rect_dist(x, y, r):
    x0, x1, y0, y1 = r
    return math.hypot(max(x0 - x, 0, x - x1), max(y0 - y, 0, y - y1))


def vent_obstacles():
    """(rects, circles) global: everything on either face of the plywood a vent must clear."""
    rects = list(board_rects().values()) + [fuse_rect()]
    rects += [wago_rect(*w) for w in WAGOS.values()]
    rects.append((POD["x0"], POD["x1"], POD["y0"], POD["y1"]))
    for sx in (-1, 1):
        cx, cy = quadrant_centre(sx, 1)
        b = build_rail(sx).val().BoundingBox()
        rects.append((cx + b.xmin, cx + b.xmax, cy + b.ymin, cy + b.ymax))
    circles = [(x, y, WASHER[0] / 2) for x, y in inserts()]
    circles += [(x, y, head[0] / 2) for _, x, y, _, head in inside_screws()]
    circles += [(x, y, TIE_HOLE / 2) for x, y in tie_holes()]
    circles += [(x, y, BUMPER_D / 2) for x, y in BUMPERS]
    return rects, circles


def vent_holes():
    rects, circles = vent_obstacles()
    r = VENT_D / 2 + VENT_CLEAR
    out = []
    n = int((HALO_INNER - r) // VENT_STEP)
    for y in VENT_ROWS:
        for i in range(-n, n + 1):
            x = i * VENT_STEP
            if abs(x) > HALO_INNER - r or abs(y) > HALO_INNER - r:
                continue
            if any(_rect_dist(x, y, q) < r for q in rects) or any(math.hypot(x - cx, y - cy) < r + cr for cx, cy, cr in circles):
                continue
            out.append((x, y))
    return out


def ply_holes():
    """[(x, y, what, d)] global."""
    holes = [(x, y, "panel screw", PLY_HOLE) for x, y in inserts()]
    holes += [(x, y, f"M3 {name.split()[0]}", M3_CLEAR) for name, x, y, _, _ in inside_screws()]
    holes += [(x, y, "cable tie", TIE_HOLE) for x, y in tie_holes()]
    holes += [(x, y, "vent", VENT_D) for x, y in vent_holes()]
    return holes


def build_ply():
    ply = box(-E, E, -E, E, Z_CEIL, Z_PLY)
    x0, x1, y0, y1 = pod_window()
    ply = ply.cut(box(x0, x1, y0, y1, Z_CEIL, Z_PLY))
    for x, y, _, d in ply_holes():
        ply = ply.cut(cyl(x, y, d, Z_CEIL, Z_PLY))
    return ply
