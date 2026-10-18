"""A prize asset (REPORT.en.md 224).

a16_pray-asset-scenarios-enc: six rows, each granting 'a brand-new <asset>'. The
asset pool is the vehicle catalogue and the prize is handed over as a vehicle.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 50,
            "cash": 1000}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
for seed in range(1, 41):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_pray-asset-scenarios-enc":
            continue
        cards += 1
        body = event["body"]
        assert "<" not in body and ">" not in body, (seed, body)
        assert "brand-new" in body, (seed, body)
        assert not event.get("residue"), (seed, event.get("residue"))
        journal = (event.get("logLines") or [""])[-1]
        assert "brand-new" in journal or journal.startswith("I "), (seed, journal)

assert cards >= 5, cards
print("cards:", cards)
print("ok")