"""Wall-mounted P3 enclosure packaging study. Millimetres. Not manufacturing release.

XY = display plane; +Z runs from the viewer toward the wall.
Run with the Python interpreter from the virtual environment described in README.md.
The LED modules are bounding envelopes, not supplier CAD.
"""

from __future__ import annotations
import argparse
from dataclasses import dataclass, asdict
import json
from pathlib import Path
from itertools import combinations
import cadquery as cq


@dataclass(frozen=True)
class Parameters:
    # Supplier nominal information, not measured on the owner's batch.
    panel_side: float = 192.0
    panel_pixels: int = 64
    # Panel depths below use approximate owner measurements; other dimensions are design assumptions.
    border: float = 8.0
    outer_corner_radius: float = 5.0
    bezel_seam_gap: float = 0.4
    backlight_strip_width: float = 10.0
    backlight_strip_thickness: float = 3.0
    backlight_strip_length: float = 160.0
    backlight_strip_offset: float = 179.0
    depth: float = 60.0
    cavity_clearance_per_side: float = 2.0
    front_lip_depth: float = 1.2
    fabric_front: float = 1.3
    fabric_thickness: float = 0.3
    diffuser_gap: float = 0.5
    carrier_outer_clearance_per_side: float = 0.4
    carrier_aperture_clearance_per_side: float = 0.2
    carrier_depth: float = 2.0
    panel_envelope_depth: float = 15.0
    connector_envelope_depth: float = 17.0
    spacer_length: float = 12.0
    mounting_half_width: float = 85.0
    mounting_row_pitch: float = 73.0
    mounting_clearance_diameter: float = 3.5
    spacer_outer_diameter: float = 6.5
    screw_head_clearance_diameter: float = 6.0
    screw_head_clearance_height: float = 3.0
    chassis_thickness: float = 4.0
    chassis_edge_clearance_per_side: float = 1.0
    electronics_front: float = 37.1
    electronics_depth: float = 18.0
    cover_front: float = 57.0
    cover_thickness: float = 2.0
    cover_edge_clearance_per_side: float = 0.5
    wall_clearance: float = 25.0

    @property
    def display(self):
        return self.panel_side * 2

    @property
    def outer(self):
        return self.display + self.border * 2

    @property
    def cavity(self):
        return self.display + self.cavity_clearance_per_side * 2

    @property
    def carrier_outer(self):
        return self.cavity - self.carrier_outer_clearance_per_side * 2

    @property
    def carrier_inner(self):
        return self.display + self.carrier_aperture_clearance_per_side * 2

    @property
    def led_front(self):
        return self.fabric_front + self.fabric_thickness + self.diffuser_gap

    @property
    def panel_back(self):
        return self.led_front + self.panel_envelope_depth

    @property
    def chassis_front(self):
        return self.panel_back + self.spacer_length

    @property
    def cover_side(self):
        return self.cavity - self.cover_edge_clearance_per_side * 2

    @property
    def chassis_side(self):
        return self.cavity - self.chassis_edge_clearance_per_side * 2


def box(w, h, d, x=0, y=0, z=0):
    return (
        cq.Workplane("XY")
        .box(w, h, d, centered=(True, True, False))
        .translate((x, y, z))
    )


def ring(outer, inner, depth, z):
    return (
        cq.Workplane("XY")
        .rect(outer, outer)
        .rect(inner, inner)
        .extrude(depth)
        .translate((0, 0, z))
    )


