"""Electronics plates (in-carrier half plates) and DC-socket coupon for the off-the-shelf wiring plan.

Millimetres. Prototype study, not validated. Local frame: XY is the panel's rear seen from the
wall (X right, Y up, origin at the panel centre); Z=0 is the plate's underside (panel side),
+Z points toward the wall and components grow in +Z. Print each plate flat on Z=0.

Placement: a plate lies INSIDE a trial carrier (trial_carrier.py), in the carrier's open middle,
PANEL_GAP above the panel's plastic frame. Four posts rise to the underside of the carrier's
rear web; M3 screws go from the carrier's rear face through four seam holes into the posts.
Lay the plate on the panel, put the carrier over it (its corner bosses pass through the plate's
notches), screw the carrier to the panel, then screw the posts. Parts taller than the posts
must stay inside the carrier's opening (CARRIER_OPENING_HALF).

Half plates: every panel has its tall connectors (HUB75 IN, power header, HUB75 OUT) in one
half of its back (PANEL_CONNECTORS, +Y here). The plates cover only the other half and stay
below PLATE_TOP_Y, so the plugs and their cables pass freely. The seam holes are symmetric under
90 degree turns, so a plate can be turned to whichever half of its panel is free.

Component sizes: see COMPONENTS (source per entry). Wiring: hardware/wiring/README.md.
"""

import cadquery as cq
from trial_carrier import BOSS_MM, OUTER_MM, PATTERN, SPACER_MM

# --- Panel rear (owner measurements and ruler photos, 2026-11-15) ---------------------------
LED_TO_FRAME_TOP = 15.0  # LED tips to the plastic frame top / insert face, measurement-basis.json
PCB_RECESS = 12.0  # frame top down to the PCB inside the windows (owner)
PCB_REAR = LED_TO_FRAME_TOP - PCB_RECESS  # ~3 mm from the LED tips
LOW_PART_H = 5.0  # 220 uF electrolytics on the PCB (owner estimate)
# Connectors, (x0, x1, y0, y1) from the panel centre, read from the owner's ruler photo of a
# Waveshare P3(2121)64X64-32S-M5.1 (about +-3 mm). All four panels hang the same way up.
PANEL_CONNECTORS = {
    "hub75_in": (-66.0, -53.0, 33.0, 64.0),
    "power": (5.0, 19.0, 35.0, 53.0),
    "hub75_out": (42.0, 61.0, 35.0, 64.0),
}
# Plug tops from the LED tips: HUB75 plug 20 mm above the PCB (owner); power cable bend 32 mm
# (owner, measurement-basis.json led_tip_to_cable_bend_mm).
PLUG_TOP = {"hub75_in": PCB_REAR + 20.0, "power": 32.0, "hub75_out": PCB_REAR + 20.0}
CONNECTOR_BAND_Y0 = min(c[2] for c in PANEL_CONNECTORS.values())
PANEL_WINDOWS = {"connector": (-81.0, 79.0, 10.0, 77.0), "free": (-81.0, 79.0, -79.0, -10.0)}
PANEL_LOW_PARTS = {"cc3_220uF": (-27.0, -46.0), "cc4_220uF": (27.0, -46.0)}

# --- Plate -----------------------------------------------------------------------------------
PANEL_GAP = 1.0  # plate underside above the frame top
PLATE_T = 2.5
PLATE_Z0 = LED_TO_FRAME_TOP + PANEL_GAP  # plate underside, mm from the LED tips
PLATE_X = 184.0
PLATE_TOP_Y = 18.0  # plates end here: >= 15 mm below the lowest connector edge
PLATE_BOTTOM_Y = -92.0
PLATE_MM = (PLATE_X, PLATE_TOP_Y - PLATE_BOTTOM_Y, PLATE_T)  # X, Y, thickness
CORNER_R = 6.0
CARRIER_OPENING_HALF = (OUTER_MM - 32.0) / 2  # 79.8: inner edge of the carrier's rear web
OPENING_MARGIN = 2.0
# Four of the carrier's eight seam holes, all in the free half; posts reach the web underside.
MOUNT_HOLES = [(-86.0, -54.0), (86.0, -54.0), (-54.0, -86.0), (54.0, -86.0)]
POST_OD = 8.0
POST_PILOT = 2.5  # M3 thread-forming screw from the carrier's rear face (M3 x 12)
POST_H = SPACER_MM - PANEL_GAP - PLATE_T  # plate top to the web underside
BOSS_CLEARANCE = BOSS_MM + 2.0  # notches for the carrier's corner bosses
SADDLE = (3.0, 2.0)  # rib width, height: parts sit on two ribs; the tie passes under the part

