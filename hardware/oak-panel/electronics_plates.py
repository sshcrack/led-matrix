"""Electronics mounting plates and DC-socket coupon for the off-the-shelf wiring plan.

Millimetres. Prototype study, not validated. Each plate bolts to the rear face of one
trial carrier (trial_carrier.py) through four of its symmetric seam holes and carries
components on its rear face. Local frame: Z=0 is the plate face that touches the carrier,
+Z points toward the wall (components grow in +Z); XY is the panel's rear seen from the
wall (X right, Y up, origin at the panel centre). Print each plate flat on Z=0.

Half plates: every panel has its tall connectors (HUB75 IN, power header, HUB75 OUT) in one
half of its back (PANEL_CONNECTORS, +Y here). The plates cover only the other half and stay
below PLATE_TOP_Y, so the plugs and their cables pass freely. The carrier's seam holes are
point-symmetric, so a plate can be turned 180 degrees if a panel is mounted the other way up.

Component sizes: see COMPONENTS (source per entry). Wiring: hardware/wiring/README.md.
"""

import cadquery as cq

# Connectors on the panel back, (x0, x1, y0, y1) from the panel centre, read from the owner's
# ruler photo of a Waveshare P3(2121)64X64-32S-M5.1 (2026-11-15, about +-3 mm). Plugged height
# above the PCB: HUB75 about 20 mm (owner); the power plug and its cable need more.
PANEL_CONNECTORS = {
    "hub75_in": (-66.0, -53.0, 33.0, 64.0),
    "power": (5.0, 19.0, 35.0, 53.0),
    "hub75_out": (42.0, 61.0, 35.0, 64.0),
}
CONNECTOR_BAND_Y0 = min(c[2] for c in PANEL_CONNECTORS.values())
PLATE_TOP_Y = 18.0  # plates end here: >= 15 mm below the lowest connector edge
# Panel PCB windows from the same photo: upper (connectors) and lower (flat parts only).
PANEL_WINDOWS = {"connector": (-81.0, 79.0, 10.0, 77.0), "free": (-81.0, 79.0, -79.0, -10.0)}
PANEL_LOW_PARTS = {"cc3_220uF": (-27.0, -46.0), "cc4_220uF": (27.0, -46.0)}  # electrolytics, height unmeasured

PLATE_X = 184.0
PLATE_BOTTOM_Y = -92.0
PLATE_T = 3.0
PLATE_MM = (PLATE_X, PLATE_TOP_Y - PLATE_BOTTOM_Y, PLATE_T)  # X, Y, thickness
CORNER_R = 6.0
# Four of the carrier's eight seam holes, all in the free half.
MOUNT_HOLES = [(-86.0, -54.0), (86.0, -54.0), (-54.0, -86.0), (54.0, -86.0)]
MOUNT_CLEARANCE = 3.4  # M3 to the carrier's 3.5 mm seam holes, nut on the carrier side
TIE_SLOT = (2.2, 5.0)  # cable-tie slot for ties up to 4.8 mm wide

# name: (x, y, z height above plate, source / status)
COMPONENTS = {
    "buck_20a": (74.0, 74.0, 32.0, "amazon.de B0CZP1Q6DD listing: 74 x 74 x 32 mm, 2x 5.3 mm flange holes; hole spacing estimated from photos"),
    "pi3b": (85.0, 56.0, 17.0, "Raspberry Pi 3B mechanical drawing: 85 x 56 mm, M2.5 holes 58 x 49 mm"),
    "pi_stack": (85.0, 56.0, 33.0, "Pi 3B + Electrodragon MPC1119 + plugged HUB75 ribbon: owner measured about 30 mm (rough, 2026-11-15); 33 mm keeps a 3 mm margin"),
    "lm2596": (43.0, 21.0, 14.0, "typical HW-411 module, measure"),
    "pro_micro": (33.0, 18.0, 6.0, "typical Pro Micro, measure"),
    "level_shifter": (16.0, 13.0, 4.0, "typical 4-channel BSS138 board, measure"),
    "wago_221_415": (18.6, 20.2, 8.4, "WAGO 221-415 (approx.), cable-tied"),
}

