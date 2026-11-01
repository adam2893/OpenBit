#!/usr/bin/env python3
"""The employee's feedback card (REPORT.en.md 329).

Sim::employeeComplimentYear is offered from the office family: a manager's year is sometimes the
named employee's compliment card instead of a work card. The card is dismiss-only, draws its
compliment from a16_compliments-enc filtered on the employee's gender, and can be answered by the
employee in the same journal entry.
"""
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path("/Users/Adam/Documents/bitlife/_re")
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"

TITLES = {"That's nice", "So sweet", "True love", "Big fan"}
FRAME_CALLED = re.compile(r"^You called your employee, (?P<name>.+), (?P<what>.+)\.$")
FRAME_TOLD = re.compile(
    r"^You told your employee, (?P<name>.+), that (?P<pron>he's|she's) (?P<what>.+)\.$"
)
FRAME_THINK = re.compile(
    r"^You told your employee, (?P<name>.+), that you think (?P<pron>he's|she's) (?P<what>.+)\.$"
)
BACK_CALLED = re.compile(r"^Your employee, (?P<name>.+), called you (?P<what>.+)!$")
BACK_TOLD = re.compile(r"^Your employee, (?P<name>.+), told you that you're (?P<what>.+)!$")
MIRROR_CALLED = re.compile(r"^I called my employee, (?P<name>.+), (?P<what>.+)\.$")
MIRROR_TOLD = re.compile(
    r"^I told my employee, (?P<name>.+), that (?P<pron>he's|she's) (?P<what>.+)\.$"
)
BACK_LINE = re.compile(r"^(?P<pron>He|She) (?:called me|told me that I'm) (?P<what>.+)\.$")


def run(seed, ages=80, office=True):
    spec = {
        "seed": seed,
        "country": "United States",
        "gender": "male",
        "ages": ages,
        "office": office,
    }
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


def cards(events):
    return [
        e for e in events if e.get("title") in TITLES and (e.get("body") or "").startswith("You ")
    ]


def compliment_rows():
    """(row index 1-based, compliment, gender) for a16_compliments-enc."""
    path = DATA / "a16_compliments-enc"
    lines = path.read_text(encoding="utf-8").splitlines()
    head = [c.strip() for c in lines[0].split(",")]
    ci, gi = head.index("Compliment"), head.index("Gender")
    rows = []
    for i, line in enumerate(lines[1:], start=1):
        cells = line.split(",")
        rows.append((i, cells[ci].strip(), cells[gi].strip() if len(cells) > gi else ""))
    return rows


ROWS = compliment_rows()


def opens_for(compliment, gender):
    want = "female" if gender == "female" else "male"
    return [i for (i, text, g) in ROWS if text == compliment and g in ("", "both", want)]


def main():
    assert cards(run(3, office=False)) == [], "a hero with no office drew an employee card"
    seen = 0
    lanes = {"called": 0, "told": 0, "think": 0, "back_called": 0, "back_told": 0}
    for seed in range(1, 51):
        events = run(seed)
        for event in cards(events):
            seen += 1
            body = event.get("body") or ""
            assert event.get("choice") is None, event
            assert event.get("table") == "a16_compliments-enc", event
            assert event.get("age") >= 18, event
            deltas = event.get("deltas") or {}
            for key, value in deltas.items():
                assert value == 0, (key, value, event)
            first, _, rest = body.partition("\n\n")
            gender = None
            if FRAME_CALLED.match(first):
                lanes["called"] += 1
                match = FRAME_CALLED.match(first)
            elif FRAME_THINK.match(first):
                lanes["think"] += 1
                match = FRAME_THINK.match(first)
                gender = "female" if match.group("pron") == "she's" else "male"
            else:
                match = FRAME_TOLD.match(first)
                assert match, first
                lanes["told"] += 1
                gender = "female" if match.group("pron") == "she's" else "male"
            name = match.group("name")
            # The row the card reports must be open to the employee's gender.
            logged = "\n".join(event.get("logLines") or [])
            back_pron = None
            back_what = None
            if rest:
                back = BACK_CALLED.match(rest) or BACK_TOLD.match(rest)
                assert back, rest
                assert back.group("name") == name, (back.group("name"), name)
                lanes["back_called" if BACK_CALLED.match(rest) else "back_told"] += 1
                back_what = back.group("what")
            line = None
            entry = None
            for candidate in (event.get("logLines") or []):
                parts = candidate.split(chr(10))
                hit = MIRROR_CALLED.match(parts[0]) or MIRROR_TOLD.match(parts[0])
                if hit:
                    line = hit
                    entry = parts
                    break
            assert line is not None, (event.get("logLines"), body)
            assert line.group("name") == name, (line.group("name"), name)
            if back_what is not None:
                assert len(entry) == 2, entry
                hit = BACK_LINE.match(entry[1])
                assert hit, entry
                back_pron = hit.group("pron")
            if back_what is not None:
                assert back_pron is not None, event.get("logLines")
                back_gender = "female" if back_pron == "She" else "male"
                if gender is not None:
                    assert back_gender == gender, (gender, back_gender, event.get("logLines"))
                gender = back_gender
            if gender is not None:
                row = int(event["row"])
                assert row in opens_for(match.group("what"), gender), (
                    row,
                    match.group("what"),
                    gender,
                )
            if back_what is not None:
                row2 = None
                assert back_what, rest
    assert seen >= 8, "only %d employee cards over 50 office lives" % seen
    assert all(lanes[key] for key in ("called", "told", "think")), lanes
    print("cards %d | lanes %s" % (seen, lanes))
    print("ok")


if __name__ == "__main__":
    main()
