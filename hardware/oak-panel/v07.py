"""v0.7 rear: carriers, seam joiners, wood rear box, halo channel pieces, hanging rails, wall bumpers, electronics plates.

Millimetres. Designed from the trial kit v6 results (issue #170, 2026-11-17). Not yet printed.

ASSEMBLY FRAME (every builder here returns its part already placed in it):
X right and Y up as seen from the WALL looking at the panel backs (rear view), origin at the
centre of the 2x2 array; Z runs from the LED tips (Z=0) toward the wall. Quadrant-local
builders take (sx, sy) in {-1, 1}: the quadrant's centre is (sx * PITCH / 2, sy * PITCH / 2)
and its OUTER edges face +sx and +sy. All four panels hang the same way up, so the carriers are
identical. The rear box is wood (cheaper than the printed trays it replaced, 2026-11-17): four
pine strips stand on the carriers' outer bands and a 3 mm HDF back closes it; small printed
channel pieces on the HDF hold the halo strip. wood_drawings.py makes the cut and drill sheets.

The front (black metal picture frame, diffuser retention) is NOT designed here: it waits for
the canvas vs. acrylic comparison and the bought frame's profile.

Stack along Z (owner measurements, measurement-basis.json):
  0      LED tips
  15     insert faces / panel frame top                     (led_tip_to_insert_face_mm)
  15-27  carrier corner bosses (12 mm)
  21-27  carrier rim, 27-31 carrier web                     (trial carrier geometry, fit OK)
  16-    electronics plates in the carriers' open middle    (electronics_plates.py)
  31-60  tray side walls on the outer edges, 60-62 tray cover
  62     rear face; halo channels open here, LEDs facing the wall
  87     wall (25 mm gap for the halo); wall-screw heads 10 mm off the wall
"""

import cadquery as cq

import electronics_plates as ep
from trial_carrier import BOSS_MM, OUTER_MM, PATTERN

# --- Fasteners (Bauhaus Karlsruhe-Oststadt, #172) --------------------------------------------
M3_CLEAR = 3.4  # screw coupon: smallest hole an M3 passes freely (owner, 2026-11-17)
M3_PILOT = 2.6  # PROVISIONAL: M3 x 12 cutting its own thread in PETG; set from the coupon test
M25_PILOT = 2.2  # PROVISIONAL: M2.5 x 10 into PETG; set from the coupon test
M25_CLEAR = 2.8

# --- Array and Z stack -----------------------------------------------------------------------
PITCH = 192.0  # module pitch: 191.6 mm bodies, glued seams 0-1 mm (owner, 2026-11-17)
HALF = OUTER_MM / 2  # 95.8
Z_INSERT = 15.0
Z_WEB_FRONT, Z_REAR = 27.0, 31.0  # carrier web; Z_REAR = carrier rear face
Z_RIM_FRONT = 21.0
# Wood rear box. Bauhaus "Rechteckleiste" 240 x 3 x 1.9 cm pine: 19 mm in plane, 30 mm deep.
STRIP_T, STRIP_D = 19.0, 30.0
STRIP_OUT = 195.0  # outer faces 3 mm outside the panel block (frame rebate is 400 mm square)
STRIP_IN = STRIP_OUT - STRIP_T  # 176: keeps >= 2.5 mm to the power modules
Z_STRIP_BACK = Z_REAR + STRIP_D  # 61: inside ceiling
HDF_T = 3.0
Z_BACK = Z_STRIP_BACK + HDF_T  # 64: rear face of the HDF
STRIP_HOLE = 5.0  # M3 x 40 through the strip: 0.8 mm play per side for hand drilling
STRIP_CBORE = (10.0, 3.0)  # washer + head sink in, so the HDF lies flat
WASHER_OD = 9.0  # DIN 9021 M3 (3.2 x 9)
WOOD_SCREW = (3.0, 20.0)  # countersunk wood screws: halo pieces and HDF corners
WOOD_PILOT = 2.0
WALL_SCREW = {"head_gap": 10.0, "head_d": 10.0, "head_t": 3.0, "shank_d": 4.5, "spacing": 260.0}
# Owner: two screws in wall plugs, level, outer-to-outer ~268 mm with 7-10 mm heads -> ~260 mm
# centres; heads set to 10 mm off the wall.

