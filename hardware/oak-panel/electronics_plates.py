"""Electronics mounting plates and DC-socket coupon for the off-the-shelf wiring plan.

Millimetres. Prototype study, not validated. Each plate bolts to the rear face of one
trial carrier (trial_carrier.py) through four of its symmetric seam holes at (+-86, +-54)
and carries components on its rear face. Local frame: Z=0 is the plate face that touches
the carrier, +Z points toward the wall (components grow in +Z); XY is the carrier's bed
view (X right, Y up). Print each plate flat on Z=0.

Component sizes: see COMPONENTS (source per entry). Wiring: hardware/wiring/README.md.
"""

import cadquery as cq

PLATE_MM = (184.0, 120.0, 3.0)  # X, Y, thickness
CORNER_R = 6.0
SEAM_HOLES = [(sx * 86.0, sy * 54.0) for sx in (-1, 1) for sy in (-1, 1)]
SEAM_CLEARANCE = 3.4  # M3 to the carrier's 3.5 mm seam holes, nut on the carrier side
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
MODULE_CENTRES = [(-42.0, 0.0), (42.0, 0.0)]

PI_HOLES = [(-29.0, -24.5), (-29.0, 24.5), (29.0, -24.5), (29.0, 24.5)]  # 58 x 49
PI_CENTRE = (-38.0, 0.0)  # board centre; holes offset per Pi drawing (3.5 mm from the edge)
PI_STANDOFF = (6.0, 6.0, 2.2)  # outer diameter, height, pilot for M2.5 thread-forming screw

DC_SOCKET = {
    "flange": (25.2, 14.2),  # Kycon KPJX-PM-4S drawing
    "screw_spacing": 19.0,
    "screw_hole": 2.8,  # M2.5 clearance through the wall; screws thread into the flange's 2.3 mm holes
    "body_cutout": 14.0,  # ASSUMED from the drawing's side view (~13.5 mm body); test with the coupon
}


def _plate():
    x, y, t = PLATE_MM
    plate = cq.Workplane("XY").rect(x, y).extrude(t).edges("|Z").fillet(CORNER_R)
    for hx, hy in SEAM_HOLES:
        plate = plate.cut(
            cq.Workplane("XY").center(hx, hy).circle(SEAM_CLEARANCE / 2).extrude(t)
        )
    return plate


def _tie_pair(plate, x, y, span, along_x=True):
    """Two cable-tie slots `span` apart, a tie loops through them around a part."""
    w, l = TIE_SLOT
    for s in (-span / 2, span / 2):
        cx, cy = (x + s, y) if along_x else (x, y + s)
        dims = (w, l) if along_x else (l, w)
        plate = plate.cut(
            cq.Workplane("XY").center(cx, cy).rect(*dims).extrude(PLATE_MM[2])
        )
    return plate


def _vents(plate, x0, x1, y0, y1, pitch=8.0, w=3.0):
    """Row of slots for air between the panel back and the cavity."""
    x = x0
    while x + w <= x1:
        plate = plate.cut(
            cq.Workplane("XY")
            .center(x + w / 2, (y0 + y1) / 2)
            .rect(w, y1 - y0)
            .extrude(PLATE_MM[2])
        )
        x += pitch
    return plate


def build_power_plate():
    """Two 24->5 V 20 A modules, WAGOs and the inline fuse holders."""
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
                .extrude(PLATE_MM[2])
            )
    # Above the modules (+Y band): WAGO row and fuse holders, cable-tied.
    for wx in (-60.0, -30.0, 0.0, 30.0, 60.0):
        plate = _tie_pair(plate, wx, 47.0, 26.0, along_x=True)
    # Below the modules (-Y band): fuse holders and lead anchors; plus vents.
    for fx in (-60.0, -20.0, 20.0, 60.0):
        plate = _tie_pair(plate, fx, -47.0, 30.0, along_x=True)
    plate = _vents(plate, -78.0, 78.0, -58.0, -53.0)
    return plate


def build_controller_plate():
    """Pi 3B + Electrodragon, LM2596, Pro Micro and level converter."""
    plate = _plate()
    od, h, pilot = PI_STANDOFF
    t = PLATE_MM[2]
    px, py = PI_CENTRE
    for hx, hy in PI_HOLES:
        x, y = px + hx, py + hy
        plate = plate.union(
            cq.Workplane("XY").center(x, y).circle(od / 2).extrude(h).translate((0, 0, t))
        )
        plate = plate.cut(
            cq.Workplane("XY").center(x, y).circle(pilot / 2).extrude(t + h)
        )
    # LM2596 (upper right), Pro Micro + level converter (lower right): cable-tied.
    plate = _tie_pair(plate, 50.0, 30.0, 50.0, along_x=True)
    plate = _tie_pair(plate, 42.0, -18.0, 40.0, along_x=True)
    plate = _tie_pair(plate, 50.0, -40.0, 24.0, along_x=True)
    # Air under the Pi, between the standoffs.
    plate = _vents(plate, px - 22.0, px + 22.0, -16.0, 16.0)
    return plate


def build_dc_socket_coupon(wall_mm=6.0):
    """Short section of the bezel's bottom wall to test-fit the socket and the plug.

    The socket's flange sits on the OUTSIDE of the wall in a 1 mm pocket, its body passes
    through the cutout and two M2.5 screws (length 6.6 mm + wall) come from inside.
    """
    fw, fh = DC_SOCKET["flange"]
    coupon = cq.Workplane("XY").rect(40.0, 26.0).extrude(wall_mm)
    coupon = coupon.cut(
        cq.Workplane("XY").circle(DC_SOCKET["body_cutout"] / 2).extrude(wall_mm)
    )
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
    plate = PLATE_MM[2]
    air, cover = 3.0, 2.0
    out = {}
    for name, tallest in (
        ("power_plate", COMPONENTS["buck_20a"][2]),
        ("controller_plate", PI_STANDOFF[1] + COMPONENTS["pi_stack"][2]),
    ):
        rear = led_to_insert_face + carrier + plate + tallest
        out[name] = {
            "tallest_part_mm": tallest,
            "led_tip_to_part_rear_mm": rear,
            "body_depth_needed_mm": rear + air + cover,
        }
    return out
