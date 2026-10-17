"""The grounded lane (REPORT.en.md 180).

The discipline card shipped without effects in §179. The handler has one lane it
details -- grounded: a 1..30 day roll, ImpactDiscipline(+1), and the class sentence
"I grounded <him/her> for N days." -- and the test pins that lane.
"""
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


grounded = 0
# A child leaves home at 18 (REPORT.en.md 182), so the eligible years are fewer than
# they were when this test was written: the sample is wider rather than the bar lower.
for seed in range(1, 61):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_child-discipline-enc":
            continue
        journal = (event.get("logLines") or [""])[-1]
        if not event["choice"].startswith("Ground "):
            assert "I grounded" not in journal, (seed, journal)
            continue
        grounded += 1
        assert " I grounded " in journal and journal.endswith(" days."), (seed, journal)
        days = int(journal.rsplit(" for ", 1)[1].split(" ")[0])
        assert 1 <= days <= 30, (seed, days)
        moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
        # pointsForImpact(+1) is 4: the table runs ±100/50/30/16/8/4 for impacts 6..1.
        assert moves.get("discipline") == 4, (seed, moves)

assert grounded >= 2, grounded
print("grounded lanes:", grounded)
print("ok")