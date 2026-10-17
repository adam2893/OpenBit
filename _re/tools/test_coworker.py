"""Row, gate, and outcome regression for the coworker cards (REPORT.en.md 105),
the unprofessional-supervisor card (REPORT.en.md 116), the team-building card
(REPORT.en.md 117) and the after-hours invitation (REPORT.en.md 118) and the supervisor hookup card (REPORT.en.md 120)."""
import collections
import csv
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLES = {
    "a16_coworker-annoy-scenarios-enc": ("Hero Gender", "Coworker Gender", 5),
    "a16_coworker-behavior-scenarios-enc": ("Recipient Gender", "Sender Gender", 4),
    "a16_coworker-pranks-enc": ("", "", 3),
    "a16_coworker-rumors-enc": ("Victim Gender", "", 3),
}
ROWS = {
    name: list(csv.DictReader((ROOT / "gamedata_plain" / name).open(newline="")))
    for name in TABLES
}
# The supervisor card: one table, a cap of five showings in a life, four
# buttons, and only the ignore lane moving the hero.
SUP = "a16_supervisor-unprofessional-scenarios-enc"
SUP_ROWS = list(csv.DictReader((ROOT / "gamedata_plain" / SUP).open(newline="")))
SUP_CHOICES = ("Attack him", "Attack her", "Have a talk with him", "Have a talk with her",
               "Report him to HR", "Report her to HR", "It doesn't bother me")
# The team-building card: no table of its own -- the sentence names a pastime --
# one showing in a life, two buttons, and both lanes moving the hero.
TB = "a16_pastimes-enc"
TB_ROWS = list(csv.DictReader((ROOT / "gamedata_plain" / TB).open(newline="")))
TB_BODY = re.compile(r"^(.+?), your supervisor at (.+?), is sponsoring a team building "
                     r"activity by taking the team (.+?)\.$")
TB_CHOICES = ("Sure, I'll go", "No, that sounds lame")
# The after-hours invitation: the same pastime pool, two sentences -- the first
# showing promises co-worker jealousy, later ones only invite -- a cap of five,
# two buttons, and only the hero's own stat moving in either lane.
AH = "a16_pastimes-enc"
AH_LONG = re.compile(r"^Your supervisor at (.+?), (.+?), has taken a liking to you and "
                     r"has asked you to go (.+?) with (him|her)\. You are worried that it "
                     r"will make your co-workers jealous\.$")
AH_SHORT = re.compile(r"^Your supervisor, (.+?), invited you to go (.+?) with (him|her)\.$")
AH_CHOICES = ("Do it", "Politely decline")

# The supervisor hookup card: one table carrying an inbound and an outbound
# column pair, a cap of five, three buttons whose wording depends on the
# direction and the two people's sexes, and three lanes.
HK = "a16_coworker-hookup-scenarios-enc"
HK_ROWS = list(csv.DictReader((ROOT / "gamedata_plain" / HK).open(newline="")))
HK_IN_ADVANCE = ("Get at it", "Let me get a piece of that", "Give it to him",
                 "Give it to her", "Hit that")
HK_OUT_ADVANCE = ("Hook up with him", "Hook up with her", "Try to seduce him",
                  "Try to seduce her")
HK_SUCCESS = re.compile(r"^I seduced my supervisor, (.+?)!$")
HK_FAILURE = re.compile(r"^I attempted to seduce my supervisor, (.+?), but failed\.$")


def hk_pattern(cell):
    """A hookup cell with the coworker token left as a named capture and every
    inline list turned into a pattern; the gender tokens become wildcards because
    the port resolves them."""
    out, i = [], 0
    while True:
        open_at = cell.find("<", i)
        if open_at < 0:
            out.append(re.escape(cell[i:]))
            break
        close_at = cell.find(">", open_at)
        out.append(re.escape(cell[i:open_at]))
        inner = cell[open_at + 1:close_at]
        if inner.startswith("coworker"):
            out.append(r"(?P<name>.+)")
        elif "/" in inner and inner.split("/")[0].lower() not in (
                "he", "she", "him", "her", "his", "hers", "he/she", "him/her", "his/her"):
            out.append("(?:" + "|".join(re.escape(part) for part in inner.split("/")) + ")")
        else:
            out.append(".+?")
        i = close_at + 1
    return "".join(out)



def cell_pattern(cell):
    """A Pastime cell with its inline one-of lists turned into a pattern: the
    port resolves each <a/b/c> before the sentence is assembled."""
    out, i = [], 0
    while True:
        open_at = cell.find("<", i)
        if open_at < 0:
            out.append(re.escape(cell[i:]))
            break
        close_at = cell.find(">", open_at)
        out.append(re.escape(cell[i:open_at]))
        inner = cell[open_at + 1:close_at]
        # A slashless bracket is a person or thing the game itself resolves
        # ('<musician>', '<supervisor>'), not a fixed list of alternatives.
        out.append(("(?:" + "|".join(re.escape(x) for x in inner.split("/")) + ")")
                   if "/" in inner else ".+?")
        i = close_at + 1
    return "".join(out)
