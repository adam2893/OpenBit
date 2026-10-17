"""People have a place (REPORT.en.md 163).

The game asks SimPerson::LivesInSamePlaceAsHero before several ex and child cards,
and the port had a place for nobody but the hero. An ex-lover and a child now carry
the city they were in, so the question can be asked at all.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 40,
            "relationship": True}
    spec.update(extra)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


docs = [run(seed, ex=True, children=1) for seed in range(1, 6)]
for doc in docs:
    final = doc["final"]
    # Every record carries a place, and it is the hero's own city: the fixture and
    # the birth card both take it from where the hero lives.
    for kid in final["children"]:
        assert "city" in kid, kid
    for ex in final["exLovers"]:
        assert "city" in ex, ex
        assert ex["city"] == final["city"], (ex, final["city"])

print("children and ex-lovers carry a place")
print("ok")