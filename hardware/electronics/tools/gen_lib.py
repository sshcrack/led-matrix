#!/usr/bin/env python3
"""Generate the project-local KiCad library (symbols + footprints).

Pin numbers and land patterns are transcribed from the TI data sheets:
  * TPSM63610  SLVSGU1A (Dec 2023), Table 5-1, package RDF0022A
  * TPS25947   SLVSFC9C (May 2026), Table 5-1, package RPW0010A
Coordinates are millimetres, KiCad footprint axes (+y down, top view).
"""

from __future__ import annotations

import sys
import uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
BOARD = HERE.parent / "power-halo-board"
LIB = BOARD / "lib"
NS = uuid.UUID("6d1f3c52-6f0e-4d5a-9a4e-1c0b5f0e7a11")


def uid(*parts: str) -> str:
    return str(uuid.uuid5(NS, "/".join(parts)))


def f(v: float) -> str:
    s = f"{v:.4f}".rstrip("0").rstrip(".")
    return "0" if s in ("", "-0") else s


# --------------------------------------------------------------------------
# Symbols
# --------------------------------------------------------------------------

def pin(ptype, x, y, ang, name, num, length=2.54):
    return (f'\t\t\t(pin {ptype} line (at {f(x)} {f(y)} {ang}) (length {f(length)})\n'
            f'\t\t\t\t(name "{name}" (effects (font (size 1.27 1.27))))\n'
            f'\t\t\t\t(number "{num}" (effects (font (size 1.27 1.27))))\n\t\t\t)\n')


def prop(name, value, x, y, hide=False, justify=None):
    eff = '(font (size 1.27 1.27))'
    if justify:
        eff += f' (justify {justify})'
    if hide:
        eff += ' (hide yes)'
    return f'\t\t(property "{name}" "{value}" (at {f(x)} {f(y)} 0) (effects {eff}))\n'


def symbol(name, ref, value, footprint, datasheet, desc, keywords, body, pins, ref_y, val_y):
    s = (f'\t(symbol "{name}" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)\n')
    s += prop("Reference", ref, 0, ref_y)
    s += prop("Value", value, 0, val_y)
    s += prop("Footprint", footprint, 0, val_y - 2.54, hide=True)
    s += prop("Datasheet", datasheet, 0, val_y - 5.08, hide=True)
    s += prop("Description", desc, 0, val_y - 7.62, hide=True)
    s += prop("ki_keywords", keywords, 0, 0, hide=True)
    s += f'\t\t(symbol "{name}_0_1"\n{body}\t\t)\n'
    s += f'\t\t(symbol "{name}_1_1"\n' + "".join(pins) + '\t\t)\n'
    s += '\t\t(embedded_fonts no)\n\t)\n'
    return s


def rect(x1, y1, x2, y2):
    return (f'\t\t\t(rectangle (start {f(x1)} {f(y1)}) (end {f(x2)} {f(y2)})\n'
            f'\t\t\t\t(stroke (width 0.254) (type default)) (fill (type background)))\n')


def poly(pts, fill="none", width=0.254):
    p = " ".join(f"(xy {f(x)} {f(y)})" for x, y in pts)
    return (f'\t\t\t(polyline (pts {p})\n'
            f'\t\t\t\t(stroke (width {f(width)}) (type default)) (fill (type {fill})))\n')


def tpsm63610_symbol():
    pins = []
    L, R, B = -17.78, 17.78, -15.24
    left = [("power_in", "VIN1", 1, 10.16), ("power_in", "VIN2", 18, 7.62),
            ("input", "EN", 17, 2.54), ("input", "SYNC/MODE", 15, 0),
            ("input", "SPSP", 14, -2.54), ("passive", "RT", 12, -5.08),
            ("open_collector", "PG", 13, -7.62), ("no_connect", "NC", 16, -10.16)]
    right = [("power_out", "VOUT1", 9, 10.16), ("passive", "VOUT2", 10, 7.62),
             ("power_in", "VLDOIN", 5, 5.08), ("input", "FB", 8, 0),
             ("power_out", "VCC", 6, -2.54), ("passive", "RBOOT", 2, -5.08),
             ("passive", "CBOOT", 3, -7.62), ("passive", "SW", 4, -10.16)]
    bottom = [("power_in", "PGND", 19, -12.7), ("power_in", "PGND", 20, -7.62),
              ("power_in", "AGND", 7, -2.54), ("power_in", "AGND", 11, 2.54),
              ("power_in", "AGND", 21, 7.62), ("power_in", "AGND", 22, 12.7)]
    for t, n, num, y in left:
        pins.append(pin(t, L, y, 0, n, num))
    for t, n, num, y in right:
        pins.append(pin(t, R, y, 180, n, num))
    for t, n, num, x in bottom:
        pins.append(pin(t, x, B, 90, n, num))
    body = rect(-15.24, 12.7, 15.24, -12.7)
    return symbol(
        "TPSM63610", "U", "TPSM63610RDFR",
        "ledpanel:TI_RDF0022A_B3QFN-22_6.5x7.5mm",
        "https://www.ti.com/lit/ds/symlink/tpsm63610.pdf",
        "TI 3-36 V in, 8 A synchronous buck power module, B3QFN-22 (RDF)",
        "buck module DC-DC TPSM63610", body, pins, 13.97, -13.97 - 2.54)


