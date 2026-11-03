#!/usr/bin/env python3
"""REPORT.en.md 388: the zoo's hiring card.

The candidate comes from a16_zoo-employee-application-color-enc; the blurb is that
row's String, and the competence is drawn inside the row's own Min/Max band.
"""
import json
import os
import re
import subprocess
import tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
TABLE = "registry:ScenarioZooEmployeeHiring"
BODY = re.compile(
    r"^Hire an employee for your zoo today!\n\n"
    r"(?P<app>.+?)\n\n"
    r"Applicant: (?P<who>[^\n]+)\n"
    r"Competence: (?P<comp>\d+)\n"
    r"Existing Competence: (?P<exist>\d+)$",
    re.S,
)


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t388.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


cards = 0
hires = 0
ignores = 0
for seed in range(1, 41):
    data = run({
        "seed": seed, "country": "United States", "gender": "male", "ages": 45,
        "cash": 3000000, "zoo": True, "zoo_bank": 1500000, "only": "zoo-hire",
    })
    for event in data["events"]:
        if event.get("table") != TABLE:
            continue
        match = BODY.match(event.get("body") or "")
        assert match, (seed, event.get("body"))
        assert event.get("title") == "Employee Candidate", event.get("title")
        assert event.get("choice") in ("Hire them", "Ignore it"), event.get("choice")
        comp = int(match.group("comp"))
        assert 0 <= comp <= 100, (seed, comp)
        assert match.group("app")[0].isupper(), (seed, match.group("app"))
        logs = event.get("logLines") or []
        assert any(line.startswith("I interviewed a candidate for the open ") for line in logs), (seed, logs)
        if event.get("choice") == "Hire them":
            hires += 1
            assert any(line.startswith("I hired ") and line.endswith(" for my zoo.") for line in logs), (seed, logs)
        else:
            ignores += 1
            assert not [line for line in logs if line.startswith("I hired ")], (seed, logs)
        cards += 1
assert cards, "the hiring card never appeared"
assert hires and ignores, (cards, hires, ignores)
print("hiring cards: %d | hires: %d | ignores: %d" % (cards, hires, ignores))
print("ok")
