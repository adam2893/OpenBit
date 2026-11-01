
#!/usr/bin/env python3
"""The zoo's customer service card (REPORT.en.md 349).

The operations manager says ticket sales would improve with more customer service
staff. Like the cleanliness card it carries no RNG anywhere, turns on the same
reallocate-or-ignore choice, and always writes the same journal line. The native body
carries its own typo ("addded"), kept as it ships.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
CARD = "registry:ScenarioZooAgeUpCustomerService"

BODY = re.compile(
    r"^Your operations manager, (?P<manager>.+), is reporting that your park's ticket "
    r"sales could improve if you addded more customer service representatives\.$")
LOG = ("My operations manager suggested I assign more of my work force to customer "
       "service-related tasks.")


def run(seed, ages=70, zoo=True, choices=None):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 3000000, "only": "zoo-customer-service"}
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
        assert card["title"] == "Customer Service", card
        body = BODY.match(card["body"] or "")
        assert body, (seed, card["body"])
        assert card["choice"] in ("Reallocate labor resources", "Ignore him",
                                  "Ignore her"), card
        assert any(line == LOG for line in card.get("logLines") or []), (seed, card.get("logLines"))
        assert not any((card.get("deltas") or {}).values()), (seed, card.get("deltas"))
        per_seed.append(card["age"])
    ages[seed] = per_seed
assert seen >= 3, seen
for seed, years in ages.items():
    years.sort()
    for a, b in zip(years, years[1:]):
        assert b - a >= 4, (seed, years)

for choice in (0, 1):
    saw = 0
    for seed in range(1, 8):
        for card in cards(run(seed, choices=[choice] * 60)):
            saw += 1
            assert any(line == LOG for line in card.get("logLines") or []), (seed, choice)
            assert not any((card.get("deltas") or {}).values()), (seed, choice)
    assert saw, choice

for seed in range(2, 6):
    assert not cards(run(seed, zoo=False)), seed
print("cards:", seen)
print("ok")
