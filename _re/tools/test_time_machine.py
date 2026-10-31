"""310: the Time Machine restores the newest 20 yearly snapshots wholesale."""
import csv
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = Path(os.environ.get("BLSIM", ROOT / "port" / "build" / "blsim"))
with (ROOT / "gamedata_plain/a16_time-machine-scenarios-enc").open(newline="") as fh:
    ROWS = list(csv.DictReader(fh))


def flag(v):
    return v.strip().lower() in ("1", "true", "yes", "x")


def run(spec):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
    p = subprocess.run([str(PORT), "--script", fh.name], capture_output=True, text=True)
    os.unlink(fh.name)
    assert p.returncode == 0, p.stderr
    return json.loads(p.stdout)


def machine(ev):
    return [i for i, e in enumerate(ev) if e["title"] == "Time Machine"]


def check_narrative(e, age, prison, dead):
    row = ROWS[int(e["row"]) - 1]
    assert e["table"] == "a16_time-machine-scenarios-enc", e
    assert prison is None or flag(row["Prison"]) == prison, (row, prison)
    assert flag(row["Was Dead"]) == dead, (row, prison, dead)
    assert not row["Min Age"].strip() or int(float(row["Min Age"])) <= age, (row, age)
    text, parts, last = row["Scenario"].strip(), [], 0
    for m in re.finditer(r"<([^<>]*)>", text):
        parts += [re.escape(text[last:m.start()]), "(?:" + "|".join(map(re.escape, m[1].split("/"))) + ")"]
        last = m.end()
    parts.append(re.escape(text[last:]))
    assert re.fullmatch("".join(parts), e["logLines"][-1], re.I), e


replays = matched = alive_cards = 0
for seed in range(1, 31):
    a = run({"seed": seed, "ages": 30})["events"]
    b = run({"seed": seed, "ages": 30, "push": [None] * 20 + [{"timeMachine": 5}]})["events"]
    if len(b) <= 20 or len(a) <= 20:
        continue  # died by card 20: the death case is checked below
    tm = machine(b)
    assert tm == [20], (seed, tm)
    e, before = b[20], b[19]["age"]
    assert e["body"] == "Travel back in time today!", e
    assert e["choice"] == f"5 years ago (Age {before - 5})", e
    check_narrative(e, before - 5, None, False)  # some heroes are jailed by then
    alive_cards += 1
    # The restored life is the life of that birthday: the next card is the year after
    # it, and with the RNG restored too the years that follow replay.
    after = b[21:]
    assert after and after[0]["age"] == before - 4, (seed, after[:1])
    ref = [x for x in a if x["age"] > before - 5]
    for x, y in zip(after, ref):
        replays += 1
        matched += (x["table"], x["row"], x["body"]) == (y["table"], y["row"], y["body"])
assert alive_cards >= 25, alive_cards
# Not every year replays: the restore drops the snapshot's queued cards
# (ResetScenarios), and a narrative with a <a/b> token draws from the restored main
# stream, which moves every later draw. Measured 288/332 (seeds 1-30).
assert matched >= 0.8 * replays, (matched, replays)

# A dead hero still gets the card, and the narrative is a 'Was Dead' row.
dead_cards = 0
for seed in range(1, 13):
    a = run({"seed": seed, "ages": 130})
    assert not a["final"]["alive"]
    died = a["events"][-1]["age"]
    if died < 21:
        continue
    b = run({"seed": seed, "ages": 130, "push": [None] * len(a["events"]) + [{"timeMachine": 20}]})
    tm = machine(b["events"])
    assert tm == [len(a["events"])], (seed, tm)
    e = b["events"][tm[0]]
    assert e["body"] == "Undo death by traveling back in time!", e
    assert e["choice"] == f"20 years ago (Age {died - 20})", e
    check_narrative(e, died - 20, None, True)
    assert b["events"][tm[0] + 1]["age"] == died - 19
    dead_cards += 1
assert dead_cards >= 8, dead_cards

# A prisoner gets a Prison row.
prison_cards = 0
for seed in range(1, 41):
    spec = {"seed": seed, "ages": 24, "prison": True}
    a = run(spec)["events"]
    idx = next((i for i, e in enumerate(a) if e["age"] == 21), None)
    if idx is None:
        continue
    b = run(dict(spec, push=[None] * (idx + 1) + [{"timeMachine": 1}]))["events"]
    tm = machine(b)
    if not tm or b[tm[0]]["row"] is None:
        continue
    # The script output keeps no prison field, so the row's own flag is counted: a
    # forced sentence from 18 is still running when the restored life is at 20. The
    # application card owns age 19 now, which pushes the arrest chain a year out, so
    # the fixture rewinds from 21 instead of from 20 (REPORT.en.md 313).
    check_narrative(b[tm[0]], 20, None, False)
    prison_cards += flag(ROWS[int(b[tm[0]]["row"]) - 1]["Prison"])
assert prison_cards >= 10, prison_cards
print("prison", prison_cards)

# Only 1/3/5/10/15/20 are destinations, and only while that birthday's file exists.
for years in (2, 25):
    b = run({"seed": 3, "ages": 30, "push": [None] * 26 + [{"timeMachine": years}]})["events"]
    assert not machine(b), years
b = run({"seed": 3, "ages": 30, "push": [None] * 3 + [{"timeMachine": 20}]})["events"]
assert not machine(b)

print(f"time machine: alive {alive_cards} replay {matched}/{replays} dead {dead_cards}")
