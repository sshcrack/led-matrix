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
    lo, hi = th["lo"], th["hi"]
    out.append((f"panel eFuse UV release (highest corner {hi['panel_uv_rise']:.2f} V) < VOUT min 4.9 V",
                hi["panel_uv_rise"] < 4.9))
    out.append(("panel eFuse OV is fixed in the TPS259823L: 7.39..7.76 V rising (datasheet, not simulated)", None))
    out.append((f"halo OV trip corners {lo['halo_ov_rise']:.2f}..{hi['halo_ov_rise']:.2f} V within 5.5..6.2 V",
                5.5 < lo["halo_ov_rise"] and hi["halo_ov_rise"] <= 6.2))
    out.append((f"hot-swap turns on below 21.6 V (brick 24 V -10 %; highest corner {hi['hs_uv_on']:.2f} V)",
                hi["hs_uv_on"] < 21.6))
    out.append((f"hot-swap OV cut-off below 36 V (converter VIN max; highest corner {hi['hs_ov_off']:.2f} V)",
                hi["hs_ov_off"] < 36.0))
    out.append((f"hot-swap OV cut-off and release above the brick's 26.4 V maximum setting"
                f" (lowest corners {lo['hs_ov_off']:.2f} / {lo['hs_ov_on']:.2f} V)",
                lo["hs_ov_off"] > 26.4 and lo["hs_ov_on"] > 26.4))
    out.append((f"hot-swap OV cut-off vs brick OVP 32.4 V: {lo['hs_ov_off']:.2f}..{hi['hs_ov_off']:.2f} V"
                " (a low part can trip just below the brick's own OVP; only a faulty brick gets there)", None))
    out.append((f"AUX buck EN alone turns on below 22 V (highest corner {hi['aux_on']:.2f} V; LM5069 PGD gates it too)",
                hi["aux_on"] < 22.0))
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
    ipk = max(max(h["ipeak_plug"], h["ipeak_start"]) for h in hp.values())
    out.append((f"hot-plug peak input current {ipk:.2f} A <= 9.8 A (all corners)", ipk <= 9.8))
    i2t = max(h["i2t"] for h in hp.values())
    out.append((f"hot-plug F1 I2t {i2t:.3f} A2s <= 4.05 A2s (20 % of the 0453008 melting I2t 20.23 A2s)", i2t <= 4.05))
    v24 = max(h["v24_max"] for h in hp.values())
    out.append((f"hot-plug +24V at the converters peaks at {v24:.1f} V < 36 V", v24 < 36.0))
    vp = max(max(h["vp_max_plug"], h["vp_max_start"]) for h in hp.values())
    vf = max(h["vf_max"] for h in hp.values())
    out.append((f"hot-plug VIN_P {vp:.1f} V and VIN_F {vf:.1f} V < 60 V (Q1/Q2 VDS; LM5069 100 V, LM74700 ANODE 65 V)",
                vp < 60 and vf < 60))
    it = max(h["itvs_max"] for h in hp.values())
    out.append((f"hot-plug SMBJ30A peak {it:.1f} A < IPP 12.4 A (10/1000 us rating)", it < 12.4))
    tst = max(h["t_start"] for h in hp.values())
    out.append((f"hot-swap start time {tst * 1e3:.2f} ms < minimum fault time 10.3 ms (CT 330 nF: 3.76 V x 330 nF / 120 uA)",
                tst < 0.0103))
    pq = max(h["pq2_max"] for h in hp.values())
    out.append((f"hot-swap Q2 start-up: peak {pq:.1f} W for {tst * 1e3:.1f} ms"
                " (CSD18540Q5B SOA ~100 W for 10 ms at 24 V, ~65 W after derating to TC 70 C)", pq < 65))
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
