"""Row, gate, and carry regression for the friend family: the three unfriendcards (REPORT.en.md 108, 110, 111) and the weapon card (112)."""
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
# ScenarioFriendUnfriendPrison.RefreshScenario's five headlines (REPORT.en.md 110).
PRISON = ("Petty prison pal", "Limited options here", "Squad: -1", "Lonely behind bars",
          "See you later...")
BODY = re.compile(r"^Your friend, (.+?), has unfriended you\.$")
WIN_NAR = re.compile(r"^You successfully salvaged your friendship with your friend, (.+?)\.$")
LOST_NAR = re.compile(r"^You tried to salvage your friendship with (.+?) but were unsuccessful\.$")
GONE_NAR = re.compile(r"^(.+?) unfriended me\.$")
# ScenarioFriendUnfriendOutbound.RefreshScenario's five headlines (REPORT.en.md 111).
OUT = ("Begone", "They're too toxic", "Blocked", "Weeding 'em out", "Left on read")
OUT_BODY = re.compile(r"^You have unfriended your friend, (.+?)\.$")
OUT_NAR = re.compile(r"^I unfriended (.+?)\.$")
# AttributeAdjustmentPointsForImpactScore (src/sim.cpp pointsForImpact): -4 -> -30,
# -3 -> -16, +3 -> +16, so the card's raw scores land on those points.
SALVAGE = {"discipline": 16, "karma": 16}
LOSE = dict(SALVAGE, happiness=-30)
INSULT = {"discipline": -30, "karma": -16, "happiness": -30}

# ScenarioFriendWeaponEncounter (REPORT.en.md 112): the weapon card's four
# label-keyed branches, their reply pools and the two run outcomes.
WEAPONS = {row["Weapon"].strip(): row["Found in Prison"].strip()
           for row in csv.DictReader((ROOT / "gamedata_plain" / "a16_weapons-enc").open(newline=""))}
ACTIONS = {row["Past Verb"].strip()
           for row in csv.DictReader((ROOT / "gamedata_plain" / "a16_custom-attack-actions-enc").open(newline=""))}
REASON = ("Woah, take it easy", "It's me remember?", "This isn't you", "Whatcha got there...",
          "*nervous chuckle*")
FORGIVE = ("It's not worth it", "Think about your future", "I'm on my knees", "Spare me",
           "Let's use our words")
ARGUE = ("Uncalled for", "What's gotten into you", "Put that away", "Are you crazy?!")
GOT_AWAY = ("Dodge, duck, dip, dive, & dodge", "Too close for comfort", "Wild goose chase",
            "No fight, just flight", "Run. Don't stop. Go!")
CAUGHT = ("Back to the wall", "Out of options", "Cat & mouse", "Dead end", "Nowhere to go")
WSNEAK = re.compile(r"^While .+, you notice out of the corner of your eye your friend, (.+?), "
                    r"sneaking up on you with (.+)\.$")
WSTAND = re.compile(r"^While .+, you turn around to discover your friend, (.+?), standing there "
                    r"with (.+)\.$")
RUN_AWAY_NAR = re.compile(r"^You ran for your life and got away from (him|her)!$")
RUN_CAUGHT_NAR = re.compile(r"^You ran for your life but (he|she) caught you!$")

