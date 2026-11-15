#!/usr/bin/env python3
"""Generate the hierarchical KiCad 9 schematic from circuit.py.

Connectivity is label based: every used pin gets a 2.54 mm wire stub ending
in a net label (local, or global for GLOBAL_NETS), a GND power symbol, or a
no-connect flag.  PWR_FLAGs are added only to nets whose sole drivers are
passive pins (connectors, fuses, FETs), never to silence other ERC checks.
UUIDs are deterministic so regeneration produces stable diffs.
"""

from __future__ import annotations

import copy
import math
import sys
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import circuit  # noqa: E402
from sexpr import Q, dump, find, find_all, parse  # noqa: E402

BOARD = Path(__file__).resolve().parent.parent / "power-halo-board"
PROJECT = "power-halo-board"
SYS_LIBS = Path("/usr/share/kicad/symbols")
NS = uuid.UUID("0b8f8a4e-2d3c-4c4f-8f62-9d1e2c7a5b30")
VERSION = "20250114"
G = 2.54


def uid(*parts) -> str:
    return str(uuid.uuid5(NS, "/".join(str(p) for p in parts)))


def snap(v: float, grid: float = G) -> float:
    return round(math.ceil(v / grid - 1e-9) * grid, 4)


# --------------------------------------------------------------------------
# Library handling
# --------------------------------------------------------------------------

_lib_cache: dict[str, list] = {}


def lib_file(lib: str) -> Path:
    if lib == "ledpanel":
        return BOARD / "lib" / "ledpanel.kicad_sym"
    return SYS_LIBS / f"{lib}.kicad_sym"


def lib_symbols(lib: str) -> dict:
    if lib not in _lib_cache:
        tree = parse(lib_file(lib).read_text())
        _lib_cache[lib] = {s[1]: s for s in find_all(tree, "symbol")}
    return _lib_cache[lib]


def flattened(lib_id: str) -> list:
    lib, name = lib_id.split(":", 1)
    syms = lib_symbols(lib)
    node = syms[name]
    ext = find(node, "extends")
    if ext is None:
        out = copy.deepcopy(node)
    else:
        parent = flattened(f"{lib}:{ext[1]}")
        pname = parent[1].split(":", 1)[1]
        out = copy.deepcopy(parent)
        child_props = {p[1]: p for p in find_all(node, "property")}
        new = [out[0], Q(name)]
        for item in out[2:]:
            if isinstance(item, list) and item and item[0] == "property" and item[1] in child_props:
                new.append(copy.deepcopy(child_props.pop(item[1])))
            elif isinstance(item, list) and item and item[0] == "symbol":
                sub = copy.deepcopy(item)
                sub[1] = Q(str(sub[1]).replace(pname + "_", name + "_", 1))
                new.append(sub)
            else:
                new.append(item)
        # remaining child-only properties go before the first sub-symbol
        idx = next(i for i, it in enumerate(new) if isinstance(it, list) and it and it[0] == "symbol")
        for prop in child_props.values():
            new.insert(idx, copy.deepcopy(prop))
            idx += 1
        out = new
    out[1] = Q(f"{lib}:{name}")
    return out


def symbol_pins(sym: list) -> list[dict]:
    pins = []
    for sub in find_all(sym, "symbol"):
        unit = int(str(sub[1]).rsplit("_", 2)[-2])
        if unit not in (0, 1):
            continue
        for p in find_all(sub, "pin"):
            at = find(p, "at")
            length = float(find(p, "length")[1])
            num = str(find(p, "number")[1])
            name = str(find(p, "name")[1])
            pins.append({"num": num, "name": name, "type": p[1], "x": float(at[1]), "y": float(at[2]),
                         "a": int(float(at[3])) if len(at) > 3 else 0, "len": length})
    return pins


def symbol_prop(sym: list, name: str):
    for p in find_all(sym, "property"):
        if p[1] == name:
            return p
    return None


def is_power_symbol(sym: list) -> bool:
    return find(sym, "power") is not None


