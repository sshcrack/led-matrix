"""One-panel mounting trial. Local Z=0 is the flat rear print face; panel is +Z.

XY is centred and read in the "bed view": looking down on the part as printed,
bosses toward the viewer (+Z toward the viewer), X right, Y up. That is also the
panel's rear seen from behind with the carrier fitted. The +Y edge carries the
orientation notch.
"""

import cadquery as cq

OUTER_MM = 191.6
WEB_MM = 4.0
SPACER_MM = 12.0
RIM_MM = 6.0
BORE_MM = 3.5
BOSS_MM = 10.0
PATTERN = [(-87, -79), (-87, 79), (87, -79), (87, 79)]
# Provisional seam-joiner interfaces on all four edges, so one carrier fits any
# tile of the 2x2 array. Not additional panel insert positions.
JOINER_HOLES = [
    (sx * 86, sy * 54) for sx in (-1, 1) for sy in (-1, 1)
] + [(sx * 54, sy * 86) for sx in (-1, 1) for sy in (-1, 1)]
# Sight holes over the drawing's intermediate inserts. The drawing's front/rear
# handedness is unlabelled, so both (+7.5, +87)/(-7.5, -87) and the mirrored
# pair are cut: whichever pair shows a centred insert identifies the variant.
SIGHT_HOLE_MM = 5.0
SIGHT_HOLES = [(-86, 0), (86, 0), (7.5, 87), (-7.5, -87), (-7.5, 87), (7.5, -87)]
# Orientation notch in the top outer corner of the +Y rib, centred on X=0.
NOTCH_MM = (8.0, 2.0, 2.0)  # X width, Y depth from outer face, Z depth from rib top


def build_trial_carrier():
    body = (
        cq.Workplane("XY")
        .rect(OUTER_MM, OUTER_MM)
        .rect(OUTER_MM - 32, OUTER_MM - 32)
        .extrude(WEB_MM)
    )
    rim = (
        cq.Workplane("XY")
        .rect(OUTER_MM, OUTER_MM)
        .rect(OUTER_MM - 8, OUTER_MM - 8)
        .extrude(RIM_MM)
        .translate((0, 0, WEB_MM))
    )
    body = body.union(rim)
    for x, y in PATTERN:
        boss = (
            cq.Workplane("XY")
            .circle(BOSS_MM / 2)
            .extrude(SPACER_MM)
            .translate((x, y, WEB_MM))
        )
        body = body.union(boss)
        body = body.cut(
            cq.Workplane("XY")
            .circle(BORE_MM / 2)
            .extrude(WEB_MM + SPACER_MM)
            .translate((x, y, 0))
        )
    for x, y in JOINER_HOLES:
        body = body.cut(
            cq.Workplane("XY")
            .circle(BORE_MM / 2)
            .extrude(WEB_MM + RIM_MM)
            .translate((x, y, 0))
        )
    for x, y in SIGHT_HOLES:
        body = body.cut(
            cq.Workplane("XY")
            .circle(SIGHT_HOLE_MM / 2)
            .extrude(WEB_MM)
            .translate((x, y, 0))
        )
    nx, ny, nz = NOTCH_MM
    body = body.cut(
        cq.Workplane("XY")
        .box(nx, ny, nz, centered=(True, False, False))
        .translate((0, OUTER_MM / 2 - ny, WEB_MM + RIM_MM - nz))
    )
    return body
