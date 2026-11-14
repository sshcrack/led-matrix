"""Derive the provisional eight-hole pattern in the owner's sketch orientation."""

import json
from pathlib import Path
from statistics import mean


def measured_pattern():
    data = json.loads(
        Path(__file__).with_name("mounting-measurements.json").read_text()
    )
    measured = {}
    for name, series in data["series"].items():
        outside = mean(series["outside"])
        measured[name] = (
            (outside + mean(series["inside"])) / 2
            if "inside" in series
            else outside - mean(series["shaft_diameters"])
        )
    spans = {
        name: measured[representative]
        for name, representative in data["template_representatives"].items()
    }
    width, height = spans["C"] + spans["D"], spans["A"] + spans["B"]
    holes = {
        "0,0": (0, 0),
        "1,0": (spans["D"], 0),
        "2,0": (width, 0),
        "0,1": (0, spans["E"]),
        "2,1": (width, spans["B"]),
        "0,2": (0, height),
        "1,2": (spans["G"], height),
        "2,2": (width, height),
    }
    return {
        "revision": data["revision"],
        "status": data["status"],
        "datum": "Hole (0,0), owner sketch orientation; NOT panel corner",
        "direct_derived_spans_mm": measured,
        "provisional_template_spans_mm": spans,
        "width_mm": width,
        "height_mm": height,
        "hole_centres_mm": holes,
        "assumptions": data["template_assumptions"],
    }