# --- Carrier v0.7 (same outline, bosses and rim as the trial carrier) ------------------------
WEB_BAND = 16.0  # web band width from the outer edge
RIM_W = 4.0
CORNER_BORE = M3_CLEAR  # M3 x 20 into the panels' M3 inserts: 16 mm grip + 4 mm thread
# Joint holes: M3 pilots with a boss below the web. On a seam a joiner bridges two carriers; on
# an outer edge an M3 x 40 through the pine strip screws into the same hole. Same positions on both opposite
# edges, so all four carriers are identical.
JOINT_HOLES = [(sx * 86.0, y) for sx in (-1, 1) for y in (-20.0, 15.0)] + [
    (x, sy * 86.0) for x in (-40.0, 40.0) for sy in (-1, 1)
]
JOINT_BOSS = (8.0, 7.0)  # OD, height below the web (bottom 5 mm above the panel frame)
PLATE_HOLES = ep.MOUNT_HOLES  # M3 x 12 from the rear face into the electronics plate posts
ep.POST_PILOT = M3_PILOT  # the plate posts take the same M3 x 12 as every other pilot
# Cable notches: the rim's panel-facing edge is raised by NOTCH_DEPTH in these zones, so a cable
# lying on the panel frame gets 9 mm instead of 6 mm (cables passed the 6 mm tunnel; plugs stay
# in the open middle). Zones avoid the joint bosses and the plate posts.
NOTCH_DEPTH = 3.0
NOTCH_ZONES_X_EDGE = [(28.0, 70.0), (-48.0, -26.0)]  # y ranges on the +/-X edges
NOTCH_ZONES_Y_EDGE = [(-30.0, 30.0)]  # x ranges on the +/-Y edges
ORIENTATION_NOTCH = (8.0, 2.0, 2.0)  # on the +Y rim, connector half up

# --- Halo channel pieces (printed, on the HDF over the strips) --------------------------------
# From the coupon test (owner, 2026-11-17): 10.7 mm inner width fits the strip; walls thickened from
# the coupon's 1.6 mm (one broke) to 2.4 mm. Two pieces per edge butt at the edge centre; the strip
# slides in from the corner end. Pieces stop short of the corners, which keeps the HDF corner screws
# free and the two bands apart. Printed floor-down (lips are a 1.2 mm overhang).
HALO = {"inner_w": 10.7, "inner_h": 3.3, "lip": 1.2, "wall": 2.4, "floor": 1.6}
HALO_BAND_W = HALO["inner_w"] + 2 * HALO["wall"]  # 15.5
HALO_DEPTH = HALO["lip"] + HALO["inner_h"] + HALO["floor"]  # 6.1 above the HDF
HALO_C = (STRIP_IN + STRIP_OUT) / 2  # 185.5: centred over the strips
HALO_SPAN = (0.25, 174.0)  # along the edge from its centre; 0.5 mm butt gap
HALO_SCREWS = (20.0, 95.0, 160.0)  # along the edge from its centre: clear of the M3 counterbores
HALO_FEED = (168.0, -168.0, 8.0)  # x, y, hole: halo cable from the Pro Micro out through the HDF
WALL_GAP = 25.0  # channel openings to the wall: halo throw (#208)
Z_WALL = Z_BACK + HALO_DEPTH + WALL_GAP  # 95.1
VENT_D, VENT_PITCH = 10.0, 22.0  # drilled vent holes in the HDF

# --- Hanging rail (top trays) and bumpers (bottom trays) -------------------------------------
# The rail's lip hooks behind the wall-screw heads; its lower edge rests on the shanks. Each
# top tray carries one rail, so each rail catches one screw; +/-26 mm sideways adjustment.
RAIL_X_IN, RAIL_X_OUT = -88.0, 60.0  # quadrant-local along +sx
RAIL_WEB_Y = (40.0, 48.0)
RAIL_LIP_Y0 = 26.0  # lip lower edge: rests on the screw shank
RAIL_LIP_T = 3.5
RAIL_SCREWS_X = (-70.0, -14.0, 42.0)  # M3 x 12 from inside, through the HDF
BUMPER = {"d": 16.0, "x": 34.0, "y": -44.0}  # bottom quadrants; M3 x 12 from inside; bears on the wall

