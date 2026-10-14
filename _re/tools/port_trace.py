"""Run blsim --script and normalize its stdout into the canonical trace shape."""
import json, subprocess, sys, os
from trace_common import dump

PORT = os.environ.get("PORT_DIR", "/Users/Adam/Documents/bitlife/_re/port")
BL = os.path.join(PORT, "build/blsim")
STAT_KEYS = ["happiness", "health", "smarts", "looks", "karma", "discipline"]


def run(script):
    p = subprocess.run([BL, "--script", "/dev/stdin"], input=json.dumps(script),
                       capture_output=True, text=True, cwd=PORT)
    if p.returncode != 0:
        raise SystemExit("blsim exit %d: %s" % (p.returncode, p.stderr[-400:]))
    return json.loads(p.stdout)


def normalize(out):
    """events carry per-event deltas; fold them forward so each age has its own stat line."""
    base = dict(out["hero"]["stats"])
    acc = dict(base)
    by_age = {}
    # Age 0 is the birth log: those lines land before the first age-up, so they get
    # their own bucket with the newborn stats.
    if out.get("birth"):
        by_age[0] = {"age": 0, "logs": list(out["birth"]), "scenarios": [], "stats": dict(base)}
    # SimHero.SetupEverythingElseForNewHero runs inside the first age-up, so the
    # age-1 fold starts from the 'newborn' snapshot, not from the ctor values.
    newborn = (out.get("newborn") or {}).get("stats")
    for ev in out["events"]:
        if newborn and ev["age"] == 1:
            acc = dict(newborn)
        acc = {k: acc.get(k, 0) + int(ev["deltas"].get(k, 0)) for k in STAT_KEYS}
        rec = by_age.setdefault(ev["age"], {"age": ev["age"], "logs": [], "scenarios": [], "stats": None})
        rec["logs"].extend(ev.get("logLines") or [])
        rec["scenarios"].append({"title": ev.get("title"), "body": ev.get("body"),
                                 "cls": "%s!%s" % (ev.get("table"), ev.get("row"))})
        rec["stats"] = dict(acc)
    return {"source": "port", "base_stats": base,
            "ages": [by_age[a] for a in sorted(by_age)],
            "final": out.get("final")}


if __name__ == "__main__":
    script_path = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else script_path.replace(".json", ".port.json")
    trace = normalize(run(json.load(open(script_path))))
    dump(trace, out_path)
    print("ages=%d logs=%d -> %s" % (len(trace["ages"]),
          sum(len(a["logs"]) for a in trace["ages"]), out_path))
