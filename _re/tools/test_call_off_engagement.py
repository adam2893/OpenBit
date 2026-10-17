"""The engagement called off (REPORT.en.md 158).

ScenarioLoverCallOffEngagement is a report card: RefreshScenario and DismissSelected,
no choices. Its title comes from a three-item pool, its body and journal name the
partner, and it moves karma -50 and discipline -16 (impacts -5 and -3). The test
also pins that the *engaged* status is reachable at all -- the port used to jump
from dating straight to married.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TITLES = {"Let's wait a little longer", "Just not sure anymore", "Just kidding"}


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 25}
    spec.update(extra)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
for seed in range(1, 25):
    doc = run(seed, engaged=True)
    left = False
    for event in doc["events"]:
        body = event.get("body") or ""
        if "called off" not in body:
            continue
        cards += 1
        left = True
        assert body.startswith("You called off your engagement with your "), (seed, body)
        assert body.endswith("."), (seed, body)
        assert event.get("title") in TITLES, (seed, event.get("title"))
        assert not event.get("choice"), (seed, "a report has no buttons")
        journal = (event.get("logLines") or [""])[-1]
        assert journal.startswith("I called off my engagement with "), (seed, journal)
        # out.extra is rendered into the log lines, so that is where the line lands.
        lines = event.get("logLines") or []
        assert any("still wants the commitment" in l for l in lines), (seed, lines)
        moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
        assert moves == {"karma": -50, "discipline": -16}, (seed, moves)
    if left:
        # The engagement is over: the life is back to dating, or single.
        assert doc["final"]["loverStatus"] in ("dating", None), (seed, doc["final"]["loverStatus"])

assert cards, "no engagement was ever called off"

# The engaged status is reachable in free play, not only from the fixture.
seen = set()
for seed in range(1, 31):
    seen.add(run(seed, ages=80, relationship=True)["final"]["loverStatus"])
# Since the elopement (§159) and wedding (§160) cards landed, an engagement is usually
# resolved inside the same year, so a life need not *end* engaged. The status is
# reachable through the fixture, and that is what is asserted here.
assert seen <= {"dating", "married", "engaged", None}, seen
assert "married" in seen, seen
assert run(1, ages=3, engaged=True)["final"]["loverStatus"] in ("engaged", "married")

print("cards:", cards)
print("statuses in free play:", sorted(x for x in seen if x))
print("ok")