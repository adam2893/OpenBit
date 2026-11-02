"""Vehicle ownership and the partner's reaction (REPORT.en.md 220).

a16_outdoor-vehicle-enc (Make, Model, Cost) is the purchase;
a16_spouse-vehicle-reactions-enc reacts to it with '<vehicle>' and '<lover>'.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    # The cash has to outlive the college tuition (§317 spends the starting savings
    # on a US education), or the hero is broke at the age the vehicles are offered
    # and the sale count collapses -- that is an affordability effect, not the sale
    # mechanic this test is about.
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 45,
            "cash": 300000, "relationship": True}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


bought = 0
reacted = 0
# Every zoo card added since 353 takes another ticket in the same wheel, so the one
# spousal reaction this run produces keeps moving further out (measured at seed 86
# over the first ninety). Walk until it turns up instead of pinning a window.
for seed in range(1, 131):
    if reacted:
        break
    for event in run(seed)["events"]:
        table = event.get("table") or ""
        if table == "a16_outdoor-vehicle-enc":
            body = event["body"]
            assert " is for sale at $" in body, (seed, body)
            assert event["choice"] in ("Buy it", "Walk away"), (seed, event["choice"])
            if event["choice"] == "Buy it":
                bought += 1
                journal = (event.get("logLines") or [""])[-1]
                assert journal.startswith("I bought a "), (seed, journal)
        elif table == "a16_spouse-vehicle-reactions-enc":
            reacted += 1
            body = event["body"]
            assert "<" not in body and ">" not in body, (seed, body)
            assert not event.get("residue"), (seed, event.get("residue"))

assert bought >= 3, bought
assert reacted >= 1, reacted
print("bought:", bought, "reactions:", reacted)
print("ok")