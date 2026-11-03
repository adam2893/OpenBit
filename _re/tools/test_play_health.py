"""Played and scripted defaults apply the same annual illness health cost."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "port/build/blsim"
spec = dict(seed=7, country="United States", gender="male", ages=3)
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "life.json"
    path.write_text(json.dumps(spec))
    scripted = subprocess.run([str(BIN), "--script", str(path)], cwd=ROOT,
                              capture_output=True, text=True)
    assert scripted.returncode == 0, scripted.stderr
    life = json.loads(scripted.stdout)
    assert life["final"]["age"] == spec["ages"] and life["final"]["alive"], life["final"]
    assert any(event["ailmentCost"] > 0 for event in life["events"]), "fixture must exercise annual illness damage"
    played = subprocess.run([str(BIN), "--script", str(path), "--play"], cwd=ROOT,
                            input=chr(10)*30, capture_output=True, text=True)
    assert played.returncode == 0, played.stderr
    health = re.findall(r"health ([0-9]+)", played.stdout)
    ages = re.findall(r"lived to ([0-9]+)", played.stdout)
    assert health and ages, played.stdout
    assert int(health[-1]) == life["final"]["stats"]["health"], (health[-1], life["final"], played.stdout)
    assert int(ages[-1]) == life["final"]["age"], (ages, life["final"])
    assert ("*** death at " not in played.stdout) == life["final"]["alive"]
print("ok: matched default lives apply annual illness damage equally; final health, age and alive state agree")