def tps259470_symbol():
    pins = []
    left = [("power_in", "IN", 5, 5.08), ("input", "EN/UVLO", 1, 0), ("input", "OVLO", 2, -2.54)]
    right = [("power_out", "OUT", 6, 5.08), ("open_collector", "~{FLT}", 4, 0),
             ("open_collector", "AUXOFF", 3, -2.54)]
    bottom = [("passive", "ILM", 9, -5.08), ("passive", "ITIMER", 10, -2.54),
              ("passive", "DVDT", 7, 0), ("power_in", "GND", 8, 5.08)]
    for t, n, num, y in left:
        pins.append(pin(t, -12.7, y, 0, n, num))
    for t, n, num, y in right:
        pins.append(pin(t, 12.7, y, 180, n, num))
    for t, n, num, x in bottom:
        pins.append(pin(t, x, -10.16, 90, n, num))
    body = rect(-10.16, 7.62, 10.16, -7.62)
    return symbol(
        "TPS259470A", "U", "TPS259470ARPWR",
        "ledpanel:TI_RPW0010A_VQFN-HR-10_2x2mm",
        "https://www.ti.com/lit/ds/symlink/tps25947.pdf",
        "TI 2.7-23 V, 5.5 A, 28 mOhm eFuse with reverse current blocking, adjustable OVLO, auto-retry, VQFN-HR-10 (RPW)",
        "eFuse hot-swap load switch reverse blocking TPS25947", body, pins, 8.89, -8.89 - 2.54)


def tvs_uni_symbol(name, value, fp, ds, desc):
    body = (poly([(1.27, 1.27), (1.27, -1.27), (-1.27, 0), (1.27, 1.27)], fill="none")
            + poly([(-0.762, 1.27), (-1.27, 1.27), (-1.27, -1.27), (-1.778, -1.27)]))
    pins = [pin("passive", -3.81, 0, 0, "K", 1), pin("passive", 3.81, 0, 180, "A", 2)]
    s = symbol(name, "D", value, fp, ds, desc, "TVS unidirectional diode", body, pins, 2.54, -2.54)
    # hide pin names for the diode
    return s.replace('(pin_names (offset 1.016))', '(pin_numbers (hide yes)) (pin_names (offset 1.016) (hide yes))')


def write_symbols():
    out = '(kicad_symbol_lib\n\t(version 20241209)\n\t(generator "ledpanel_gen_lib")\n\t(generator_version "1.0")\n'
    out += tpsm63610_symbol()
    out += tps259470_symbol()
    out += tvs_uni_symbol(
        "SMBJ30A", "SMBJ30A", "Diode_SMD:D_SMB",
        "https://www.littelfuse.com/products/tvs-diodes/surface-mount/smbj",
        "600 W unidirectional TVS, VRWM 30 V, VBR min 33.3 V, SMB (DO-214AA)")
    out += tvs_uni_symbol(
        "SMAJ6.0A", "SMAJ6.0A", "Diode_SMD:D_SMA",
        "https://www.littelfuse.com/products/tvs-diodes/surface-mount/smaj",
        "400 W unidirectional TVS, VRWM 6.0 V, SMA (DO-214AC)")
    out += ')\n'
    (LIB / "ledpanel.kicad_sym").write_text(out)


# --------------------------------------------------------------------------
# Footprints
# --------------------------------------------------------------------------

PAD_LAYERS = '(layers "F.Cu" "F.Paste" "F.Mask")'


