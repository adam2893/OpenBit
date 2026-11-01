
#!/usr/bin/env python3
"""The zoo's cleanliness card (REPORT.en.md 348).

The operations manager reports that the guests are complaining about the park's
cleanliness. The native card carries no RNG: it deals whenever the zoo is old enough
and the custodial score sits under twenty, turns on one reallocate-or-ignore choice,
and always writes the same journal line.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
CARD = "registry:ScenarioZooAgeUpCleanliness"

BODY = re.compile(
    r"^Your operations manager, (?P<manager>.+), is reporting that your guests are "
    r"complaining about the cleanliness of the park\.\n\n(?:She|He) suggests increasing "
    r"your zoo's custodial team to improve things\.$")
LOG = "My operations manager suggested I assign more of my work force to custodial tasks."


def run(seed, ages=70, zoo=True, choices=None):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 3000000, "only": "zoo-cleanliness"}
    if zoo:
        spec["zoo"] = True
    if choices is not None:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    done = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                          capture_output=True, text=True)
    assert done.returncode == 0, done.stderr[-800:]
    return json.loads(done.stdout)


def cards(doc):
    return [e for e in doc["events"] if e.get("table") == CARD]


seen = 0
ages = {}
for seed in range(1, 16):
    per_seed = []
    for card in cards(run(seed)):
        seen += 1
        assert card["title"] == "Cleanliness", card
        assert card["body"].count("\n") >= 2
        body = BODY.match(card["body"])
        assert body, (seed, card["body"])
        assert card["choice"] in ("Reallocate labor resources", "Ignore him",
                                  "Ignore her"), card
        log = [line for line in card.get("logLines") or [] if line == LOG]
        assert log, (seed, card.get("logLines"))
        deltas = card.get("deltas") or {}
        assert not any(deltas.values()), (seed, deltas)
        per_seed.append(card["age"])
    ages[seed] = per_seed
assert seen >= 3, seen
for seed, years in ages.items():
    years.sort()
    for a, b in zip(years, years[1:]):
        assert b - a >= 4, (seed, years)

# Both answers log the same line and move nothing.
for choice in (0, 1):
    saw = 0
    for seed in range(1, 8):
        for card in cards(run(seed, choices=[choice] * 60)):
            saw += 1
            assert any(line == LOG for line in card.get("logLines") or []), (seed, choice)
            assert not any((card.get("deltas") or {}).values()), (seed, choice)
    assert saw, choice

# A hero without a zoo never sees the card.
for seed in range(2, 6):
    assert not cards(run(seed, zoo=False)), seed
print("cards:", seen)
print("ok")