# name: (x, y, z height, source / status)
COMPONENTS = {
    "buck_20a": (74.0, 74.0, 32.0, "amazon.de B0CZP1Q6DD listing drawing: 74 x 74 x 32 mm, 2x 5.3 mm flange holes ~66 mm apart, wires exit a hole-free side"),
    "pi3b": (85.0, 56.0, 17.0, "Raspberry Pi 3B mechanical drawing: 85 x 56 mm, M2.5 holes 58 x 49 mm"),
    "pi_stack": (85.0, 56.0, 33.0, "Pi 3B + Electrodragon MPC1119 + plugged HUB75 ribbon: owner measured about 30 mm (rough, 2026-11-15); 33 mm keeps a 3 mm margin"),
    "lm2596": (43.0, 21.0, 14.0, "typical HW-411 module, measure"),
    "pro_micro": (33.0, 18.0, 6.0, "typical Pro Micro, measure"),
    "level_shifter": (16.0, 13.0, 4.0, "typical 4-channel BSS138 board, measure"),
    "wago_221_415": (18.6, 20.2, 8.4, "WAGO 221-415 (approx.)"),
    "fuse_holder": (30.0, 12.0, 12.0, "inline blade fuse holder body (approx.), cable-tied"),
}

# Module flange holes: 5.3 mm, centred on two opposite sides (the wires leave from one of the
# other two sides). Spacing ~65.7 mm, scaled from the listing's dimension drawing (74 mm body =
# 404 px; holes 359 px apart, 4.1 mm in from the edge); not measured on a part. Slots cover
# 62..70 mm so +/-4 mm of error still fits; M4 screw + washer + nut.
MODULE_HOLE_SPACING_RANGE = (62.0, 70.0)
MODULE_SLOT_W = 4.5
MODULE_CENTRES = [(-40.0, -40.5), (40.0, -40.5)]

PI_HOLES = [(-29.0, -24.5), (-29.0, 24.5), (29.0, -24.5), (29.0, 24.5)]  # 58 x 49
PI_CENTRE = (-40.0, -44.0)  # hole-pattern centre; the board centre is PI_BOARD_OFFSET_X to +X
PI_BOARD_OFFSET_X = 10.0  # holes sit 3.5 mm from the board's -X edge
PI_STANDOFF = (6.0, 3.5, 2.2)  # outer diameter, height, pilot for M2.5 thread-forming screw

# Cable-tied parts on saddles: plate -> {name: (component, x, y)}
SADDLE_PARTS = {
    "power-plate": {
        **{f"wago_{i}": ("wago_221_415", x, 8.0) for i, x in enumerate((-50.0, -25.0, 0.0, 25.0, 50.0))},
    },
    "controller-plate": {
        "lm2596": ("lm2596", 52.0, 3.0),
        "pro_micro": ("pro_micro", 52.0, -28.0),
        "level_shifter": ("level_shifter", 52.0, -52.0),
    },
}
# Inline fuse holders ride along their cables (no fixed seat).

DC_SOCKET = {
    "flange": (25.2, 14.2),  # Kycon KPJX-PM-4S drawing
    "screw_spacing": 19.0,
    "screw_hole": 2.8,  # M2.5 clearance through the wall; screws thread into the bosses' 2.3 mm holes (Kycon drawing)
    "body_cutout": 14.0,  # body at most 12.6 mm (Kycon IGES of KPJX-PM-4S-S, 2026-11-18); bosses need their own pockets
}


