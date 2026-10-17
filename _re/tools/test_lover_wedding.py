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
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 60,
            "engaged": True, "money": 300000}
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
            assert any("The wedding cost" in l for l in lines), (seed, lines)
        else:
            assert any("called off my wedding with" in l for l in lines), (seed, lines)

assert lanes[DO] and lanes[NOPE], dict(lanes)
assert len(venues) >= 3, venues   # the venue really is drawn from the table

print("lanes:", dict(lanes))
print("distinct venues:", len(venues))
print("ok")