seen = collections.Counter()
for seed in range(1, 41):
    for gender in ("male", "female"):
        spec = {"seed": seed, "country": "United States", "gender": gender, "ages": 80}
        run = subprocess.run([str(PORT), "--script", "/dev/stdin"],
                             input=json.dumps(spec), text=True, capture_output=True,
                             check=True, cwd=ROOT / "port")
        ended = []
        for event in json.loads(run.stdout)["events"]:
            out_match = OUT_BODY.match(str(event.get("body") or ""))
            if out_match:
                # ScenarioFriendUnfriendOutbound (REPORT.en.md 111): the hero's own
                # half, queued by the ending this life already showed.
                seen["out"] += 1
                name = out_match.group(1)
                assert not event.get("table"), event
                assert event["age"] >= 12, event
                assert not event.get("choice"), event
                assert event["title"] in OUT, event
                narrative = event["logLines"][0]
                assert OUT_NAR.match(narrative) and OUT_NAR.match(narrative).group(1) == name, event
                deltas = {k: v for k, v in event["deltas"].items() if v}
                assert deltas == {"discipline": 8, "happiness": -4}, event
                assert any(age < event["age"] and friend == name for age, friend in ended), (event, ended)
                continue
            wm = WSNEAK.match(str(event.get("body") or "")) or WSTAND.match(
                str(event.get("body") or ""))
            if wm:
                # ScenarioFriendWeaponEncounter (REPORT.en.md 112): a friend walks
                # in with a weapon and the four buttons are keyed by their label.
                seen["weapon"] += 1
                name, armed = wm.group(1), wm.group(2)
                assert not event.get("table"), event
                # The class's own gate: hero age >= 18 and not in prison.
                assert event["age"] >= 18, event
                article, weapon_name = armed.split(" ", 1)
                assert article in ("a", "an") and weapon_name in WEAPONS, event
                # DataHub.GetRandomAssaultWeaponAndIsInPrison(false): the rows
                # flagged Found in Prison are out of the pool, and the article is
                # the card's own AddIndefiniteArticleToString.
                assert not WEAPONS[weapon_name], event
                assert (article == "an") == (weapon_name[0] in "aeiouAEIOU"), event
                choice = str(event.get("choice") or "")
                narrative = event["logLines"][0]
                deltas = {k: v for k, v in event["deltas"].items() if v}
                # The two labels that name the friend carry the friend's pronoun;
                # the other two leave it to the log line, so the line is read for it.
                if choice.startswith("Reason with "):
                    seen["weapon_reason"] += 1
                    said = re.match(r"^You attemped to reason with (him|her)!$", narrative)
                    assert said and choice == "Reason with " + said.group(1), event
                    assert event["title"] in REASON, event
                    assert deltas == {"happiness": -16, "discipline": 8}, event
                elif choice == "Beg for forgiveness":
                    seen["weapon_forgive"] += 1
                    assert re.match(r"^You begged (him|her) for forgiveness!$", narrative), event
                    assert event["title"] in FORGIVE, event
                    assert deltas == {"happiness": -50, "discipline": -16}, event
                elif choice.startswith("Argue with "):
                    seen["weapon_argue"] += 1
                    argued = re.match(r"^You (.+) (him|her)!$", narrative)
                    assert argued and choice == "Argue with " + argued.group(2), event
                    assert argued.group(1) in ACTIONS, event
                    assert event["title"] in ARGUE, event
                    assert deltas == {"happiness": -8, "discipline": -16}, event
                elif choice == "Run for my life":
                    away = RUN_AWAY_NAR.match(narrative)
                    caught = RUN_CAUGHT_NAR.match(narrative)
                    assert bool(away) != bool(caught), event
                    if away:
                        seen["weapon_run_away"] += 1
                        assert event["title"] in GOT_AWAY, event
                    else:
                        seen["weapon_run_caught"] += 1
                        assert event["title"] in CAUGHT, event
                    assert deltas == {"happiness": -30, "discipline": -16}, event
                else:
                    raise AssertionError(event)
                continue
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
            elif not choice:
                # ScenarioFriendUnfriendPrison (REPORT.en.md 110): the same body,
                # no buttons, one of five prison headlines, the default journal
                # line, and the hero's -4 impact.
                seen["prison"] += 1
                assert event["title"] in PRISON, event
                assert GONE_NAR.match(narrative) and GONE_NAR.match(narrative).group(1) == name, event
                assert deltas == {"happiness": -30}, event
            elif choice in ("Let him go", "Let her go"):
                seen["let_go"] += 1
                ended.append((event["age"], name))
                assert not event["title"], event
                assert GONE_NAR.match(narrative) and GONE_NAR.match(narrative).group(1) == name, event
                assert deltas == {"happiness": -30}, event
            elif choice in ("Insult him one last time", "Insult her one last time"):
                seen["insult"] += 1
                ended.append((event["age"], name))
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
assert seen["cards"] and seen["weapon"], seen
print("unfriend cards:", dict(sorted(seen.items())))