# --------------------------------------------------------------------------
# Placement primitives
# --------------------------------------------------------------------------

DIRS = {0: (-1, 0), 90: (0, 1), 180: (1, 0), 270: (0, -1)}   # outward direction, schematic coords


def label_len(name: str, is_global: bool) -> float:
    return 1.27 * 0.62 * len(name) + (3.2 if is_global else 0.8)


class Sheet:
    def __init__(self, name, file, instances):
        self.name = name
        self.file = file
        self.instances = instances  # list of (instance key, sheet path string, ref formatter)
        self.items: list = []
        self.lib_ids: set = set()
        self.notes: list = []

    def add(self, node):
        self.items.append(node)


class Ctx:
    def __init__(self):
        self.pwr = 0
        self.flg = 0


def fx(v):
    return round(v, 4)


def wire(sheet, key, x1, y1, x2, y2):
    sheet.add(["wire", ["pts", ["xy", fx(x1), fx(y1)], ["xy", fx(x2), fx(y2)]],
               ["stroke", ["width", 0], ["type", "default"]], ["uuid", Q(uid(sheet.file, "w", key))]])


def label(sheet, key, net, x, y, outward):
    ang = {(1, 0): 0, (0, -1): 90, (-1, 0): 180, (0, 1): 270}[outward]
    is_global = net in circuit.GLOBAL_NETS
    if is_global:
        just = "left" if ang in (0, 90) else "right"
        sheet.add(["global_label", Q(net), ["shape", "bidirectional"], ["at", fx(x), fx(y), ang],
                   ["fields_autoplaced", "yes"],
                   ["effects", ["font", ["size", 1.27, 1.27]], ["justify", just]],
                   ["uuid", Q(uid(sheet.file, "gl", key))],
                   ["property", Q("Intersheetrefs"), Q("${INTERSHEET_REFS}"), ["at", fx(x), fx(y), ang],
                    ["effects", ["font", ["size", 1.27, 1.27]], ["justify", just], ["hide", "yes"]]]])
    else:
        just = ["justify", "left" if ang in (0, 90) else "right", "bottom"]
        sheet.add(["label", Q(net), ["at", fx(x), fx(y), ang], ["fields_autoplaced", "yes"],
                   ["effects", ["font", ["size", 1.27, 1.27]], just],
                   ["uuid", Q(uid(sheet.file, "l", key))]])


def place_symbol(sheet, ctx, lib_id, x, y, refs, value, fields, pin_nums, key,
                 in_bom=True, on_board=True, hide_ref=False, rot=0):
    """refs: list of (path, reference) per sheet instance."""
    sym = flattened(lib_id)
    sheet.lib_ids.add(lib_id)
    node = ["symbol", ["lib_id", Q(lib_id)], ["at", fx(x), fx(y), rot], ["unit", 1],
            ["exclude_from_sim", "no"], ["in_bom", "yes" if in_bom else "no"],
            ["on_board", "yes" if on_board else "no"], ["dnp", "no"],
            ["uuid", Q(uid(sheet.file, "sym", key))]]
    base = {"Reference": refs[0][1], "Value": value}
    base.update(fields)
    for pname in ["Reference", "Value", "Footprint", "Datasheet", "Description"]:
        lp = symbol_prop(sym, pname)
        val = base.get(pname, str(lp[2]) if lp is not None else "")
        if lp is not None:
            at = find(lp, "at")
            lx, ly = float(at[1]), float(at[2])
            la = float(at[3]) if len(at) > 3 else 0
            if rot:
                r = math.radians(rot)
                lx, ly = lx * math.cos(r) - ly * math.sin(r), lx * math.sin(r) + ly * math.cos(r)
                la = (la + rot) % 360 if rot in (90, 270) else la
            eff = copy.deepcopy(find(lp, "effects"))
        else:
            lx, ly, la, eff = 0, 0, 0, ["effects", ["font", ["size", 1.27, 1.27]], ["hide", "yes"]]
        if pname in ("Footprint", "Datasheet", "Description") or (pname == "Reference" and hide_ref):
            if find(eff, "hide") is None:
                eff.append(["hide", "yes"])
        node.append(["property", Q(pname), Q(val), ["at", fx(x + lx), fx(y - ly), la], eff])
    for k, v in fields.items():
        if k in ("Footprint", "Datasheet", "Description"):
            continue
        node.append(["property", Q(k), Q(v), ["at", fx(x), fx(y), 0],
                     ["effects", ["font", ["size", 1.27, 1.27]], ["hide", "yes"]]])
    for n in pin_nums:
        node.append(["pin", Q(n), ["uuid", Q(uid(sheet.file, "pin", key, n))]])
    inst = ["project", Q(PROJECT)]
    for path, ref in refs:
        inst.append(["path", Q(path), ["reference", Q(ref)], ["unit", 1]])
    node.append(["instances", inst])
    sheet.add(node)
    return sym


