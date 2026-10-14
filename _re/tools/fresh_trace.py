"""Build a canonical bench trace from the *live* frida harness (fresh life, ages 0..N).

The harness reports a log line's own age field unreliably on a brand-new life (its
LifeLog AgeAtLogStart stays 0), so ages are bucketed by the SimHero upgrade ticks
instead: bucket k holds everything emitted before tick k+1.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, "/Users/Adam/Documents/bitlife/_re")
import blhrpc
from trace_common import dump
from game_trace import stats_of


def build():
    # n is a keyword: a bare second argument is the RPC *timeout*, so the old
    # calls silently asked for the harness default (a couple dozen rows) and
    # every "long life" trace came back as ages 0..10.
    logs = blhrpc.rpc('logs', n=4000, timeout=120) or []
    scens = blhrpc.rpc('scenarios', n=2000, timeout=120) or []
    ticks = blhrpc.rpc('ticks', n=2000, timeout=120) or []
    hero = blhrpc.rpc('hero')
    ticks = sorted(ticks, key=lambda t: t.get('t', 0))
    times = [t.get('t', 0) for t in ticks]

    def bucket(t):
        n = 0
        for tt in times:
            if tt <= t:
                n += 1
        return n

    ages = {}
    def rec(k):
        return ages.setdefault(k, {"age": k, "logs": [], "scenarios": [], "stats": None})

    if isinstance(hero, dict):
        rec(0)["stats"] = stats_of(hero)
    for l in logs:
        if isinstance(l, dict) and l.get('text'):
            rec(bucket(l.get('t', 0)))["logs"].append(l['text'])
    for s in scens:
        if isinstance(s, dict):
            rec(bucket(s.get('t', 0)))["scenarios"].append(
                {"title": s.get('Title'), "body": s.get('Body'), "cls": s.get('_class')})
    for i, t in enumerate(ticks):
        st = stats_of(t)
        if st:
            rec(i + 1)["stats"] = st
    return {"source": "game", "ages": [ages[k] for k in sorted(ages)]}


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "/tmp/fresh.norm.json"
    tr = build()
    dump(tr, out)
    n_logs = sum(len(a["logs"]) for a in tr["ages"])
    n_sc = sum(len(a["scenarios"]) for a in tr["ages"])
    print("wrote %s ages=%s logs=%d scenarios=%d" % (out, [a["age"] for a in tr["ages"]], n_logs, n_sc))
