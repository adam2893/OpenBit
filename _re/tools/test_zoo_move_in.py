#!/usr/bin/env python3
"""REPORT.en.md 390: the animal move-in card.

The habitat cards (379/380) build and clear habitats but never put an animal on
display; the catalogue names the habitats each species tolerates, so the join is exact.
"""
import json
import os
import re
import subprocess
import tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
TABLE = "registry:ScenarioZooHabitatMoveIn"
BODY = re.compile(
    r"^Move an animal into this habitat today!\n\n"
    r"Habitat: (?P<hab>.+? \((?P<type>[a-z ]+)\))\n"
    r"Animal: (?P<animal>.+ the .+)\n\n"
    r"(?P<line>[A-Z].+)$",
    re.S,
)


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t390.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


cards = 0
moved = 0
declined = 0
for seed in range(1, 13):
    doc = run({"seed": seed, "country": "United States", "gender": "female", "ages": 40,
               "cash": 3000000, "zoo": True, "zoo_bank": 2000000, "zoo_habitats": 2,
               "only": "zoo-move-in"})
    for event in doc["events"]:
        if event.get("table") != TABLE:
            continue
        match = BODY.match(event.get("body") or "")
        assert match, (seed, event.get("body"))
        assert event.get("choice") in ("Move it in", "Not now"), (seed, event.get("choice"))
        logs = event.get("logLines") or []
        if event.get("choice") == "Move it in":
            moved += 1
            assert any(line.startswith("I moved ") and " into " in line for line in logs), (seed, logs)
            assert any(line.endswith("is now on display in %s." % match.group("hab")) for line in logs), (seed, logs)
        else:
            declined += 1
            assert any(line.startswith("I left ") and " off display for now." in line for line in logs), (seed, logs)
        cards += 1
assert cards, "the move-in card never appeared"
assert moved and declined, (cards, moved, declined)
print("move-in cards: %d | moved: %d | declined: %d" % (cards, moved, declined))
print("ok")