def power_refs(sheet, ctx, prefix):
    out = []
    for path in sheet.instance_paths:
        if prefix == "#PWR":
            ctx.pwr += 1
            out.append((path, f"#PWR{ctx.pwr:03d}"))
        else:
            ctx.flg += 1
            out.append((path, f"#FLG{ctx.flg:02d}"))
    return out


def gnd_symbol(sheet, ctx, key, x, y, rot=0):
    place_symbol(sheet, ctx, "power:GND", x, y, power_refs(sheet, ctx, "#PWR"), "GND", {}, ["1"],
                 key, hide_ref=True, rot=rot)


# --------------------------------------------------------------------------
# Part placement
# --------------------------------------------------------------------------

def part_geometry(sym, part):
    """Return (pins with absolute offsets, bbox) in schematic coordinates relative to origin."""
    pins = symbol_pins(sym)
    seen = {}
    pts = []
    out = []
    for p in pins:
        ex, ey = p["x"], -p["y"]
        a = p["a"]
        ca, sa = DIRS[a]
        rx, ry = ex - ca * p["len"], ey - sa * p["len"]
        pts += [(ex, ey), (rx, ry)]
        posk = (round(ex, 3), round(ey, 3))
        net = part.pins.get(p["num"], "__missing__")
        if net == "__missing__":
            raise SystemExit(f"{part.ref}: pin {p['num']} ({p['name']}) of {part.lib} has no net entry")
        if posk in seen:
            if seen[posk] != net:
                raise SystemExit(f"{part.ref}: stacked pins at {posk} on different nets")
            continue
        seen[posk] = net
        out.append({**p, "ex": ex, "ey": ey, "out": (ca, sa), "net": net})
        if net is None or p["type"] == "no_connect":
            continue
        sx, sy = ex + ca * G, ey + sa * G
        if net == "GND":
            if (ca, sa) == (0, 1):
                pts += [(sx - 1.5, sy), (sx + 1.5, sy + 3.0)]
            elif (ca, sa) == (0, -1):
                pts += [(sx - 1.5, sy), (sx + 1.5, sy - 3.0)]
            else:
                pts += [(sx, sy - 1.5), (sx + ca * 3.0, sy + 1.5)]
        else:
            ll = label_len(net, net in circuit.GLOBAL_NETS)
            lx, ly = sx + ca * ll, sy + sa * ll
            pts += [(sx, sy), (lx, ly), (lx + sa * 1.4, ly + ca * 1.4), (lx - sa * 1.4, ly - ca * 1.4)]
    extra = set(part.pins) - {p["num"] for p in pins}
    if extra:
        raise SystemExit(f"{part.ref}: nets given for unknown pins {sorted(extra)}")
    xs = [p[0] for p in pts] or [0]
    ys = [p[1] for p in pts] or [0]
    minx, maxx, miny, maxy = min(xs), max(xs), min(ys), max(ys)
    # room for reference/value text
    two_pin_vertical = len(out) == 2 and all(p["a"] in (90, 270) for p in out)
    if two_pin_vertical:
        maxx = max(maxx, 9.0)
    if not out:
        minx, maxx, miny, maxy = -3, 6, -4, 4
    else:
        miny -= 2.6
        maxy += 2.6
    return out, (minx - 1.27, miny - 1.27, maxx + 1.27, maxy + 1.27)


