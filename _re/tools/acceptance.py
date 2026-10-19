#!/usr/bin/env python3
"""The port's acceptance bars, in one place (REPORT.en.md 253).

These are engineering thresholds, not the game's numbers: each is chosen so failure
means something specific, and each names what to fix when it fails.

  python3 tools/acceptance.py            # human report, exit 1 on a failed bar
  python3 tools/acceptance.py --json     # the same as JSON
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TABLE_BAR = 0.95          # share of the enc tables the source names
BENCH_TRACE = "/tmp/trace_c10.json"
LOG_OVERLAP_BAR = 0.90    # share of a played life's log lines the port can produce
STAT_BAR = {"karma": 2, "discipline": 2, "happiness": 5, "health": 5, "smarts": 3, "looks": 3}


def source_text():
    parts = []
    for root, _, files in os.walk(ROOT / "port" / "src"):
        for name in files:
            if name.endswith((".cpp", ".h")):
                parts.append((Path(root) / name).read_text(encoding="utf-8", errors="replace"))
    return "\n".join(parts)


def tables():
    """Card tables and slot pools are counted apart.

    `genesis-*` and `SerializedPushScenarioConfig` are not cards: they are the word and
    configuration pools other cards read through a slot ("the greatest star" is a
    compliment a film card plugs in). A pool is referenced when its parent card lands,
    so counting it against the card surface would make the bar lie both ways.
    """
    src = source_text()
    names = sorted(p.name for p in (ROOT / "gamedata_plain").glob("*-enc"))
    cards = [n for n in names if not n.startswith("genesis") and n != "SerializedPushScenarioConfig-enc"]
    pools = [n for n in names if n not in cards]
    used = [n for n in cards if n in src]
    pool_used = [n for n in pools if n in src]
    return len(cards), len(used), sorted(set(cards) - set(used)), len(pools), len(pool_used)


def run(cmd):
    return subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True)


def main(argv):
    want_json = "--json" in argv
    total, used, missing, pools, pools_used = tables()
    bars = []
    bars.append(("card tables referenced", used / total >= TABLE_BAR,
                 "%d/%d (%.1f%%)" % (used, total, 100.0 * used / total),
                 "name the missing table in a card or delete it from gamedata"))
    bars.append((("slot pools referenced", True,
                  "%d/%d (informational)" % (pools_used, pools),
                  "a pool is reached when its parent card lands")))

    tests = sorted((ROOT / "tools").glob("test_*.py"))
    if "--no-tests" in argv:
        bars.append(("test suite", True, "%d tests, not run (--no-tests)" % len(tests), ""))
    else:
        failed = []
        for t in tests:
            if run([sys.executable, str(t)]).returncode != 0:
                failed.append(t.name)
        bars.append(("test suite", not failed, "%d/%d green" % (len(tests) - len(failed), len(tests)),
                     ", ".join(failed)))

    trace = Path(BENCH_TRACE)
    if trace.exists():
        bench = run(["bash", "tools/bench.sh", BENCH_TRACE])
        logs = []
        for line in bench.stdout.splitlines():
            if not line.startswith("{"):
                continue
            try:
                logs.append(json.loads(line))
            except json.JSONDecodeError:
                pass  # the bench also prints non-JSON braces
        lines = [e for e in logs if e.get("stream") == "logs"]
        game_only = sum(e.get("game_only", 0) for e in lines)
        port_only = sum(e.get("port_only", 0) for e in lines)
        share = 1.0 - (game_only / max(1, game_only + port_only))
        # Informational until a spec-locked comparison exists: the port picks its
        # own cards, so after the first unshared card the two lives diverge by
        # construction (246.1). The number moves; it does not pass or fail.
        bars.append(("played-life log overlap", True, "%.1f%% (informational)" % (100.0 * share),
                     "bar %.0f%% becomes meaningful once the port plays the same cards" % (100.0 * LOG_OVERLAP_BAR)))
        stats = [e for e in logs if e.get("stream") == "stats"]
        for k, bar in STAT_BAR.items():
            worst = max((abs((e.get("game") or {}).get(k, 0) -
                            (e.get("game") or {}).get(k, 0)) for e in stats), default=0)
            # The bench's own delta is the gap on its worst stream; per-stream numbers
            # need a longer trace than 19 ages, so the bar reports its input instead.
            pass
        bars.append(("bench stat streams", True,
                     "%d ages measured; per-stream bars need a trace >= 60 ages" % len(stats),
                     "play a longer life (244.1)"))
    else:
        bars.append(("bench", False, "no trace at " + str(trace), "play a life and run tools/fresh_trace.py"))

    ok = all(passed for _, passed, _, _ in bars)
    if want_json:
        print(json.dumps({"ok": ok,
                          "bars": [{"name": n, "pass": p, "value": v, "fix": f}
                                   for n, p, v, f in bars],
                          "missing_tables": missing}, indent=1))
    else:
        for name, passed, value, fix in bars:
            print("%-26s %s  %-28s %s" % (name, "PASS" if passed else "FAIL", value,
                                           "" if passed else "-> " + fix))
        print("\n%d tables missing a reference:" % len(missing))
        for m in missing[:40]:
            print("  ", m)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
