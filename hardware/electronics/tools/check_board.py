#!/usr/bin/env python3
"""Verify the committed board and write prototype outputs to ``exports/`` (git-ignored).

The committed .kicad_pcb keeps its zones unfilled (repo size), so this script copies the
project to ``exports/work/``, fills every zone there, and runs on that copy:

* ERC (all severities) and the independent netlist check (tools/check_netlist.py)
* DRC with schematic parity (all severities, nothing excluded)
* outputs: schematic PDF, BOM CSV (also copied to docs/bom.csv), Gerbers + drill + job file, position file, STEP,
  board SVG/PDF renders, the filled board itself

Every output is stamped PROTOTYPE / UNVALIDATED (directory name and README) and is not a
fabrication release.  Exit status is non-zero when ERC, DRC, parity or the netlist check fail.

Usage:  python3 tools/check_board.py [--no-export]
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
BOARD = ROOT / "power-halo-board"
NAME = "power-halo-board"
EXPORTS = ROOT / "exports"
WORK = EXPORTS / "work"
FAB = EXPORTS / "fab-PROTOTYPE-UNVALIDATED"
REPORTS = EXPORTS / "reports"


def run(cmd, **kw):
    print("+", " ".join(str(c) for c in cmd))
    return subprocess.run([str(c) for c in cmd], check=True, **kw)


def copy_project():
    if WORK.exists():
        shutil.rmtree(WORK)
    shutil.copytree(BOARD, WORK, ignore=shutil.ignore_patterns("*.kicad_prl", "*-backups", "fp-info-cache"))


def fill_zones():
    code = (
        "import pcbnew, sys\n"
        "b = pcbnew.LoadBoard(sys.argv[1])\n"
        "pcbnew.ZONE_FILLER(b).Fill(b.Zones())\n"
        "pcbnew.SaveBoard(sys.argv[1], b)\n"
    )
    run([sys.executable, "-c", code, WORK / f"{NAME}.kicad_pcb"], stderr=subprocess.DEVNULL)


def erc():
    out = REPORTS / "erc.json"
    run(["kicad-cli", "sch", "erc", "--severity-all", "--format", "json", "-o", out, WORK / f"{NAME}.kicad_sch"])
    data = json.loads(out.read_text())
    violations = [v for s in data["sheets"] for v in s["violations"]]
    run(["kicad-cli", "sch", "erc", "--severity-all", "-o", REPORTS / "erc.rpt", WORK / f"{NAME}.kicad_sch"])
    return len(violations)


def netlist_check():
    net = REPORTS / f"{NAME}.net"
    run(["kicad-cli", "sch", "export", "netlist", "-o", net, WORK / f"{NAME}.kicad_sch"])
    res = subprocess.run([sys.executable, HERE / "check_netlist.py", net], capture_output=True, text=True)
    (REPORTS / "netlist-check.txt").write_text(res.stdout + res.stderr)
    print(res.stdout.strip().splitlines()[-1] if res.stdout.strip() else res.stderr)
    return res.returncode


def drc():
    out = REPORTS / "drc.json"
    pcb = WORK / f"{NAME}.kicad_pcb"
    run(["kicad-cli", "pcb", "drc", "--severity-all", "--schematic-parity", "--format", "json", "-o", out, pcb])
    run(["kicad-cli", "pcb", "drc", "--severity-all", "--schematic-parity", "-o", REPORTS / "drc.rpt", pcb])
    data = json.loads(out.read_text())
    return len(data["violations"]), len(data["unconnected_items"]), len(data["schematic_parity"])


def export():
    pcb = WORK / f"{NAME}.kicad_pcb"
    sch = WORK / f"{NAME}.kicad_sch"
    if FAB.exists():
        shutil.rmtree(FAB)
    (FAB / "gerbers").mkdir(parents=True)
    run(["kicad-cli", "sch", "export", "pdf", "-o", EXPORTS / f"{NAME}-schematic.pdf", sch])
    run(["kicad-cli", "sch", "export", "bom", "-o", EXPORTS / f"{NAME}-bom.csv",
         "--fields", "Reference,Value,Footprint,MPN,Manufacturer,Rating,Datasheet,Description,${QUANTITY},${DNP}",
         "--labels", "Refs,Value,Footprint,MPN,Manufacturer,Rating,Datasheet,Description,Qty,DNP",
         "--group-by", "Value,Footprint,MPN", "--ref-range-delimiter", "", "--exclude-dnp", sch])
    layers = "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts"
    run(["kicad-cli", "pcb", "export", "gerbers", "--layers", layers, "--subtract-soldermask",
         "--board-plot-params", "-o", FAB / "gerbers", pcb])
    run(["kicad-cli", "pcb", "export", "drill", "--format", "excellon", "--excellon-separate-th",
         "--generate-map", "--map-format", "pdf", "-o", str(FAB / "gerbers") + "/", pcb])
    run(["kicad-cli", "pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "front",
         "--exclude-dnp", "-o", FAB / f"{NAME}-pos-front.csv", pcb])
    run(["kicad-cli", "pcb", "export", "step", "--subst-models", "--include-tracks", "--include-zones",
         "--force", "-o", EXPORTS / f"{NAME}.step", pcb], stderr=subprocess.STDOUT)
    run(["kicad-cli", "pcb", "export", "svg", "--layers", "Edge.Cuts,User.Drawings", "--mode-single",
         "--exclude-drawing-sheet", "-o", EXPORTS / f"{NAME}-outline.svg", pcb])
    run(["kicad-cli", "pcb", "export", "dxf", "--layers", "Edge.Cuts", "--output-units", "mm", "--mode-single",
         "-o", EXPORTS / f"{NAME}-outline.dxf", pcb])
    for side, layers in (("top", "F.Cu,F.Silkscreen,F.Mask,Edge.Cuts"), ("bottom", "B.Cu,B.Silkscreen,Edge.Cuts"),
                         ("in1", "In1.Cu,Edge.Cuts"), ("in2", "In2.Cu,Edge.Cuts"),
                         ("assembly", "F.Fab,F.Silkscreen,Edge.Cuts")):
        run(["kicad-cli", "pcb", "export", "pdf", "--layers", layers, "--mode-single",
             "-o", EXPORTS / f"{NAME}-{side}.pdf", pcb])
    shutil.copy(pcb, EXPORTS / f"{NAME}-filled.kicad_pcb")
    shutil.copy(EXPORTS / f"{NAME}-bom.csv", ROOT / "docs" / "bom.csv")   # small text copy kept in git
    (FAB / "README.txt").write_text(
        "PROTOTYPE / UNVALIDATED - NOT A FABRICATION RELEASE\n\n"
        "Generated by hardware/electronics/tools/check_board.py from the committed design.\n"
        "No fabrication approval has been given.  See hardware/electronics/docs/verification.md\n"
        "for what was and was not verified before using these files for anything.\n"
        "Stackup: 4 layers, 1.6 mm, 2 oz outer / 1 oz inner, ENIG.\n")
    shutil.make_archive(str(EXPORTS / "gerbers-PROTOTYPE-UNVALIDATED"), "zip", FAB)


def main():
    copy_project()
    REPORTS.mkdir(parents=True, exist_ok=True)
    fill_zones()
    n_erc = erc()
    rc_net = netlist_check()
    n_drc, n_unc, n_par = drc()
    print(f"ERC violations: {n_erc}; netlist check exit {rc_net}; "
          f"DRC violations: {n_drc}, unconnected: {n_unc}, parity: {n_par}")
    if "--no-export" not in sys.argv:
        export()
        print("outputs in", EXPORTS)
    sys.exit(1 if (n_erc or rc_net or n_drc or n_unc or n_par) else 0)


if __name__ == "__main__":
    main()
