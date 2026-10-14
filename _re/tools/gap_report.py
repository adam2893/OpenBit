#!/usr/bin/env python3
"""Turn a bench diff into an ordered gap list.

Reads the JSON-lines diff.py prints (one object per age and stream) plus the
push file the bench fed the port, and separates two very different things:

  pushed year   the port replayed the game's own pick, so any difference is a
                real port gap (mechanism, text or impact model)
  unpushed year the port drew its own scenario, so a difference is expected
                divergence and only tells you the year needs a push entry

Usage: python3 tools/gap_report.py /tmp/blbench/diff.jsonl /tmp/blbench/push.json
"""
import collections, json, sys

def main():
    diff_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/blbench/diff.jsonl"
    push_path = sys.argv[2] if len(sys.argv) > 2 else "/tmp/blbench/push.json"
    push = (json.load(open(push_path)) or {}).get("push", [])
    rows = []
    for line in open(diff_path):
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            rows.append(json.loads(line))
        except Exception:
            pass
    per = collections.defaultdict(dict)
    for r in rows:
        per[r.get("age")][r.get("stream")] = r
    pushed_years = 0
    print("%-5s %-7s %-18s %-7s %-24s" % ("age", "pushed", "logs g/p/near", "scen", "stats worst"))
    gaps = []
    for age in sorted(k for k in per if isinstance(k, int)):
        y = per[age]
        is_pushed = age < len(push) and bool(push[age])
        pushed_years += 1 if is_pushed else 0
        lg = y.get("logs", {})
        sc = y.get("scenarios", {})
        stt = y.get("stats", {})
        worst = stt.get("worst")
        delta = stt.get("delta")
        print("%-5d %-7s %d/%d/%-13d %d/%-5d %s%s" % (
            age, "yes" if is_pushed else "no",
            lg.get("game_only", 0), lg.get("port_only", 0), lg.get("near", 0),
            sc.get("game_only", 0), sc.get("port_only", 0),
            worst or "-", (" %+d" % delta) if isinstance(delta, int) else ""))
        if is_pushed and (lg.get("game_only") or lg.get("port_only") or sc.get("game_only") or sc.get("port_only")):
            gaps.append((age, "logs" if (lg.get("game_only") or lg.get("port_only")) else "scenarios",
                         lg.get("first_game_only"), lg.get("first_port_only")))
    print()
    print("years with a push entry: %d of %d" % (pushed_years, len(per)))
    print("real gaps on pushed years: %d" % len(gaps))
    for age, stream, g, p in gaps[:40]:
        print("  age %-4d %-9s game=%r port=%r" % (age, stream, (g or "")[:60], (p or "")[:60]))
    classify(diff_path, per, push)


# Classes the port renders. A game line on a pushed year is only a port gap if
# the year's own scenario set is something the port claims to model; a line from
# a scenario class the port never implemented is expected output (the registry
# still has to grow), not a mechanism defect.
IMPLEMENTED = {
    "ScenarioChildhoodVaccination", "ScenarioSchoolEnroll", "ScenarioFriendMeet",
    "ScenarioRetentionCrime", "ScenarioSchoolClassmateActUp", "ScenarioEthicsBreakSomething",
    "ScenarioScriptEventSiblingBorn", "ScenarioScriptEventParentsBreakUp",
    "ScenarioScriptEventParentMarriesStepparent", "ScenarioPetDeath",
    "ScenarioDiseaseContract", "ScenarioFriendNew", "DumbScenario",
}


def classify(diff_path, per, push):
    """Split each pushed year's game-only lines into expected / unexplained."""
    import os
    norm = os.path.join(os.path.dirname(os.path.abspath(diff_path)), "game.norm.json")
    if not os.path.exists(norm):
        return
    game = json.load(open(norm))
    port = json.load(open(os.path.join(os.path.dirname(os.path.abspath(diff_path)), "port.norm.json")))
    by_age = {a["age"]: a for a in port.get("ages", [])}
    print()
    print("per-year line provenance (pushed years only):")
    for a in game.get("ages", []):
        age = a["age"]
        if age not in per or not (age < len(push) and push[age]):
            continue
        gl = [l for l in a.get("logs", []) if isinstance(l, str) and l.strip()]
        pl = [l for l in (by_age.get(age) or {}).get("logs", []) if isinstance(l, str) and l.strip()]
        missing = [l for l in gl if l not in pl]
        if not missing:
            continue
        classes = [(s.get("cls") or "") for s in a.get("scenarios", [])]
        unmodelled = [c for c in classes if c and c not in IMPLEMENTED]
        tag = "expected (unmodelled: %s)" % ",".join(sorted(set(unmodelled))[:3]) if unmodelled else "UNEXPLAINED"
        print("  age %-4d %-2d missing line(s) -> %s" % (age, len(missing), tag))


if __name__ == "__main__":
    main()

