#!/usr/bin/env python3
"""Attribute each captured DataDumbScenario to its source table.

pushdumb rows come from DumbScenarioInterface.GetARandomPushDumbScenario with
the Data row object now read (DumbScenario.Data @0x170). A row carries
<Index> and <Title>, which is enough to find the table it came from: every
a16_dumb-scenarios-* table has Index and Title columns.

Usage: python3 tools/pushdumb_stats.py /tmp/c7_pushdumb.json
"""
import csv, glob, json, os, sys

GD = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gamedata_plain")

def load(path):
    with open(path, newline="", encoding="utf-8", errors="replace") as f:
        r = list(csv.reader(f))
    if not r:
        return None, []
    return r[0], r[1:]

def tables():
    out = {}
    for p in sorted(glob.glob(os.path.join(GD, "a16_dumb-scenarios-*"))):
        h, rows = load(p)
        if h and "Index" in h and "Title" in h:
            out[os.path.basename(p)] = (h, rows)
    return out

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/c7_pushdumb.json"
    data = json.load(open(path))
    rows = data.get("rows", data) if isinstance(data, dict) else data
    T = tables()
    print("tables with Index+Title:", len(T))
    hits = 0
    per = {}
    for i, r in enumerate(rows):
        title = r.get("title")
        idx = r.get("index")
        if title is None and idx is None:
            print("%3d  (empty)  cls=%s" % (i, r.get("cls")))
            continue
        found = []
        for name, (h, tr) in T.items():
            ci, ct = h.index("Index"), h.index("Title")
            for j, row in enumerate(tr, start=1):
                if len(row) <= max(ci, ct):
                    continue
                if idx is not None and row[ci].strip() != str(idx):
                    continue
                if row[ct].strip() != (title or "").strip():
                    continue
                found.append("%s row %d" % (name, j))
        hits += 1
        key = found[0] if len(found) == 1 else ",".join(found) or "UNMATCHED"
        per[key] = per.get(key, 0) + 1
        print("%3d  idx=%-5s type=%s  %-28s  %s" % (i, idx, r.get("type"), (title or "")[:28], key))
    print()
    print("captured rows:", len(rows), "with data:", hits)
    for k, v in sorted(per.items(), key=lambda kv: -kv[1]):
        print("  %-60s %d" % (k, v))

if __name__ == "__main__":
    main()