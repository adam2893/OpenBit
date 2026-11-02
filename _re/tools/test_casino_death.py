#!/usr/bin/env python3
"""The casino's two death cards (REPORT.en.md 355).

A regular on the floor and the guest act in residence can both die of old age or of
natural causes. Whichever funeral answer is given, the card removes them from the
casino; attending the funeral is worth a point of karma, skipping it is worth nothing.

The native walks a persistent gambler list and asks each person's script for a death
age. The port keeps a count of regulars plus one lazily drawn regular (and one lazily
drawn guest-act age), so this test only checks the card surface and its effects.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TIERS = r"Beginner|Amateur|Novice|Regular|High Roller"

BODY = re.compile(
    r"^Your casino's (?:" + TIERS + r"), (?P<name>.+?), passed away at the age of "
    r"(?P<age>\d+)\.\n\n(?P<who>She|He) (?P<cause>died of old age|died of natural causes)\.$")
GAMBLER_LOG = re.compile(r"^(?P<name>.+?) died\.$")
CELEB_LOG = re.compile(r"^(?:She|He) died died (?:of old age|of natural causes)\.$")
ATTEND = re.compile(r"^Attend (?:her|his) funeral$")


def run(seed, only, guest="same", casino=True, ages=95):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 5000000, "only": only}
    if casino:
        spec["casino"] = True
    if guest:
        spec["casino_guest"] = guest
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    try:
        out = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                             capture_output=True, text=True, check=True).stdout
    finally:
        Path(path).unlink()
    return json.loads(out)


def deaths(doc):
    return [e for e in doc["events"]
            if e.get("title") == "Death" and "Casino" in (e.get("table") or "")]


def check(doc, label, log_re):
    cards = deaths(doc)
    for card in cards:
        m = BODY.match(card["body"])
        assert m, (label, card["body"])
        age = int(m.group("age"))
        who = m.group("who")
        cause = m.group("cause")
        assert age >= 55, (label, age)
        if age > 69:
            assert cause == "died of old age", (label, age, cause)
        else:
            assert cause == "died of natural causes", (label, age, cause)
        choice = card["choice"]
        assert choice == "Skip it" or ATTEND.match(choice), (label, choice)
        if choice != "Skip it":
            who2 = "She" if "her" in choice else "He"
            assert who2 == who, (label, who, choice)
        assert card["deltas"]["karma"] == (1 if choice != "Skip it" else 0), (label, card)
        assert any(log_re.match(line) for line in card["logLines"]), (label, card["logLines"])
    return cards


def main():
    gamblers = []
    for seed in range(1, 26):
        gamblers += check(run(seed, "casino-gambler-death"), "gambler", GAMBLER_LOG)
    celebs = []
    for seed in range(1, 26):
        celebs += check(run(seed, "casino-celeb-death"), "celeb", CELEB_LOG)

    for label, cards in (("gambler", gamblers), ("celeb", celebs)):
        assert cards, label
        lanes = {c["choice"] for c in cards}
        assert "Skip it" in lanes, (label, lanes)
        assert any(ATTEND.match(x) for x in lanes), (label, lanes)

    # A hero who owns no casino never sees either card, even when the family is pinned.
    for only in ("casino-gambler-death", "casino-celeb-death"):
        assert not deaths(run(1, only, casino=False)), only

    print("gambler cards: %d | celeb cards: %d | lanes: %s" % (
        len(gamblers), len(celebs),
        {"gambler": len({c["choice"] for c in gamblers}),
         "celeb": len({c["choice"] for c in celebs})}))
    print("ok")


if __name__ == "__main__":
    main()
