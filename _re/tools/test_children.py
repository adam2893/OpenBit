"""The child model (REPORT.en.md 146).

The game keeps a SimChild array on the life; four of the five cards the sprint is
blocked on read it (145). The array now exists with the fields those cards read,
ages with the hero, and is reachable from a script so a test can see it. What does
not exist yet is a birth: the game gets a child through its baby scenarios, and
inventing a lifecycle line for one would be text the game does not have.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed, ages=20, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "female", "ages": ages}
    spec.update(extra)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


# No children unless the script asks for them.
for seed in range(1, 6):
    assert run(seed)["final"]["children"] == [], seed

# The fixture puts them on the life, and they age with the hero.
for seed in range(1, 6):
    doc = run(seed, children=2)
    kids = doc["final"]["children"]
    assert len(kids) == 2, (seed, kids)
    grew = doc["final"]["age"]
    for i, kid in enumerate(kids):
        # A child is born during the life, not before it (REPORT.en.md 149): its age is
        # counted from its own bornAtHeroAge, so it is never older than the hero.
        assert kid["age"] <= grew, (seed, kid, grew)
        # Before its birth age the child is simply not born yet (age 0).
        if grew < kid["bornAtHeroAge"]:
            assert kid["age"] == 0, (seed, kid, grew)
        else:
            assert kid["age"] <= grew - kid["bornAtHeroAge"] + 1, (seed, kid, grew)
        assert kid["female"] is (i % 2 == 1), (seed, kid)
        assert kid["alive"] is True, (seed, kid)
        # SimChild.MovedOut: only an adult can have left home.
        assert kid["movedOut"] is False or kid["age"] >= 18, (seed, kid)
        assert kid["paternityAsked"] == 0, (seed, kid)
        # Named from the game's own pool (REPORT.en.md 148): the court-order card
        # prints the child's name in its body.
        assert kid["name"] and kid["name"][0].isupper(), (seed, kid)
        assert " " not in kid["name"], (seed, kid)   # a first name, not a full name
        assert kid["withCurrentLover"] is False, (seed, kid)

# The count is bounded, and a bad value is an error rather than a silent zero.
spec = {"seed": 1, "ages": 2, "children": 99}
with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
    json.dump(spec, fh)
    path = fh.name
proc = subprocess.run([str(PORT), "--script", path], text=True,
                      capture_output=True, cwd=str(ROOT / "port"))
Path(path).unlink()
assert proc.returncode == 2, (proc.returncode, proc.stderr[-200:])
assert "children must be between 0 and 12" in proc.stderr, proc.stderr[-200:]

print("child model: ok")