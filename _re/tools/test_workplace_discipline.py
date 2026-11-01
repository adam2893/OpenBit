"""325: the workplace discipline card (ScenarioWorkplaceDisciplineSupervisor).

The card is pushed by the eight coworker incident cards in the native game, so this
test pins the port's own trigger and then checks everything the card itself carries:
the title, the sentence with the allegation, the four reaction labels, the warning the
decision appends, the cooldown, and the termination the decision can reach.
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

TITLE = "Workplace Discipline"
BODY = re.compile(r"^Your supervisor, (?P<sup>.+), has called you into "
                  r"(?P<poss>his|her) office to discuss allegations that you "
                  r"(?P<what>.+?)\.$")
# The five allegations the family draws: (template, seriousness).
# The refusal lane replaces the discipline sentence with its own (#50762).
REJECT = re.compile(r"^Your supervisor, .+?, rejected your attempt to seduce (him|her)!$")

# The termination arm files a reaction line before the news (four of the five the
# recovery could reach; the verb-pool line #70509 is not printable here).
REACTION = re.compile(
    r"^(?:.+ (?:shook (?:his|her) head and sighed\.|asked what's wrong with you!|"
    r"told you to collect your belongings and get out of (?:his|her) sight!|"
    r"called security to escort you out of the building!))$")

ALLEGATIONS = [
    "made a sexual advance on your {0}, {1}",
    "started an untrue rumor about your {0}, {1}",
    "attacked your {0}, {1}",
    "called your {0}, {1}, {2}",
    "tried to give your {0}, {1}, {2}",
]
OPTIONS = ["Apologize for my actions", "Defend my actions", "Insult ",
           "Try to seduce "]


def run(seed=3, ages=60, choices=None, office=True, military=False):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 50000, "only": "workplace-discipline"}
    if office:
        spec["office"] = True
    if military:
        spec["military"] = True
    if choices is not None:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-400:]
    return json.loads(proc.stdout)


def cards(doc):
    return [e for e in doc["events"] if e.get("title") == TITLE]


def warnings():
    with open(DATA / "a16_supervisor-warnings-enc", newline="") as fh:
        return [row["Warning"].strip() for row in csv.DictReader(fh)]


def cell_hits(cell, text):
    # '<supervisor>' resolves to a relationship phrase with spaces in it, so a token is
    # any run of text, not a single word.
    parts = re.split(r"<[^>]+>", cell)
    pattern = r".+?".join(re.escape(part) for part in parts)
    return re.fullmatch(pattern, text) is not None


def allegation_hits(what):
    for template in ALLEGATIONS:
        pieces = re.split(r"\{\d\}", template)
        pattern = r".+?".join(re.escape(piece) for piece in pieces)
        if re.fullmatch(pattern, what):
            return True
    return False


# The card is an adult's: a sixteen-year-old never sees it.
doc = run(ages=17, choices=[0] * 17, office=False)
assert not cards(doc), "the card fired for a minor"

# Every reaction lane answers with its own label and no residue.
for lane, prefix in enumerate(OPTIONS):
    doc = run(choices=[lane] * 60)
    got = cards(doc)
    assert got, f"no card for lane {lane}"
    first = got[0]
    assert first["choice"].startswith(prefix), (lane, first["choice"])
    assert first.get("residue") == [], first.get("residue")
    body = first["body"].split("\n\n")[0]
    m = BODY.match(body)
    if lane == 3 and not m:
        # The seduction has its own sentence when it is turned down (#50762); the
        # allegation is not on screen in that case.
        assert REJECT.match(body), body
        continue
    assert m, body
    assert allegation_hits(m.group("what")), m.group("what")

# The warning the decision appends comes from the table, capitalised, and the log's
# second line is fixed.
rows = warnings()
seen = 0
for lane in (0, 1, 2):
    doc = run(choices=[lane] * 60)
    for event in cards(doc):
        tail = event["body"].split("\n\n")[1:]
        if not tail:
            continue
        assert len(tail) == 1, tail
        assert any(cell_hits(row, tail[0]) for row in rows), tail[0]
        assert tail[0][0].isupper(), tail[0]
        assert any(line.endswith("I received a warning.") for line in event["logLines"])
        seen += 1
assert seen, "no warning lane seen"

# The cooldown: one card every four years at most.
doc = run(choices=[0] * 60)
ages = [event["age"] for event in cards(doc)]
assert ages, "no cards"
for before, after in zip(ages, ages[1:]):
    assert after - before >= 4, ages

# The seduce lane tells the year after through the morning-after card.
doc = run(choices=[3] * 60)
seduced = [e for e in cards(doc)
           if any("I seduced my supervisor," in line for line in e["logLines"])]
assert seduced, "no successful seduction"

# The decision can terminate: the news names the disciplinary reason and the job is
# gone from the next card on. Termination is rare (one in several hundred cards), so
# the test sweeps seeds and stops at the first one.
lost = 0
for seed in range(1, 90):
    doc = run(seed=seed, choices=[2] * 60)
    events = doc["events"]
    for index, event in enumerate(events):
        if event.get("title") != TITLE:
            continue
        body = event.get("body") or ""
        if "for disciplinary issues." not in body:
            continue
        assert "for disciplinary issues." in " ".join(event["logLines"]), event["logLines"]
        assert event["deltas"]["happiness"] == -6, event["deltas"]
        # Before the news the card files one of its four printable reaction lines
        # (#70292, #69859, #70341, #69884 over the supervisor and her pronouns).
        first = body.split("\n\n")[0]
        assert REACTION.match(first), first
        lost += 1
    if lost:
        break
assert lost, "no termination seen"

print("cards:", len(cards(run(choices=[0] * 60))), "| warnings:", len(rows),
      "| terminations:", lost)
print("ok")
