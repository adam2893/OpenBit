"""The hero is let go (REPORT.en.md 201).

a16_termination-scenarios-enc, 109 rows: Condition, Scenario, Gender, Clean. The port
offers the rows whose reason it can have -- no condition, or Age past 55 -- and clears
the job when one fires.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 70}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
for seed in range(1, 31):
    doc = run(seed)
    for event in doc["events"]:
        if event.get("table") != "a16_termination-scenarios-enc":
            continue
        cards += 1
        body = event["body"]
        assert body[0].isupper(), (seed, body)
        assert not event.get("choice"), (seed, "a report has no buttons")
        lines = event.get("logLines") or []
        assert any("I was let go from my job as" in l for l in lines), (seed, lines)

assert cards >= 3, cards
print("cards:", cards)
print("ok")