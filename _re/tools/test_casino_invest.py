#!/usr/bin/env python3
"""REPORT.en.md 382: the casino's invest card.

The card's body is the recovered literal #27215; the rows reuse the labels its
sibling menu cards already carry. The wheel is crowded and a casino usually goes
bankrupt young, so the test arms a funded owner and sweeps enough seeds to see
the card.
"""
import json
import os
import re
import subprocess
import tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
CARD = "registry:ScenarioCasinoInvest"
BODY = re.compile(
    r"^Invest money into your casino today!\n\n"
    r"Casino: (?P<name>[^\n]+)\n"
    r"Available Funds: (?P<funds>-?\$[\d,]+)\n"
    r"Amount: (?P<amount>-?\$[\d,]+)$"
)


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t382.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


cards = 0
topped = 0
for seed in range(1, 61):
    data = run({
        "seed": seed,
        "country": "United States",
        "gender": "male",
        "ages": 45,
        "cash": 3000000,
        "casino": True,
        "casino_bank": 800000,
    })
    for event in data["events"]:
        if event.get("table") != CARD:
            continue
        match = BODY.match(event.get("body") or "")
        assert match, (seed, event.get("body"))
        assert event.get("choice") == "Invest", event.get("choice")
        cards += 1
        if match.group("funds") == "$0":
            topped += 1
assert cards, "the invest card never appeared"
print("cards: %d | topped up from negative: %d" % (cards, topped))
print("ok")
