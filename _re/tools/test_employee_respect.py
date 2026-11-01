#!/usr/bin/env python3
"""The employee respect card and the three cards its buttons chain (REPORT.en.md 330).

ScenarioEmployeeRespect is pushed by the business annual report every year, so the port offers it
from the office family's turns. Its four buttons are the native ones: give a bonus (chains the
bonus card), recognize (chains the recognition card), warn (chains the warning card), take no
action (no card, karma -1, and the employee's respect and morale drop).
"""
import csv
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path("/Users/Adam/Documents/bitlife/_re")
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"

RESPECT_TITLE = "Respect"
BONUS_TITLE = "Bonus"
RECOGNIZE_TITLES = {
    "Employee spotlight!",
    "Much deserved recognition",
    "Everyone take notes",
    "Star team member",
    "Keep it up!",
}
WARN_TITLES = {"Playing hardball", "Make my day", "You ready to go?!", "I WILL end you", "My turn"}
BODY_EMPLOYEE = re.compile(r"(?i)your employee, (?P<name>.+?),")
IGNORED = re.compile(r"\nI ignored (him|her)\.$")


def run(seed, ages=80, office=True, choices=None):
    spec = {
        "seed": seed,
        "country": "United States",
        "gender": "male",
        "ages": ages,
        "office": office,
    }
    if choices:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    try:
        proc = subprocess.run(
            [str(PORT), "--script", path], cwd=str(ROOT / "port"), capture_output=True, text=True
        )
    finally:
        os.unlink(path)
    assert proc.returncode == 0, proc.stderr[-600:]
    return json.loads(proc.stdout)["events"]


ROWS = {}
with (DATA / "a16_employee-respect-scenarios-enc").open(encoding="utf-8", newline="") as fh:
    reader = csv.reader(fh)
    head = [c.strip() for c in next(reader)]
    for cells in reader:
        if len(cells) < len(head):
            continue
        row = dict(zip(head, [c.strip() for c in cells]))
        ROWS[len(ROWS) + 1] = {
            "scenario": row["Scenario"],
            "log": row["Log Version"],
            "reaction": row["Can Be Reaction"].lower(),
            "push": row["Can Be Push"].lower(),
            "score": row["Score"],
        }


def respect_cards(events):
    return [e for e in events if e.get("title") == RESPECT_TITLE]


def main():
    assert respect_cards(run(3, office=False)) == [], "a hero with no office drew a respect card"

    seen = 0
    chain = {"bonus": 0, "recognize": 0, "warn": 0}
    for seed in range(1, 26):
        events = run(seed)
        for event in respect_cards(events):
            seen += 1
            body = event.get("body") or ""
            assert event.get("table") == "a16_employee-respect-scenarios-enc", event
            assert event.get("age") >= 18, event
            hit = BODY_EMPLOYEE.search(body)
            assert hit, body
            name = hit.group("name")
            row = int(event["row"])
            cells = ROWS[row]
            assert cells["push"] == "x", cells
            assert cells["score"] in {"1", "2", "3", "4", "5"}, cells
            joined = "\n".join(event.get("logLines") or [])
            assert name.lower() in joined.lower(), joined
            for later in events:
                if later is event or later.get("age") != event["age"]:
                    continue
                title = later.get("title")
                if title in RECOGNIZE_TITLES:
                    chain["recognize"] += 1
                elif title in WARN_TITLES:
                    chain["warn"] += 1
                elif title == BONUS_TITLE:
                    chain["bonus"] += 1

    assert seen >= 8, "only %d respect cards over 25 office lives" % seen
    assert all(chain[key] for key in chain), chain

    # The ignore lane leaves no card behind, only the line and the karma point.
    ignored = 0
    for seed in range(1, 26):
        for event in respect_cards(run(seed)):
            joined = "\n".join(event.get("logLines") or [])
            if IGNORED.search(joined):
                ignored += 1
                deltas = event.get("deltas") or {}
                assert deltas.get("karma", 0) == -1, deltas
    assert ignored, "no respect card took the ignore lane over 25 lives"

    # A bonus card pays out and writes the amount into its journal.
    paid = 0
    for seed in range(1, 26):
        for event in run(seed):
            if event.get("title") == BONUS_TITLE:
                if "a bonus in the amount of $" in "\n".join(event.get("logLines") or []):
                    paid += 1
    assert paid, "no bonus card paid out"
    print(
        "respect cards %d | chain %s | bonuses paid %d | ignored %d"
        % (seen, chain, paid, ignored)
    )
    print("ok")


if __name__ == "__main__":
    main()