# --- Electronics placement -------------------------------------------------------------------
POWER_QUADRANT = (-1, -1)  # near the DC socket (wiring README: modules and fuses at the bottom)
CONTROLLER_QUADRANT = (1, -1)
DC_SOCKET_X = 18.0  # global, in the bottom strip's outer face (toward the Pi side)
DC_SOCKET_Z = Z_REAR + STRIP_D / 2  # 46: centred in the strip's 30 mm face
DC_SOCKET_BODY = (13.8, 20.0)  # envelope from the outer face: diameter, length incl. cups and wire bends
# Kycon drawing (datasheet KPJX-PM-4S, 2026-11-18): flange 25.2 x 14.2, holes 2.3 on 19.0; the two
# plastic screw bosses and the body reach 6.05 behind the flange front. Kycon IGES of the shielded
# KPJX-PM-4S-S (the owner's part, measured 2026-11-18): flange 25.7 x 14.7, about 1.55 thick (metal +
# plastic); bosses Ø4.8; body at most Ø12.6; solder cups end 14.0 behind the flange front, so about
# 12.3 into the strip, inside the 14 mm hole (solder before mounting). Mounted from
# outside on the wood: body in the 14 mm hole, bosses in two pockets, and the flange and boss holes
# opened to 3.5 so two 3 x 16 pan-head wood screws go through them into the pine.
DC_SOCKET_BOSS_POCKET = (6.0, 7.0)  # diameter, depth from the outer face
DC_SOCKET_SCREW = (3.0, 16.0)  # pan-head wood screw
DC_SOCKET_PILOT = (2.0, 17.0)  # diameter, depth from the outer face
PI_STANDOFF_V07 = (6.0, 7.0)  # OD, height: M2.5 x 10 through the 1.6 mm board -> 8.4 mm thread
PI_CENTRE_V07 = (-32.0, -36.0)  # moved in from the edge: the stack must stay out of the halo band


def box(x0, x1, y0, y1, z0, z1):
    x0, x1 = sorted((x0, x1))
    y0, y1 = sorted((y0, y1))
    z0, z1 = sorted((z0, z1))
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))


def cyl(x, y, d, z0, z1, axis="Z"):
    c = cq.Workplane("XY").circle(d / 2).extrude(z1 - z0)
    if axis == "Z":
        return c.translate((x, y, z0))
    raise ValueError(axis)


def cyl_y(x, z, d, y0, y1):
    """Cylinder along Y from y0 to y1 at (x, z)."""
    y0, y1 = sorted((y0, y1))
    return cq.Workplane("XZ").circle(d / 2).extrude(-(y1 - y0)).translate((x, y0, z))


def quadrant_centre(sx, sy):
    return (sx * PITCH / 2, sy * PITCH / 2)