def fp_header(name, descr, tags, ref_y, val_y):
    return (f'(footprint "{name}"\n\t(version 20241229)\n\t(generator "ledpanel_gen_lib")\n'
            f'\t(generator_version "1.0")\n\t(layer "F.Cu")\n\t(descr "{descr}")\n\t(tags "{tags}")\n'
            f'\t(property "Reference" "REF**" (at 0 {f(ref_y)} 0) (layer "F.SilkS") (uuid "{uid(name, "ref")}")\n'
            f'\t\t(effects (font (size 1 1) (thickness 0.15))))\n'
            f'\t(property "Value" "{name}" (at 0 {f(val_y)} 0) (layer "F.Fab") (uuid "{uid(name, "val")}")\n'
            f'\t\t(effects (font (size 1 1) (thickness 0.15))))\n'
            f'\t(property "Datasheet" "" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{uid(name, "ds")}")\n'
            f'\t\t(effects (font (size 1.27 1.27) (thickness 0.15))))\n'
            f'\t(property "Description" "{descr}" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{uid(name, "desc")}")\n'
            f'\t\t(effects (font (size 1.27 1.27) (thickness 0.15))))\n'
            f'\t(attr smd)\n')


def fp_line(name, k, x1, y1, x2, y2, layer, w):
    return (f'\t(fp_line (start {f(x1)} {f(y1)}) (end {f(x2)} {f(y2)}) (stroke (width {f(w)}) (type solid))'
            f' (layer "{layer}") (uuid "{uid(name, "line", str(k))}"))\n')


def fp_rect(name, k, x1, y1, x2, y2, layer, w):
    return (f'\t(fp_rect (start {f(x1)} {f(y1)}) (end {f(x2)} {f(y2)}) (stroke (width {f(w)}) (type solid))'
            f' (fill no) (layer "{layer}") (uuid "{uid(name, "rect", str(k))}"))\n')


def fp_circle(name, k, x, y, r, layer, w, fill=False):
    return (f'\t(fp_circle (center {f(x)} {f(y)}) (end {f(x + r)} {f(y)}) (stroke (width {f(w)}) (type solid))'
            f' (fill {"yes" if fill else "no"}) (layer "{layer}") (uuid "{uid(name, "circ", str(k))}"))\n')


def fp_model(name):
    """3D model reference; gen_3dmodels.py writes the envelope box it points at."""
    return (f'\t(model "${{KIPRJMOD}}/lib/3dmodels/{name}.wrl"\n'
            f'\t\t(offset (xyz 0 0 0))\n\t\t(scale (xyz 1 1 1))\n\t\t(rotate (xyz 0 0 0))\n\t)\n')


def rect_pad(name, num, x, y, w, h, k, paste=True):
    layers = PAD_LAYERS if paste else '(layers "F.Cu" "F.Mask")'
    return (f'\t(pad "{num}" smd rect (at {f(x)} {f(y)}) (size {f(w)} {f(h)}) {layers}'
            f' (uuid "{uid(name, "pad", str(k))}"))\n')


def custom_pad(name, num, ax, ay, aw, ah, pts, k):
    # pts are absolute footprint coordinates; primitives are relative to the pad anchor
    rel = " ".join(f"(xy {f(x - ax)} {f(y - ay)})" for x, y in pts)
    return (f'\t(pad "{num}" smd custom (at {f(ax)} {f(ay)}) (size {f(aw)} {f(ah)}) {PAD_LAYERS}\n'
            f'\t\t(options (clearance outline) (anchor rect))\n'
            f'\t\t(primitives (gr_poly (pts {rel}) (width 0) (fill yes)))\n'
            f'\t\t(uuid "{uid(name, "pad", str(k))}"))\n')


def rdf0022a():
    name = "TI_RDF0022A_B3QFN-22_6.5x7.5mm"
    s = fp_header(name, "TI RDF0022A B3QFN-22 6.5x7.5 mm power module (TPSM63610), land pattern per SLVSGU1A; corner-pad notches simplified to rectangles",
                  "TPSM63610 B3QFN RDF0022A", -4.9, 4.9)
    k = 0
    ys = [-1.95, -1.3, -0.65, 0.0, 0.65, 1.3, 1.95]
    for i, y in enumerate(ys):          # pins 2..8, top to bottom
        s += rect_pad(name, 2 + i, -2.853, y, 1.2, 0.25, k); k += 1
    for i, y in enumerate(reversed(ys)):  # pins 11..17, bottom to top
        s += rect_pad(name, 11 + i, 2.853, y, 1.2, 0.25, k); k += 1
    corners = {1: (-2.752, -2.927), 9: (-2.752, 2.927), 10: (2.752, 2.927), 18: (2.752, -2.927)}
    for num, (x, y) in corners.items():
        s += rect_pad(name, num, x, y, 1.4, 0.9, k); k += 1
    for num, y in zip((19, 20, 21, 22), (-2.25, -0.75, 0.75, 2.25)):
        s += rect_pad(name, num, 0, y, 3.3, 1.0, k); k += 1
    # body / fab / silk / courtyard
    s += fp_rect(name, 0, -3.25, -3.75, 3.25, 3.75, "F.Fab", 0.1)
    s += fp_line(name, 1, -3.25, -3.0, -2.5, -3.75, "F.Fab", 0.1)
    # silk: short ticks on the top and bottom edges clear of pads, plus pin-1 dot
    s += fp_line(name, 2, -1.6, -3.86, 1.6, -3.86, "F.SilkS", 0.12)
    s += fp_line(name, 3, -1.6, 3.86, 1.6, 3.86, "F.SilkS", 0.12)
    s += fp_circle(name, 4, -3.9, -3.9, 0.15, "F.SilkS", 0.3, fill=True)
    s += fp_rect(name, 5, -3.75, -4.1, 3.75, 4.1, "F.CrtYd", 0.05)
    s += fp_model(name)
    s += '\t(embedded_fonts no)\n)\n'
    return name, s


