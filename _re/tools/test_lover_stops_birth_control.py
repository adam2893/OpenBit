"""The mirror card: the partner wants off birth control (REPORT.en.md 144).

ScenarioLoverStopsBirthControl reads the chosen button's own Title, lowers it and
dispatches on Contains("disapprove"). That lane moves karma and discipline by -2;
the other clears the person's OnBirthControl flag. The port offers those two of
the game's three buttons -- the third opens the argue flow, which is not ported.
"""
import collections
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
YES = "Yes, stop taking it"
NO = "Disapprove of that idea"


def run(seed, gender, ages=80, **extra):
    spec = {"seed": seed, "country": "United States", "gender": gender,
            "ages": ages, "relationship": True, "birthControl": True}
    spec.update(extra)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


lanes = collections.Counter()
stopped = 0
for seed in range(1, 41):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        off = False
        last = None   # the last birth-control card in the life decides the flag
        for event in doc["events"]:
            body = event.get("body") or ""
            # The demand card of REPORT.en.md 142 turns the flag the other way.
            if "wants you to go on birth control" in body:
                if event["choice"] == "Agree to go on birth control":
                    last = True
                continue
            if "wants to stop taking" not in body:
                continue
            assert body.startswith("Your "), body
            assert body.endswith(" birth control."), body
            choice = event["choice"]
            assert choice in (YES, NO), (seed, gender, choice)
            lanes[choice] += 1
            journal = (event.get("logLines") or [""])[-1]
            moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
            if choice == YES:
                off = True
                last = False
                assert journal.endswith("I agreed that was a good idea."), journal
                assert not moves, (seed, gender, moves)
            else:
                assert journal.endswith("I don't approve of that."), journal
                # pointsForImpact(-2) == -8: the handler passes an impact score.
                assert moves == {"karma": -8, "discipline": -8}, (seed, gender, moves)
        # A demand card later in the same life can turn the flag back on, so the
        # final state is whichever card came last, not whichever came first.
        if last is not None:
            assert doc["final"]["birthControl"] is last, (seed, gender, last)
        if off:
            stopped += 1

assert lanes[YES] and lanes[NO], dict(lanes)
assert stopped, "no life ever stopped"

# The mirror gate: a hero who is not on birth control is never asked to stop.
for seed in range(1, 16):
    doc = run(seed, "female", birthControl=False)
    hits = [e for e in doc["events"] if "wants to stop taking" in (e.get("body") or "")]
    assert not hits, (seed, hits[:1])

print("lanes:", dict(lanes))
print("lives that stopped:", stopped)
print("ok")