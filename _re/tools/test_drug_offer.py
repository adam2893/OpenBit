"""A drug offered (REPORT.en.md 214).

a16_drug-scenarios-enc: Condition, Scenario, CTA, Yes Choice, No Choice,
Specific Drug. '<drug>' comes from a16_drugs-enc via the pool token layer (213).
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 40}
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
        if event.get("table") != "a16_drug-scenarios-enc":
            continue
        cards += 1
        body = event["body"]
        assert "<" not in body and ">" not in body, (seed, body)
        choice = event["choice"]
        assert choice and "<" not in choice, (seed, choice)
        assert not event.get("residue"), (seed, event.get("residue"))
        moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
        assert moves, (seed, "a lane should move something")

assert cards >= 1, cards
print("cards:", cards)
print("ok")