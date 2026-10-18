"""The hero buys a home (REPORT.en.md 218).

a16_house-types-enc, with the card's own Price Score scaled to money. Buying sets
Hero::home, which is the state the spouse-reaction tables need.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 40,
            "cash": 400000}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
bought = 0
for seed in range(1, 31):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_house-types-enc":
            continue
        cards += 1
        body = event["body"]
        assert "$" in body and " on the market for " in body, (seed, body)
        assert event["choice"] in ("Buy it", "Keep looking"), (seed, event["choice"])
        moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
        assert not moves, (seed, "the price is money, not a stat")
        if event["choice"] == "Buy it":
            bought += 1
            journal = (event.get("logLines") or [""])[-1]
            assert journal.startswith("I bought a "), (seed, journal)

assert cards >= 2, cards
assert bought >= 1, bought
print("cards:", cards, "bought:", bought)
print("ok")