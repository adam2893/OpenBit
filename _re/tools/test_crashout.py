"""A famous hero loses it in public (REPORT.en.md 215).

a16_crashout-scenario-enc: Severity, Location, Scenario, Magazine Snippet, BitLife
Snippet, Log String, Hero Gender, Relatable, Fame Impact. The card is gated on fame
(>= 40) and the row's own Fame Impact moves the score.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 40,
            "fame": 80}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
# 120 seeds, not 60: the wheel turned wider again when the zoo's and the casino's
# rename cards joined the family list (REPORT 376), so a famous life meets the
# crashout card less often still.
for seed in range(1, 121):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_crashout-scenario-enc":
            continue
        cards += 1
        body = event["body"]
        assert "<" not in body and ">" not in body, (seed, body)
        assert body[0].isupper(), (seed, body)
        assert not event.get("residue"), (seed, event.get("residue"))

assert cards >= 2, cards
# No crashout before the fame gate: a plain life never sees one.
for seed in range(1, 11):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 40}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    doc = json.loads(proc.stdout)
    hits = [e for e in doc["events"] if e.get("table") == "a16_crashout-scenario-enc"]
    assert not hits, (seed, hits[:1])

print("cards:", cards)
print("ok")