def placements(plate):
    """Component envelopes on a plate: [(name, component, x, y, z0 above the plate top)]."""
    if plate == "power-plate":
        out = [(f"module_{'AB'[i]}", "buck_20a", x, y, 0.0) for i, (x, y) in enumerate(MODULE_CENTRES)]
    else:
        out = [("pi_stack", "pi_stack", PI_CENTRE[0] + PI_BOARD_OFFSET_X, PI_CENTRE[1], PI_STANDOFF[1])]
    out += [(n, c, x, y, SADDLE[1]) for n, (c, x, y) in SADDLE_PARTS[plate].items()]
    return out


def _plate():
    y_mid = (PLATE_TOP_Y + PLATE_BOTTOM_Y) / 2
    plate = (
        cq.Workplane("XY")
        .center(0, y_mid)
        .rect(PLATE_X, PLATE_TOP_Y - PLATE_BOTTOM_Y)
        .extrude(PLATE_T)
        .edges("|Z")
        .fillet(CORNER_R)
    )
    for bx, by in PATTERN:
        plate = plate.cut(cq.Workplane("XY").center(bx, by).circle(BOSS_CLEARANCE / 2).extrude(PLATE_T))
    for hx, hy in MOUNT_HOLES:
        post = cq.Workplane("XY").center(hx, hy).circle(POST_OD / 2).extrude(POST_H).translate((0, 0, PLATE_T))
        plate = plate.union(post)
        plate = plate.cut(cq.Workplane("XY").center(hx, hy).circle(POST_PILOT / 2).extrude(PLATE_T + POST_H))
    return plate


def _saddle(plate, x, y, w, d):
    """Two ribs under a part; a cable tie loops over the part and through the gap under it."""
    rw, rh = SADDLE
    for sx in (-1, 1):
        rib = (
            cq.Workplane("XY")
            .center(x + sx * (w / 2 - rw / 2 - 1.0), y)
            .rect(rw, max(d - 2.0, 6.0))
            .extrude(rh)
            .translate((0, 0, PLATE_T))
        )
        plate = plate.union(rib)
    return plate


def _vents(plate, x0, x1, y0, y1, pitch=8.0, w=3.0):
    """Slots for air between the panel back and the cavity."""
    x = x0
    while x + w <= x1:
        plate = plate.cut(
            cq.Workplane("XY").center(x + w / 2, (y0 + y1) / 2).rect(w, y1 - y0).extrude(PLATE_T)
        )
        x += pitch
    return plate


def _add_saddles(plate, name):
    for comp, x, y in SADDLE_PARTS[name].values():
        w, d = COMPONENTS[comp][:2]
        plate = _saddle(plate, x, y, w, d)
    return plate


def build_power_plate():
    """Two 24->5 V 20 A modules and the WAGO row; fuse holders ride along their cables."""
    plate = _plate()
    lo, hi = MODULE_HOLE_SPACING_RANGE
    for mx, my in MODULE_CENTRES:
        for side in (-1, 1):
            # Flange holes sit left/right of the module centre along X.
            a, b = mx + side * lo / 2, mx + side * hi / 2
            plate = plate.cut(
                cq.Workplane("XY")
                .center((a + b) / 2, my)
                .slot2D(abs(b - a) + MODULE_SLOT_W, MODULE_SLOT_W)
                .extrude(PLATE_T)
            )
    plate = _add_saddles(plate, "power-plate")
    plate = _vents(plate, -1.5, 1.5, -74.0, -10.0)  # between the modules
    return plate


def build_controller_plate():
    """Pi 3B + Electrodragon, LM2596, Pro Micro and level converter."""
    plate = _plate()
    od, h, pilot = PI_STANDOFF
    px, py = PI_CENTRE
    for hx, hy in PI_HOLES:
        x, y = px + hx, py + hy
        plate = plate.union(
            cq.Workplane("XY").center(x, y).circle(od / 2).extrude(h).translate((0, 0, PLATE_T))
        )
        plate = plate.cut(cq.Workplane("XY").center(x, y).circle(pilot / 2).extrude(PLATE_T + h))
    plate = _add_saddles(plate, "controller-plate")
    plate = _vents(plate, px - 22.0, px + 22.0, py - 16.0, py + 16.0)  # under the Pi
    return plate