def build(p):
    assert p.border > p.cavity_clearance_per_side
    assert 0 < p.outer_corner_radius < p.border
    assert p.wall_clearance > p.backlight_strip_thickness
    assert 0 < p.bezel_seam_gap < 1
    assert p.carrier_outer > p.carrier_inner
    assert p.fabric_front >= p.front_lip_depth
    assert p.panel_back < p.chassis_front
    assert p.spacer_length > 0
    assert p.spacer_outer_diameter > p.mounting_clearance_diameter
    assert p.mounting_half_width + p.spacer_outer_diameter / 2 < p.panel_side / 2
    assert p.mounting_row_pitch + p.spacer_outer_diameter / 2 < p.panel_side / 2
    assert p.chassis_front + p.chassis_thickness < p.electronics_front
    assert p.electronics_front + p.electronics_depth < p.cover_front
    assert p.cover_front + p.cover_thickness < p.depth
    components = []

    def add(name, shape, role, material, color, note, explode):
        components.append(
            dict(
                name=name,
                shape=shape,
                role=role,
                material=material,
                color=color,
                note=note,
                explode=explode,
            )
        )

    # Narrow black printed bezel, split into four bed-sized L quadrants.
    shell = box(p.outer, p.outer, p.depth).edges("|Z").fillet(p.outer_corner_radius)
    shell = shell.cut(box(p.display, p.display, p.depth + 2, z=-1))
    shell = shell.cut(box(p.cavity, p.cavity, p.depth, z=p.front_lip_depth))
    for sign in [-1, 1]:
        y = sign * (p.cavity + p.outer) / 4
        for x in range(-136, 137, 34):
            shell = shell.cut(box(26, p.border + 4, 7, x, y, p.depth - 6))
    shell = shell.cut(
        box(10, p.border + 4, 12, 0, -(p.cavity + p.outer) / 4, p.depth - 11)
    )
    half = p.outer / 2 - p.bezel_seam_gap / 2
    centre = half / 2 + p.bezel_seam_gap / 2
    for ix in [-1, 1]:
        for iy in [-1, 1]:
            mask = box(half, half, p.depth + 2, ix * centre, iy * centre, -1)
            quadrant = shell.intersect(mask)
            add(
                f"printed_bezel_{ix}_{iy}",
                quadrant,
                "concept_part",
                "PETG",
                (0.065, 0.065, 0.07),
                "Black L-shaped quadrant, front face down. Seam alignment, corner joints and attachment unvalidated.",
                0,
            )

    add(
        "diffuser_carrier",
        ring(
            p.carrier_outer,
            p.carrier_inner,
            p.carrier_depth,
            p.fabric_front + p.fabric_thickness,
        ),
        "reference",
        "optical carrier envelope only",
        (0.19, 0.21, 0.23),
        "Carrier envelope only; tensioning/clamps and retention not designed.",
        -12,
    )
    add(
        "fabric_sample",
        box(p.carrier_outer, p.carrier_outer, p.fabric_thickness, z=p.fabric_front),
        "reference",
        "fabric sample",
        (0.77, 0.78, 0.72),
        "Nominal thickness; optical and tension behaviour untested.",
        -26,
    )
    for ix in range(2):
        for iy in range(2):
            x = (ix - 0.5) * p.panel_side
            y = (iy - 0.5) * p.panel_side
            n = f"panel_{'left' if ix == 0 else 'right'}_{'bottom' if iy == 0 else 'top'}"
            add(
                n,
                box(
                    p.panel_side,
                    p.panel_side,
                    p.panel_envelope_depth,
                    x,
                    y,
                    p.led_front,
                ),
                "reference",
                "supplier panel envelope",
                (0.08, 0.19, 0.13),
                "Supplier nominal XY; owner reports 15 mm to mounting face. Mount holes omitted.",
                12,
            )
            add(
                n + "_connector_space",
                box(50, 40, p.connector_envelope_depth, x, y, p.panel_back),
                "keepout",
                "reserved space",
                (0.85, 0.56, 0.15),
                "Owner reports about 32 mm total from LED tips with this cable bend. XY location/extent assumed.",
                12,
            )

    # Each panel gets a ribbed carrier with integrated spacer bosses.
    # Tile pitch is 192 mm; 191.6 mm bodies leave 0.4 mm seam clearance.
    joint_holes = [(x, y) for y in [-150, 150] for x in [-10, 10]]
    joint_holes += [(x, y) for x in [-150, 150] for y in [-10, 10]]
    for ix, px in enumerate([-p.panel_side / 2, p.panel_side / 2]):
        for iy, py in enumerate([-p.panel_side / 2, p.panel_side / 2]):
            side = p.panel_side - 0.4
            body = ring(
                side, side - 32, p.chassis_thickness, p.chassis_front
            ).translate((px, py, 0))
            rib = ring(side, side - 8, 6, p.chassis_front - 6).translate((px, py, 0))
            body = body.union(rib)
            for dx in [-p.mounting_half_width, p.mounting_half_width]:
                for dy in [-p.mounting_row_pitch, 0, p.mounting_row_pitch]:
                    x, y = px + dx, py + dy
                    boss = (
                        cq.Workplane("XY")
                        .circle(5)
                        .extrude(p.spacer_length)
                        .translate((x, y, p.panel_back))
                    )
                    body = body.union(boss)
                    bore = (
                        cq.Workplane("XY")
                        .circle(p.mounting_clearance_diameter / 2)
                        .extrude(p.spacer_length + p.chassis_thickness + 2)
                        .translate((x, y, p.panel_back - 1))
                    )
                    body = body.cut(bore)
                    head = (
                        cq.Workplane("XY")
                        .circle(p.screw_head_clearance_diameter / 2)
                        .extrude(p.screw_head_clearance_height)
                        .translate((x, y, p.chassis_front + p.chassis_thickness))
                    )
                    add(
                        f"screw_head_space_{ix}_{iy}_{dx}_{dy}",
                        head,
                        "keepout",
                        "reserved space",
                        (0.85, 0.56, 0.15),
                        "M3 head clearance; actual screw length and insert engagement unselected.",
                        34,
                    )
            for x, y in joint_holes:
                if abs(x - px) < side / 2 and abs(y - py) < side / 2:
                    bore = (
                        cq.Workplane("XY")
                        .circle(1.75)
                        .extrude(p.chassis_thickness + 2)
                        .translate((x, y, p.chassis_front - 1))
                    )
                    body = body.cut(bore)
            add(
                f"printed_carrier_{ix}_{iy}",
                body,
                "concept_part",
                "PETG",
                (0.30, 0.33, 0.36),
                "Ribbed print concept with six integrated 12 mm spacer bosses. Print flat rear face down. Strength/creep untested.",
                34,
            )
    for n, (x, y, angle) in enumerate(
        [(0, -150, 0), (0, 150, 0), (-150, 0, 90), (150, 0, 90)]
    ):
        bridge = box(50, 16, 4, z=p.chassis_front + p.chassis_thickness)
        for dx in [-10, 10]:
            bridge = bridge.cut(
                cq.Workplane("XY")
                .circle(1.75)
                .extrude(6)
                .translate((dx, 0, p.chassis_front + p.chassis_thickness - 1))
            )
        bridge = bridge.rotate((0, 0, 0), (0, 0, 1), angle).translate((x, y, 0))
        add(
            f"printed_joiner_{n}",
            bridge,
            "concept_part",
            "PETG",
            (0.50, 0.53, 0.56),
            "Bolted seam bridge, two M3 through-holes. Bolt/nut access and structural performance require fit tests.",
            38,
        )
    add(
        "controller_space",
        box(100, 75, p.electronics_depth, -65, 0, p.electronics_front),
        "keepout",
        "reserved space",
        (0.25, 0.50, 0.86),
        "Not an actual Pi or carrier. Validate selected board, headers, and harness.",
        42,
    )
    add(
        "power_board_space",
        box(140, 90, p.electronics_depth, 85, 0, p.electronics_front),
        "keepout",
        "reserved space",
        (0.85, 0.56, 0.15),
        "Packaging reservation only; no PCB or regulator layout exists.",
        42,
    )
    add(
        "antenna_space",
        box(28, 42, p.electronics_depth, 168, 140, p.electronics_front),
        "keepout",
        "reserved space",
        (0.59, 0.37, 0.82),
        "RF placement reservation; antenna choice and keepouts require RF validation.",
        42,
    )

    cover = box(p.cover_side, p.cover_side, p.cover_thickness, z=p.cover_front)
    for y in [-165, 165]:
        for x in range(-140, 141, 40):
            cover = cover.cut(
                box(28, 4, p.cover_thickness + 2, x, y, p.cover_front - 1)
            )
    cover = cover.cut(
        box(10, 16, p.cover_thickness + 2, 0, -p.cover_side / 2 + 4, p.cover_front - 1)
    )
    # Reserved pass-through for the optional rear-light harness.
    cover = cover.cut(box(6, 10, p.cover_thickness + 2, 170, 0, p.cover_front - 1))
    for ix in [-1, 1]:
        for iy in [-1, 1]:
            half = p.cover_side / 2 - 0.2
            mask = box(
                half,
                half,
                p.cover_thickness + 2,
                ix * (half / 2 + 0.2),
                iy * (half / 2 + 0.2),
                p.cover_front - 1,
            )
            tile = cover.intersect(mask)
            add(
                f"printed_cover_{ix}_{iy}",
                tile,
                "concept_part",
                "PETG",
                (0.31, 0.34, 0.36),
                "Split vented cover concept; attachment/clips and anti-warp support not designed.",
                66,
            )
    add(
        "halo_driver_space",
        box(16, 24, p.electronics_depth, 170, 0, p.electronics_front),
        "keepout",
        "reserved space",
        (0.8, 0.5, 0.2),
        "Optional rear-light driver allowance; interface and power budget unselected.",
        42,
    )
    add(
        "halo_wire_passage",
        box(
            6,
            10,
            p.cover_front
            + p.cover_thickness
            + p.backlight_strip_thickness
            - p.electronics_front
            - p.electronics_depth,
            170,
            0,
            p.electronics_front + p.electronics_depth,
        ),
        "keepout",
        "reserved space",
        (0.8, 0.5, 0.2),
        "Harness passage through the split rear cover; actual wire/connector and edge protection unselected.",
        66,
    )
    # Rear light-strip volumes are reservations, not an electrically selected product.
    # Eight short segments avoid bridging the split-cover seams.
    backlight_z = p.cover_front + p.cover_thickness
    for side in [-1, 1]:
        for segment in [-1, 1]:
            for axis in ["horizontal", "vertical"]:
                length = p.backlight_strip_length
                width = p.backlight_strip_width
                along = segment * (length / 2 + 4)
                if axis == "horizontal":
                    shape = box(
                        length,
                        width,
                        p.backlight_strip_thickness,
                        along,
                        side * p.backlight_strip_offset,
                        backlight_z,
                    )
                else:
                    shape = box(
                        width,
                        length,
                        p.backlight_strip_thickness,
                        side * p.backlight_strip_offset,
                        along,
                        backlight_z,
                    )
                add(
                    f"halo_strip_{axis}_{side}_{segment}",
                    shape,
                    "keepout",
                    "rear LED-strip reservation",
                    (0.8, 0.5, 0.2),
                    "Rear-facing LED strip envelope only; type, driver, wattage, optics and mounting unselected.",
                    70,
                )
    add(
        "hanger_space",
        box(280, 30, 8, 0, 130, p.depth),
        "keepout",
        "reserved space",
        (0.42, 0.47, 0.53),
        "Reserve for rated cleat or hanger; no load-bearing hardware designed.",
        80,
    )
    for x in [-140, 140]:
        add(
            "bumper_space_" + str(x),
            box(20, 20, p.wall_clearance, x, -150, p.depth),
            "keepout",
            "reserved space",
            (0.42, 0.47, 0.53),
            "Wall standoff reservation; dimensions assumed.",
            80,
        )
    return components


