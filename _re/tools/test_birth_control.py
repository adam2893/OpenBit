"""The birth-control state (REPORT.en.md 141).

The game keeps it on the person (Person.OnBirthControl) and stamps the life with
two ages; three lover-family cards are written around it. The state has to exist
before any of them can be ported, and this test pins the one thing a state with no
card yet can be pinned on: the spec switch reaches the hero and comes back out of
the JSON, and the default is off.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "female", "ages": 12}
    spec.update(extra)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


# Off by default, in every life.
for seed in range(1, 6):
    assert run(seed)["final"]["birthControl"] is False, seed

# And on, when the spec says so -- both spellings of the value.
for seed in range(1, 6):
    assert run(seed, birthControl=True)["final"]["birthControl"] is True, seed
    assert run(seed, birthControl=False)["final"]["birthControl"] is False, seed

# A value that is not a boolean is an error, not a silent false.
spec = {"seed": 1, "ages": 2, "birthControl": 1}
with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
    json.dump(spec, fh)
    path = fh.name
proc = subprocess.run([str(PORT), "--script", path], text=True,
                      capture_output=True, cwd=str(ROOT / "port"))
Path(path).unlink()
assert proc.returncode == 2, (proc.returncode, proc.stderr[-200:])
assert "birthControl is not a boolean" in proc.stderr, proc.stderr[-200:]

print("birth control state: ok")