def build_dc_socket_coupon(wall_mm=6.0):
    """Short section of the bezel's bottom wall to test-fit the socket and the plug.

    The socket's flange sits on the OUTSIDE of the wall in a 1 mm pocket, its body passes
    through the cutout and two M2.5 screws (length 6.6 mm + wall) come from inside.
    """
    fw, fh = DC_SOCKET["flange"]
    coupon = cq.Workplane("XY").rect(40.0, 26.0).extrude(wall_mm)
    coupon = coupon.cut(cq.Workplane("XY").circle(DC_SOCKET["body_cutout"] / 2).extrude(wall_mm))
    s = DC_SOCKET["screw_spacing"] / 2
    for dy in (-s, s):
        coupon = coupon.cut(
            cq.Workplane("XY").center(0, dy).circle(DC_SOCKET["screw_hole"] / 2).extrude(wall_mm)
        )
    # Rotate 90 deg so the 25.2 mm flange runs along the coupon's 40 mm length.
    coupon = coupon.rotate((0, 0, 0), (0, 0, 1), 90)
    pocket = (
        cq.Workplane("XY")
        .rect(fh + 0.6, fw + 0.6)
        .extrude(1.0)
        .edges("|Z")
        .fillet(3.0)
        .rotate((0, 0, 0), (0, 0, 1), 90)
        .translate((0, 0, wall_mm - 1.0))
    )
    return coupon.cut(pocket)


# Halo strip channel: the strip (LEDs toward the wall) slides in from one end and two lips hold
# its edges, so no adhesive is needed (IP65 silicone sleeves take tape badly, and it is warm behind
# the display). Owner's strip measured 10.0 x 2.63 mm (2026-11-15); the coupon tries three inner
# widths around it. The opening between the lips stays wider than the 5 mm LEDs.
HALO_STRIP_MM = (10.0, 2.63)  # width, thickness (owner measurement)
HALO_CHANNEL = {
    "inner_widths": (10.4, 10.7, 11.0),  # printed channels come out a bit narrow
    "inner_height": 3.3,  # strip 2.63 mm + 0.7 (lips sag a little when printed)
    "lip": 1.2,  # each side: opening 8.0..8.6 mm
    "wall": 1.6,
    "floor": 1.6,
    "length": 70.0,  # two LEDs at 30/m
}


def build_halo_channel_coupon():
    """Three short channels side by side to test which width holds the halo strip."""
    c = HALO_CHANNEL
    wall, floor, h, lip, length = c["wall"], c["floor"], c["inner_height"], c["lip"], c["length"]
    total_w = sum(w + wall for w in c["inner_widths"]) + wall
    height = floor + h + lip
    coupon = cq.Workplane("XY").box(total_w, length, height, centered=(False, True, False))
    x = wall
    for w in c["inner_widths"]:
        coupon = coupon.cut(cq.Workplane("XY").box(w, length, h, centered=(False, True, False)).translate((x, 0, floor)))
        coupon = coupon.cut(
            cq.Workplane("XY").box(w - 2 * lip, length, lip, centered=(False, True, False)).translate((x + lip, 0, floor + h))
        )
        # Width label: notches on the near end, one per millimetre above 8.
        for k in range(c["inner_widths"].index(w) + 1):
            coupon = coupon.cut(
                cq.Workplane("XY").box(1.0, 1.0, 1.0).translate((x + w / 2 - 1.5 * k, -length / 2 + 0.5, floor / 2))
            )
        x += w + wall
    return coupon.translate((-total_w / 2, 0, 0))


def depth_budget():
    """Depth from the LED tips to each plate's tallest part, and the body depth it needs."""
    air, cover = 3.0, 2.0
    out = {}
    for name in ("power-plate", "controller-plate"):
        tallest = max(z0 + COMPONENTS[c][2] for _, c, _, _, z0 in placements(name))
        rear = PLATE_Z0 + PLATE_T + tallest
        out[name] = {
            "tallest_part_above_plate_mm": tallest,
            "led_tip_to_part_rear_mm": rear,
            "body_depth_needed_mm": rear + air + cover,
        }
    return out