def export(p, components, out):
    out.mkdir(parents=True, exist_ok=True)
    partsdir = out / "parts"
    partsdir.mkdir(exist_ok=True)
    assembled = cq.Assembly(name="oak_p3_packaging_concept")
    exploded = cq.Assembly(name="oak_p3_exploded_concept")
    reserves = cq.Assembly(name="space_reservations_not_hardware")
    for c in components:
        target = reserves if c["role"] == "keepout" else assembled
        target.add(c["shape"], name=c["name"], color=cq.Color(*c["color"]))
        if c["role"] != "keepout":
            exploded.add(
                c["shape"].translate((0, 0, c["explode"])),
                name=c["name"],
                color=cq.Color(*c["color"]),
            )
        if c["role"] == "concept_part":
            cq.exporters.export(c["shape"], str(partsdir / (c["name"] + ".step")))
    assembled.export(str(out / "assembly.step"))
    exploded.export(str(out / "exploded.step"))
    reserves.export(str(out / "space-reservations.step"))
    # Two dimensionless vector CAD views, additionally used in the HTML presentation.
    cq.exporters.export(
        assembled.toCompound(),
        str(out / "isometric.svg"),
        opt={
            "width": 900,
            "height": 700,
            "projectionDir": (1, -1, -1.1),
            "showHidden": False,
            "strokeWidth": 0.5,
            "marginLeft": 30,
            "marginTop": 30,
        },
    )
    cq.exporters.export(
        assembled.toCompound(),
        str(out / "front.svg"),
        opt={
            "width": 700,
            "height": 700,
            "projectionDir": (0, 0, -1),
            "showHidden": False,
            "strokeWidth": 0.7,
        },
    )
    checks = {
        "notice": "Geometric packaging checks only; no physical fit, optics, thermal, structural or electrical validation.",
        "cadquery_version": cq.__version__,
        "parameters": asdict(p),
        "derived": dict(
            display_mm=p.display,
            outer_mm=p.outer,
            depth_mm=p.depth,
            installed_depth_mm=p.depth + p.wall_clearance,
            fabric_to_led_gap_mm=p.diffuser_gap,
            mounting_spacer_length_mm=p.spacer_length,
            mounting_clearance_diameter_mm=p.mounting_clearance_diameter,
            backlight_to_wall_clearance_mm=p.depth
            + p.wall_clearance
            - p.cover_front
            - p.cover_thickness
            - p.backlight_strip_thickness,
            cable_protrusion_behind_chassis_mm=max(
                0,
                p.panel_back
                + p.connector_envelope_depth
                - p.chassis_front
                - p.chassis_thickness,
            ),
            electronics_to_cover_clearance_mm=p.cover_front
            - p.electronics_front
            - p.electronics_depth,
        ),
        "components": [],
        "intersections": [],
    }
    for c in components:
        shape = c["shape"].val()
        assert shape.isValid(), c["name"] + " invalid"
        assert len(shape.Solids()) == 1, c["name"] + " must be one solid"
        b = shape.BoundingBox()
        checks["components"].append(
            dict(
                name=c["name"],
                role=c["role"],
                material=c["material"],
                volume_mm3=shape.Volume(),
                bounds_mm=dict(
                    xmin=b.xmin,
                    xmax=b.xmax,
                    ymin=b.ymin,
                    ymax=b.ymax,
                    zmin=b.zmin,
                    zmax=b.zmax,
                ),
                note=c["note"],
            )
        )
    # Includes reservations: no reserved space may intersect a concept part/reference.
    for a, b in combinations(components, 2):
        volume = a["shape"].val().intersect(b["shape"].val()).Volume()
        if volume > 0.001:
            checks["intersections"].append(
                dict(a=a["name"], b=b["name"], volume_mm3=volume)
            )
    assert not checks["intersections"], checks["intersections"]
    imported = cq.importers.importStep(str(out / "assembly.step"))
    original = assembled.toCompound()
    assert imported.val().isValid()
    assert len(imported.val().Solids()) == len(original.Solids())
    delta = abs(imported.val().Volume() - original.Volume())
    assert delta / max(original.Volume(), 1) < 1e-7
    checks["step_roundtrip"] = dict(
        valid=True, solids=len(imported.val().Solids()), volume_delta_mm3=delta
    )
    checks["result"] = (
        "PASS: modeled solids valid, no modeled volume intersections, assembly STEP roundtrip passed"
    )
    (out / "geometry-checks.json").write_text(json.dumps(checks, indent=2) + "\n")
    (out / "parameters.json").write_text(json.dumps(asdict(p), indent=2) + "\n")
    print(checks["result"])
    print(
        "Frame:",
        p.outer,
        "x",
        p.outer,
        "x",
        p.depth,
        "mm; installed depth",
        p.depth + p.wall_clearance,
        "mm",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--parameters", type=Path, help="JSON overrides for Parameters fields"
    )
    parser.add_argument(
        "--output", type=Path, default=Path(__file__).resolve().parent / "exports"
    )
    args = parser.parse_args()
    p = Parameters(
        **(json.loads(args.parameters.read_text()) if args.parameters else {})
    )
    export(p, build(p), args.output)
