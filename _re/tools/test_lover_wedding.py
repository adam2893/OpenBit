"""The wedding plan (REPORT.en.md 160).

ScenarioLoverWedding is a plan: a venue from a16_wedding-venues-enc (12 rows with a
Cost Score and a Lover Happiness Score), a honeymoon destination, a total, and three
buttons. The port has the venue half and two of the class's own strings.
"""
import collections
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DO = "Do it"
NOPE = "Actually, never mind"


def run(seed, **extra):
    # Cash is a fixture (REPORT.en.md 161): without it only the courthouse is
    # affordable and the venue table's other eleven rows are never drawn.
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 60,
            "engaged": True, "cash": 100000}
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
venues = set()
for seed in range(1, 21):
    doc = run(seed)
    for event in doc["events"]:
        body = event.get("body") or ""
        if "You have chosen to marry" not in body:
            continue
        assert body.startswith("You have chosen to marry "), (seed, body)
        assert body.endswith("."), (seed, body)
        assert event.get("table") == "a16_wedding-venues-enc", (seed, event.get("table"))
        venues.add(body.rsplit(" at ", 1)[-1].rstrip("."))
        choice = event["choice"]
        assert choice in (DO, NOPE), (seed, choice)
        lanes[choice] += 1
        lines = event.get("logLines") or []
        if choice == DO:
            # Either the wedding was paid for, or the card says it could not be.
            assert (any("The wedding cost" in l for l in lines) or
                    any("more than I had" in l for l in lines)), (seed, lines)
        else:
            assert any("called off my wedding with" in l for l in lines), (seed, lines)

assert lanes[DO] and lanes[NOPE], dict(lanes)
assert len(venues) >= 3, venues   # the venue really is drawn from the table

# And a wedding that is paid for really marries (REPORT.en.md 161): the earlier
# failure was an unaffordable venue, not a status overwrite.
paid = [e for e in run(2, ages=40)["events"] if "chosen to marry" in (e.get("body") or "")]
assert paid and paid[0]["choice"] == "Do it", paid
assert run(2, ages=40)["final"]["loverStatus"] == "married", run(2, ages=40)["final"]

print("lanes:", dict(lanes))
print("distinct venues:", len(venues))
print("ok")