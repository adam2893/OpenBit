#!/usr/bin/env python3
"""The identity card that follows a divorce (REPORT.en.md 397).

ScenarioNameChangeDivorce shares the marriage card's title, row and honourific pool
but asks the opposite question: with the marriage over, does the hero take the old
surname back? The card is only offered when the wedding card actually changed the
name, so the drive below must marry, change the name, and then divorce.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "port", "build", "blsim")
TABLE = "registry:ScenarioNameChangeDivorce"
BODY = re.compile(
    r"^Now that you're divorced, you're thinking about changing your name back\.\n\n"
    r"What will you do\?\n\nName: (?P<name>.+)$")


def run(spec):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    try:
        out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    finally:
        os.unlink(path)
    if out.returncode != 0:
        raise SystemExit("blsim failed: " + out.stderr[-400:])
    return json.loads(out.stdout)


def cards(seed):
    data = run({"seed": seed, "country": "United States", "gender": "female",
                "ages": 90, "cash": 300000, "engaged": True})
    return [e for e in data["events"] if (e.get("table") or "") == TABLE]


def main():
    lanes = {}
    shapes = 0
    for seed in range(1, 201):
        for ev in cards(seed):
            m = BODY.match(ev["body"])
            assert m, ev["body"]
            assert ev["title"] == "Identity", ev["title"]
            choice = ev["choice"] or ""
            key = choice.split(" ")[0]
            assert key in ("Go", "Stay"), choice
            # The button quotes the same full name the row shows.
            name = m.group("name")
            # Stay as {0} {1} quotes the name the row shows; Go back to {0} {1}
            # quotes the maiden name, which the row cannot show.
            if key == "Stay":
                assert choice == "Stay as " + name, (choice, name)
            else:
                assert choice.startswith("Go back to "), choice
            lanes[key] = lanes.get(key, 0) + 1
            # logLines carry every journal line written that year; the card's own
            # line is the last one.
            logs = ev.get("logLines") or []
            assert logs, ev
            if key == "Go":
                back = choice[len("Go back to "):]
                assert logs[-1] == "I went back to the name " + back + ".", logs
            else:
                assert logs[-1] == "I stayed " + name + ".", logs
            shapes += 1
    assert lanes, "no divorce cards over 200 married lives"
    assert set(lanes) == {"Go", "Stay"}, lanes
    # A hero who never married can never be offered the card.
    for seed in range(1, 21):
        data = run({"seed": seed, "country": "United States", "gender": "male",
                    "ages": 80, "cash": 200000})
        assert not [e for e in data["events"] if (e.get("table") or "") == TABLE], seed
    print("cards: {}".format(shapes), "| lanes:", dict(sorted(lanes.items())))
    print("ok")


if __name__ == "__main__":
    main()