def draw_part(sheet, ctx, part, x, y, refs, key):
    sym = flattened(part.lib)
    fields = {"Footprint": part.footprint}
    if part.datasheet:
        fields["Datasheet"] = part.datasheet
    if part.desc:
        fields["Description"] = part.desc
    if part.mpn:
        fields["MPN"] = part.mpn
    if part.mfr:
        fields["Manufacturer"] = part.mfr
    for k, v in part.extra.items():
        fields[k] = v
    pins, _ = part_geometry(sym, part)
    place_symbol(sheet, ctx, part.lib, x, y, refs, part.value, fields,
                 [p["num"] for p in symbol_pins(sym)], key, in_bom=part.in_bom, on_board=part.on_board)
    for p in pins:
        ex, ey = x + p["ex"], y + p["ey"]
        ca, sa = p["out"]
        pk = f"{key}.{p['num']}"
        if p["net"] is None or p["type"] == "no_connect":
            sheet.add(["no_connect", ["at", fx(ex), fx(ey)], ["uuid", Q(uid(sheet.file, "nc", pk))]])
            continue
        sx, sy = ex + ca * G, ey + sa * G
        wire(sheet, pk, ex, ey, sx, sy)
        if p["net"] == "GND":
            if (ca, sa) == (0, 1):
                gnd_symbol(sheet, ctx, pk, sx, sy, 0)
            elif (ca, sa) == (0, -1):
                gnd_symbol(sheet, ctx, pk, sx, sy, 180)
            else:
                # horizontal pin: rotate the ground symbol so it points outward (never drops onto a neighbour)
                gnd_symbol(sheet, ctx, pk, sx, sy, 90 if ca > 0 else 270)
        else:
            label(sheet, pk, p["net"], sx, sy, (ca, sa))


def text(sheet, key, s, x, y, size=1.8, bold=True):
    eff = ["effects", ["font", ["size", size, size]] + ([["bold", "yes"]] if bold else []),
           ["justify", "left", "bottom"]]
    sheet.add(["text", Q(s), ["exclude_from_sim", "no"], ["at", fx(x), fx(y), 0], eff,
               ["uuid", Q(uid(sheet.file, "txt", key))]])


PAPERS = [("A3", 420, 297), ("A2", 594, 420), ("A1", 841, 594)]


def layout_sheet(sheet, ctx, groups, ref_for):
    """Pack groups in reading order. ``ref_for(ref_template) -> list[(path, ref)]``."""
    for paper, W, H in PAPERS:
        placements = []
        x0, y = 15.24, 25.4
        xmax = W - 15
        ok = True
        for gi, (title, parts) in enumerate(groups):
            placements.append(("text", gi, title, x0, y))
            y += 5.08
            x = x0
            row_h = 0
            for pi, part in enumerate(parts):
                sym = flattened(part.lib)
                _, (bx0, by0, bx1, by1) = part_geometry(sym, part)
                w, h = bx1 - bx0, by1 - by0
                if x + w > xmax and x > x0:
                    x = x0
                    y += row_h + 2.54
                    row_h = 0
                ox, oy = snap(x - bx0), snap(y - by0)
                placements.append(("part", gi, part, ox, oy, pi))
                x = ox + bx1 + 2.54
                row_h = max(row_h, oy + by1 - y)
            y += row_h + 7.62
            if y > H - 45:
                ok = False
        if ok:
            break
    sheet.paper = paper
    for pl in placements:
        if pl[0] == "text":
            _, gi, title, tx, ty = pl
            text(sheet, f"g{gi}", title, tx, ty)
        else:
            _, gi, part, ox, oy, pi = pl
            draw_part(sheet, ctx, part, ox, oy, ref_for(part.ref), f"g{gi}p{pi}")
    sheet.next_y = y


