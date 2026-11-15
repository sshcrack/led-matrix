#!/usr/bin/env python3
"""Cross-check the KiCad-exported netlist against circuit.py (independent of ERC)."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import circuit
from sexpr import parse, find, find_all

def expected():
    exp = {}  # (ref, pin) -> canonical net name
    for name, file, builder, insts in circuit.SHEETS:
        for n in insts:
            sname = f"{name}{n}" if n is not None else name
            for _, parts in builder():
                for part in parts:
                    ref = part.ref.format(n=n) if n is not None else part.ref
                    for pin, net in part.pins.items():
                        if net is None:
                            continue
                        full = net if net in circuit.GLOBAL_NETS else f"/{sname}/{net}"
                        exp[(ref, pin)] = full
    return exp

def actual(path):
    tree = parse(Path(path).read_text())
    nets = find(tree, "nets")
    act = {}
    for net in find_all(nets, "net"):
        name = str(find(net, "name")[1])
        for node in find_all(net, "node"):
            ref = str(find(node, "ref")[1]); pin = str(find(node, "pin")[1])
            act[(ref, pin)] = name
    comps = {str(find(c, "ref")[1]) for c in find_all(find(tree, "components"), "comp")}
    return act, comps

def main(path):
    exp = expected()
    act, comps = actual(path)
    errors = []
    # group expected pins into nets and make sure each expected net maps to exactly one actual net
    groups = {}
    for k, v in exp.items():
        groups.setdefault(v, set()).add(k)
    for net, members in groups.items():
        got = {act.get(m, "<missing>") for m in members}
        if len(got) != 1:
            errors.append(f"net {net} split across {got}")
        else:
            g = got.pop()
            others = {k for k, v in act.items() if v == g} - members
            if others:
                errors.append(f"net {net} ({g}) has unexpected extra pins {sorted(others)}")
    unconnected = [k for k, v in act.items() if v.startswith("unconnected-") and k in exp]
    if unconnected:
        errors.append(f"expected-connected pins left unconnected: {unconnected}")
    print(f"{len(comps)} components, {len(groups)} expected nets, {len(exp)} connected pins checked")
    for e in errors:
        print("ERROR", e)
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
