"""Measure how much of the captured scenario stream the bench can replay.

row_recover.py turns a rendered scenario back into {"table","row"} for the port,
and its REGISTRY mirror handles the code-built classes. Everything it cannot
name is a year the long-life bench cannot push, so this tool reports recovery
by class across every captured trace.
"""
import glob, json, collections, subprocess, os, sys

RE = "/Users/Adam/Documents/bitlife/_re"

def collect():
    ages = {}
    n = 0
    for p in sorted(glob.glob(RE + "/trace_game*.json")) + ["/tmp/trace_c5.json", "/tmp/trace_c6.json", "/tmp/trace_c7.json"]:
        try:
            d = json.load(open(p))
        except Exception:
            continue
        if isinstance(d, dict) and "ages" in d:
            for a in d["ages"]:
                for s in a.get("scenarios", []):
                    c = s.get("cls") or s.get("_class") or "?"
                    n += 1
                    b = ages.setdefault(n, {"age": n, "scenarios": [], "logs": []})
                    b["scenarios"].append(
                        {"cls": c, "title": s.get("title") or s.get("Title"),
                         "body": s.get("body") or s.get("Body"),
                         "choices": s.get("choices") or s.get("_choices") or []})
        elif isinstance(d, list):
            for r in d:
                for lg in (r.get("logs") or []):
                    # The round's own logs belong to the bucket of the first scenario
                    # that follows them, and the age field has to equal that bucket
                    # key or every push index shifts by one.
                    txt = lg.get("text") if isinstance(lg, dict) else lg
                    if txt:
                        ages.setdefault(n + 1 if n else 1,
                                        {"age": n + 1 if n else 1, "scenarios": [], "logs": []})["logs"].append(txt)
                for s in (r.get("scenarios") or []):
                    c = s.get("_class") or s.get("cls") or "?"
                    n += 1
                    b = ages.setdefault(n, {"age": n, "scenarios": [], "logs": []})
                    b["scenarios"].append({"cls": c, "title": s.get("Title"), "body": s.get("Body"),
                                           "choices": s.get("_choices") or s.get("choices") or []})
    return {"source": "game", "ages": [ages[k] for k in sorted(ages)]}

def main():
    tr = collect()
    json.dump(tr, open("/tmp/coverage_trace.json", "w"), indent=1)
    r = subprocess.run([sys.executable, RE + "/tools/row_recover.py", "/tmp/coverage_trace.json",
                        "--out", "/tmp/coverage_push.json"], capture_output=True, text=True, cwd=RE)
    out = json.load(open("/tmp/coverage_push.json"))
    push = out["push"]
    per = collections.Counter(); ok = collections.Counter()
    items = [s for a in tr["ages"] for s in a["scenarios"]]
    for i, s in enumerate(items):
        c = s["cls"] or "?"
        per[c] += 1
        p = push[i] if i < len(push) else None
        if p and (p.get("table") or p.get("scenario")): ok[c] += 1
    print("captured scenarios:", len(items), " recoverable:", sum(ok.values()))
    print("%-46s %5s %5s" % ("class", "seen", "push"))
    for c, n in per.most_common():
        print("%-46s %5d %5d" % (c, n, ok[c]))
    print()
    print("row_recover stdout tail:")
    print("\n".join(r.stdout.strip().splitlines()[-6:]))

if __name__ == "__main__":
    main()