#!/usr/bin/env python3
"""Write the KiCad project file (net classes, board rules) and custom DRC rules."""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_sch import PROJECT, uid  # noqa: E402

BOARD = Path(__file__).resolve().parent.parent / "power-halo-board"

# Netclass track widths are the autorouter defaults for taps; high-current paths are copper zones.
# Copper: 4 layers, 1.6 mm, 2 oz outer / 1 oz inner (stackup in gen_pcb.STACKUP, README.md)
NETCLASSES = [
    # name, track, clearance, via dia, via drill, colour
    ("Default", 0.20, 0.20, 0.6, 0.3),
    ("Power24", 0.50, 0.30, 0.8, 0.4),
    ("Power5", 0.40, 0.20, 0.8, 0.4),
    ("GND", 0.40, 0.20, 0.6, 0.3),
    ("Power5AUX", 0.40, 0.20, 0.6, 0.3),
]
PATTERNS = [
    ("Power24", "+24V"), ("Power24", "/Input/VIN_*"),
    ("Power5AUX", "+5V_AUX"), ("Power5", "/Panel*/VOUT"), ("Power5", "/Panel*/VPANEL"),
    ("Power5", "/Aux/VPI"), ("Power5", "/Halo/VHALO"),
    ("GND", "GND"),
]


def netclass(name, track, clr, vd, vdr):
    return {
        "name": name, "bus_width": 12, "clearance": clr, "diff_pair_gap": 0.25,
        "diff_pair_via_gap": 0.25, "diff_pair_width": 0.2, "line_style": 0,
        "microvia_diameter": 0.3, "microvia_drill": 0.1, "pcb_color": "rgba(0, 0, 0, 0.000)",
        "priority": 2147483647 if name == "Default" else {"Power24": 0, "Power5": 1, "GND": 2, "Power5AUX": 3}[name],
        "schematic_color": "rgba(0, 0, 0, 0.000)", "track_width": track,
        "tuning_profile": "", "via_diameter": vd, "via_drill": vdr, "wire_width": 6,
    }


