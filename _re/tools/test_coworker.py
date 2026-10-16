"""Row, gate, and outcome regression for the coworker cards (REPORT.en.md 105),
the unprofessional-supervisor card (REPORT.en.md 116) and the team-building card
(REPORT.en.md 117)."""
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
        out.append("(?:" + "|".join(re.escape(x) for x in cell[open_at + 1:close_at].split("/")) + ")")
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
                assert re.search(r"my supervisor, " + re.escape(match[1]) + r"(?:'s)?,", event["logLines"][0]), event
                choice = event["choice"]
                assert choice in SUP_CHOICES, event
                sex = row["Supervisor Gender"].strip().lower()
                if sex in ("male", "female") and choice.startswith(("Attack ", "Have a talk with ", "Report ")):
                    pronoun = "him" if sex == "male" else "her"
                    assert pronoun in choice, event
                expected = ({"happiness": -4, "discipline": 8}
                            if choice == "It doesn't bother me" else {})
                assert {k: v for k, v in event["deltas"].items() if v} == expected, event
                assert choice == "It doesn't bother me" or event["logLines"][0].endswith("."), event
                if choice == "It doesn't bother me":
                    assert event["logLines"][0].endswith(" I didn't let it bother me."), event
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
                    assert event["logLines"][0] == (
                        "I went " + activity + " with my company on a team building exercise."), event
                else:
                    assert deltas == {"discipline": -16, "karma": -8}, event
                    assert event["logLines"][0] == (
                        "I refused to go " + activity
                        + " with my company on a team building exercise."), event
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
assert set(seen) == set(TABLES) | {SUP, "team building"}, seen
print("coworker cards by table:", dict(sorted(seen.items())))
print("supervisors seen:", len(names))
print("team building cards:", seen["team building"])
