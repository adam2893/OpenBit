"""The relationship lifecycle, across cards (REPORT.en.md 165).

Eleven cards now write loverName, loverStatus, relationship, exLovers or
breakUpPending. Each has its own test; none of them checks a whole life, where the
writes meet. These are invariants that must hold in every life, whatever fired:

  * status is one of the four the port knows, or empty;
  * a status implies a name, and a name implies a status;
  * every ex-lover has a name, a status and a year that is not in the future;
  * a married hero has a relationship score in range (it may sit at 0: nothing in
    the game dissolves a marriage at 0 -- the divorce card is a chance, not a rule,
    so a spouse can stay married and miserable);
  * a card never leaves the hero married to nobody.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
STATUSES = {"dating", "engaged", "married"}


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 80,
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


lives = 0
weddings = 0
divorces = 0
marriedLives = 0
warmMarriages = 0
for seed in range(1, 61):
    for extra in ({}, {"cash": 120000, "engaged": True}):
        doc = run(seed, **extra)
        lives += 1
        final = doc["final"]
        status = final.get("loverStatus")
        name = final.get("loverName")
        relationship = final.get("relationship")
        assert status is None or status in STATUSES, (seed, status)
        if status:
            assert name, (seed, "a status with nobody attached")
            assert 0 <= relationship <= 100, (seed, relationship)
            if status == "married":
                marriedLives += 1
                if relationship > 0:
                    warmMarriages += 1
        if name:
            assert status, (seed, "a partner with no status")
        for ex in final["exLovers"]:
            assert ex["name"], (seed, ex)
            assert ex["status"] in STATUSES, (seed, ex)
            assert 0 <= ex["year"] <= final["age"], (seed, ex, final["age"])
        for event in doc["events"]:
            body = event.get("body") or ""
            if body.startswith("You have chosen to marry"):
                weddings += 1
            if body.endswith(" wants a divorce."):
                divorces += 1

assert marriedLives, "nobody ended a life married"
# The port must still produce warm marriages somewhere; the 0 case is the decayed one.
assert warmMarriages, "every marriage decayed to nothing"
assert weddings, "no wedding in 120 lives"
assert divorces, "no divorce proposal in 120 lives"

print("lives:", lives, "weddings:", weddings, "divorce proposals:", divorces,
      "married at end:", marriedLives, "of those warm:", warmMarriages)
print("ok")
# One life through the whole arc: engagement -> wedding -> divorce proposal.
trajectories = 0
for seed in range(1, 121):
    doc = run(seed, cash=120000, engaged=True)
    weddings = []
    divorces = []
    for event in doc["events"]:
        body = event.get("body") or ""
        if body.startswith("You have chosen to marry") and event.get("choice") == "Do it":
            weddings.append(event["age"])
        if body.endswith(" wants a divorce.") and (event.get("choice") or "").startswith("Wish "):
            divorces.append(event["age"])
    # A life can marry twice, so the arc is "some wedding before some divorce", not
    # "the first of each" -- seed 101 marries again at 79 after a divorce at 65.
    arcs = [(w, d) for w in weddings for d in divorces if w <= d]
    if arcs:
        ages = {"wedding": arcs[0][0], "divorce": arcs[0][1]}
        married_exes = [e for e in doc["final"]["exLovers"] if e["status"] == "married"]
        assert married_exes, (seed, "divorced but no married ex recorded")
        trajectories += 1

assert trajectories, "no life went engagement -> wedding -> divorce"
print("full arcs:", trajectories)
print("all invariants hold")
