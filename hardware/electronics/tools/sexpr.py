"""Minimal KiCad S-expression reader/writer that preserves string quoting.

Quoted tokens become :class:`Q` (a ``str`` subclass) so that re-serialised
library symbols keep KiCad's distinction between keywords and strings.
"""

from __future__ import annotations

import re


class Q(str):
    """A token that must be written as a quoted string."""


_TOKEN = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))', re.S)


def parse(text: str):
    stack: list[list] = [[]]
    pos = 0
    n = len(text)
    while pos < n:
        m = _TOKEN.match(text, pos)
        if not m:
            if text[pos:].strip() == "":
                break
            raise ValueError(f"cannot tokenise at {pos}: {text[pos:pos + 40]!r}")
        pos = m.end()
        if m.group(1):
            stack.append([])
        elif m.group(2):
            node = stack.pop()
            stack[-1].append(node)
        elif m.group(3) is not None:
            stack[-1].append(Q(m.group(3).replace('\\"', '"').replace("\\\\", "\\")))
        elif m.group(4) is not None:
            stack[-1].append(m.group(4))
    return stack[0][0] if len(stack[0]) == 1 else stack[0]


def fmt_num(v: float) -> str:
    if isinstance(v, bool):
        return "yes" if v else "no"
    if isinstance(v, int):
        return str(v)
    s = f"{v:.4f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def _atom(a) -> str:
    if isinstance(a, Q):
        return '"' + a.replace("\\", "\\\\").replace('"', '\\"') + '"'
    if isinstance(a, (int, float)):
        return fmt_num(a)
    return str(a)


def dump(node, indent: int = 0) -> str:
    if not isinstance(node, list):
        return _atom(node)
    # short lists on one line
    flat = all(not isinstance(c, list) for c in node)
    if flat:
        return "(" + " ".join(_atom(c) for c in node) + ")"
    pad = "\t" * (indent + 1)
    parts = []
    head = []
    for c in node:
        if isinstance(c, list):
            break
        head.append(_atom(c))
    rest = node[len(head):]
    out = "(" + " ".join(head)
    for c in rest:
        if isinstance(c, list):
            simple = all(not isinstance(x, list) for x in c) or (
                len(c) <= 4 and all(not isinstance(x, list) or all(not isinstance(y, list) for y in x) for x in c)
                and len(dump_inline(c)) < 90)
            if simple:
                parts.append("\n" + pad + dump_inline(c))
            else:
                parts.append("\n" + pad + dump(c, indent + 1))
        else:
            parts.append(" " + _atom(c))
    return out + "".join(parts) + "\n" + "\t" * indent + ")"


def dump_inline(node) -> str:
    if not isinstance(node, list):
        return _atom(node)
    return "(" + " ".join(dump_inline(c) for c in node) + ")"


def find(node, key):
    for c in node:
        if isinstance(c, list) and c and c[0] == key:
            return c
    return None


def find_all(node, key):
    return [c for c in node if isinstance(c, list) and c and c[0] == key]
