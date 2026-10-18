"""A prayer answered with money (REPORT.en.md 216).

a16_pray-wealth-scenarios-enc. Both strings carry '<value>' (the amount this card
rolls) and optionally '<circumstance-public>' (the captured scene pool, report 38).
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
        if event.get("table") != "a16_pray-wealth-scenarios-enc":
            continue
        cards += 1
        body = event["body"]
        assert "<" not in body and ">" not in body, (seed, body)
        # The boon rows name an amount; the curse rows take funds and name none.
        # Rows either name an amount, take funds, or thank the hero for a donation; all
        # three are the class's own sentences, so the only hard rule is no token.
        assert body[0].isupper(), (seed, body)
        journal = (event.get("logLines") or [""])[-1]
        assert "<" not in journal, (seed, journal)
        assert not event.get("residue"), (seed, event.get("residue"))

assert cards >= 3, cards
print("cards:", cards)
print("ok")