# ============================================================================================
# Carrier
# ============================================================================================
def build_carrier():
    """One carrier, quadrant-local XY, in assembly Z (bosses down on the inserts at Z 15)."""
    web = cq.Workplane("XY").rect(OUTER_MM, OUTER_MM).rect(OUTER_MM - 2 * WEB_BAND, OUTER_MM - 2 * WEB_BAND)
    body = web.extrude(Z_REAR - Z_WEB_FRONT).translate((0, 0, Z_WEB_FRONT))
    rim = cq.Workplane("XY").rect(OUTER_MM, OUTER_MM).rect(OUTER_MM - 2 * RIM_W, OUTER_MM - 2 * RIM_W)
    body = body.union(rim.extrude(Z_WEB_FRONT - Z_RIM_FRONT).translate((0, 0, Z_RIM_FRONT)))
    for x, y in PATTERN:
        body = body.union(cyl(x, y, BOSS_MM, Z_INSERT, Z_WEB_FRONT))
        body = body.cut(cyl(x, y, CORNER_BORE, Z_INSERT, Z_REAR))
    od, h = JOINT_BOSS
    for x, y in JOINT_HOLES:
        body = body.union(cyl(x, y, od, Z_WEB_FRONT - h, Z_WEB_FRONT))
        body = body.cut(cyl(x, y, M3_PILOT, Z_WEB_FRONT - h, Z_REAR))
    for x, y in PLATE_HOLES:
        body = body.cut(cyl(x, y, M3_CLEAR, Z_WEB_FRONT, Z_REAR))
    z0, z1 = Z_RIM_FRONT, Z_RIM_FRONT + NOTCH_DEPTH
    for s in (-1, 1):
        for y0, y1 in NOTCH_ZONES_X_EDGE:
            body = body.cut(box(s * (HALF - RIM_W), s * (HALF + 1), y0, y1, z0, z1))
        for x0, x1 in NOTCH_ZONES_Y_EDGE:
            body = body.cut(box(x0, x1, s * (HALF - RIM_W), s * (HALF + 1), z0, z1))
    nx, ny, nz = ORIENTATION_NOTCH
    body = body.cut(box(-nx / 2, nx / 2, HALF - ny, HALF + 1, z0 + NOTCH_DEPTH, z0 + NOTCH_DEPTH + nz))
    return body


# ============================================================================================
# Seam joiner
# ============================================================================================
JOINER = (30.0, 12.0, 4.0)
JOINT_SPAN = 2 * (HALF - 86.0) + (PITCH - OUTER_MM)  # 20.0 mm hole to hole at a 0.4 mm seam
JOINER_SLOT_PLAY = 1.2  # seams are 0-1 mm: slots absorb it


def build_joiner(cx, cy, along_x):
    """Joiner centred on a seam, lying on the carriers' rear faces, holes along X or Y."""
    lx, w, t = JOINER
    j = box(-lx / 2, lx / 2, -w / 2, w / 2, Z_REAR, Z_REAR + t)
    for s in (-1, 1):
        slot = (
            cq.Workplane("XY")
            .center(s * JOINT_SPAN / 2, 0)
            .slot2D(M3_CLEAR + JOINER_SLOT_PLAY, M3_CLEAR)
            .extrude(t)
            .translate((0, 0, Z_REAR))
        )
        j = j.cut(slot)
    if not along_x:
        j = j.rotate((0, 0, 0), (0, 0, 1), 90)
    return j.translate((cx, cy, 0))


def joiner_positions():
    """(cx, cy, along_x) for the 8 joiners on the four half-seams (global XY)."""
    out = []
    for sy in (-1, 1):
        for y in (-20.0, 15.0):  # vertical seam, X-edge joint holes
            out.append((0.0, sy * PITCH / 2 + y, True))
    for sx in (-1, 1):
        for x in (-40.0, 40.0):  # horizontal seam, Y-edge joint holes
            out.append((sx * PITCH / 2 + x, 0.0, False))
    return out


# ============================================================================================
# Wood rear box (reference solids; cut and drilled by hand from wood_drawings.py)
# ============================================================================================
def strip_screws():
    """[(x, y, what)] global: M3 x 40 through the strips into carrier pilots and plate posts."""
    pts = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = quadrant_centre(sx, sy)
            pts += [(cx + x, cy + y, "carrier") for x, y in JOINT_HOLES]
    for quad, plate in ((POWER_QUADRANT, "power plate"), (CONTROLLER_QUADRANT, "controller plate")):
        cx, cy = quadrant_centre(*quad)
        pts += [(cx + x, cy + y, plate) for x, y in PLATE_HOLES]
    return [(x, y, w) for x, y, w in pts if max(abs(x), abs(y)) > STRIP_IN]


def strip_boxes():
    """{name: (x0, x1, y0, y1)}: top and bottom run the full width, the sides fit between them."""
    o, i = STRIP_OUT, STRIP_IN
    return {
        "strip-top": (-o, o, i, o),
        "strip-bottom": (-o, o, -o, -i),
        "strip-left": (-o, -i, -i, i),
        "strip-right": (i, o, -i, i),
    }


