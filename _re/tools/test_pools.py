"""The flavour pools wired in REPORT.en.md 305.

Two pools, one extra journal line each:
  a16_pray-feedback-enc (Log, Pray, Curse, Commonness Score) -- the prayer itself, the
  year a pray-wealth card fires, on the side the card's own Positive column picks;
  a16_crashout-public-response-string-enc (String, Positive Reception, Commonness) --
  the crowd's answer to a famous hero's crashout, on the side its Fame Impact picks.
Only plain-sentence rows are wired (a row carrying '<' stays out of the port, so the
line costs no draw from the main stream); this test checks the line, its side and the
no-token rule.
"""
import csv
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"


def table(name):
    with open(DATA / name, encoding="utf-8-sig", newline="") as fh:
        return list(csv.DictReader(fh))


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 40}
    spec.update(extra)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-400:]
    return json.loads(proc.stdout)


pray_rows = table("a16_pray-feedback-enc")
pray_bless = {r["Log"].strip() for r in pray_rows
              if "x" in r["Pray"] and "<" not in r["Log"]}
pray_curse = {r["Log"].strip() for r in pray_rows
              if "x" in r["Curse"] and "<" not in r["Log"]}
assert pray_bless and pray_curse, (sorted(pray_bless), sorted(pray_curse))
assert not (pray_bless & pray_curse)

crash_rows = table("a16_crashout-public-response-string-enc")
crash_good = {r["String"].strip() for r in crash_rows
              if "x" in r["Positive Reception"] and "<" not in r["String"]}
crash_bad = {r["String"].strip() for r in crash_rows
             if "x" not in r["Positive Reception"] and "<" not in r["String"]}
assert crash_good and crash_bad, (sorted(crash_good)[:2], sorted(crash_bad)[:2])
assert not (crash_good & crash_bad)

pray_table = table("a16_pray-wealth-scenarios-enc")
crash_table = table("a16_crashout-scenario-enc")

# The ambient prayer gate is one in forty on a single wheel ticket (REPORT.en.md 313,
# 315), so the prayer pool needs twice the lives the other pools do.
pray_cards = 0
for seed in range(1, 161):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_pray-wealth-scenarios-enc":
            continue
        pray_cards += 1
        positive = "x" in pray_table[int(event["row"]) - 1]["Positive"]
        lines = [line for line in event.get("logLines") or [] if line.strip()]
        pool = [line for line in lines if line in pray_bless | pray_curse]
        assert len(pool) == 1, (seed, lines)
        assert pool[0] in (pray_bless if positive else pray_curse), (seed, lines)
        assert "<" not in pool[0], (seed, lines)

# The crash-out pool rides the same wheel; the zoo gift shop's stocking card (REPORT.en.md
# 368) joined it and pushed the pool past its old eightieth seed, so the sweep is wider.
crash_cards = 0
for seed in range(1, 141):
    for event in run(seed, fame=80)["events"]:
        if event.get("table") != "a16_crashout-scenario-enc":
            continue
        crash_cards += 1
        impact = float(crash_table[int(event["row"]) - 1]["Fame Impact"] or 0)
        lines = [line for line in event.get("logLines") or [] if line.strip()]
        pool = [line for line in lines if line in crash_good | crash_bad]
        assert len(pool) == 1, (seed, lines)
        assert pool[0] in (crash_good if impact >= 0 else crash_bad), (seed, lines)
        assert "<" not in pool[0], (seed, lines)

assert pray_cards >= 5, pray_cards
assert crash_cards >= 3, crash_cards
print("pray cards:", pray_cards, "crashout cards:", crash_cards)
print("ok")
