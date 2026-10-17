"""Somebody is following the hero (REPORT.en.md 187).

a16_stalker-scenarios-enc, 37 rows: Scenario, Log Version, Can Assault, Stalker Gender,
Hero Gender. A report card filtered by the hero's gender.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 90}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


bodies = set()
for seed in range(1, 41):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_stalker-scenarios-enc":
            continue
        assert event["age"] >= 16, (seed, event["age"])
        assert not event.get("choice"), (seed, "a report has no buttons")
        body = event["body"]
        assert body[0].isupper(), (seed, body)
        bodies.add(body)
        journal = (event.get("logLines") or [""])[-1]
        assert journal and journal[0].isupper(), (seed, journal)

assert len(bodies) >= 5, bodies
print("distinct stalker cards:", len(bodies))
print("ok")