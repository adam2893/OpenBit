"""The paparazzi (REPORT.en.md 139).

a16_paparazzi-scenarios-enc has two columns and the class builds three buttons; the
lane is chosen by the *label text* in ChoiceSelectedAtIndex @0x73f3bd7e28, and each
lane writes its own journal and its own impacts. The test pins the gate (fame), the
three lane outcomes and the fame arithmetic end to end.
"""
import collections
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLE = "a16_paparazzi-scenarios-enc"
POSE = "Pose for his photo"
PRETEND = "Pretend he's not there"
COVER = "Cover my face"
# The fame each lane moves. ImpactFame takes an *impact score* and applies
# pointsForImpact to it, so ImpactFame(2) is +8 fame and ImpactFame(-1) is -4.
TAILS = {
    POSE: (" I smiled for his picture.", 8),
    PRETEND: (" I pretended not to notice him.", 0),
    COVER: (" I covered my face to make it hard for him to take my picture.", -4),
}
# pointsForImpact: 2 -> 8, 3 -> 16 (REPORT.en.md 121's table).
EXPECT = {
    POSE: {"discipline": 16, "karma": 16},
    PRETEND: {"discipline": 8, "karma": 8},
    COVER: {},
}


def run(seed, fame, ages=60):
    spec = {"seed": seed, "country": "United States", "gender": "female",
            "ages": ages, "fame": fame}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


lanes = collections.Counter()
cards = 0
for seed in range(1, 41):
    doc = run(seed, 80)
    poses = covers = 0
    for event in doc["events"]:
        if event.get("table") != TABLE:
            continue
        cards += 1
        assert event["age"] >= 18, (seed, event["age"])
        choice = event["choice"]
        assert choice in TAILS, (seed, choice)
        lanes[choice] += 1
        tail, fame_delta = TAILS[choice]
        # eventJson carries the branch's journal as the last log line.
        narrative = (event.get("logLines") or [""])[-1]
        assert narrative.endswith(tail), (seed, narrative)
        assert "<" not in event["body"] and "<" not in narrative, (seed, event)
        got = {k: v for k, v in (event.get("deltas") or {}).items() if v}
        assert got == EXPECT[choice], (seed, choice, got)
        if fame_delta == 8:
            poses += 1
        elif fame_delta == -4:
            covers += 1
    # Every card that reports a fame delta moves the score, and the clamp applies
    # *after each one*: three poses from 80 reach the 100 ceiling and a later -8
    # ends at 92, not 96 (REPORT.en.md 276.1).
    want = 80.0
    for e in doc["events"]:
        delta = int((e.get("deltas") or {}).get("fame") or 0)
        if (e.get("table") or "") == TABLE:
            choice = e.get("choice")
            delta = TAILS[choice][1]
        want = min(100.0, max(0.0, want + delta))
    assert abs(doc["final"]["fame"] - want) < 1e-3, (seed, doc["final"]["fame"], want)

assert cards, "no paparazzi card in 40 famous lives"
assert set(lanes) == set(TAILS), dict(lanes)

for fame in (0, 74):
    for seed in range(1, 11):
        doc = run(seed, fame)
        # The card's own gate is fame >= 75. A life that starts below it can still reach
        # it: the magazine shoot (REPORT.en.md 335) pays fame, so the running score is
        # tracked and the card may only appear once it is over the line.
        running = float(fame)
        for e in doc["events"]:
            if (e.get("table") or "") == TABLE:
                assert running >= 75.0, (seed, fame, running, e)
            running += float((e.get("deltas") or {}).get("fame") or 0)
at75 = sum(1 for seed in range(1, 11)
           if any(e.get("table") == TABLE for e in run(seed, 75)["events"]))
assert at75, "fame 75 never produced the card"

print("paparazzi cards:", cards)
print("lanes:", dict(lanes))
print("lives at fame 75 with a card:", at75, "of 10")
print("ok")