#!/usr/bin/env python3
"""Split the captured scenario population into the three subsystems it really mixes.

coverage.py counted every captured scenario in one denominator, which mixes three
things a push-file bench can never treat alike:

  narrative  a one-shot line the life loop emits (the port renders it from a table
             row or from its own scripted-event registry)
  menu       the scenario hands the player a choice ("What will you do?"); the port
             has no menu layer, and the auto-driver picks for it
  minigame   a modal screen (blackjack, eye exam, paywall, profile); there is no
             narrative line to replay at all

Usage: python3 tools/scenario_classes.py [trace ...]
"""
import collections, json, os, re, subprocess, sys

RE = "/Users/Adam/Documents/bitlife/_re"
MENU_RE = re.compile(r"what will you do\?|will you join|how will you|what do you do\?", re.I)
MINIGAME_RE = re.compile(r"game|casino|eyeexam|paywall|profilehero|carrepair", re.I)
AGG = "/tmp/coverage_trace.json"


def bucket(cls, body):
    """Which subsystem a captured class belongs to. Minigame wins on the class name,
    then the choice prompt decides between a menu and a plain narrative line."""
    if MINIGAME_RE.search(cls):
        return "minigame"
    if MENU_RE.search(body or ""):
        return "menu"
    return "narrative"


def load_instances(traces):
    inst = []
    for t in traces:
        try:
            d = json.load(open(t))
        except Exception:
            continue
        for rec in d["ages"]:
            for s in rec.get("scenarios", []) or []:
                inst.append((rec["age"], s.get("cls") or "?", s.get("body") or ""))
    if len(inst) < 20 and os.path.exists(AGG):
        d = json.load(open(AGG))
        inst = [(r["age"], s.get("cls") or "?", s.get("body") or "")
                for r in d["ages"] for s in (r.get("scenarios") or [])]
        return inst, AGG
    return inst, None


def main():
    traces = sys.argv[1:] or [os.path.join(RE, "trace_fresh.json")]
    inst, agg = load_instances(traces)

    per = collections.defaultdict(list)
    seen = collections.Counter()
    for age, cls, body in inst:
        b = bucket(cls, body)
        per[b].append((age, cls))
        seen[b] += 1

    pushed = collections.Counter()
    if agg:
        log = subprocess.run([sys.executable, RE + "/tools/row_recover.py", agg,
                              "--out", "/tmp/push_cov4.json"],
                             capture_output=True, text=True).stdout
        idx = collections.defaultdict(list)
        for age, cls, body in inst:
            idx[(age, cls)].append(body)
        for line in log.splitlines():
            m = re.match(r"  age (\d+)\s+(\S+)\s", line)
            if not m:
                continue
            key = (int(m.group(1)), m.group(2))
            if key not in idx:
                continue
            if "UNRECOVERED" not in line:
                pushed[bucket(key[1], idx[key][0])] += 1

    print("%-10s %5s %5s %6s" % ("bucket", "seen", "push", "cover"))
    for b in ("narrative", "menu", "minigame"):
        print("%-10s %5d %5d %5.0f%%"
              % (b, seen[b], pushed[b], 100.0 * pushed[b] / seen[b] if seen[b] else 0))
    print()
    for b in ("narrative", "menu", "minigame"):
        names = collections.Counter(c for _, c in per[b])
        print("%s: %d instances, %d classes" % (b, seen[b], len(names)))
        for c, n in names.most_common():
            print("   %-38s %d" % (c, n))


if __name__ == "__main__":
    main()
