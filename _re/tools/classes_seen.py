import json, glob, collections, os, re

cls = collections.Counter(); titles = collections.defaultdict(set)
for p in sorted(glob.glob("/Users/Adam/Documents/bitlife/_re/trace_game*.json")) + ["/tmp/trace_c5.json", "/tmp/trace_c6.json", "/tmp/trace_c7.json"]:
    try:
        d = json.load(open(p))
    except Exception:
        continue
    if isinstance(d, dict) and "ages" in d:
        for a in d["ages"]:
            for s in a.get("scenarios", []):
                c = s.get("cls") or s.get("_class") or "?"
                cls[c] += 1; titles[c].add((s.get("title") or s.get("Title") or "")[:30])
    elif isinstance(d, list):
        for r in d:
            for s in (r.get("scenarios") or []):
                c = s.get("_class") or s.get("cls") or "?"
                cls[c] += 1; titles[c].add((s.get("Title") or s.get("title") or "")[:30])
print("classes seen:", len(cls), "instances:", sum(cls.values()))
for c, n in cls.most_common(200):
    print("%-46s %3d  %s" % (c, n, "|".join(sorted(titles[c])[:3])))