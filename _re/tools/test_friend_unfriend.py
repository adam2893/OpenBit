"""Row, gate, and carry regression for the unfriend card (REPORT.en.md 108)."""
import collections
import csv
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
INSULTS = list(csv.DictReader((ROOT / "gamedata_plain" / "a16_insults-enc").open(newline="")))
BY_INSULT = {row["Insult"].strip(): row["Gender"].strip().lower() for row in INSULTS}
WIN = ("Under the bridge", "Beef behind us", "Forgiven", "Can't shake me",
       "This isn't Facebook")
LOST = ("Bye then", "Unfriended", "It's not me it's you", "Done for good",
        "Stale mate", "So long, partner")
BAD = ("Nail in the coffin", "Done for good", "Irreparable", "No going back")
BODY = re.compile(r"^Your friend, (.+?), has unfriended you\.$")
WIN_NAR = re.compile(r"^You successfully salvaged your friendship with your friend, (.+?)\.$")
LOST_NAR = re.compile(r"^You tried to salvage your friendship with (.+?) but were unsuccessful\.$")
GONE_NAR = re.compile(r"^(.+?) unfriended me\.$")
# AttributeAdjustmentPointsForImpactScore (src/sim.cpp pointsForImpact): -4 -> -30,
# -3 -> -16, +3 -> +16, so the card's raw scores land on those points.
SALVAGE = {"discipline": 16, "karma": 16}
LOSE = dict(SALVAGE, happiness=-30)
INSULT = {"discipline": -30, "karma": -16, "happiness": -30}

seen = collections.Counter()
for seed in range(1, 41):
    for gender in ("male", "female"):
        spec = {"seed": seed, "country": "United States", "gender": gender, "ages": 80}
        run = subprocess.run([str(PORT), "--script", "/dev/stdin"],
                             input=json.dumps(spec), text=True, capture_output=True,
                             check=True, cwd=ROOT / "port")
        for event in json.loads(run.stdout)["events"]:
            match = BODY.match(str(event.get("body") or ""))
            if not match:
                continue
            seen["cards"] += 1
            name = match.group(1)
            assert not event.get("table"), event
            # The card's own gate reads the friend's age (>= 12) and the port's
            # drawn friend has none, so the hero's age carries the floor
            # (REPORT.en.md 108, corrected in 109).
            assert event["age"] >= 12, event
            choice = str(event.get("choice") or "")
            narrative = event["logLines"][0]
            deltas = {k: v for k, v in event["deltas"].items() if v}
            if choice == "Try to salvage our friendship":
                assert event["title"] in WIN + LOST, event
                if event["title"] in WIN:
                    seen["salvage_win"] += 1
                    assert WIN_NAR.match(narrative) and WIN_NAR.match(narrative).group(1) == name, event
                    assert deltas == SALVAGE, event
                else:
                    seen["salvage_lost"] += 1
                    assert LOST_NAR.match(narrative) and LOST_NAR.match(narrative).group(1) == name, event
                    assert deltas == LOSE, event
            elif choice in ("Let him go", "Let her go"):
                seen["let_go"] += 1
                assert not event["title"], event
                assert GONE_NAR.match(narrative) and GONE_NAR.match(narrative).group(1) == name, event
                assert deltas == {"happiness": -30}, event
            elif choice in ("Insult him one last time", "Insult her one last time"):
                seen["insult"] += 1
                assert event["title"] in BAD, event
                # The insult sentence names the friend, so its own name is the
                # anchor: the friend's name can hold a space ('Seo Taji').
                match2 = re.match(r"^You called " + re.escape(name) +
                                  r" (.+?) as (he|she) was walking out the door\.$", narrative)
                assert match2, event
                word, pronoun = match2.group(1), match2.group(2)
                assert word in BY_INSULT, (word, narrative)
                # SimPerson.GetRandomInsultForGender reads a16_insults-enc by the
                # friend's gender, and the sentence's own pronoun is the friend's,
                # so a gendered insult row and the pronoun agree.
                insult_gender = BY_INSULT[word]
                assert insult_gender in ("", "both", "all", "any", "male", "female"), narrative
                assert (pronoun == "she") == (choice.split()[1] == "her"), event
                if insult_gender == "female":
                    assert pronoun == "she", event
                if insult_gender == "male":
                    assert pronoun == "he", event
                assert deltas == INSULT, event
            else:
                raise AssertionError(event)
assert seen["cards"], seen
print("unfriend cards:", dict(sorted(seen.items())))
