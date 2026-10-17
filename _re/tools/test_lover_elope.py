"""The elopement offer (REPORT.en.md 159).

ScenarioLoverElopePush asks the hero to run off and get eloped. The handler
classifies the label: Contains("agree") pushes the wedding-planning card,
Contains("plan") is the counter-offer. The planning card is not ported, so agreeing
marries on the spot and the counter-offer leaves the couple engaged.
"""
import collections
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
ELOPE = "Agree to elope"
PLAN = "Plan a wedding instead"


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 60,
            "relationship": True}
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
for seed in range(1, 21):
    doc = run(seed)
    for event in doc["events"]:
        body = event.get("body") or ""
        if "get eloped" not in body:
            continue
        assert body.startswith("Your ") and body.endswith("has asked you to run off and get eloped."), (seed, body)
        assert event["age"] >= 18, (seed, event["age"])
        choice = event["choice"]
        assert choice in (ELOPE, PLAN), (seed, choice)
        lanes[choice] += 1
        journal = (event.get("logLines") or [""])[-1]
        if choice == ELOPE:
            assert journal.endswith(" asked me to get eloped."), (seed, journal)
        else:
            assert journal.endswith(" I suggested we plan a normal wedding instead."), (seed, journal)

assert lanes[ELOPE] and lanes[PLAN], dict(lanes)

# A prisoner is not offered an elopement (ScenarioIsEligibleForRandomPush).
for seed in range(1, 11):
    doc = run(seed, prison=True)
    hits = [e for e in doc["events"] if "get eloped" in (e.get("body") or "")
            and (doc["final"].get("inPrison") or False)]
    assert not hits, (seed, hits[:1])

print("lanes:", dict(lanes))
print("ok")