#!/usr/bin/env python3
"""REPORT.en.md 383: the two install cards.

Both bodies were recovered (#26763 casino game, #26765 zoo attraction); the rows
reuse the labels their sibling menu cards already carry. A casino bleeds fast, so
its card gets a wide sweep; the zoo survives longer.
"""
import json
import os
import re
import subprocess
import tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
GAME = "registry:ScenarioCasinoInstallGame"
ZOO = "registry:ScenarioZooAttractionInstall"
GBODY = re.compile(
    r"^Install a casino game in your empty game room today!\n\n"
    r"Casino: (?P<casino>[^\n]+)\nGame: (?P<game>[^\n]+)\n"
    r"Installation Cost: \$\S+\nAvailable Funds: -?\$[\d,]+$"
)
ZBODY = re.compile(
    r"^Install a new attraction at your zoo today!\n\n"
    r"Zoo: (?P<zoo>[^\n]+)\nAttraction: (?P<attraction>[^\n]+)\n"
    r"Installation Cost: \$\S+\nZoo's Current Capital: -?\$[\d,]+$"
)


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t383.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


games = 0
zoo_cards = 0
for seed in range(1, 81):
    data = run({
        "seed": seed, "country": "United States", "gender": "male", "ages": 40,
        "cash": 3000000, "casino": True, "casino_bank": 1500000,
        "only": "casino-install-game",
    })
    for event in data["events"]:
        if event.get("table") != GAME:
            continue
        match = GBODY.match(event.get("body") or "")
        assert match, (seed, event.get("body"))
        assert event.get("choice") == "OK", event.get("choice")
        games += 1
for seed in range(1, 41):
    data = run({
        "seed": seed, "country": "United States", "gender": "male", "ages": 45,
        "cash": 3000000, "zoo": True, "zoo_bank": 1500000,
    })
    for event in data["events"]:
        if event.get("table") != ZOO:
            continue
        match = ZBODY.match(event.get("body") or "")
        assert match, (seed, event.get("body"))
        assert event.get("choice") == "OK", event.get("choice")
        zoo_cards += 1
assert games, "the casino game install card never appeared"
assert zoo_cards, "the zoo attraction install card never appeared"
print("casino game cards: %d | zoo attraction cards: %d" % (games, zoo_cards))
print("ok")
