"""A drink is offered (REPORT.en.md 204).

a16_alcohol-scenarios-enc picks who is offering (Job/Club/Lover/None) and states the
scenario; a16_alcohol-enc answers the three drink forms its own columns carry.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 40,
            "relationship": True}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
for seed in range(1, 31):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_alcohol-scenarios-enc":
            continue
        cards += 1
        body = event["body"]
        choice = event["choice"]
        # Every drink form is answered: three token spellings, none left over.
        assert "<" not in body and ">" not in body, (seed, body)
        assert choice and choice[0].isupper(), (seed, choice)
        assert "<" not in choice, (seed, choice)
        journal = (event.get("logLines") or [""])[-1]
        assert journal and "<" not in journal, (seed, journal)
        moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
        assert moves, (seed, "a lane should move something")

assert cards >= 3, cards
print("cards:", cards)
print("ok")