"""The ex has married someone else (REPORT.en.md 174).

ScenarioExLoverMarriedNow: the ex refuses because she or he is married. Three
buttons; the third opens the court chain, which the port does not have.
"""
import collections
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 60,
            "relationship": True, "ex": True}
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
for seed in range(1, 41):
    for event in run(seed)["events"]:
        body = event.get("body") or ""
        if not body.startswith("Your ex-") or " is married" not in body:
            continue
        assert " can't " in body, (seed, body)
        assert ("get back together with" in body) or ("start dating" in body), (seed, body)
        assert body.endswith("."), (seed, body)
        choice = event["choice"]
        assert choice in ("Respect that",) or choice.startswith(("Ask ", "Convince ")), (seed, choice)
        lanes[choice.split()[0]] += 1
        journal = (event.get("logLines") or [""])[-1]
        assert journal.startswith("My ex-"), (seed, journal)

assert len(lanes) >= 2, lanes
print("lanes:", dict(lanes))
print("ok")