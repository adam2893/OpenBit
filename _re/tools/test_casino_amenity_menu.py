#!/usr/bin/env python3
"""REPORT.en.md 385: the amenity menu's install and demolish rows.

Purchasing a casino now leaves one amenity slot empty -- the state the install row
exists for -- so both cards get their own only-mode sweep. The bodies are the
recovered #26768 / #17499, and the demolish card carries the recovered confirmation
#11807 instead of opening a second screen.
"""
import json
import os
import subprocess
import tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
INSTALL = "registry:ScenarioCasinoAmenityInstall"
DEMOLISH = "registry:ScenarioCasinoAmenityDemolish"


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t385.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


installs = 0
demolitions = 0
installed_names = set()
for seed in range(1, 61):
    for only, table in (("casino-amenity-install", INSTALL), ("casino-amenity-demolish", DEMOLISH)):
        data = run({
            "seed": seed, "country": "United States", "gender": "male", "ages": 40,
            "cash": 3000000, "casino": True, "casino_bank": 1500000, "only": only,
        })
        for event in data["events"]:
            if event.get("table") != table:
                continue
            body = event.get("body") or ""
            if table == INSTALL:
                assert body.startswith("Install an amenity in your casino today!"), (seed, body)
                assert "\nAmenity Capacity: " in body, (seed, body)
                assert "\nAmenity Installation: $" in body, (seed, body)
                assert event.get("choice") == "Install a new amenity", event.get("choice")
                installed_names.add(body.split("\nAmenity: ")[1].split("\n")[0])
                installs += 1
            else:
                assert body.startswith("Demolish one of your amenities today!"), (seed, body)
                assert body.endswith("Are you sure you want to demolish this amenity?"), (seed, body)
                assert event.get("choice") == "Demolish Amenity", event.get("choice")
                demolitions += 1
assert installs, "the install card never appeared"
assert demolitions, "the demolish card never appeared"
print("install cards: %d (%d amenities) | demolish cards: %d" % (installs, len(installed_names), demolitions))
print("ok")
