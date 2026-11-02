#!/usr/bin/env python3
"""REPORT.en.md 387: the zoo's comment card.

The card is a patron's suggestion drawn from a16_zoo-comment-card-color-enc; the price
row is that attraction's Installation Cost, and the two answers install it or ignore it.
"""
import json
import os
import re
import subprocess
import tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
TABLE = "registry:ScenarioZooCommentCard"
BODY = re.compile(
    r"^Review comment cards left by your zoo's patrons to see what attraction would appeal to them today!\n\n"
    r"(?P<snippet>.+)\n\n"
    r"Attraction: (?P<type>[^\n]+)\n"
    r"Estimated Install Cost: \$(?P<cost>[\d,]+)$"
)
INSTALL_LOG = "A zoo patron suggested I install "
READ_LOG = "I began reviewing comment cards for ideas about which zoo attraction I should install."


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t387.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


cards = 0
installs = 0
ignores = 0
for seed in range(1, 41):
    data = run({
        "seed": seed, "country": "United States", "gender": "male", "ages": 45,
        "cash": 3000000, "zoo": True, "zoo_bank": 1500000, "only": "zoo-comment-card",
    })
    for event in data["events"]:
        if event.get("table") != TABLE:
            continue
        match = BODY.match(event.get("body") or "")
        assert match, (seed, event.get("body"))
        assert event.get("title") == "Comment Card", event.get("title")
        assert event.get("choice") in ("Install it", "Ignore it"), event.get("choice")
        logs = event.get("logLines") or []
        assert READ_LOG in logs, (seed, logs)
        if event.get("choice") == "Install it":
            installs += 1
            assert INSTALL_LOG + match.group("type") + "." in logs, (seed, logs, match.group("type"))
        else:
            ignores += 1
            assert not [line for line in logs if line.startswith(INSTALL_LOG)], (seed, logs)
        cards += 1
assert cards, "the comment card never appeared"
assert installs and ignores, (cards, installs, ignores)
print("comment cards: %d | installs: %d | ignores: %d" % (cards, installs, ignores))
print("ok")
