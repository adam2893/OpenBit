"""254: the hero graduates high school."""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
LINE = "I graduated from high school."


def run(seed, ages=40):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


seen = 0
for seed in range(1, 41):
    doc = run(seed)
    hits = [e for e in doc["events"]
            if any(l == LINE for l in (e.get("logLines") or []))]
    if doc["final"]["age"] >= 18:
        assert len(hits) == 1, (seed, "graduations", len(hits))
        assert hits[0]["age"] == 18, (seed, hits[0]["age"])
        seen += 1
    else:
        assert not hits, (seed, "graduated but died before 18", hits[:1])

assert seen >= 30, seen
print("graduations:", seen)
print("ok")
