"""One-panel mounting trial. Local Z=0 is the flat rear print face; panel is +Z."""

import cadquery as cq

OUTER_MM = 191.6
WEB_MM = 4.0
SPACER_MM = 12.0
RIM_MM = 6.0
BORE_MM = 3.5
BOSS_MM = 10.0
PATTERN = [(-87, -79), (-87, 79), (87, -79), (87, 79)]
# Bottom-left tile's provisional internal seam interfaces from the v0.6 concept.
JOINER_HOLES = [(86, -54), (-54, 86)]


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
        # Future seam joiner positions, not additional panel insert positions.
        body = body.cut(
            cq.Workplane("XY")
            .circle(BORE_MM / 2)
            .extrude(WEB_MM + RIM_MM)
            .translate((x, y, 0))
        )
    return body
