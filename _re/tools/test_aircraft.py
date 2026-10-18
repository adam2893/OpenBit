"""Aircraft ownership and a flight that goes wrong (REPORT.en.md 222).

a16_aircraft-enc (Make, Model, Cost, Luxury Only) is the purchase;
a16_aircraft-danger-scenarios-enc fires once the hero owns one, and '<aircraft>'
returns the owned aircraft rather than a random row.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 45,
            "cash": 2000000, "luxury": True}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


bought = 0
danger = 0
for seed in range(1, 31):
    owned = None
    for event in run(seed)["events"]:
        table = event.get("table") or ""
        if table == "a16_aircraft-enc":
            assert " is for sale at $" in event["body"], (seed, event["body"])
            if event["choice"] == "Buy it":
                bought += 1
                owned = (event.get("logLines") or [""])[-1]
                assert owned.startswith("I bought a "), (seed, owned)
        elif table == "a16_aircraft-danger-scenarios-enc":
            danger += 1
            assert "<" not in event["body"], (seed, event["body"])
            assert not event.get("residue"), (seed, event.get("residue"))
            # A danger card can only follow a purchase in the same life.
            assert owned is not None, (seed, "danger without an aircraft")

assert bought >= 3, bought
assert danger >= 2, danger
print("bought:", bought, "danger:", danger)
print("ok")