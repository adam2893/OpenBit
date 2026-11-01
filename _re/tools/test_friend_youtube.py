#!/usr/bin/env python3
"""The friend's YouTube card (REPORT.en.md 341).

ScenarioFriendYouTube is a relationship-menu activity: the game offers it for any friend
who lives in the same place once the hero is six, the player picks a channel from a
dropdown of up to five distinct names, and the single green choice then accepts or
refuses by threshold -- the friend's stored relationship strength over 20.0, or a channel
whose EnjoymentScore is 5. The port draws the channel from the same table, draws the
friend's strength on the card's own stream and folds the follow-up card into the body.
"""
import csv
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLE = "a16_youtube-channels-enc"

with open(ROOT / "gamedata_plain" / TABLE, newline="", encoding="utf-8") as fh:
    rows = list(csv.reader(fh))
HEADER, DATA = rows[0], rows[1:]
assert HEADER == ["Channel", "Commonness Score", "Enjoyment Score", "Gender"], HEADER

MALE = {"He's great!", "His editing is clean", "Man's a legend", "That's my guy",
        "I need his merch"}
FEMALE = {"She's great!", "Love her style", "She's hilarious", "She has a stan in me",
          "I need her merch"}
BODY = re.compile(r"^Watch YouTube with your friend, (?P<name>.+), today!$")
ACCEPT_LOG = re.compile(r"^I watched (?P<ch>.+) on YouTube with my friend, (?P<name>.+?)\.$")
REFUSE_LOG = re.compile(r"^My friend, (?P<name>.+), refused to watch (?P<ch>.+) with me on "
                        r"YouTube\.$")
CHOICE = re.compile(r"^Watch it with (him|her)$")
# The accept moves happiness +1 and the refusal -2 raw points through the port's impact
# conversion (REPORT 341); both are whole points on the year's delta.
PTS_ACCEPT = 4
PTS_REFUSE = -8


def run(seed, ages=40, cash=5000):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": cash, "only": "friend-youtube"}
    fd, path = tempfile.mkstemp(suffix=".json")
    os.write(fd, json.dumps(spec).encode())
    os.close(fd)
    try:
        out = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                             capture_output=True, text=True)
    finally:
        os.unlink(path)
    assert out.returncode == 0, out.stderr[-800:]
    return json.loads(out.stdout[out.stdout.find("{"):])


cards = accepts = refuses = 0
for seed in range(1, 41):
    seen = set()
    for event in run(seed)["events"]:
        if event.get("table") != TABLE:
            continue
        cards += 1
        age = event["age"]
        assert age >= 6, (seed, age)
        assert age not in seen, (seed, age)
        seen.add(age)
        channel, _, enjoyment, gender = DATA[int(event["row"]) - 1]
        body = event["body"]
        delta = event["deltas"]
        assert not event.get("residue"), (seed, event.get("residue"))
        assert event["title"] == "YouTube", (seed, event["title"])
        head = BODY.match(body.split("\n\n")[0])
        assert head, (seed, body)
        name = head.group("name")
        assert CHOICE.match(event.get("choice") or ""), (seed, event.get("choice"))
        (log,) = [l for l in event["logLines"] if "YouTube" in l]
        if "together on YouTube" in body:
            accepts += 1
            # Body: the card, then the folded follow-up card as its title line (the
            # gender-flavoured quip) and its #47624 body one newline below.
            quip, follow = body.split("\n\n")[1].split("\n")
            assert quip in (FEMALE if gender == "female" else MALE), (seed, channel, quip)
            assert follow == ("You and your friend, " + name + ", watched " + channel +
                              " together on YouTube."), (seed, follow)
            m = ACCEPT_LOG.match(log)
            assert m and m.group("ch") == channel and m.group("name") == name, (seed, log)
            assert delta["happiness"] == PTS_ACCEPT, (seed, delta)
        else:
            refuses += 1
            quip, follow = body.split("\n\n")[1].split("\n")
            assert quip == "Seriously?" and "refused to watch" in follow, (seed, body)
            assert follow == ("Your friend, " + name + ", refused to watch " + channel +
                              " with you on YouTube."), (seed, follow)
            m = REFUSE_LOG.match(log)
            assert m and m.group("ch") == channel and m.group("name") == name, (seed, log)
            assert delta["happiness"] == PTS_REFUSE, (seed, delta)

# The menu item's only gate is the hero's sixth birthday: a five-year-old never sees it.
assert not [e for e in run(7, ages=5)["events"] if e.get("table") == TABLE], "minor card"
assert cards >= 20, cards
assert accepts >= 5, accepts
assert refuses >= 2, refuses
print("cards:", cards, "| accepts:", accepts, "| refuses:", refuses)
print("ok")