def build_strips():
    out = {}
    for name, (x0, x1, y0, y1) in strip_boxes().items():
        s = box(x0, x1, y0, y1, Z_REAR, Z_STRIP_BACK)
        for x, y, _ in strip_screws():
            if x0 < x < x1 and y0 < y < y1:
                s = s.cut(cyl(x, y, STRIP_HOLE, Z_REAR, Z_STRIP_BACK))
                s = s.cut(cyl(x, y, STRIP_CBORE[0], Z_STRIP_BACK - STRIP_CBORE[1], Z_STRIP_BACK))
        for x, y, _, d in hdf_holes("corner"):
            if x0 < x < x1 and y0 < y < y1:
                s = s.cut(cyl(x, y, WOOD_PILOT, Z_STRIP_BACK - WOOD_SCREW[1] + HDF_T, Z_STRIP_BACK))
        for x, y in halo_screws():
            if x0 < x < x1 and y0 < y < y1:
                depth = WOOD_SCREW[1] - HDF_T - HALO["floor"]
                s = s.cut(cyl(x, y, WOOD_PILOT, Z_STRIP_BACK - depth, Z_STRIP_BACK))
        if name == "strip-bottom":
            s = _add_dc_socket(s)
        out[name] = s
    return out


def _add_dc_socket(strip):
    """Kycon KPJX-PM-4S on the bottom strip's outer face: 14 mm through, boss pockets, screw pilots."""
    x, z = DC_SOCKET_X, DC_SOCKET_Z
    strip = strip.cut(cyl_y(x, z, ep.DC_SOCKET["body_cutout"], -STRIP_OUT, -STRIP_IN))
    s = ep.DC_SOCKET["screw_spacing"] / 2
    bd, bdepth = DC_SOCKET_BOSS_POCKET
    d, depth = DC_SOCKET_PILOT
    for dx in (-s, s):
        strip = strip.cut(cyl_y(x + dx, z, bd, -STRIP_OUT, -STRIP_OUT + bdepth))
        strip = strip.cut(cyl_y(x + dx, z, d, -STRIP_OUT, -STRIP_OUT + depth))
    return strip


def halo_screws():
    """(x, y) global wood screws through the halo pieces and the HDF into the strips."""
    pts = []
    for a in HALO_SCREWS:
        for s in (-1, 1):
            pts += [(s * a, HALO_C), (s * a, -HALO_C), (HALO_C, s * a), (-HALO_C, s * a)]
    return pts


def _keep_clear(x, y, r, taken):
    return all(((x - tx) ** 2 + (y - ty) ** 2) ** 0.5 >= r + tr + 3.0 for tx, ty, tr in taken)


def hdf_holes(kind=None):
    """[(x, y, what, diameter)] global, drilled in the HDF (pilots for the halo pieces are drilled
    through the printed pieces on assembly and are not listed)."""
    holes = []
    o = HALO_C
    holes += [(sx * o, sy * o, "corner", 3.2) for sx in (-1, 1) for sy in (-1, 1)]
    for sx in (-1, 1):
        cx, cy = quadrant_centre(sx, 1)
        holes += [(cx + sx * x, cy + sum(RAIL_WEB_Y) / 2, "rail", M3_CLEAR) for x in RAIL_SCREWS_X]
        cx, cy = quadrant_centre(sx, -1)
        holes.append((cx + sx * BUMPER["x"], cy + BUMPER["y"], "bumper", M3_CLEAR))
    holes.append((HALO_FEED[0], HALO_FEED[1], "halo cable", HALO_FEED[2]))
    taken = [(x, y, d / 2) for x, y, _, d in holes]
    # Keep-outs: rail footprints and bumper footprints (plus the holes above).
    for sx in (-1, 1):
        x0, x1 = sorted((sx * (PITCH / 2 + RAIL_X_IN), sx * (PITCH / 2 + RAIL_X_OUT)))
        for x in range(int(x0), int(x1) + 1, 4):
            taken.append((x, PITCH / 2 + sum(RAIL_WEB_Y) / 2, 6.0))
        cx, cy = quadrant_centre(sx, -1)
        taken.append((cx + sx * BUMPER["x"], cy + BUMPER["y"], BUMPER["d"] / 2))
    # Vents: a 3 x 3 grid over each power module and over the Pi, and a row along the top.
    centres = []
    pcx, pcy = quadrant_centre(*POWER_QUADRANT)
    centres += [(pcx + mx, pcy + my) for mx, my in ep.MODULE_CENTRES]
    ccx, ccy = quadrant_centre(*CONTROLLER_QUADRANT)
    x0, x1, y0, y1 = pi_board_box()
    centres.append((ccx + (x0 + x1) / 2, ccy + (y0 + y1) / 2))
    cand = [(gx + i * VENT_PITCH, gy + j * VENT_PITCH) for gx, gy in centres for i in (-1, 0, 1) for j in (-1, 0, 1)]
    cand += [(x, 160.0) for x in range(-165, 166, 30)]
    r = VENT_D / 2
    for x, y in cand:
        if max(abs(x), abs(y)) + r <= STRIP_IN - 3.0 and _keep_clear(x, y, r, taken):
            holes.append((float(x), float(y), "vent", VENT_D))
            taken.append((x, y, r))
    return [h for h in holes if kind is None or h[2] == kind]


