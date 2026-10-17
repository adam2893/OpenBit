"""The partner argues (REPORT.en.md 153).

ScenarioLoverArgues is one class with ten constructors and a Refresh that switches
on which object it was built with. The int reason is the one the port can feed, and
its reason 1 is the cheating argument. The report rides the same age-up as the
hookup that caused it, through the pending queue (REPORT 133).
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 60,
            "relationship": True, "office": True}
    spec.update(extra)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


reports = 0
chained = 0
for seed in range(1, 41):
    doc = run(seed)
    events = doc["events"]
    for i, event in enumerate(events):
        body = event.get("body") or ""
        if "is arguing with you because you cheated" not in body:
            continue
        reports += 1
        assert body.startswith("She ") or body.startswith("He "), (seed, body)
        assert body.endswith("."), (seed, body)
        assert "girlfriend" in body or "boyfriend" in body or "wife" in body \
               or "husband" in body, (seed, body)
        assert not event.get("choice"), (seed, "the report has no buttons")
        journal = (event.get("logLines") or [""])[-1]
        assert journal.endswith("because I cheated on her.") or \
               journal.endswith("because I cheated on him."), (seed, journal)
        # It rides the same age-up as the hookup that caused it (REPORT 133).
        if i and "seduced" in ((events[i - 1].get("body") or "") + str(events[i - 1].get("logLines"))).lower():
            chained += 1
            assert events[i - 1]["age"] == event["age"], (seed, event["age"])

assert reports, "no argue report in 40 lives"
assert chained, "the report never rode the same age as its hookup"
print("argue reports:", reports, "same-age as the hookup:", chained)
print("ok")