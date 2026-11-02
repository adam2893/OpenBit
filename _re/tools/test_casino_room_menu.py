#!/usr/bin/env python3
"""REPORT.en.md 384: the game room's rename and clear-out rows.

Both bodies were recovered (#21733 and #15708); the remove card appends the
recovered confirmation sentence #11790 instead of opening a second screen. A
casino goes bankrupt young and the wheel is crowded, so each card gets its own
only-mode sweep.
"""
import json
import os
import re
import subprocess
import tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
NAME = "registry:ScenarioCasinoGameRoomName"
REMOVE = "registry:ScenarioCasinoGameRoomRemove"
NBODY = re.compile(
    r"^Give your game room a new name today!\n\n"
    r"Game: (?P<game>[^\n]+)\nCurrent Name: (?P<cur>[^\n]+)\nName: (?P<new>[^\n]+)$"
)
RBODY = re.compile(
    r"^Clear out a game room today!\n\n"
    r"Game: (?P<game>[^\n]+)\nGame Rooms Installed: (?P<n>\d+)\n\n"
    r"Are you sure you want to clear out this game room\?$"
)


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t384.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


names = 0
removes = 0
for seed in range(1, 41):
    for only, table in (("casino-room-name", NAME), ("casino-room-remove", REMOVE)):
        data = run({
            "seed": seed, "country": "United States", "gender": "male", "ages": 35,
            "cash": 3000000, "casino": True, "casino_bank": 1200000, "only": only,
        })
        for event in data["events"]:
            if event.get("table") != table:
                continue
            body = event.get("body") or ""
            if table == NAME:
                match = NBODY.match(body)
                assert match, (seed, body)
                assert match.group("new") != match.group("cur"), (seed, body)
                assert event.get("choice") == "OK", event.get("choice")
                names += 1
            else:
                match = RBODY.match(body)
                assert match, (seed, body)
                assert event.get("choice") == "Remove Game Room", event.get("choice")
                removes += 1
assert names, "the rename card never appeared"
assert removes, "the clear-out card never appeared"
print("name cards: %d | remove cards: %d" % (names, removes))
print("ok")