def sheet_text(sheet, root_uuid, title, comment):
    lib = ["lib_symbols"] + [flattened(l) for l in sorted(sheet.lib_ids)]
    head = ["kicad_sch", ["version", VERSION], ["generator", Q("eeschema")], ["generator_version", Q("9.0")],
            ["uuid", Q(sheet.uuid)], ["paper", Q(sheet.paper)],
            ["title_block", ["title", Q(title)], ["date", Q("2026-11-15")], ["rev", Q("B0")],
             ["company", Q("led-matrix project")],
             ["comment", 1, Q("PROTOTYPE - UNVALIDATED - NOT RELEASED FOR FABRICATION")],
             ["comment", 2, Q(comment)]],
            lib]
    body = head + sheet.items
    if sheet.is_root:
        body.append(["sheet_instances", ["path", Q("/"), ["page", Q("1")]]])
    body.append(["embedded_fonts", "no"])
    return dump(body) + "\n"


# --------------------------------------------------------------------------
# PWR_FLAG analysis
# --------------------------------------------------------------------------

def collect_nets(sheet_defs):
    """net key -> {'types': set, 'sheet': first sheet}; local nets keyed per sheet file."""
    nets = {}
    for name, file, builder, inst in sheet_defs:
        for _, parts in builder():
            for part in parts:
                sym = flattened(part.lib)
                pins = symbol_pins(sym)
                for p in pins:
                    net = part.pins.get(p["num"])
                    if net is None:
                        continue
                    k = net if net in circuit.GLOBAL_NETS else f"{file}:{net}"
                    e = nets.setdefault(k, {"types": set(), "file": file, "net": net})
                    e["types"].add(p["type"])
    return nets