def build_hdf():
    o = STRIP_OUT
    panel = box(-o, o, -o, o, Z_STRIP_BACK, Z_BACK)
    for x, y, _, d in hdf_holes():
        panel = panel.cut(cyl(x, y, d, Z_STRIP_BACK, Z_BACK))
    for x, y in halo_screws():
        panel = panel.cut(cyl(x, y, WOOD_SCREW[0] + 0.2, Z_STRIP_BACK, Z_BACK))
    return panel


# ============================================================================================
# Halo channel piece (printed)
# ============================================================================================
def build_halo_piece():
    """One piece along +X from x = 0, centred on y = 0, floor on z = 0 (print orientation)."""
    length = HALO_SPAN[1] - HALO_SPAN[0]
    iw, ow = HALO["inner_w"], HALO["inner_w"] - 2 * HALO["lip"]
    f, top = HALO["floor"], HALO_DEPTH
    p = box(0, length, -HALO_BAND_W / 2, HALO_BAND_W / 2, 0, top)
    p = p.cut(box(0, length, -iw / 2, iw / 2, f, top - HALO["lip"]))
    p = p.cut(box(0, length, -ow / 2, ow / 2, top - HALO["lip"], top))
    for a in HALO_SCREWS:
        x = a - HALO_SPAN[0]
        p = p.cut(cyl(x, 0, WOOD_SCREW[0] + 0.4, 0, f))
        cone = cq.Solid.makeCone((WOOD_SCREW[0] + 0.4) / 2, 3.3, 3.3 - (WOOD_SCREW[0] + 0.4) / 2,
                                 cq.Vector(x, 0, f - (3.3 - (WOOD_SCREW[0] + 0.4) / 2)), cq.Vector(0, 0, 1))
        p = p.cut(cq.Workplane("XY").add(cone))
    return p


def halo_piece_placements():
    """[(name, edge, half)]: two pieces per edge."""
    return [(f"{e}-{'a' if h > 0 else 'b'}", e, h) for e in ("top", "bottom", "left", "right") for h in (1, -1)]


def place_halo_piece(piece, edge, half):
    p = piece.translate((HALO_SPAN[0], 0, 0))
    if half < 0:
        p = p.rotate((0, 0, 0), (0, 0, 1), 180)
    if edge in ("left", "right"):
        p = p.rotate((0, 0, 0), (0, 0, 1), 90)
    off = {"top": (0, HALO_C), "bottom": (0, -HALO_C), "left": (-HALO_C, 0), "right": (HALO_C, 0)}[edge]
    return p.translate((off[0], off[1], Z_BACK))


# ============================================================================================
# Rail and bumper
# ============================================================================================
def rail_z():
    lip_front = Z_WALL - WALL_SCREW["head_gap"]  # bears on the head's underside
    return lip_front, lip_front + RAIL_LIP_T


