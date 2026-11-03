#!/usr/bin/env python3
"""REPORT.en.md 389: the zoo candidate's interview card.

The hiring card no longer hires on the answer alone: natively the candidate screen
hands off to an InterviewQuestion card whose answers carry scores, so the port chains
a second card through the same table the office hiring flow already uses.
"""
import json
import os
import re
import subprocess
import tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
TABLE = "registry:ScenarioZooEmployeeHiring"
CARD = re.compile(r"^Hire an employee for your zoo today!\n\n(?P<app>.+?)\n\nApplicant: (?P<who>[^\n]+)\nCompetence: (?P<comp>\d+)\nExisting Competence: (?P<exist>\d+)$", re.S)
QUESTION = re.compile(r"^(?P<q>.+?)\n\nHow will you respond\?$", re.S)


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t389.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


candidates = 0
interviews = 0
hires = 0
refusals = 0
for seed in range(1, 26):
    doc = run({"seed": seed, "country": "United States", "gender": "male", "ages": 55,
               "cash": 3000000, "zoo": True, "zoo_bank": 1500000, "only": "zoo-hire"})
    interviewed = False
    for event in doc["events"]:
        if event.get("table") != TABLE:
            continue
        body = event.get("body") or ""
        logs = event.get("logLines") or []
        if CARD.match(body):
            candidates += 1
            assert event.get("choice") in ("Hire them", "Ignore it"), (seed, event.get("choice"))
            if event.get("choice") == "Hire them":
                interviewed = True
            continue
        match = QUESTION.match(body)
        assert match, (seed, body)
        assert interviewed, (seed, "interview without a hiring answer")
        interviews += 1
        assert event.get("choice"), (seed, "the interview card offers no answer")
        assert any(line.startswith("I interviewed ") and " for the open " in line for line in logs), (seed, logs)
        if any(line.startswith("I hired ") for line in logs):
            hires += 1
        else:
            refusals += 1
            assert any("so there was no hire" in line for line in logs), (seed, logs)
assert candidates and interviews, (candidates, interviews)
assert hires and refusals, (hires, refusals)
print("candidates: %d | interviews: %d | hires: %d | refusals: %d" % (candidates, interviews, hires, refusals))
print("ok")
