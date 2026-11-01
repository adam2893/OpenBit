"""327: the agent's feedback card (ScenarioAgentCompliment).

The native card is dismiss-only: no buttons, and the journal entry is the card's own
first-person narrative. Its only effect is the relationship slot it stamps on the
agent, which the port carries as the agent's opinion of the hero. It is pushed by the
agent's relationship list and by a BluePrint, neither of which draws, so the port
offers it on the years the actor family is asked for a card, with a coin of its own.

Not asserted here: the opinion deltas. They are internal state the scripted run does
not print, so what a lane's band does is only checkable by reading the code beside
REPORT.en.md 327.
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

TITLES = {"A joy to be around", "There's that smile", "Good natured"}
CALLED = re.compile(r"^You called your agent, (?P<name>.+), (?P<what>.+)\.$")
TOLD = re.compile(r"^You told your agent, (?P<name>.+), that (?P<pron>he's|she's) (?P<what>.+)\.$")
THINK = re.compile(r"^You told your agent, (?P<name>.+), that you think (?P<pron>he's|she's) (?P<what>.+)\.$")
IGNORED = re.compile(r"\n\n(?P<pron>He|She) ignored you\.$")
MIRROR_CALLED = re.compile(r"^I called my agent, (?P<name>.+), (?P<what>.+)\.$")
MIRROR_TOLD = re.compile(r"^I told my agent, (?P<name>.+), that (?:he's|she's) (?P<what>.+)\.$")
BACK_CALLED = re.compile(r"^(?P<pron>He|She) called me (?P<what>.+)\.$")
BACK_TOLD = re.compile(r"^(?P<pron>He|She) told me that I'm (?P<what>.+)\.$")


def run(seed, ages=80, actor=True):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    if actor:
        spec["actor"] = True
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-400:]
    return json.loads(proc.stdout)


def cards(doc):
    out = []
    for e in doc["events"]:
        body = e.get("body") or ""
        if body.startswith("You called your agent, ") or body.startswith("You told your agent, "):
            out.append(e)
    return out


def name_genders():
    with open(DATA / "a16_casino-casino-celebrity-name-enc", newline="") as fh:
        return {r["First Name"] + " " + r["Last Name"]: r["Gender"] for r in csv.DictReader(fh)}


def compliments():
    with open(DATA / "a16_compliments-enc", newline="") as fh:
        return [(r["Compliment"], r["Gender"]) for r in csv.DictReader(fh)]


def fits(what, gender, pool):
    """A compliment the card could have drawn for that gender: male, female or both."""
    hits = [(i + 1, c) for i, (c, g) in enumerate(pool) if c == what]
    assert hits, f"{what!r} is not in a16_compliments-enc"
    return [row for row, _ in hits if pool[row - 1][1] in (gender, "both", "")]


def main():
    # A hero who never acts has no agent and cannot draw the card.
    assert cards(run(26, actor=False)) == []

    pool = compliments()
    genders = name_genders()
    seen = 0
    lanes = {"called": 0, "told": 0, "think": 0, "back": 0, "ignored": 0}
    for seed in range(1, 81):
        for e in cards(run(seed)):
            seen += 1
            body = e["body"]
            assert e["choice"] is None, e
            assert e["table"] == "a16_compliments-enc", e
            assert e["title"] in TITLES, e
            assert e["age"] >= 18, e
            assert e["deltas"] == {"karma": 0, "discipline": 0, "happiness": 0, "health": 0,
                                   "smarts": 0, "looks": 0, "fame": 0}, e
            # The ignored line is appended as its own paragraph, so the sentence the
            # regexes below describe is the body up to the first blank line.
            sentence = body.split("\n\n")[0]
            m = CALLED.match(sentence) or TOLD.match(sentence) or THINK.match(sentence)
            assert m, body
            lanes["called" if body.startswith("You called") else
                  ("think" if "that you think" in body else "told")] += 1
            name = m.group("name")
            assert name in genders, f"{name!r} is not a celebrity-name row"
            gender = genders[name]
            if "pron" in m.groupdict():
                assert m.group("pron") == ("she's" if gender == "female" else "he's"), (body, gender)
            # The recorded row is the compliment the card called the agent, 1-based.
            what = m.group("what")
            rows = fits(what, gender, pool)
            assert int(e["row"]) in rows, (e["row"], rows, what)
            ignored = IGNORED.search(body)
            if ignored:
                lanes["ignored"] += 1
                assert ignored.group("pron") == ("She" if gender == "female" else "He"), body
            # The journal is the card's narrative: the mirror of the body, or the
            # agent's return compliment (which replaces it).
            lines = [l for l in e["logLines"]
                     if MIRROR_CALLED.match(l) or MIRROR_TOLD.match(l)
                     or BACK_CALLED.match(l) or BACK_TOLD.match(l)]
            assert len(lines) == 1, (body, e["logLines"])
            line = lines[0]
            mirror = MIRROR_CALLED.match(line) or MIRROR_TOLD.match(line)
            if mirror:
                assert mirror.group("name") == name, (line, name)
                assert mirror.group("what") == what, (line, what)
            else:
                lanes["back"] += 1
                back = BACK_CALLED.match(line) or BACK_TOLD.match(line)
                assert back.group("pron") == ("She" if gender == "female" else "He"), (line, gender)
                # The return compliment describes the hero, so it is filtered on his gender.
                assert int(e["row"]) != 0
                assert [g for c, g in pool if c == back.group("what")], line
    assert seen >= 10, f"only {seen} cards over 80 acting lives"
    assert lanes["called"] and lanes["told"] and lanes["think"], lanes
    assert lanes["back"], lanes
    print(f"cards {seen} | lanes {lanes}")
    print("ok")


if __name__ == "__main__":
    main()