def build_rail(sx):
    lip_front, back = rail_z()
    x0, x1 = sx * RAIL_X_IN, sx * RAIL_X_OUT
    rail = box(x0, x1, RAIL_WEB_Y[0], RAIL_WEB_Y[1], Z_BACK, back)
    rail = rail.union(box(x0, x1, RAIL_LIP_Y0, RAIL_WEB_Y[1], lip_front, back))
    ym = sum(RAIL_WEB_Y) / 2
    for x in RAIL_SCREWS_X:
        rail = rail.cut(cyl(sx * x, ym, M3_PILOT, Z_BACK, Z_BACK + 12))
    return rail


def build_bumper(sx):
    x, y = sx * BUMPER["x"], BUMPER["y"]
    b = cyl(x, y, BUMPER["d"], Z_BACK, Z_WALL)
    b = b.edges(">Z").chamfer(1.0)
    return b.cut(cyl(x, y, M3_PILOT, Z_BACK, Z_BACK + 12))


# ============================================================================================
# Electronics plates v0.7 (from electronics_plates.py)
# ============================================================================================
TIE_SLOT = (3.0, 6.0)  # x, y: cable tie slots either side of each 20 A module
TIE_ROWS = (-20.0, 20.0)  # two ties per module, relative to its centre


def build_power_plate_v07():
    """Modules held by two cable ties each (no M4); WAGO saddles and vents as v3."""
    plate = ep._plate()
    mw = ep.COMPONENTS["buck_20a"][0]
    for mx, my in ep.MODULE_CENTRES:
        for side in (-1, 1):
            for dy in TIE_ROWS:
                plate = plate.cut(
                    box(mx + side * (mw / 2 + 1.0), mx + side * (mw / 2 + 1.0 + TIE_SLOT[0]),
                        my + dy - TIE_SLOT[1] / 2, my + dy + TIE_SLOT[1] / 2, 0, ep.PLATE_T)
                )
    plate = ep._add_saddles(plate, "power-plate")
    plate = ep._vents(plate, -1.5, 1.5, -74.0, -10.0)
    return plate


def pi_board_box():
    px, py = PI_CENTRE_V07
    cx = px + ep.PI_BOARD_OFFSET_X
    w, d = ep.COMPONENTS["pi_stack"][:2]
    return (cx - w / 2, cx + w / 2, py - d / 2, py + d / 2)


def build_controller_plate_v07():
    """Pi on four 7 mm standoffs for M2.5 x 10 (same screws as the DC socket)."""
    plate = ep._plate()
    od, h = PI_STANDOFF_V07
    px, py = PI_CENTRE_V07
    for hx, hy in ep.PI_HOLES:
        x, y = px + hx, py + hy
        plate = plate.union(cyl(x, y, od, ep.PLATE_T, ep.PLATE_T + h))
        plate = plate.cut(cyl(x, y, M25_PILOT, ep.PLATE_T + h - 9.0, ep.PLATE_T + h))
    plate = ep._add_saddles(plate, "controller-plate")
    plate = ep._vents(plate, px - 22.0, px + 22.0, py - 16.0, py + 16.0)
    return plate


def plate_z():
    return ep.PLATE_Z0  # plate underside from the LED tips (16)


def component_envelopes(plate):
    """[(name, shape)] in plate-local XY and assembly Z, v0.7 placements."""
    out = []
    z_top = plate_z() + ep.PLATE_T
    if plate == "power-plate":
        for i, (x, y) in enumerate(ep.MODULE_CENTRES):
            w, d, h = ep.COMPONENTS["buck_20a"][:3]
            out.append((f"module_{'AB'[i]}", box(x - w / 2, x + w / 2, y - d / 2, y + d / 2, z_top, z_top + h)))
    else:
        x0, x1, y0, y1 = pi_board_box()
        h = ep.COMPONENTS["pi_stack"][2]
        z0 = z_top + PI_STANDOFF_V07[1]
        out.append(("pi_stack", box(x0, x1, y0, y1, z0, z0 + h)))
    for name, (comp, x, y) in ep.SADDLE_PARTS[plate].items():
        w, d, h = ep.COMPONENTS[comp][:3]
        z0 = z_top + ep.SADDLE[1]
        out.append((name, box(x - w / 2, x + w / 2, y - d / 2, y + d / 2, z0, z0 + h)))
    return out