def main():
    ctx = Ctx()
    root_uuid = uid("root")
    sheet_defs = circuit.SHEETS
    nets = collect_nets(sheet_defs)
    flags = {}
    for k, e in nets.items():
        if "power_in" in e["types"] and "power_out" not in e["types"]:
            flags.setdefault(e["file"], []).append(e["net"])

    # Root sheet ------------------------------------------------------------
    root = Sheet("Root", f"{PROJECT}.kicad_sch", [])
    root.uuid = root_uuid
    root.is_root = True
    root.instance_paths = [f"/{root_uuid}"]
    sheet_syms = []
    page = 2
    file_instances = {}
    for name, file, builder, insts in sheet_defs:
        for n in insts:
            sname = f"{name}{n}" if n is not None else name
            suid = uid("sheet", sname)
            sheet_syms.append((sname, file, suid, page))
            file_instances.setdefault(file, []).append((f"/{root_uuid}/{suid}", n, page))
            page += 1

    # Sub sheets -------------------------------------------------------------
    sub_sheets = []
    for name, file, builder, insts in sheet_defs:
        sh = Sheet(name, file, None)
        sh.uuid = uid("file", file)
        sh.is_root = False
        sh.instance_paths = [p for p, _, _ in file_instances[file]]
        inst_n = [n for _, n, _ in file_instances[file]]

        def ref_for(tmpl, paths=sh.instance_paths, ns=inst_n):
            return [(path, tmpl.format(n=n) if n is not None else tmpl) for path, n in zip(paths, ns)]

        groups = list(builder())
        flag_parts = []
        for net in flags.get(file, []):
            flag_parts.append(circuit.Part("#FLG", "power:PWR_FLAG", "PWR_FLAG", "", {"1": net},
                                           desc="Net driven by passive pins only (connector/fuse/FET)", in_bom=False, on_board=False))
        if flag_parts:
            groups.append(("Power flags (nets driven only through passive parts)", flag_parts))

        def ref_for2(tmpl, sh=sh, base=ref_for):
            if tmpl == "#FLG":
                return power_refs(sh, ctx, "#FLG")
            return base(tmpl)

        layout_sheet(sh, ctx, groups, ref_for2)
        sub_sheets.append(sh)

    # root content
    x, y = 25.4, 40.64
    for i, (sname, file, suid, pg) in enumerate(sheet_syms):
        col, row = i % 4, i // 4
        sx, sy = 25.4 + col * 96.52, 45.72 + row * 45.72
        root.add(["sheet", ["at", fx(sx), fx(sy)], ["size", 76.2, 25.4], ["exclude_from_sim", "no"],
                  ["in_bom", "yes"], ["on_board", "yes"], ["dnp", "no"], ["fields_autoplaced", "yes"],
                  ["stroke", ["width", 0.1524], ["type", "solid"]], ["fill", ["color", 0, 0, 0, 0.0]],
                  ["uuid", Q(suid)],
                  ["property", Q("Sheetname"), Q(sname), ["at", fx(sx), fx(sy - 0.7116), 0],
                   ["effects", ["font", ["size", 1.27, 1.27]], ["justify", "left", "bottom"]]],
                  ["property", Q("Sheetfile"), Q(file), ["at", fx(sx), fx(sy + 25.4 + 0.5846), 0],
                   ["effects", ["font", ["size", 1.27, 1.27]], ["justify", "left", "top"]]],
                  ["instances", ["project", Q(PROJECT), ["path", Q(f"/{root_uuid}"), ["page", Q(str(pg))]]]]])
    text(root, "title", "24 V -> 5 V power distribution and halo controller for the 2x2 HUB75 panel display", 25.4, 30.48, 2.5)
    notes = [
        "Rev B, PROTOTYPE / UNVALIDATED, one-off hobby build. Input: 24 V brick (Mean Well GST160A24-R7B) via KPJX-PM-4S jack -> pigtail -> J1.",
        "Hot-plug into a running brick is supported: LM74700 ideal diode, SMBJ30A TVS, LM5069-2 hot-swap (inrush, 7.9 A limit, UV 17.9 V, OV 33.5 V).",
        "Two panel outputs, each feeding a pair of 5 V / 4 A panels: LM61495 buck (8 A) + TPS259823 eFuse (9.8 A limit) + M3.5 barrier terminal.",
        "AUX: TPSM63610 for Pi, halo and logic; it starts only after the hot-swap reports power good (PGD on EN_AUX).",
        "HUB75 data stays on the Electrodragon RGB matrix driver V2 (MPC1119). This board never touches HUB75 pins.",
        "Pi interface: GPIO14 (TXD, UART to halo MCU) and GPIO26 (panel enable) only; both unused at --led-parallel=2.",
        "Software brightness limits are not protection: each panel pair is limited in hardware (eFuse, ~9.8 A).",
    ]
    for i, n in enumerate(notes):
        text(root, f"note{i}", n, 25.4, 150 + i * 5.08, 1.5, bold=False)
    root.paper = "A3"
    mech = circuit.sheet_root_parts()
    root.next_y = 0

    def root_ref(tmpl):
        return [(f"/{root_uuid}", tmpl)]
    # mechanical symbols in a row at the bottom
    for gi, (title, parts) in enumerate(mech):
        text(root, f"mech{gi}", title, 25.4, 200.66)
        for pi, part in enumerate(parts):
            draw_part(root, ctx, part, 30.48 + pi * 17.78, 213.36, root_ref(part.ref), f"mech{pi}")

    BOARD.mkdir(parents=True, exist_ok=True)
    (BOARD / root.file).write_text(sheet_text(root, root_uuid, "Power + halo board - top level",
                                              "2x2 Waveshare P3 64x64 HUB75E, Raspberry Pi 3B + Electrodragon V2"))
    for sh in sub_sheets:
        title = {"Input": "24 V input protection and hot-swap", "Panel": "Panel pair power channel (x2)",
                 "Aux": "AUX 5 V and Raspberry Pi branch", "Halo": "Halo controller and Pi logic interface"}[sh.name]
        (BOARD / sh.file).write_text(sheet_text(sh, root_uuid, title, "Power + halo board"))
    print("sheets:", root.file, *[f"{s.file}({s.paper})" for s in sub_sheets])
    print("PWR_FLAG nets:", flags)


if __name__ == "__main__":
    main()
