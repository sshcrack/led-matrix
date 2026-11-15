#!/usr/bin/env python3
"""Worst-case power and thermal budget for the power + halo board (prints a Markdown report).

Every input is listed in ``INPUTS`` with its source.  "Worst case" is all four panels at their
rated 4 A (full white at full brightness), the Pi at its eFuse budget and the halo at full white,
all at once and sustained.  Real content draws far less; the hardware limits (eFuse current
limits, converter thermal shutdown) are what bound the worst case, not software brightness.

Usage: python3 tools/power_budget.py   (docs/power-and-thermal.md quotes its output)
"""

from __future__ import annotations

import math

INPUTS = {
    # loads
    "panel_a": (4.0, "Waveshare P3 64x64 rating 5 V / 4 A (<= 20 W) per panel"),
    "pi_a": (2.5, "Pi 3B recommended supply 2.5 A, incl. Electrodragon HAT + Wi-Fi peaks"),
    "halo_a": (2.4, "40 x WS2812B at full white, ~60 mA each"),
    "logic_a": (0.06, "ATtiny1616, buffer, status/fault LEDs, pull-ups"),
    "vout": (5.02, "TPSM63610 FB divider 100k/24.9k"),
    # conversion
    "eta_panel": (0.93, "TPSM63610 datasheet 7.5: 24 V -> 5 V, 4 A, 1 MHz = 93 %"),
    "eta_aux": (0.925, "same curve at ~5 A (estimate, 0.5 % below the 4 A point)"),
    # resistances, hot
    "r_efuse": (0.040, "TPS259470A RON 28.3 mOhm typ at 25 C, x1.4 at TJ ~100 C"),
    "r_q1": (0.0078, "PSMN5R2-60YL 5.2 mOhm, x1.5 hot"),
    "r_fuse": (0.010, "0453008 cold resistance, assumed (not verified)"),
    "r_minifit_pole": (0.006, "Mini-Fit Jr ~10 mOhm/contact, 2 contacts per pole, +1 mOhm crimp"),
    "r_vh_pole": (0.006, "JST VH ~10 mOhm/contact, 2 contacts per pole"),
    "r_awg18_m": (0.0210, "AWG18 copper 20.9 mOhm/m"),
    "cable_24v_m": (1.0, "jack to J1 pigtail inside the enclosure, both conductors 0.5 m"),
    "cable_panel_m": (1.0, "J1x1 to panel, 0.5 m, both conductors"),
    # thermal
    "rth_module": (21.6, "TPSM63610 EVM RthJA 21.6 C/W (datasheet note); single module"),
    "rth_efuse": (74.5, "TPS25947 RthJA 74.5 C/W on JEDEC 2s2p without thermal vias (this board has none"
                        " under the eFuses; 41.7 C/W with 8 vias)"),
    "h_board": (12.0, "natural convection + radiation, W/m2K per face, still air"),
    "psu_w": (160.0, "Mean Well GST160A24-R7B 24 V / 6.67 A"),
    "t_encl_air": (45.0, "design limit for enclosure air (room 30 C + 15 K rise)"),
}


def v(k):
    return INPUTS[k][0]


