#!/usr/bin/env python3
"""Resolve il2cpp metadata-usage cells (the 0x5cXXXXX addresses a method loads its
string literals through) to the literal they name.

The cell in .data.rel.ro is zero in the file; .rela.dyn fills it with a pointer to a
.data slot, and that slot holds the encoded usage token. A token whose top three bits
are 5 (0xa0000000) is a string literal, index (tok & 0x1FFFFFFF) >> 1.

usage: python3 tools/lit_cell.py 0x5c60538 [0x... ...]
"""
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SO = ROOT / "lib" / "arm64-v8a" / "libil2cpp.so"


def load():
    so = SO.read_bytes()
    shoff = struct.unpack_from("<Q", so, 0x28)[0]
    shentsize, shnum = struct.unpack_from("<HH", so, 0x3A)
    rel = {}
    loads = []
    phoff = struct.unpack_from("<Q", so, 0x20)[0]
    phentsize, phnum = struct.unpack_from("<HH", so, 0x36)
    for i in range(phnum):
        t, _, off, va, _, fs, _, _ = struct.unpack_from("<IIQQQQQQ", so, phoff + i * phentsize)
        if t == 1:
            loads.append((va, va + fs, va - off))
    for i in range(shnum):
        s = struct.unpack_from("<IIQQQQIIQQ", so, shoff + i * shentsize)
        if s[1] == 4:  # SHT_RELA
            for k in range(s[5] // 24):
                off, _, add = struct.unpack_from("<QQq", so, s[4] + k * 24)
                rel[off] = add
    return so, rel, loads


def literals(indices):
    out = {}
    text = subprocess.run([sys.executable, str(ROOT / "tools" / "metadata_strings.py"), "list"],
                          capture_output=True, text=True, cwd=str(ROOT)).stdout
    for line in text.splitlines():
        p = line.split("\t")
        if len(p) >= 3 and p[1].startswith("#") and int(p[1][1:]) in indices:
            out[int(p[1][1:])] = p[2]
    return out


def main(argv):
    so, rel, loads = load()
    rows = []
    for a in argv:
        cell = int(a, 16)
        slot = rel.get(cell)
        if slot is None:
            rows.append((a, None, "no relocation"))
            continue
        delta = next((d for lo, hi, d in loads if lo <= slot < hi), None)
        tok = struct.unpack_from("<Q", so, slot - delta)[0] if delta is not None else 0
        if tok >> 29 & 7 != 5:
            rows.append((a, None, "not a string literal (token %s)" % hex(tok)))
            continue
        rows.append((a, (tok & 0x1FFFFFFF) >> 1, None))
    lits = literals({r[1] for r in rows if r[1] is not None})
    for a, idx, err in rows:
        print(a, err if err else "#%d %s" % (idx, lits.get(idx, "?")))


if __name__ == "__main__":
    main(sys.argv[1:])
