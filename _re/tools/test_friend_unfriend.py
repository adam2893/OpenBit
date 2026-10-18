"""Row, gate, and carry regression for the friend family: the three unfriend
cards (REPORT.en.md 108, 110, 111), the weapon card (112), the assault it links
(113) and the threesome (114)."""
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

# ScenarioAttackInbound (REPORT.en.md 113): the assault card the weapon card's
# shared tail links, carrying the encounter's friend and weapon. The sentence is
# '{0} {1} you{2}!' with {1} the weapon's own Verb column and {2} its article form.
WEAPON_VERBS = {row["Weapon"].strip(): row["Verb"].strip()
                for row in csv.DictReader((ROOT / "gamedata_plain" / "a16_weapons-enc").open(newline=""))}
ATTACK_BODY = re.compile(r"^Your friend, (.+?), (.+?) you with (.+)!$")
# ScenarioFriendThreesome (REPORT.en.md 114): the family's one adult card and a
# real random push. Its body names the hero's own partner (when it has one), then
# the friend, then the third person the class generated for the pair.
THREESOME_BODY = re.compile(
    r"^(?:Even though you (.+?), you|You) and your friend, (.+?), have an opportunity to have "
    r"a threesome with (a|an) (.+?) named (.+)[.]$")
THIRD_FEMALE = ("little girl", "girl", "young woman", "woman")
THIRD_MALE = ("little boy", "boy", "young man", "man")
THREESOME_DECLINE = re.compile(
    r"^I turned down an opportunity to have a threesome with my friend, (.+?), and (.+)[.]$")
THREESOME_ACCEPT = re.compile(
    r"^My friend, (.+?), and I had a threesome with (.+)[.]$")