def main():
    vo = v("vout")
    p_panel = vo * v("panel_a")
    i_aux = v("pi_a") + v("halo_a") + v("logic_a")
    p_aux = vo * i_aux
    loss_panel = p_panel * (1 / v("eta_panel") - 1)
    loss_aux = p_aux * (1 / v("eta_aux") - 1)
    loss_efuse_panel = v("panel_a") ** 2 * v("r_efuse")
    loss_efuse_pi = v("pi_a") ** 2 * v("r_efuse")
    loss_efuse_halo = v("halo_a") ** 2 * v("r_efuse")
    p_in_conv = 4 * (p_panel + loss_panel) + p_aux + loss_aux + 4 * loss_efuse_panel + loss_efuse_pi + loss_efuse_halo
    # input current solved at the brick voltage minus input path drop (one iteration is enough)
    r_in = v("r_q1") + v("r_fuse") + 2 * v("r_minifit_pole") + v("r_awg18_m") * v("cable_24v_m")
    i_in = p_in_conv / 24.0
    i_in = p_in_conv / (24.0 - i_in * r_in)
    loss_q1 = i_in ** 2 * v("r_q1")
    loss_fuse = i_in ** 2 * v("r_fuse")
    loss_j1 = i_in ** 2 * 2 * v("r_minifit_pole")
    loss_vh = 4 * v("panel_a") ** 2 * 2 * v("r_vh_pole")
    board_loss = 4 * loss_panel + loss_aux + 4 * loss_efuse_panel + loss_efuse_pi + loss_efuse_halo \
        + loss_q1 + loss_fuse + loss_j1 + loss_vh
    p_brick = i_in * 24.0
    panel_drop = v("panel_a") * (v("r_efuse") + 2 * v("r_vh_pole") + v("r_awg18_m") * v("cable_panel_m"))
    area = 0.136 * 0.086
    board_rise = board_loss / (v("h_board") * 2 * area)

    rows = [
        ("Panel output, each", f"{p_panel:.1f} W", f"{v('panel_a'):.1f} A at {vo} V"),
        ("AUX output (Pi + halo + logic)", f"{p_aux:.1f} W", f"{i_aux:.2f} A, TPSM63610 rated 8 A"),
        ("Panel buck loss, each", f"{loss_panel:.2f} W", f"eta {v('eta_panel'):.0%}"),
        ("AUX buck loss", f"{loss_aux:.2f} W", f"eta {v('eta_aux'):.1%}"),
        ("Panel eFuse loss, each", f"{loss_efuse_panel:.2f} W", "I^2 R, hot RON"),
        ("Pi / halo eFuse loss", f"{loss_efuse_pi:.2f} / {loss_efuse_halo:.2f} W", ""),
        ("Q1 + fuse + J1 loss", f"{loss_q1 + loss_fuse + loss_j1:.2f} W", f"input {i_in:.2f} A"),
        ("Panel connector loss (4x J*01)", f"{loss_vh:.2f} W", ""),
        ("**Board dissipation, worst case**", f"**{board_loss:.1f} W**", ""),
        ("Brick output", f"{p_brick:.0f} W", f"{p_brick / v('psu_w'):.0%} of {v('psu_w'):.0f} W"),
        ("Input current", f"{i_in:.2f} A", f"0453008 8 A fuse: {i_in / 8:.0%} of rating"),
        ("Panel 5 V drop at 4 A (eFuse + J + 0.5 m AWG18 pair)", f"{panel_drop * 1000:.0f} mV",
         f"panel sees {vo - panel_drop:.2f} V"),
    ]
    tj_panel = v("t_encl_air") + board_rise * 0.5 + loss_panel * v("rth_module")
    tj_aux = v("t_encl_air") + board_rise * 0.5 + loss_aux * v("rth_module")
    tj_efuse = v("t_encl_air") + board_rise * 0.5 + loss_efuse_panel * v("rth_efuse")
    therm = [
        ("Board mean rise over enclosure air (both faces, still air)", f"{board_rise:.0f} K"),
        ("TPSM63610 panel TJ (air 45 C + half board rise + P*RthJA)", f"{tj_panel:.0f} C (limit 125 C, TSD 168 C)"),
        ("TPSM63610 AUX TJ", f"{tj_aux:.0f} C"),
        ("TPS259470A panel TJ", f"{tj_efuse:.0f} C (limit 125 C)"),
    ]
    # enclosure air: heat that ends up inside the enclosure, natural-draft venting
    p_inside = board_loss + 4.0  # Pi ~4 W; panel heat leaves mostly through the front face
    p_inside_with_panels = board_loss + 4.0 + 0.5 * 4 * p_panel
    dt_air = 15.0
    flow = p_inside_with_panels / (1.2 * 1005 * dt_air)
    chimney_h = 0.30
    vel = 0.6 * math.sqrt(2 * 9.81 * chimney_h * dt_air / 300.0)
    vent_area = flow / vel
    lines = ["# Power and thermal budget (generated by tools/power_budget.py)", "",
             "Worst case: four panels at 4 A, Pi at 2.5 A, halo at 2.4 A, all sustained.", "",
             "| Item | Value | Note |", "|---|---|---|"]
    lines += [f"| {a} | {b} | {c} |" for a, b, c in rows]
    lines += ["", "| Thermal item | Value |", "|---|---|"] + [f"| {a} | {b} |" for a, b in therm]
    lines += ["", "Enclosure ventilation (natural draft, 0.30 m between low inlet and high outlet):", "",
              f"- heat released inside the enclosure: board {board_loss:.0f} W + Pi ~4 W"
              f" (+ up to {0.5 * 4 * p_panel:.0f} W if half the panels' heat goes backwards) = "
              f"{p_inside:.0f}..{p_inside_with_panels:.0f} W",
              f"- airflow for a {dt_air:.0f} K air rise at the upper figure: {flow * 1000:.1f} L/s",
              f"- draft velocity ~{vel:.2f} m/s -> free vent area >= {vent_area * 1e4:.0f} cm2 at the inlet"
              " and the same at the outlet", "",
              "Inputs:", ""]
    lines += [f"- `{k}` = {val}: {src}" for k, (val, src) in INPUTS.items()]
    print("\n".join(lines))


if __name__ == "__main__":
    main()
