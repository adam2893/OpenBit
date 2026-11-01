"""328: the zoo keeper's compliment card (ScenarioZooEmployeeComplimentFeedback).

The native card is opened from the keeper's entry in the zoo's employee list, not pushed by a
yearly set, so the port offers it on the years the zoo family is asked for a card, on a coin of
its own. It is dismiss-only: the text is the effect, plus a relationship band on the keeper and a
competence nudge.

Not asserted here: the keeper's relationship and competence after the card. They are internal
state the scripted run does not print, so the band and the ImpactCompetence call are only
checkable by reading the code beside REPORT.en.md 328.
"""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"

TITLES = {"That's nice", "So sweet", "True love", "Big fan"}
CALLED = re.compile(r"^You called your employee, (?P<name>.+), (?P<what>.+)\.$")
TOLD = re.compile(r"^You told your employee, (?P<name>.+), that (?P<pron>he's|she's) (?P<what>.+)\.$")
THINK = re.compile(r"^You told your employee, (?P<name>.+), that you think (?P<pron>he's|she's) (?P<what>.+)\.$")
BACK_CALLED = re.compile(r"^Your employee, (?P<name>.+), called you (?P<what>.+)!$")
BACK_TOLD = re.compile(r"^Your employee, (?P<name>.+), told you that you're (?P<what>.+)!$")
MIRROR_CALLED = re.compile(r"^I called my employee, (?P<name>.+), (?P<what>.+)\.$")
MIRROR_TOLD = re.compile(r"^I told my employee, (?P<name>.+), that (?:he's|she's) (?P<what>.+)\.$")
BACK_LINE = re.compile(r"^(?P<pron>He|She) (?:called me|told me that I'm) (?P<what>.+)\.$")


def run(seed, ages=80, zoo=True):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    if zoo:
        spec["zoo"] = True
        spec["only"] = "zoo"
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-400:]
    return json.loads(proc.stdout)


def cards(doc):
    return [e for e in doc["events"]
            if e.get("title") in TITLES and (e.get("body") or "").startswith("You ")]


def compliments():
    with open(DATA / "a16_compliments-enc", newline="") as fh:
        return [(r["Compliment"], r["Gender"]) for r in csv.DictReader(fh)]


def rows_for(what, gender, pool):
    """The rows the keeper's compliment could have come from: its gender or anybody's."""
    hits = [(i + 1, g) for i, (c, g) in enumerate(pool) if c == what]
    assert hits, f"{what!r} is not in a16_compliments-enc"
    return [row for row, g in hits if g in (gender, "both", "")]


def main():
    # No zoo, no keeper.
    assert cards(run(3, zoo=False)) == []

    pool = compliments()
    seen = 0
    lanes = {"called": 0, "told": 0, "think": 0, "back_called": 0, "back_told": 0}
    for seed in range(1, 41):
        for e in cards(run(seed)):
            seen += 1
            body = e["body"]
            assert e["choice"] is None, e
            assert e["table"] == "a16_compliments-enc", e
            assert e["age"] >= 18, e
            assert e["deltas"] == {"karma": 0, "discipline": 0, "happiness": 0, "health": 0,
                                   "smarts": 0, "looks": 0, "fame": 0}, e
            first, _, second = body.partition("\n\n")
            m = CALLED.match(first) or TOLD.match(first) or THINK.match(first)
            assert m, body
            lanes["called" if first.startswith("You called") else
                  ("think" if "that you think" in first else "told")] += 1
            name, what = m.group("name"), m.group("what")
            # The "called" frame carries no pronoun of its own, so the keeper's gender is read
            # from the return line below and cross-checked against the frame when it has one.
            pron = m.group("pron") if "pron" in m.groupdict() else None
            # The compliment the keeper returns describes the hero, and it is always there:
            # the only thing that suppresses it is the year cache, and the port can hold only
            # one card a year.
            b = BACK_CALLED.match(second) or BACK_TOLD.match(second)
            assert b, body
            assert b.group("name") == name, body
            lanes["back_called" if second.startswith("Your employee, " + name + ", called")
                  else "back_told"] += 1
            # Its journal line must be a row open to the hero's gender (the run is a man).
            assert [g for c, g in pool if c == b.group("what") and g in ("male", "both", "")], \
                (b.group("what"), "not a male compliment")
            # The journal carries both lines of the card, joined by a newline (#69343).
            lines = [l for l in e["logLines"] if "\n" in l and "my employee" in l]
            assert len(lines) == 1, (body, e["logLines"])
            mine, theirs = lines[0].split("\n")
            mirror = MIRROR_CALLED.match(mine) or MIRROR_TOLD.match(mine)
            assert mirror and mirror.group("name") == name, (mine, name)
            assert mirror.group("what") == what, (mine, what)
            back = BACK_LINE.match(theirs)
            assert back, theirs
            gender = "female" if back.group("pron") == "She" else "male"
            if pron is not None:
                assert pron == ("she's" if gender == "female" else "he's"), (body, theirs)
            # The compliment the hero pays is a row of the table open to the keeper's gender.
            assert int(e["row"]) in rows_for(what, gender, pool), (e["row"], what, gender)
    assert seen >= 10, f"only {seen} keeper cards over 40 zoo lives"
    assert lanes["called"] and lanes["told"] and lanes["think"], lanes
    assert lanes["back_called"] and lanes["back_told"], lanes
    print(f"cards {seen} | lanes {lanes}")
    print("ok")


if __name__ == "__main__":
    main()
