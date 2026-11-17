"""v0.7 rear parts: carriers, seam joiners, rear trays, hanging rails, wall bumpers, electronics plates.

Millimetres. Designed from the trial kit v6 results (issue #170, 2026-11-17). Not yet printed.

ASSEMBLY FRAME (every builder here returns its part already placed in it):
X right and Y up as seen from the WALL looking at the panel backs (rear view), origin at the
centre of the 2x2 array; Z runs from the LED tips (Z=0) toward the wall. Quadrant-local
builders take (sx, sy) in {-1, 1}: the quadrant's centre is (sx * PITCH / 2, sy * PITCH / 2)
and its OUTER edges face +sx and +sy. All four panels hang the same way up, so the carriers are
identical; the trays differ only by which edges are outer.

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
M3_HEAD_ACCESS = 7.0  # screwdriver / head clearance down to a buried screw head

# --- Array and Z stack -----------------------------------------------------------------------
PITCH = 192.0  # module pitch: 191.6 mm bodies, glued seams 0-1 mm (owner, 2026-11-17)
HALF = OUTER_MM / 2  # 95.8
Z_INSERT = 15.0
Z_WEB_FRONT, Z_REAR = 27.0, 31.0  # carrier web; Z_REAR = carrier rear face
Z_RIM_FRONT = 21.0
Z_COVER_IN, Z_COVER_OUT = 60.0, 62.0
WALL_GAP = 25.0  # cover rear face to wall: halo throw (#208)
Z_WALL = Z_COVER_OUT + WALL_GAP  # 87
WALL_SCREW = {"head_gap": 10.0, "head_d": 10.0, "head_t": 3.0, "shank_d": 4.5, "spacing": 260.0}
# Owner: two screws in wall plugs, level, outer-to-outer ~268 mm with 7-10 mm heads -> ~260 mm
# centres; heads set to 10 mm off the wall.

# --- Carrier v0.7 (same outline, bosses and rim as the trial carrier) ------------------------
WEB_BAND = 16.0  # web band width from the outer edge
RIM_W = 4.0
CORNER_BORE = M3_CLEAR  # M3 x 20 into the panels' M3 inserts: 16 mm grip + 4 mm thread
# Joint holes: M3 pilots with a boss below the web. On a seam a joiner bridges two carriers; on
# an outer edge the tray's column screws into the same hole. Same positions on both opposite
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

# --- Tray (rear cover quadrant + outer side walls) -------------------------------------------
TRAY_WALL = 3.0
COLUMN = 9.0  # square column under each outer-edge joint hole, joined to the side wall
COLUMN_FLOOR = 3.0  # screw head bears on this floor; M3 x 12 -> 9 mm into the 11 mm pilot
# Halo channel, from the coupon test (owner, 2026-11-17): 10.7 mm inner width fits the strip;
# walls thickened from the coupon's 1.6 mm (one broke) to 2.4 mm. Prints lips-down on the bed,
# like the coupon. Set in from the edge so the tray columns' screw access stays clear.
HALO = {"inner_w": 10.7, "inner_h": 3.3, "lip": 1.2, "wall": 2.4, "floor": 1.6, "centre": 73.0}
HALO_BAND_W = HALO["inner_w"] + 2 * HALO["wall"]
HALO_DEPTH = HALO["lip"] + HALO["inner_h"] + HALO["floor"]  # 6.1 from the rear face
HALO_CORNER_END = 60.0  # channels stop short of the corner so the two bands never cross
HALO_WIRE_HOLE = 6.0
VENT = (3.0, 36.0, 8.0)  # slot width, length, pitch

# --- Hanging rail (top trays) and bumpers (bottom trays) -------------------------------------
# The rail's lip hooks behind the wall-screw heads; its lower edge rests on the shanks. Each
# top tray carries one rail, so each rail catches one screw; +/-26 mm sideways adjustment.
RAIL_X_IN, RAIL_X_OUT = -88.0, 60.0  # quadrant-local along +sx
RAIL_WEB_Y = (40.0, 48.0)
RAIL_LIP_Y0 = 26.0  # lip lower edge: rests on the screw shank
RAIL_LIP_T = 3.5
RAIL_SCREWS_X = (-70.0, -14.0, 42.0)  # M3 x 12 from inside the tray, through the cover
BUMPER = {"d": 16.0, "x": 34.0, "y": -44.0}  # bottom trays; bears on the wall at Z_WALL

# --- Electronics placement -------------------------------------------------------------------
POWER_QUADRANT = (-1, -1)  # near the DC socket (wiring README: modules and fuses at the bottom)
CONTROLLER_QUADRANT = (1, -1)
DC_SOCKET_X = -78.0  # quadrant-local on the bottom wall of the controller quadrant
DC_SOCKET_Z = 46.0
DC_SOCKET_BODY = (14.0, 20.0)  # envelope behind the wall: diameter, depth (approx.)
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
# Tray: rear-cover quadrant + side walls on its two outer edges
# ============================================================================================
def _outer_columns(sx, sy):
    cols = [(sx * 86.0, y, "x") for y in (-20.0, 15.0)]
    cols += [(x, sy * 86.0, "y") for x in (-40.0, 40.0)]
    return cols


def _halo_runs(sx, sy):
    """[(x0, x1, y0, y1, axis)] channel centreline boxes (band extents) for the two outer edges."""
    c, w = HALO["centre"], HALO_BAND_W
    runs = [
        (sx * (c - w / 2), sx * (c + w / 2), -sy * HALF, sy * HALO_CORNER_END, "y"),  # along Y on the X edge
        (-sx * HALF, sx * HALO_CORNER_END, sy * (c - w / 2), sy * (c + w / 2), "x"),  # along X on the Y edge
    ]
    return runs


def _keepouts(sx, sy):
    """Rectangles on the cover (quadrant-local) where vents must not go."""
    k = []
    for x, y, _ in _outer_columns(sx, sy):
        k.append((x - 7, x + 7, y - 7, y + 7))
    for x0, x1, y0, y1, _ in _halo_runs(sx, sy):
        k.append((min(x0, x1) - 3, max(x0, x1) + 3, min(y0, y1) - 3, max(y0, y1) + 3))
    if sy == 1:
        xs = sorted((sx * RAIL_X_IN, sx * RAIL_X_OUT))
        k.append((xs[0] - 3, xs[1] + 3, RAIL_LIP_Y0 - 10, RAIL_WEB_Y[1] + 4))
    else:
        bx, by = sx * BUMPER["x"], BUMPER["y"]
        r = BUMPER["d"] / 2 + 4
        k.append((bx - r, bx + r, by - r, by + r))
    return k


def build_tray(sx, sy):
    lim = HALF
    tray = box(-lim, lim, -lim, lim, Z_COVER_IN, Z_COVER_OUT)
    wall_in = HALF - TRAY_WALL
    tray = tray.union(box(sx * wall_in, sx * HALF, -lim, lim, Z_REAR, Z_COVER_IN))
    tray = tray.union(box(-lim, lim, sy * wall_in, sy * HALF, Z_REAR, Z_COVER_IN))
    # Columns: carry the tray on the carrier's outer-edge joint holes. Screw from the rear
    # through the cover's access hole; the head sits on the column floor.
    for x, y, axis in _outer_columns(sx, sy):
        h = COLUMN / 2
        if axis == "x":
            col = box(x - sx * h, sx * wall_in, y - h, y + h, Z_REAR, Z_COVER_IN)
        else:
            col = box(x - h, x + h, y - sy * h, sy * wall_in, Z_REAR, Z_COVER_IN)
        tray = tray.union(col)
        tray = tray.cut(cyl(x, y, M3_CLEAR, Z_REAR, Z_REAR + COLUMN_FLOOR))
        tray = tray.cut(cyl(x, y, M3_HEAD_ACCESS, Z_REAR + COLUMN_FLOOR, Z_COVER_OUT))
    # Halo channels (open toward the wall).
    zf = Z_COVER_OUT - HALO_DEPTH
    for x0, x1, y0, y1, axis in _halo_runs(sx, sy):
        tray = tray.union(box(x0, x1, y0, y1, zf, Z_COVER_IN))
        cx = (x0 + x1) / 2 if axis == "y" else None
        cy = (y0 + y1) / 2 if axis == "x" else None
        z_cav0 = zf + HALO["floor"]
        z_lip = Z_COVER_OUT - HALO["lip"]
        iw, ow = HALO["inner_w"], HALO["inner_w"] - 2 * HALO["lip"]
        if axis == "y":
            tray = tray.cut(box(cx - iw / 2, cx + iw / 2, y0, y1, z_cav0, z_lip))
            tray = tray.cut(box(cx - ow / 2, cx + ow / 2, y0, y1, z_lip, Z_COVER_OUT))
            tray = tray.cut(cyl(cx, sy * (HALO_CORNER_END - 8), HALO_WIRE_HOLE, zf, z_cav0))
        else:
            tray = tray.cut(box(x0, x1, cy - iw / 2, cy + iw / 2, z_cav0, z_lip))
            tray = tray.cut(box(x0, x1, cy - ow / 2, cy + ow / 2, z_lip, Z_COVER_OUT))
            tray = tray.cut(cyl(sx * (HALO_CORNER_END - 8), cy, HALO_WIRE_HOLE, zf, z_cav0))
    # Rail or bumper screw holes.
    if sy == 1:
        ym = sum(RAIL_WEB_Y) / 2
        for x in RAIL_SCREWS_X:
            tray = tray.cut(cyl(sx * x, ym, M3_CLEAR, Z_COVER_IN, Z_COVER_OUT))
    else:
        tray = tray.cut(cyl(sx * BUMPER["x"], BUMPER["y"], M3_CLEAR, Z_COVER_IN, Z_COVER_OUT))
    # Vents over the electronics.
    sw, sl, sp = VENT
    keep = _keepouts(sx, sy)
    for y0 in (-56.0, -12.0):
        x = -60.0
        while x <= 60.0:
            r = (x - sw / 2, x + sw / 2, y0, y0 + sl)
            if all(r[1] < k[0] or r[0] > k[1] or r[3] < k[2] or r[2] > k[3] for k in keep):
                tray = tray.cut(box(*r, Z_COVER_IN, Z_COVER_OUT))
            x += sp
    if (sx, sy) == CONTROLLER_QUADRANT:
        tray = _add_dc_socket(tray, sy)
    return tray


def _add_dc_socket(tray, sy):
    """Kycon KPJX-PM-4S in the bottom wall: flange in a 1 mm pocket outside, M2.5 x 10 from inside."""
    fw, fh = ep.DC_SOCKET["flange"]
    pad = 1.4  # wall locally 4.4 mm: 3.4 mm screw grip, 1 mm pocket
    y_out = sy * HALF
    y_in = sy * (HALF - TRAY_WALL - pad)
    x, z = DC_SOCKET_X, DC_SOCKET_Z
    tray = tray.union(box(x - fw / 2 - 3, x + fw / 2 + 3, sy * (HALF - TRAY_WALL), y_in, z - fh / 2 - 3, z + fh / 2 + 3))
    tray = tray.cut(cyl_y(x, z, ep.DC_SOCKET["body_cutout"], y_out, y_in))
    tray = tray.cut(box(x - fw / 2 - 0.3, x + fw / 2 + 0.3, y_out, y_out - sy * 1.0, z - fh / 2 - 0.3, z + fh / 2 + 0.3))
    s = ep.DC_SOCKET["screw_spacing"] / 2
    for dx in (-s, s):
        tray = tray.cut(cyl_y(x + dx, z, M25_CLEAR, y_out, y_in))
    return tray


# ============================================================================================
# Rail and bumper
# ============================================================================================
def rail_z():
    lip_front = Z_WALL - WALL_SCREW["head_gap"]  # bears on the head's underside
    return lip_front, lip_front + RAIL_LIP_T


def build_rail(sx):
    lip_front, back = rail_z()
    x0, x1 = sx * RAIL_X_IN, sx * RAIL_X_OUT
    rail = box(x0, x1, RAIL_WEB_Y[0], RAIL_WEB_Y[1], Z_COVER_OUT, back)
    rail = rail.union(box(x0, x1, RAIL_LIP_Y0, RAIL_WEB_Y[1], lip_front, back))
    ym = sum(RAIL_WEB_Y) / 2
    for x in RAIL_SCREWS_X:
        rail = rail.cut(cyl(sx * x, ym, M3_PILOT, Z_COVER_OUT, Z_COVER_OUT + 12))
    return rail


def build_bumper(sx):
    x, y = sx * BUMPER["x"], BUMPER["y"]
    b = cyl(x, y, BUMPER["d"], Z_COVER_OUT, Z_WALL)
    b = b.edges(">Z").chamfer(1.0)
    return b.cut(cyl(x, y, M3_PILOT, Z_COVER_OUT, Z_COVER_OUT + 12))


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