# Module flange holes: 5.3 mm, centre spacing estimated ~60 mm from listing photos. Slots
# cover 54..68 mm so the real spacing fits; M4 screw + washer + nut.
MODULE_HOLE_SPACING_RANGE = (54.0, 68.0)
MODULE_SLOT_W = 4.5
MODULE_CENTRES = [(-42.0, -46.0), (42.0, -46.0)]
POWER_TIE_ROWS = {"wago": 10.0, "fuse": -3.0}  # y of the tie rows above the modules

PI_HOLES = [(-29.0, -24.5), (-29.0, 24.5), (29.0, -24.5), (29.0, 24.5)]  # 58 x 49
PI_CENTRE = (-40.0, -44.0)  # hole-pattern centre; the board centre is PI_BOARD_OFFSET_X to +X
PI_BOARD_OFFSET_X = 10.0  # holes sit 3.5 mm from the board's -X edge
PI_STANDOFF = (6.0, 6.0, 2.2)  # outer diameter, height, pilot for M2.5 thread-forming screw
# Small boards on the controller plate: name -> (component, x, y, tie span along X)
CONTROLLER_SMALL = {
    "lm2596": ("lm2596", 52.0, 3.0, 50.0),
    "pro_micro": ("pro_micro", 50.0, -30.0, 40.0),
    "level_shifter": ("level_shifter", 50.0, -54.0, 24.0),
}

DC_SOCKET = {
    "flange": (25.2, 14.2),  # Kycon KPJX-PM-4S drawing
    "screw_spacing": 19.0,
    "screw_hole": 2.8,  # M2.5 clearance through the wall; screws thread into the flange's 2.3 mm holes
    "body_cutout": 14.0,  # ASSUMED from the drawing's side view (~13.5 mm body); test with the coupon
}


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
    for hx, hy in MOUNT_HOLES:
        plate = plate.cut(
            cq.Workplane("XY").center(hx, hy).circle(MOUNT_CLEARANCE / 2).extrude(PLATE_T)
        )
    return plate


def _tie_pair(plate, x, y, span, along_x=True):
    """Two cable-tie slots `span` apart, a tie loops through them around a part."""
    w, l = TIE_SLOT
    for s in (-span / 2, span / 2):
        cx, cy = (x + s, y) if along_x else (x, y + s)
        dims = (w, l) if along_x else (l, w)
        plate = plate.cut(cq.Workplane("XY").center(cx, cy).rect(*dims).extrude(PLATE_T))
    return plate


def _vents(plate, x0, x1, y0, y1, pitch=8.0, w=3.0):
    """Row of slots for air between the panel back and the cavity."""
    x = x0
    while x + w <= x1:
        plate = plate.cut(
            cq.Workplane("XY").center(x + w / 2, (y0 + y1) / 2).rect(w, y1 - y0).extrude(PLATE_T)
        )
        x += pitch
    return plate


def build_power_plate():
    """Two 24->5 V 20 A modules, with WAGOs and inline fuse holders cable-tied above them."""
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
    for wx in (-60.0, -30.0, 0.0, 30.0, 60.0):
        plate = _tie_pair(plate, wx, POWER_TIE_ROWS["wago"], 26.0, along_x=True)
    for fx in (-60.0, -20.0, 20.0, 60.0):
        plate = _tie_pair(plate, fx, POWER_TIE_ROWS["fuse"], 30.0, along_x=True)
    # Air between the two modules.
    plate = _vents(plate, -1.5, 1.5, -78.0, -14.0)
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
    for _, x, y, span in CONTROLLER_SMALL.values():
        plate = _tie_pair(plate, x, y, span, along_x=True)
    # Air under the Pi, between the standoffs.
    plate = _vents(plate, px - 22.0, px + 22.0, py - 16.0, py + 16.0)
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


def depth_budget():
    """Depth from the LED tips to each plate's tallest part, and the body depth it needs."""
    led_to_insert_face = 15.0  # owner measurement, measurement-basis.json
    carrier = 16.0  # trial carrier, boss face to rear face
    air, cover = 3.0, 2.0
    out = {}
    for name, tallest in (
        ("power_plate", COMPONENTS["buck_20a"][2]),
        ("controller_plate", PI_STANDOFF[1] + COMPONENTS["pi_stack"][2]),
    ):
        rear = led_to_insert_face + carrier + PLATE_T + tallest
        out[name] = {
            "tallest_part_mm": tallest,
            "led_tip_to_part_rear_mm": rear,
            "body_depth_needed_mm": rear + air + cover,
        }
    return out
