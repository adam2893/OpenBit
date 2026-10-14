#!/usr/bin/env python3
"""Map libil2cpp module offsets back to il2cpp_dump.txt class::method names.

The dump holds absolute addresses from the run that produced it; a hook reports
module offsets from today's run. One (name -> both addresses) anchor fixes the
base, every further anchor is a consistency check.

Usage:
  python3 il2cpp_offsets.py --dump il2cpp_dump.txt --anchors anchors.json --frames 0x1a,0x2b [--tol 0x400]
  python3 il2cpp_offsets.py --dump il2cpp_dump.txt --anchors anchors.json --prod prod.json
"""
import argparse, json, re, sys

METHOD = re.compile(r"^\s+method @0x([0-9a-f]+) (.+?) ([A-Za-z_<>.\`0-9]+)\(([^)]*)\)")
CLASS = re.compile(r"^class (\S+)")


def parse_dump(path):
    entries = []  # (addr, class, name, argc)
    cls = "?"
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if line.startswith("class "):
                m = CLASS.match(line)
                if m:
                    cls = m.group(1)
                continue
            m = METHOD.match(line)
            if not m:
                continue
            addr = int(m.group(1), 16)
            name = m.group(3)
            args = m.group(4).strip()
            argc = 0 if args == "" else args.count(",") + 1
            entries.append((addr, cls, name, argc))
    entries.sort()
    return entries


def anchor_base(entries, anchors):
    """Return (base, notes). base = dump_addr - module_offset."""
    by_key = {}
    for addr, cls, name, argc in entries:
        by_key.setdefault((cls, name, argc), []).append(addr)
    bases, notes = [], []
    for a in anchors:
        key = (a["cls"], a["name"], a.get("argc", 0))
        hit = by_key.get(key)
        off = int(a["off"], 16)
        if not hit:
            notes.append(f"anchor {a['cls']}::{a['name']}({key[2]}) not in dump")
            continue
        if len(hit) > 1:
            notes.append(f"anchor {a['cls']}::{a['name']}({key[2]}) ambiguous x{len(hit)}")
            continue
        bases.append((hit[0] - off, a["cls"], a["name"]))
    if not bases:
        return None, notes
    base = bases[0][0]
    for b, cls, name in bases[1:]:
        if b != base:
            notes.append(f"anchor disagreement {cls}::{name}: base {hex(b)} vs {hex(base)}")
    return base, notes


def resolve(entries, base, off, tol=0x400):
    target = base + off
    lo, hi = 0, len(entries) - 1
    best = -1
    while lo <= hi:
        mid = (lo + hi) // 2
        if entries[mid][0] <= target:
            best = mid
            lo = mid + 1
        else:
            hi = mid - 1
    if best < 0:
        return None, None
    addr, cls, name, argc = entries[best]
    gap = target - addr
    if gap > tol:
        return None, gap
    return (f"{cls}::{name}", gap)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", default="il2cpp_dump.txt")
    ap.add_argument("--anchors", required=True, help="JSON file: [{cls,name,argc,off}] or prod payload")
    ap.add_argument("--frames", help="comma-separated module offsets")
    ap.add_argument("--prod", help="prod RPC payload JSON with rows[].frames")
    ap.add_argument("--tol", default="0x400")
    args = ap.parse_args()

    tol = int(args.tol, 16) if args.tol.startswith("0x") else int(args.tol)
    payload = json.load(open(args.anchors))
    anchors = payload.get("anchors", payload) if isinstance(payload, dict) else payload
    entries = parse_dump(args.dump)
    base, notes = anchor_base(entries, anchors)
    if base is None:
        print(json.dumps({"error": "no usable anchor", "notes": notes}))
        return 1
    print(f"dump entries {len(entries)}  base_dump {hex(base)}")
    for n in notes:
        print("  note:", n)

    if args.prod:
        rows = json.load(open(args.prod))
        rows = rows.get("rows", rows)
        out = []
        for r in rows:
            names = []
            for off in r.get("frames", []):
                nm, gap = resolve(entries, base, int(off, 16), tol)
                names.append({"off": off, "name": nm, "gap": gap})
            out.append({"cls": r.get("cls"), "n": r.get("n"), "frames": names})
        print(json.dumps(out, indent=1))
        return 0

    if args.frames:
        for off in args.frames.split(","):
            nm, gap = resolve(entries, base, int(off.strip(), 16), tol)
            print(f"{off.strip()} -> {nm} (gap {gap})")
        return 0
    print("nothing to resolve: pass --frames or --prod")
    return 0


if __name__ == "__main__":
    sys.exit(main())
