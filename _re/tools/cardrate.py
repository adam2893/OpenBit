"""How often each card actually fires (REPORT.en.md 166).

Nothing in the suite measures a rate: the probes check that a card can fire, the
tests check what it does when it does. This counts cards per 100 lives, keyed by the
table they drew from plus the first words of the body, so two lanes of one card do
not look like two cards.

  python3 tools/cardrate.py --lives 200
"""
import argparse
import collections
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
WORDS = 6


def key_of(event):
    body = (event.get("body") or "").strip()
    if not body:
        return None
    words = re.sub(r"\s+", " ", body).split(" ")[:WORDS]
    return (event.get("table") or "-") + " | " + " ".join(words)


def run(seed, gender):
    spec = {"seed": seed, "country": "United States", "gender": gender, "ages": 85,
            "relationship": True}
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
    ap.add_argument("--lives", type=int, default=200)
    args = ap.parse_args()
    counts = collections.Counter()
    lives = 0
    for seed in range(1, args.lives // 2 + 1):
        for gender in ("male", "female"):
            lives += 1
            for event in run(seed, gender)["events"]:
                k = key_of(event)
                if k:
                    counts[k] += 1
    by_table = collections.Counter()
    for k, v in counts.items():
        by_table[k.split(" | ")[0]] += v
    print("lives:", lives, "distinct faces:", len(counts))
    print()
    # A FACE is one title's sentence; a CARD is the pool. The table is the card, so the
    # table summary is the rate that means anything (REPORT.en.md 167).
    print("per 100 lives   count  table (the card)")
    for k, v in by_table.most_common(20):
        print("%13.1f %7d  %s" % (100.0 * v / lives, v, k))
    print()
    print("per 100 lives   count  face (one rendering of a card)")
    for k, v in counts.most_common(12):
        print("%13.1f %7d  %s" % (100.0 * v / lives, v, k))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())