seen = collections.Counter()
names = collections.Counter()
for seed in range(1, 41):
    for gender in ("male", "female"):
        spec = {"seed": seed, "country": "United States", "gender": gender, "ages": 80}
        run = subprocess.run([str(PORT), "--script", "/dev/stdin"],
                             input=json.dumps(spec), text=True, capture_output=True,
                             check=True, cwd=ROOT / "port")
        counts = collections.Counter()
        coworker = None
        for event in json.loads(run.stdout)["events"]:
            name = event.get("table")
            if name == SUP:
                row = SUP_ROWS[int(event["row"]) - 1]
                counts[SUP] += 1
                seen[SUP] += 1
                assert counts[SUP] <= 5, event
                assert event["title"] == "Unprofessional Boss", event
                assert event["age"] >= 18, event
                assert row["Hero Gender"].strip().lower() in ("", "both", gender), event
                assert row["Supervisor Gender"].strip().lower() in ("", "both", "male", "female"), event
                # The card trails the row own Scenario cell, with the supervisor
                # and his possessive resolved and the bracket alternatives drawn.
                assert event["body"][0].isupper(), event
                assert not re.search(r"<[a-zA-Z][^<>]*>", " ".join(event["logLines"])), event
                match = re.search(r"[Yy]our supervisor, (.+?)(?:'s)?,", event["body"])
                assert match, event
                assert re.search(r"my supervisor, " + re.escape(match[1]) + r"(?:'s)?,", event["logLines"][-1]), event
                choice = event["choice"]
                assert choice in SUP_CHOICES, event
                sex = row["Supervisor Gender"].strip().lower()
                if sex in ("male", "female") and choice.startswith(("Attack ", "Have a talk with ", "Report ")):
                    pronoun = "him" if sex == "male" else "her"
                    assert pronoun in choice, event
                expected = ({"happiness": -4, "discipline": 8}
                            if choice == "It doesn't bother me" else {})
                assert {k: v for k, v in event["deltas"].items() if v} == expected, event
                assert choice == "It doesn't bother me" or event["logLines"][-1].endswith("."), event
                if choice == "It doesn't bother me":
                    assert event["logLines"][-1].endswith(" I didn't let it bother me."), event
                names[match[1]] += 1
                continue
            if event.get("title") == "Team Building":
                counts["team building"] += 1
                seen["team building"] += 1
                assert counts["team building"] <= 1, event
                assert name == TB, event
                assert event["age"] >= 21, event
                match = TB_BODY.match(event["body"])
                assert match, event
                activity = match[3]
                row = TB_ROWS[int(event["row"]) - 1]
                assert re.fullmatch(cell_pattern(row["Pastime"].strip()), activity), event
                assert not re.search(r"<[a-zA-Z][^<>]*>", event["body"]), event
                choice = event["choice"]
                assert choice in TB_CHOICES, event
                deltas = {k: v for k, v in event["deltas"].items() if v}
                if choice == "Sure, I'll go":
                    assert deltas == {"happiness": 4, "discipline": 16, "karma": 8}, event
                    assert event["logLines"][-1] == (
                        "I went " + activity + " with my company on a team building exercise."), event
                else:
                    assert deltas == {"discipline": -16, "karma": -8}, event
                    assert event["logLines"][-1] == (
                        "I refused to go " + activity
                        + " with my company on a team building exercise."), event
                continue
            if event.get("title") == "After Hours":
                counts["after hours"] += 1
                seen["after hours"] += 1
                assert counts["after hours"] <= 5, event
                assert name == AH, event
                assert event["age"] >= 21, event
                match = AH_LONG.match(event["body"])
                if match:
                    assert counts["after hours"] == 1, event
                    supervisor, activity, pronoun = match[2], match[3], match[4]
                    assert match[1] == "the company", event
                else:
                    match = AH_SHORT.match(event["body"])
                    assert match, event
                    supervisor, activity, pronoun = match[1], match[2], match[3]
                row = TB_ROWS[int(event["row"]) - 1]
                assert re.fullmatch(cell_pattern(row["Pastime"].strip()), activity), event
                assert not re.search(r"<[a-zA-Z][^<>]*>", event["body"]), event
                assert pronoun in ("him", "her"), event
                choice = event["choice"]
                assert choice in AH_CHOICES, event
                deltas = {k: v for k, v in event["deltas"].items() if v}
                if choice == "Do it":
                    assert deltas == {"happiness": 16}, event
                    assert event["logLines"][-1] == (
                        "I went " + activity + " with my supervisor, " + supervisor + "."), event
                else:
                    assert deltas == {"discipline": 4}, event
                    assert event["logLines"][-1] == (
                        "I politely declined an opportunity to go " + activity
                        + " with my supervisor, " + supervisor + "."), event
                continue
            if event.get("title") in ("Seducer", "Seductress"):
                counts["hookup"] += 1
                seen["hookup"] += 1
                assert counts["hookup"] <= 5, event
                assert name == HK, event
                assert event["age"] >= 18, event
                body, journal = event["body"], event["logLines"][-1]
                assert not re.search(r"<[a-zA-Z][^<>]*>", body), event
                inbound = "your supervisor, " in body.lower()
                row = HK_ROWS[int(event["row"]) - 1]
                if inbound:
                    assert row["Supervisor Appropriate"].strip(), event
                cell = (row["Inbound Scenario"] if inbound else row["Outbound Scenario"]).strip()
                pattern = hk_pattern(cell)
                match = (re.fullmatch(pattern, body)
                         or re.fullmatch(pattern, body[:1].lower() + body[1:]))
                assert match, (cell, body)
                supervisor = (match.groupdict().get("name") or "").strip()
                for lead in ("your supervisor, ", "my supervisor, "):
                    if supervisor.lower().startswith(lead):
                        supervisor = supervisor[len(lead):]
                supervisor = supervisor.rstrip(",")
                if supervisor.endswith("'s"): supervisor = supervisor[:-2]
                choice = event["choice"]
                deltas = {k: v for k, v in event["deltas"].items() if v}
                if choice.startswith("Reject "):
                    assert choice.endswith(" advances"), event
                    assert deltas == {"discipline": 16}, event
                elif choice.startswith("Report "):
                    assert deltas == {}, event
                elif choice in HK_IN_ADVANCE:
                    assert inbound, event
                    assert deltas == {"discipline": -16}, event
                else:
                    assert choice in HK_OUT_ADVANCE, event
                    assert not inbound, event
                    match = HK_SUCCESS.match(journal)
                    if match:
                        assert deltas == {}, event
                    else:
                        match = HK_FAILURE.match(journal)
                        assert match, journal
                        assert deltas == {"happiness": -4}, event
                    assert match[1] in body, (journal, body)
                # the body capture is lazy, so the journal is the arbiter of the
                # name the card really wrote
                named = re.search(r"(?:my supervisor|your supervisor), (.+?),", journal)
                if named and supervisor:
                    assert named[1] in body, (journal, body)
                continue
            if name not in TABLES:
                continue
            row = ROWS[name][int(event["row"]) - 1]
            hero_col, colleague_col, cap = TABLES[name]
            counts[name] += 1
            seen[name] += 1
            assert counts[name] <= cap, (name, counts[name])
            assert event["age"] >= 18, event
            assert not hero_col or row[hero_col].strip().lower() in ("", gender), event
            if name.endswith("pranks-enc"):
                assert not row["Vampire"].strip(), event
            text = event["body"] + " ".join(event["logLines"])
            match = re.search(r"[Yy]our coworker, (.+?)(?:'s)?,", event["body"])
            if match:
                if coworker is None:
                    coworker = match[1]
                assert coworker == match[1], event
            assert event["choice"] and event["logLines"], event
            assert not re.search(r"<[a-zA-Z][^<>]*>", text), event
            choice = event["choice"]
            expected = ({"happiness": -8, "discipline": 8}
                        if choice in ("Laugh it off", "Pay no attention", "It doesn't bother me")
                        or choice.startswith("Ignore ") else
                        {"happiness": 16, "discipline": -16}
                        if choice.startswith("Start a rumor about ") else {})
            assert {key: value for key, value in event["deltas"].items() if value} == expected, event
            if colleague_col and row[colleague_col].strip().lower() in ("male", "female"):
                pronoun = "him" if row[colleague_col].strip().lower() == "male" else "her"
                if choice.startswith(("Attack ", "Have a talk with ", "Report ")):
                    assert re.search(r"\b" + pronoun + r"\b", choice), event
            if choice.startswith("Start a rumor about "):
                assert coworker and choice == "Start a rumor about " + coworker, event
assert set(seen) == set(TABLES) | {SUP, "team building", "after hours", "hookup"}, seen
print("coworker cards by table:", dict(sorted(seen.items())))
print("supervisors seen:", len(names))
print("team building cards:", seen["team building"])
print("after hours cards:", seen["after hours"])
print("hookup cards:", seen["hookup"])
