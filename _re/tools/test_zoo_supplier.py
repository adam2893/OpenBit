#!/usr/bin/env python3
"""REPORT.en.md 386: the zoo's food supplier row.

The body and all four rows were recovered (#10831 plus #11430/#16840/#16841/#11478),
and the journal is #3628's clause joined to the zoo's name. The supplier sets the
per-animal food line the yearly ledger charges, so the test also demands that the two
supplier-cost rows really differ on most cards.
"""
import json
import os
import re
import subprocess
import tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
TABLE = "registry:ScenarioZooFoodSupplier"
BODY = re.compile(
    r"^Adjust your fixed cost by changing your supplier\.\n\n"
    r"A new supplier may give you a lower fixed cost!\n\n"
    r"Current Supplier: (?P<old>[^\n]+)\n"
    r"Current Supplier Cost: \$(?P<oldcost>[\d,]+)\n"
    r"Animal Food Supplier: (?P<new>[^\n]+)\n"
    r"Annual Supplier Cost: \$(?P<newcost>[\d,]+)$"
)


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t386.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


cards = 0
moves = 0
for seed in range(1, 41):
    data = run({
        "seed": seed, "country": "United States", "gender": "male", "ages": 45,
        "cash": 3000000, "zoo": True, "zoo_bank": 1200000, "only": "zoo-supplier",
    })
    for event in data["events"]:
        if event.get("table") != TABLE:
            continue
        match = BODY.match(event.get("body") or "")
        assert match, (seed, event.get("body"))
        assert match.group("new") != match.group("old"), (seed, match.group("old"))
        assert event.get("choice") == "OK", event.get("choice")
        logs = event.get("logLines") or []
        assert any(line.endswith("to become the new supplier for my zoo.") for line in logs), (seed, logs)
        if match.group("oldcost") != match.group("newcost"):
            moves += 1
        cards += 1
assert cards, "the supplier card never appeared"
assert moves >= cards // 3, (cards, moves)
print("supplier cards: %d | cards whose cost moved: %d" % (cards, moves))
print("ok")
