"""326: the walk's animal encounter (ScenarioEncounterAnimal).

The card comes from the walk activity's ninth outcome in the native game; the port
has no walk, so it is its own family. Everything the card carries is the animal row's
own four scores, so this test pins the row, the four labels, the two escape lanes,
the two hands-on lanes, the kill and the cooldown.
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

TITLE = "Encounter"
BODY = re.compile(r"^While on your walk, you encounter (a|an) (?P<animal>.+)\.$")
# The successful hands-on lanes append the card's own 'You ... the animal!' line, so
# the opening sentence is matched on its own.
HEAD = "\n\nWhat will you do?"
OPTIONS = ["Run for my life", "Retreat slowly", "Try to rescue it", "Pet it"]
SUCCESS = re.compile(r"^You (?P<verb>.+) the (?P<animal>.+)!$")


def run(seed=5, ages=60, choices=None, country="United States"):
    spec = {"seed": seed, "country": country, "gender": "male", "ages": ages,
            "only": "animal-encounter"}
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


def animals():
    with open(DATA / "a16_animals-enc", newline="") as fh:
        return [row for row in csv.DictReader(fh)]


def continent_of(country):
    with open(ROOT / "gamedata_json" / "a16_countries-enc.json") as fh:
        for row in json.load(fh):
            if row["<Name>k__BackingField"] == country:
                return row["<Continent>k__BackingField"]
    raise AssertionError(f"no country row for {country}")


def check_body(event, country):
    body = (event.get("body") or "").split(HEAD)[0]
    m = BODY.match(body)
    assert m, body
    name = m.group("animal")
    cont = continent_of(country)
    rows = animals()
    for index, row in enumerate(rows):
        if row["Animal"].strip() != name:
            continue
        cell = row["Continent"].strip()
        # 'all 7 continents' or a comma-separated list the hero's own continent is in.
        assert cell == "all 7 continents" or cont in [c.strip() for c in cell.split(",")], \
            (name, cell, cont)
        assert event["table"] == "a16_animals-enc", event["table"]
        assert int(event["row"]) == index + 1, (event["row"], name)
        assert (m.group(1) == "an") == (name[0] in "aeiou"), name
        return row
    raise AssertionError(f"{name} is not an animal row")


def lane(seed, want, ages=60, country="United States"):
    for pad in range(18, ages + 1):
        doc = run(seed=seed, ages=ages, choices=[-1] * pad + [want], country=country)
        hits = [e for e in cards(doc) if e["age"] == pad]
        if hits:
            return doc, hits[0]
    raise AssertionError(f"lane {want} not reachable")


# A minor never meets one.
doc = run(ages=17, choices=[0] * 17)
assert not cards(doc), "the card fired for a minor"

# Every lane answers with its own label, and the card names a real row of the
# hero's continent.
seen = {}
for want, prefix in enumerate(OPTIONS):
    doc, card = lane(3, want)
    assert card["choice"].startswith(prefix), (want, card["choice"])
    assert card.get("residue") == [], card.get("residue")
    row = check_body(card, "United States")
    seen[prefix] = (doc, card, row)

# Run: Aggression 4 and up may chase (roll of 0,2 == 0), everything else evades.
evaded = chased = 0
for seed in range(1, 90):
    doc = run(seed=seed, ages=60, choices=[0] * 60)
    for event in cards(doc):
        row = check_body(event, "United States")
        log = event["logLines"][-1]
        if log.endswith("I evaded it."):
            evaded += 1
        elif "but it chased me!" in log:
            chased += 1
    if evaded and chased:
        break
assert evaded, "no clean escape in the sweep"

# Retreat: roll(0,30)+1 <= Aggression chases; the safe lane is the common one.
safe = chased_r = 0
for seed in range(1, 90):
    doc = run(seed=seed, ages=60, choices=[1] * 60)
    for event in cards(doc):
        check_body(event, "United States")
        log = event["logLines"][-1]
        if log.endswith("but retreated carefully."):
            safe += 1
        elif "tried to retreat but it chased me!" in log:
            chased_r += 1
    if safe and chased_r:
        break
assert safe, "no careful retreat in the sweep"

# Rescue and pet: a success writes the card's own 'You ... the animal!' line and
# pays three karma.
for want, done in ((2, "successfully rescued"), (3, "gently caressed")):
    hits = 0
    for seed in range(1, 120):
        doc = run(seed=seed, ages=60, choices=[want] * 60)
        for event in cards(doc):
            check_body(event, "United States")
            log = event["logLines"][-1]
            m = SUCCESS.match(event["body"].split("\n\n")[-1])
            if not m:
                continue
            hits += 1
            assert event["deltas"]["karma"] == 3, event["deltas"]
            assert log.endswith("and " + done + " it."), log
            # The verb is the same one the log uses, except at Danger 4 and above.
            row = check_body(event, "United States")
            if int(row["Danger Score"]) > 3:
                assert m.group("verb") == "are freaking crazy", m.group("verb")
            else:
                assert m.group("verb") == done, m.group("verb")
        if hits:
            break
    assert hits, f"no {done} success in the sweep"

# The kill: a chase ends in the animal's own Attack String and the hero's death.
killed = 0
for seed in range(1, 160):
    doc = run(seed=seed, ages=60, choices=[0] * 60)
    if doc["final"]["alive"]:
        continue
    for event in cards(doc):
        row = check_body(event, "United States")
        log = event["logLines"][-1]
        if not log.endswith(" me!"):
            continue
        killed += 1
        assert log.endswith(" The %s %s me!" % (row["Animal"].strip(),
                                               row["Attack String"].strip())), log
        break
    if killed:
        break
assert killed, "no animal kill in the sweep"

# Cooldown: two cards in one life are at least four years apart.
for seed in range(1, 40):
    ages = [e["age"] for e in cards(run(seed=seed, ages=80, choices=[-1] * 80))]
    if len(ages) > 1:
        assert all(b - a >= 4 for a, b in zip(ages, ages[1:])), ages
        break

print("lanes:", list(seen), "| evaded", evaded, "chased", chased,
      "| retreat safe", safe, "chased", chased_r, "| kills", killed)
print("ok")
