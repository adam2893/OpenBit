"""The hero's feelings (REPORT.en.md 217).

a16_sexuality-scenarios-enc gates all 102 rows on a Sexuality column ('gay' or 'bi'
only), so the port needed the value before the card could fire; the script's
"sexuality" key sets it and 'straight' is the default -- which means, faithfully, no card.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 40}
    spec.update(extra)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
for seed in range(1, 61):
    for event in run(seed, sexuality="gay")["events"]:
        if event.get("table") != "a16_sexuality-scenarios-enc":
            continue
        cards += 1
        body = event["body"]
        assert "<" not in body and ">" not in body, (seed, body)
        assert body[0].isupper(), (seed, body)
        assert not event.get("residue"), (seed, event.get("residue"))

assert cards >= 2, cards
# The table carries no 'straight' rows, so a straight hero never sees this card.
for seed in range(1, 21):
    doc = run(seed)
    hits = [e for e in doc["events"] if e.get("table") == "a16_sexuality-scenarios-enc"]
    assert not hits, (seed, hits[:1])

print("cards:", cards)
print("ok")