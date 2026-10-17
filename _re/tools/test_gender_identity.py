"""The hero's sense of self (REPORT.en.md 194).

a16_gender-scenarios-enc, 28 rows: Scenario, Log Version, Gender, Min/Max Age, Transgender
Identity, Other Identity, Student, Both Parents Alive. A report card filtered by the
hero's gender and age; the Student flag is checked against the port's `schooling`.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 40}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
bodies = set()
for seed in range(1, 41):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_gender-scenarios-enc":
            continue
        cards += 1
        assert not event.get("choice"), (seed, "a report has no buttons")
        body = event["body"]
        assert body[0].isupper(), (seed, body)
        journal = (event.get("logLines") or [""])[-1]
        assert journal and journal[0].isupper(), (seed, journal)
        bodies.add(body)

assert cards >= 5, cards
assert len(bodies) >= 4, bodies
print("cards:", cards, "distinct:", len(bodies))
print("ok")