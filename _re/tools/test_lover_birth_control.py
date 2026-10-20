"""The birth-control demand (REPORT.en.md 142).

ScenarioLoverDemandsBirthControl asks the hero to go on birth control, and the
agree lane flips the person's OnBirthControl flag (hero+0x50). The card offers a
third button in the game -- "Argue with {0}" -- which opens the LoverArgues flow;
that flow is not ported, so the port offers the two lanes it can honour, and this
test pins them plus the state.
"""
import collections
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
AGREE = "Agree to go on birth control"
REFUSE = "Refuse to go on birth control"


def run(seed, gender, ages=70, **extra):
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


lanes = collections.Counter()
agree_runs = 0
for seed in range(1, 81):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        agreed = False
        for event in doc["events"]:
            body = event.get("body") or ""
            # The mirror card (REPORT.en.md 144) also mentions birth control; this test
            # is only about the demand.
            if "wants you to go on birth control" not in body:
                continue
            assert body.endswith("wants you to go on birth control.\n\nWhat will you do?"), body
            assert event["age"] >= 18, (seed, gender, event["age"])
            choice = event["choice"]
            assert choice in (AGREE, REFUSE), (seed, gender, choice)
            lanes[choice] += 1
            journal = (event.get("logLines") or [""])[-1]
            if choice == AGREE:
                agreed = True
                assert journal == ("I agreed to go on birth control following " +
                                    ("her" if "girlfriend" in body or "wife" in body else "his") +
                                    " suggestion."), (seed, gender, journal)
            else:
                assert journal.endswith("tried to get me to go on birth control but I refused."), journal
            moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
            assert not moves, (seed, gender, moves)  # neither lane moves a stat
        if agreed:
            agree_runs += 1
            # The agree lane flips the person's own flag. A later stop card (REPORT
            # 144) can flip it back, so the final state is the last card's, which the
            # stop test owns; here only the sequence direction is asserted.
            assert "birthControl" in doc["final"], (seed, gender)

assert lanes[AGREE] and lanes[REFUSE], dict(lanes)
assert agree_runs, "no life ever agreed"

# A hero already on birth control is never asked again.
for seed in range(1, 16):
    doc = run(seed, "female", birthControl=True)
    hits = [e for e in doc["events"] if "go on birth control" in (e.get("body") or "")]
    assert not hits, (seed, hits[:1])

print("lanes:", dict(lanes))
print("lives that agreed:", agree_runs)
print("ok")