def rpw0010a():
    name = "TI_RPW0010A_VQFN-HR-10_2x2mm"
    s = fp_header(name, "TI RPW0010A VQFN-HR-10 2x2 mm (TPS25947), land pattern per SLVSFC9C",
                  "TPS25947 VQFN-HR RPW0010A", -2.2, 2.2)
    k = 0
    s += rect_pad(name, 2, -0.9, -0.225, 0.6, 0.25, k); k += 1
    s += rect_pad(name, 3, -0.9, 0.225, 0.6, 0.25, k); k += 1
    s += rect_pad(name, 9, 0.9, -0.225, 0.6, 0.25, k); k += 1
    s += rect_pad(name, 8, 0.9, 0.225, 0.6, 0.25, k); k += 1
    s += rect_pad(name, 5, -0.25, 0, 0.3, 2.4, k); k += 1
    s += rect_pad(name, 6, 0.25, 0, 0.3, 2.4, k); k += 1

    def lpad(num, sx, sy):
        # L-shaped corner terminal drawn as two overlapping rectangles with the same number:
        # horizontal arm [0.6,1.2]x[0.55,0.85] and vertical arm [0.6,0.85]x[0.55,1.2] (union = TI L land).
        # Plain rectangles keep the land exact while staying convex for autorouters/CAM.
        a = rect_pad(name, num, sx * 0.9, sy * 0.7, 0.6, 0.3, 200 + num)
        b = rect_pad(name, num, sx * 0.725, sy * 0.875, 0.25, 0.65, 300 + num)
        return a + b

    s += lpad(1, -1, -1)
    s += lpad(4, -1, 1)
    s += lpad(10, 1, -1)
    s += lpad(7, 1, 1)
    s += fp_rect(name, 0, -1.0, -1.0, 1.0, 1.0, "F.Fab", 0.1)
    s += fp_line(name, 1, -1.0, -0.6, -0.6, -1.0, "F.Fab", 0.1)
    s += fp_circle(name, 2, -1.45, -1.45, 0.12, "F.SilkS", 0.24, fill=True)
    s += fp_rect(name, 3, -1.5, -1.5, 1.5, 1.5, "F.CrtYd", 0.05)
    s += fp_model(name)
    s += '\t(embedded_fonts no)\n)\n'
    return name, s


def write_footprints():
    d = LIB / "ledpanel.pretty"
    d.mkdir(parents=True, exist_ok=True)
    for gen in (rdf0022a, rpw0010a):
        name, text = gen()
        (d / f"{name}.kicad_mod").write_text(text)


def write_tables():
    # The project table also lists the stock KiCad libraries the schematic uses, so the project
    # resolves every symbol even where no global sym-lib-table has been configured.
    stock = ["Device", "power", "Connector", "Connector_Generic", "Mechanical", "Power_Management",
             "Transistor_FET", "MCU_Microchip_ATtiny", "74xGxx", "Jumper"]
    rows = ''.join(f'\t(lib (name "{n}")(type "KiCad")(uri "${{KICAD9_SYMBOL_DIR}}/{n}.kicad_sym")(options "")(descr "KiCad stock library"))\n'
                   for n in stock)
    (BOARD / "sym-lib-table").write_text(
        '(sym_lib_table\n\t(version 7)\n'
        '\t(lib (name "ledpanel")(type "KiCad")(uri "${KIPRJMOD}/lib/ledpanel.kicad_sym")(options "")(descr "Project-local symbols"))\n'
        + rows + ')\n')
    (BOARD / "fp-lib-table").write_text(
        '(fp_lib_table\n\t(version 7)\n'
        '\t(lib (name "ledpanel")(type "KiCad")(uri "${KIPRJMOD}/lib/ledpanel.pretty")(options "")(descr "Project-local footprints"))\n)\n')


def main():
    LIB.mkdir(parents=True, exist_ok=True)
    write_symbols()
    write_footprints()
    write_tables()
    print("library written to", LIB)


if __name__ == "__main__":
    sys.exit(main())