def project():
    return {
        "board": {
            "3dviewports": [],
            "design_settings": {
                "defaults": {
                    "copper_line_width": 0.2, "copper_text_size_h": 1.5, "copper_text_size_v": 1.5,
                    "copper_text_thickness": 0.3, "silk_line_width": 0.12, "silk_text_size_h": 1.0,
                    "silk_text_size_v": 1.0, "silk_text_thickness": 0.15, "fab_line_width": 0.1,
                    "courtyard_line_width": 0.05, "other_line_width": 0.1,
                    "pads": {"drill": 0.8, "height": 1.6, "width": 1.6},
                    "zones": {"min_clearance": 0.25},
                },
                "rule_severities": {
                    "lib_footprint_issues": "warning", "lib_footprint_mismatch": "warning",
                    "silk_overlap": "warning", "silk_over_copper": "warning", "silk_edge_clearance": "warning",
                    "text_height": "warning", "text_thickness": "warning",
                    "unconnected_items": "error", "clearance": "error", "shorting_items": "error",
                    "track_dangling": "warning", "via_dangling": "warning", "starved_thermal": "error",
                    "courtyards_overlap": "error", "copper_edge_clearance": "error",
                },
                "rules": {
                    "min_clearance": 0.15, "min_connection": 0.0, "min_copper_edge_clearance": 0.5,
                    "min_hole_clearance": 0.25, "min_hole_to_hole": 0.25, "min_microvia_diameter": 0.2,
                    "min_microvia_drill": 0.1, "min_resolved_spokes": 2, "min_silk_clearance": 0.0,
                    "min_text_height": 0.8, "min_text_thickness": 0.08, "min_through_hole_diameter": 0.3,
                    "min_track_width": 0.15, "min_via_annular_width": 0.13, "min_via_diameter": 0.5,
                    "solder_mask_to_copper_clearance": 0.0, "use_height_for_length_calcs": True,
                },
                "track_widths": [0.0, 0.2, 0.3, 0.5, 1.0, 2.0, 3.0],
                "via_dimensions": [{"diameter": 0.0, "drill": 0.0}, {"diameter": 0.6, "drill": 0.3},
                                   {"diameter": 0.8, "drill": 0.4}],
                "zones_allow_external_fillets": False,
            },
            "ipc2581": {}, "layer_presets": [], "viewports": [],
        },
        "boards": [],
        "cvpcb": {"equivalence_files": []},
        "erc": {
            "erc_exclusions": [],
            "meta": {"version": 0},
            "pin_map": [],
            "rule_severities": {},
        },
        "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
        "meta": {"filename": f"{PROJECT}.kicad_pro", "version": 3},
        "net_settings": {
            "classes": [netclass(*c) for c in NETCLASSES],
            "meta": {"version": 4},
            "net_colors": None,
            "netclass_assignments": None,
            "netclass_patterns": [{"netclass": c, "pattern": p} for c, p in PATTERNS],
        },
        "pcbnew": {"last_paths": {}, "page_layout_descr_file": ""},
        "schematic": {
            "annotate_start_num": 0, "bom_export_filename": "${PROJECTNAME}.csv",
            "bom_fmt_presets": [], "bom_fmt_settings": {"field_delimiter": ",", "keep_line_breaks": False,
                                                          "keep_tabs": False, "name": "CSV", "ref_delimiter": ",",
                                                          "ref_range_delimiter": "", "string_delimiter": "\""},
            "bom_presets": [], "bom_settings": {}, "connection_grid_size": 50.0,
            "drawing": {"default_line_thickness": 6.0, "default_text_size": 50.0, "field_names": [],
                        "intersheets_ref_own_page": False, "intersheets_ref_prefix": "",
                        "intersheets_ref_short": False, "intersheets_ref_show": False,
                        "intersheets_ref_suffix": "", "junction_size_choice": 3, "label_size_ratio": 0.375,
                        "operating_point_overlay_i_precision": 3, "operating_point_overlay_i_range": "~A",
                        "operating_point_overlay_v_precision": 3, "operating_point_overlay_v_range": "~V",
                        "overbar_offset_ratio": 1.23, "pin_symbol_size": 25.0, "text_offset_ratio": 0.15},
            "legacy_lib_dir": "", "legacy_lib_list": [], "meta": {"version": 1},
            "net_format_name": "", "page_layout_descr_file": "", "plot_directory": "",
            "space_save_all_events": True, "spice_current_sheet_as_root": False,
            "spice_external_command": "spice \"%I\"", "spice_model_current_sheet_as_root": True,
            "spice_save_all_currents": False, "spice_save_all_dissipations": False,
            "spice_save_all_voltages": False, "subpart_first_id": 65, "subpart_id_separator": 0,
        },
        "sheets": [[uid("root"), "Root"]] + [[uid("sheet", n), n] for n in
                                              ["Input", "Panel1", "Panel2", "Panel3", "Panel4", "Aux", "Halo"]],
        "text_variables": {},
    }


DRU = """(version 1)
# Custom rules for the power + halo board.  Generic limits live in the .kicad_pro.

(rule "24V net clearance"
  (condition "A.NetClass == 'Power24' && B.Net != A.Net")
  (constraint clearance (min 0.3mm)))

# Branch power nets.  +5V_AUX is excluded: its current runs in the In2 plane and F.Cu zones, and
# its F.Cu tracks are short taps to pull-ups, LEDs and logic supplies (< 50 mA each).
(rule "Power track width"
  (condition "A.NetClass == 'Power24' || A.NetClass == 'Power5'")
  (layer outer)
  (constraint track_width (min 0.4mm)))

(rule "Board edge"
  (constraint edge_clearance (min 0.5mm)))
"""


def main():
    (BOARD / f"{PROJECT}.kicad_pro").write_text(json.dumps(project(), indent=2) + "\n")
    (BOARD / f"{PROJECT}.kicad_dru").write_text(DRU)
    print("project written")


if __name__ == "__main__":
    main()
