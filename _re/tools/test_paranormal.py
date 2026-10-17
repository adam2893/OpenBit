"""A paranormal encounter (REPORT.en.md 186).

a16_paranormal-encounters-enc, 19 rows: Scenario, Log Version, Gender, Entity, Danger
Score. A report card -- the row is the whole exchange.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed, gender="male"):
    spec = {"seed": seed, "country": "United States", "gender": gender, "ages": 90}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


bodies = set()
count = 0
for seed in range(1, 41):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_paranormal-encounters-enc":
            continue
        count += 1
        body = event["body"]
        assert body[0].isupper(), (seed, body)
        assert not event.get("choice"), (seed, "a report has no buttons")
        journal = (event.get("logLines") or [""])[-1]
        assert journal, (seed, "no journal")
        bodies.add(body)

assert count >= 3, count
assert len(bodies) >= 3, bodies
print("encounters:", count, "distinct:", len(bodies))
print("ok")