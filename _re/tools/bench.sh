#!/usr/bin/env bash
# Phase-1 bench in one command: build the port, recover the game's picked rows from a
# captured trace, replay them through blsim, and report the first divergence.
#
#   tools/bench.sh [trace.json]        (default: trace_fresh.json)
#   SEED=12345 COUNTRY="United States" GENDER=male tools/bench.sh
#
# Artifacts land in $TMP (default /tmp/blbench): push.json, script.json, game.norm.json, port.norm.json.
set -euo pipefail
cd "$(dirname "$0")/.."
TRACE=${1:-trace_fresh.json}
SEED=${SEED:-12345} COUNTRY=${COUNTRY:-"United States"} GENDER=${GENDER:-male}
TMP=${TMP:-/tmp/blbench}
mkdir -p "$TMP"

cmake --build port/build -j >/dev/null
python3 tools/row_recover.py "$TRACE" --out "$TMP/push.json"
python3 - "$TMP/push.json" "$TRACE" "$TMP/script.json" "$SEED" "$COUNTRY" "$GENDER" <<'PY'
import json, sys
push_file, trace_file, out, seed, country, gender = sys.argv[1:7]
spec = {"seed": int(seed), "country": country, "gender": gender,
        "ages": max(a["age"] for a in json.load(open(trace_file))["ages"]),
        "push": json.load(open(push_file))["push"]}
json.dump(spec, open(out, "w"), indent=1)
print("script: seed %s %s %s ages %s push %s row(s)" % (seed, country, gender, spec["ages"],
      sum(1 for p in spec["push"] if p)))
PY
python3 tools/port_trace.py "$TMP/script.json" "$TMP/port.norm.json" >/dev/null
python3 tools/game_trace.py "$TRACE" "$TMP/game.norm.json" >/dev/null
# Keep the raw diff as JSON lines so tools/gap_report.py can tell a real port gap
# (a pushed year) from expected divergence (a year the game's pick could not be
# replayed and the port drew its own).
python3 tools/diff.py "$TMP/game.norm.json" "$TMP/port.norm.json" -v | tee "$TMP/diff.jsonl"
