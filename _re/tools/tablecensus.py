"""Which tables the port names, and which of them a life ever touches.

A card can be "ported" — its table loaded, its row builder written — and still never
fire, because its gate is unreachable or another pool always claims the year first.
Section 155 found one by accident; this tool looks for the rest: it runs a batch of
lives, counts the `table` of every event (not the title -- a title is content, the
table is identity), and diffs that against every `a16_*` name the port references."""

""".

  python3 tools/tablecensus.py            # 60 lives, both genders
  python3 tools/tablecensus.py --seeds 20 --ages 90

The output is a CANDIDATE list. A table named in the source can be reached through
a token, a cross-reference or a card the batch never qualified for; absence here is
a question to answer, not a defect to fix.
"""
import argparse
import collections
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
SOURCES = [ROOT / "port" / "src"]


def referenced_tables():
    names = set()
    for base in SOURCES:
        for path in base.rglob("*.cpp"):
            text = path.read_text(encoding="utf-8", errors="replace")
            names.update(re.findall(r"a16_[a-z0-9-]+", text))
    return names


def run(seed, gender, ages, extra):
    spec = {"seed": seed, "country": "United States", "gender": gender,
            "ages": ages, "relationship": True}
    spec.update(extra)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=60)
    ap.add_argument("--ages", type=int, default=80)
    # A JSON object of extra script switches, so a caller can turn the families on:
    #   --switches '{"prison": true, "office": true, "cult": true}'
    ap.add_argument("--switches", default="{}")
    # One family's tables at a time is what the numbers can support: the reference
    # set also holds tables named only by a token, so a whole-source diff is noise.
    ap.add_argument("--only", default="")
    args = ap.parse_args()

    extra = json.loads(args.switches)
    seen = collections.Counter()
    for seed in range(1, args.seeds + 1):
        for gender in ("male", "female"):
            for event in run(seed, gender, args.ages, extra)["events"]:
                table = event.get("table") or ""
                if table:
                    seen[table] += 1

    named = referenced_tables()
    touched = set(seen)
    if args.only:
        named = {n for n in named if args.only in n}
        touched = {t for t in touched if args.only in t}
    silent = sorted(named - touched)
    print("lives:", args.seeds * 2)
    print("tables referenced in src/:", len(named))
    print("tables a life touched:", len(touched))
    print("referenced but never touched:", len(silent))
    for name in silent:
        print("  " + name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())