seen = collections.Counter()
for seed in range(1, 41):
    for gender in ("male", "female"):
        spec = {"seed": seed, "country": "United States", "gender": gender, "ages": 80}
        run = subprocess.run([str(PORT), "--script", "/dev/stdin"],
                             input=json.dumps(spec), text=True, capture_output=True,
                             check=True, cwd=ROOT / "port")
        ended = []
        armed_ended = []
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
                narrative = event["logLines"][-1]
                assert OUT_NAR.match(narrative) and OUT_NAR.match(narrative).group(1) == name, event
                deltas = {k: v for k, v in event["deltas"].items() if v}
                assert deltas == {"discipline": 8, "happiness": -4}, event
                assert any(age < event["age"] and friend == name for age, friend in ended), (event, ended)
                continue
            tm = THREESOME_BODY.match(str(event.get("body") or ""))
            if tm:
                # ScenarioFriendThreesome (REPORT.en.md 114): the hero and a friend
                # are offered a third person the class generates on the spot.
                seen["threesome"] += 1
                assert not event.get("table"), event
                # The class's own gate: hero age >= 18, out of prison, a friend.
                assert event["age"] >= 18, event
                partner, name, article, term, third = tm.groups()
                # {3} is GetGenericTermForPersonOfGenderAndAge over a person the
                # card drew at eighteen plus 0..9, so the 18-29 bucket's words are
                # the ones the port carries, behind the card's own article helper.
                assert term in THIRD_FEMALE + THIRD_MALE, event
                assert (article == "an") == (term[0] in "aeiou"), event
                assert name and third and name != third, event
                # PersonGenerator was handed the sex opposite the hero's own
                # EffectiveGender, and the test knows the hero's from its spec.
                assert (term in THIRD_FEMALE) == (gender == "male"), event
                if partner:
                    # The prefix half is the hero's own lover, read off
                    # SimLover.RelationshipStatusString's three cells.
                    assert re.fullmatch(r"(are (married|engaged) to .+|have a "
                                        r"(girlfriend|boyfriend))", partner), event
                choice = str(event.get("choice") or "")
                narrative = event["logLines"][-1]
                deltas = {k: v for k, v in event["deltas"].items() if v}
                assert event["title"] == "Threesome", event
                if choice == "No way!":
                    seen["threesome_decline"] += 1
                    said = THREESOME_DECLINE.match(narrative)
                    assert said and said.groups() == (name, third), event
                    # ImpactDiscipline(3) over the hero, on top of the friend's own
                    # -2 relationship move, which the port does not model.
                    assert deltas == {"discipline": 16}, event
                elif choice == "I'm in":
                    seen["threesome_accept"] += 1
                    said = THREESOME_ACCEPT.match(narrative)
                    assert said and said.groups() == (name, third), event
                    # ImpactKarma(-4) and ImpactDiscipline(-4).
                    assert deltas == {"karma": -30, "discipline": -30}, event
                else:
                    raise AssertionError(event)
                continue
            wm = WSNEAK.match(str(event.get("body") or "")) or WSTAND.match(
                str(event.get("body") or ""))
            if wm:
                # ScenarioFriendWeaponEncounter (REPORT.en.md 112): a friend walks
                # in with a weapon and the four buttons are keyed by their label.
                seen["weapon"] += 1
                name, armed = wm.group(1), wm.group(2)
                armed_ended.append((event["age"], name))
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
                narrative = event["logLines"][-1]
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
            # ScenarioAttackInbound (REPORT.en.md 113): the assault the weapon
            # card's shared tail links, queued by the encounter and shown on the
            # next age-up with the friend and the weapon the encounter drew.
            am = ATTACK_BODY.match(str(event.get("body") or ""))
            if am:
                seen["attack"] += 1
                name, verb, armed = am.group(1), am.group(2), am.group(3)
                # The encounter hands both halves over, so the assault names a
                # friend this life already saw walk in armed.
                assert any(friend == name for _, friend in armed_ended), (event, armed_ended)
                # The encounter's own gate demanded eighteen and a hero out of
                # prison, and the hero cannot lose an age between the two cards.
                assert event["age"] >= 18, event
                assert event["title"] == "Aggravated Assault", event
                assert not event.get("table"), event
                article, weapon_name = armed.split(" ", 1)
                assert article in ("a", "an") and weapon_name in WEAPONS, event
                # DataWeapon.Verb is the word the sentence puts between the
                # friend and the hero, with 'attacked' the class's own fallback.
                assert verb in (WEAPON_VERBS.get(weapon_name, ""), "attacked"), event
                choice = str(event.get("choice") or "")
                narrative = event["logLines"][-1]
                if choice.startswith("Run away from "):
                    seen["attack_run"] += 1
                    ran = re.match(r"^I ran away from (him|her)[.]$", narrative)
                    assert ran and choice == "Run away from " + ran.group(1), event
                    deltas = {k: v for k, v in event["deltas"].items() if v}
                    assert deltas == {"discipline": 4}, event
                elif choice == "Fight back":
                    seen["attack_fight"] += 1
                    # ImpactDiscipline(4) over the friend; the arm's own message
                    # stays the empty string the dispatch loads, so the reply
                    # writes no log line at all.
                    deltas = {k: v for k, v in event["deltas"].items() if v}
                    assert deltas == {"discipline": 30}, event
                    # A family line can precede the card line, so the branch is found by
                    # membership rather than by position (REPORT.en.md 236).
                    assert "Fight back" in event["logLines"], event
                elif choice.startswith("Call the police on "):
                    seen["attack_police"] += 1
                    called = re.match(r"^I called the police on (him|her)( and | but |[.] )", narrative)
                    assert called and choice == "Call the police on " + called.group(1), event
                    # The three GetRandomNumber(0,10) outcomes, each keyed to the
                    # same pronoun the label used.
                    subject = "he" if called.group(1) == "him" else "she"
                    assert (narrative.endswith(" and " + subject + " was arrested.")
                            or narrative.endswith(" but " + subject + " fled before they arrived.")
                            or narrative.endswith(". The police warned me for wasting their time.")), event
                    deltas = {k: v for k, v in event["deltas"].items() if v}
                    assert deltas == {"karma": -4}, event
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
            narrative = event["logLines"][-1]
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
assert seen["cards"] and seen["weapon"] and seen["attack"] and seen["threesome"], seen
print("unfriend cards:", dict(sorted(seen.items())))
