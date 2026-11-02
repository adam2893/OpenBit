"""The talk show and the commercial (REPORT.en.md 365).

SimPlace::RefreshAvailableFameOpportunities builds one SimTalkShowOpportunity and one
SimCommercial per age; ScenarioFameTalkShow and ScenarioFameCommercial ask for the answer.
The port has no place object and no menu, so both offers are rebuilt on the hero's own
tick and pushed as pending cards. This pins the gates, the appeal arithmetic behind the
payment, the reach words, both lanes and the follow-up lines.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

TALK = "registry:ScenarioFameTalkShow"
COMM = "registry:ScenarioFameCommercial"

# The five headlines the talk show's follow-up card draws from, and the seven the
# commercial's does (a16 literals #40002/#26038/#22879/#31088/#41357 and
# #38541/#18503/#28337/#44222/#44253/#41810/#39415).
TALK_HEADS = {"Stay up for it", "I'm on tonight", "Hi Mom!", "Nailed it", "That went well, right?"}
COMM_HEADS = {"Sell out", "Easy money", "Killed it", "Tuned in!", "Turn the TV on!",
              "The camera loves me", "Solid seven seconds"}

TALK_OFFER = re.compile(r"^'(?P<show>.+?)' has invited you to appear on the show\.\n\n"
                        r"What will you do\?\n\n(?P<head>[^\n]+)\n"
                        r"You were interviewed on '(?P<show2>.+?)'\.$")
TALK_ASK = re.compile(r"^'(?P<show>.+?)' has invited you to appear on the show\.\n\n"
                      r"What will you do\?$")
TALK_LOG = re.compile(r"^I appeared on (?P<show>.+)\.(?: I made a great impression on viewers"
                      r" of the show\.| I did (?:a decent|an okay) job in my interview\.|"
                      r" I embarrassed myself on live TV\.)$")
TALK_NO = re.compile(r"^I was invited to appear on '(?P<show>.+?)' but I declined the "
                     r"opportunity\.$")

COMM_OFFER = re.compile(r"^(?P<co>.+?) has offered to pay you to appear in "
                        r"(?:a|an) (?P<reach>local|regional|international) television "
                        r"commercial\.\n\nWhat will you do\?\n\n(?P<head>[^\n]+)\n"
                        r"You earned (?P<amount>\$[\d,]+) by appearing in the "
                        r"(?P<reach2>local|regional|international) television commercial "
                        r"for (?P<co2>.+?)\.$")
COMM_ASK = re.compile(r"^(?P<co>.+?) has offered to pay you to appear in "
                      r"(?:a|an) (?P<reach>local|regional|international) television "
                      r"commercial\.\n\nWhat will you do\?$")
COMM_YES = re.compile(r"^I earned (?P<amount>\$[\d,]+) by appearing in (?:a|an) "
                      r"(?P<reach>local|regional|international) television commercial for "
                      r"(?P<co>.+?)\.$")
# #25512 hard-codes the article 'a' whatever the reach word starts with.
COMM_NO = re.compile(r"^I turned down an opportunity to earn (?P<amount>\$[\d,]+) by "
                     r"appearing in a (?P<reach>local|regional|international) television "
                     r"commercial for (?P<co>.+?)\.$")


def run(seed, fame=95, ages=60):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 200000, "fame": fame}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


def money(text):
    return int(text.replace("$", "").replace(",", ""))


talk = talk_yes = talk_no = 0
comm = comm_yes = comm_no = 0
reaches = set()


def walk(seed, doc):
    global talk, talk_yes, talk_no, comm, comm_yes, comm_no
    for event in doc["events"]:
        table = event.get("table")
        if table == TALK:
            talk += 1
            assert event["age"] >= 18, (seed, event["age"])
            assert event["title"] == "Talk Show", (seed, event["title"])
            choice = event["choice"]
            assert choice in ("Go on the show", "Decline the opportunity"), (seed, choice)
            if choice == "Decline the opportunity":
                talk_no += 1
                m = TALK_ASK.match(event["body"])
                assert m, (seed, event["body"][:200])
                # #25670: the decline writes the show's name into the log.
                logs = [TALK_NO.match(line) for line in event["logLines"]]
                logs = [x for x in logs if x]
                assert len(logs) == 1, (seed, event["logLines"])
                assert logs[0].group("show") == m.group("show"), (seed, event["logLines"])
                assert (event["deltas"] or {}).get("fame", 0) < 0, (seed, event["deltas"])
                continue
            talk_yes += 1
            m = TALK_OFFER.match(event["body"])
            assert m, (seed, event["body"][:200])
            assert m.group("show") == m.group("show2"), (seed, m.groupdict())
            assert m.group("head") in TALK_HEADS, (seed, m.group("head"))
            logs = [TALK_LOG.match(line) for line in event["logLines"]]
            logs = [x for x in logs if x]
            assert len(logs) == 1, (seed, event["logLines"])
            assert logs[0].group("show") == m.group("show"), (seed, event["logLines"])
        elif table == COMM:
            comm += 1
            assert event["age"] >= 18, (seed, event["age"])
            assert event["title"] == "Commercial", (seed, event["title"])
            choice = event["choice"]
            assert choice in ("Agree to do it", "Turn it down"), (seed, choice)
            if choice == "Turn it down":
                comm_no += 1
                m = COMM_ASK.match(event["body"])
                assert m, (seed, event["body"][:200])
                reaches.add(m.group("reach"))
                logs = [COMM_NO.match(line) for line in event["logLines"]]
                logs = [x for x in logs if x]
                assert len(logs) == 1, (seed, event["logLines"])
                assert logs[0].group("co") == m.group("co"), (seed, event["logLines"])
                continue
            comm_yes += 1
            m = COMM_OFFER.match(event["body"])
            assert m, (seed, event["body"][:200])
            assert m.group("co") == m.group("co2"), (seed, m.groupdict())
            assert m.group("reach") == m.group("reach2"), (seed, m.groupdict())
            reaches.add(m.group("reach"))
            assert m.group("head") in COMM_HEADS, (seed, m.group("head"))
            paid = money(m.group("amount"))
            # GetPaymentAmount's rounding: a whole currency, a multiple of 100, at least
            # 1000 (the United States' own cost-of-living ratio is 1.0).
            assert paid % 100 == 0 and paid >= 1000, (seed, paid)
            logs = [COMM_YES.match(line) for line in event["logLines"]]
            logs = [x for x in logs if x]
            assert len(logs) == 1, (seed, event["logLines"])
            assert logs[0].group("co") == m.group("co"), (seed, event["logLines"])
            assert money(logs[0].group("amount")) == paid, (seed, event["logLines"])


for seed in range(1, 11):
    walk(seed, run(seed))

print("talk show cards:", talk, "| yes:", talk_yes, "| no:", talk_no)
print("commercial cards:", comm, "| yes:", comm_yes, "| no:", comm_no,
      "| reaches:", sorted(reaches))
assert talk > 0 and talk_yes > 0 and talk_no > 0, (talk, talk_yes, talk_no)
assert comm > 0 and comm_yes > 0 and comm_no > 0, (comm, comm_yes, comm_no)

# The talk show needs fame 90 and both cards need an adult; a quiet hero gets neither.
for seed in range(1, 4):
    # A career lifts fame on its own, so the control life is short enough that the hero
    # cannot reach the gate: measured at 56 by the age of twenty.
    doc = run(seed, fame=40, ages=20)
    tables = [e.get("table") for e in doc["events"]]
    assert TALK not in tables, (seed, "talk show under fame 90")
for seed in range(1, 4):
    doc = run(seed, ages=17)
    tables = [e.get("table") for e in doc["events"]]
    assert TALK not in tables and COMM not in tables, (seed, "a minor was offered a card")
print("ok")
