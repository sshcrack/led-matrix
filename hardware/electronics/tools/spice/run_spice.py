#!/usr/bin/env python3
"""Run the behavioral ngspice decks in this directory and check their results against limits.

Writes results.txt next to this script.  Exit status is non-zero when a check fails.
The decks are behavioral (no vendor models); see docs/verification.md for what they do not cover.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DECKS = ["thresholds.cir", "en_sequence.cir", "hotplug.cir"]


def run(deck):
    out = subprocess.run(["ngspice", "-b", str(HERE / deck)], capture_output=True, text=True, cwd=HERE)
    rows = []
    for line in out.stdout.splitlines():
        if line.startswith("RESULT"):
            _, name, *kv = line.split()
            rows.append((name, dict(x.split("=", 1) for x in kv)))
    errors = [l for l in (out.stdout + out.stderr).splitlines() if re.search(r"\berror\b", l, re.I)]
    return rows, errors


def checks(results):
    """(description, ok) pairs; ok None marks an informational value.  Limits come from the datasheets cited in docs/power-and-thermal.md."""
    out = []
    th = {r["corner"]: {k: float(v) for k, v in r.items() if k != "corner"}
          for n, r in results if n == "thresholds"}
    vout_max = 5.02 * 1.02  # FB 1.0 V +-1 % plus 1 % resistors (envelope)
    out.append((f"panel OV trip (lowest corner {th['lo']['panel_ov_rise']:.2f} V) > VOUT max {vout_max:.2f} V + 0.4 V",
                th["lo"]["panel_ov_rise"] > vout_max + 0.4))
    out.append((f"panel OV release (lowest corner {th['lo']['panel_ov_fall']:.2f} V) > VOUT max {vout_max:.2f} V",
                th["lo"]["panel_ov_fall"] > vout_max))
    out.append((f"panel OV trip (highest corner {th['hi']['panel_ov_rise']:.2f} V) <= 6.2 V",
                th["hi"]["panel_ov_rise"] <= 6.2))
    out.append((f"panel UV release (highest corner {th['hi']['panel_uv_rise']:.2f} V) < VOUT min 4.9 V",
                th["hi"]["panel_uv_rise"] < 4.9))
    out.append((f"halo OV trip corners {th['lo']['halo_ov_rise']:.2f}..{th['hi']['halo_ov_rise']:.2f} V within 5.5..6.2 V",
                5.5 < th["lo"]["halo_ov_rise"] and th["hi"]["halo_ov_rise"] <= 6.2))
    out.append((f"AUX turns on below 22 V (highest corner {th['hi']['aux_on']:.2f} V; brick 24 V -3 % = 23.3 V)",
                th["hi"]["aux_on"] < 22.0))
    out.append((f"AUX turns off above 6 V (lowest corner {min(t['aux_off'] for t in th.values()):.2f} V)",
                min(t["aux_off"] for t in th.values()) > 6.0))
    en = {r["jp"]: r for n, r in results if n == "en_sequence"}
    e12 = {k: float(v) for k, v in en["1-2"].items() if k != "jp"}
    e23 = {k: float(v) for k, v in en["2-3"].items() if k != "jp"}
    out.append((f"JP601 1-2: EN_PANEL stays below VEN_WAKE 0.4 V before GPIO26 ({e12['en_max_before_gpio']:.3f} V)",
                e12["en_max_before_gpio"] < 0.4))
    out.append((f"JP601 1-2: panels enabled {e12['panel_on_delay'] * 1e3:.2f} ms after GPIO26 high (< 20 ms)",
                0 < e12["panel_on_delay"] < 0.02))
    out.append((f"JP601 1-2: panels disabled {e12['panel_off_delay'] * 1e3:.3f} ms after GPIO26 low (< 5 ms)",
                0 < e12["panel_off_delay"] < 0.005))
    out.append((f"JP601 2-3: EN_PANEL crosses 1.263 V {e23['panel_on_after_aux'] * 1e3:+.2f} ms relative to +5V_AUX = 4.75 V"
                " (> -2 ms: panel bucks start after AUX is above 4 V)", e23["panel_on_after_aux"] > -0.002))
    hp = {r["case"]: {k: float(v) for k, v in r.items() if k != "case"} for n, r in results if n == "hotplug"}
    vmax = max(hp[c]["vmax"] for c in ("1", "2", "3"))
    out.append((f"hot-plug with bulk caps: +24V peak {vmax:.1f} V < 36 V (TPSM63610 VIN max)", vmax < 36.0))
    out.append((f"hot-plug without bulk caps (case 4): +24V peak {hp['4']['vmax']:.1f} V (reference only)", None))
    i2t = max(hp[c]["i2t"] for c in ("1", "2", "3"))
    out.append((f"hot-plug fuse stress: peak {max(hp[c]['ipeak'] for c in hp):.0f} A, I2t {i2t:.2f} A2s"
                " (compare with 0453008 melting I2t; datasheet value not yet verified)", None))
    return out


def main():
    results, lines = [], []
    failed = False
    for deck in DECKS:
        rows, errors = run(deck)
        if errors or not rows:
            failed = True
            lines.append(f"{deck}: ngspice errors: {errors or 'no RESULT lines'}")
        results += rows
        lines += [f"{deck}: {name} " + " ".join(f"{k}={v}" for k, v in r.items()) for name, r in rows]
    lines.append("")
    for desc, ok in checks(results):
        failed |= ok is False
        lines.append({True: "PASS ", False: "FAIL ", None: "INFO "}[ok] + desc)
    text = "\n".join(lines) + "\n"
    (HERE / "results.txt").write_text(text)
    print(text)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
