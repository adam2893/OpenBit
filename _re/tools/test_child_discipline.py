"""A child misbehaves (REPORT.en.md 179).

ScenarioChildDiscipline: the text is a row of a16_child-discipline-enc (107 rows,
filtered by age and gender), the buttons are the class's own literals, and the
journal is the row's Log Version with <child> substituted.
"""
import collections
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 40,
            "relationship": True, "children": 2, "cash": 100000}
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
bodies = set()
for seed in range(1, 31):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_child-discipline-enc":
            continue
        body = event["body"]
        assert "<child>" not in body and "<child-possessive>" not in body, (seed, body)
        assert body[0].isupper(), (seed, body)
        bodies.add(body)
        choice = event["choice"]
        assert choice == "Do nothing" or choice.startswith(("Yell at ", "Ground ", "Have a talk with ")), (seed, choice)
        lanes[choice.split()[0]] += 1
        journal = (event.get("logLines") or [""])[-1]
        assert journal and "<child>" not in journal, (seed, journal)

assert len(lanes) >= 2, lanes
assert len(bodies) >= 4, bodies   # the row really is drawn from the table
print("lanes:", dict(lanes))
print("distinct bodies:", len(bodies))
print("ok")