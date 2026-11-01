"""346: the zoo's theft card."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLE = ROOT / "gamedata_plain" / "a16_zoo-animals-enc"

BODY = re.compile(r"^Your operations manager, (?P<manager>.+), is reporting that someone "
                  r"snuck into (?P<zoo>.+) and stole (?P<name>.+) the (?P<breed>.+)!"
                  r"\n\n(?P<pron>She|He) suggests increasing your zoo's security team to "
                  r"avoid further thefts\.$")
STOLE = re.compile(r"^An animal burglar stole (?P<name>.+) the (?P<breed>.+) from my zoo\.$")
KEEP = re.compile(r"^I decided they could keep (?P<them>him|her)\.$")
BACK = re.compile(r"^I filed a police report and they apprehended the criminals and returned "
                  r"(?P<them>him|her) safely to my care\.$")
LOST = re.compile(r"^I filed a police report, but they couldn't recover (?P<them>him|her) "
                  r"safely to my care\.$")


def breeds():
    with open(TABLE, newline="", encoding="utf-8") as fh:
        return {r["Breed"].strip().lower() for r in csv.DictReader(fh)}


BREEDS = breeds()


def lines(event):
    out = []
    for entry in event.get("logLines") or []:
        out.extend(part for part in str(entry).split("\n") if part.strip())
    return out


def run(seed=3, ages=70, zoo=True, choices=None):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 3000000, "zoo": zoo, "only": "zoo-theft"}
    if choices:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    done = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                          capture_output=True, text=True)
    assert done.returncode == 0, done.stderr[-2000:]
    return json.loads(done.stdout)


TABLES = "registry:ScenarioZooAgeUpTheft"
cards = 0
lanes = {"File a police report": 0, "Accept the loss": 0}
tails = {"keep": 0, "back": 0, "lost": 0}
ages_per_seed = {}

for seed in range(1, 41):
    doc = run(seed)
    mine = []
    for event in doc["events"]:
        if event.get("table") != TABLES:
            continue
        cards += 1
        mine.append(event["age"])
        body = event.get("body") or ""
        m = BODY.match(body)
        assert m, (seed, body)
        log = lines(event)
        stole = next((line for line in log if STOLE.match(line)), None)
        assert stole, (seed, log[:4])
        s = STOLE.match(stole)
        # The body and the journal name the same animal.
        assert s.group("name") == m.group("name"), (seed, stole, body)
        assert s.group("breed") == m.group("breed"), (seed, stole, body)
        assert m.group("breed").strip().lower() in BREEDS, (seed, m.group("breed"))
        assert m.group("zoo").strip(), (seed, body)
        assert event.get("title") == "Theft", (seed, event.get("title"))
        assert event.get("choice") in lanes, (seed, event.get("choice"))
        assert not (event.get("residue") or ""), (seed, event.get("residue"))
        assert event["age"] >= 18, (seed, event["age"])
        # The card moves no money and no stat: it is a report and a choice.
        assert not any((event.get("deltas") or {}).values()), (seed, event.get("deltas"))
        lanes[event["choice"]] += 1
        if event["choice"] == "Accept the loss":
            assert any(KEEP.match(line) for line in log), (seed, log)
            tails["keep"] += 1
        else:
            if any(BACK.match(line) for line in log):
                tails["back"] += 1
            else:
                assert any(LOST.match(line) for line in log), (seed, log)
                tails["lost"] += 1
    if mine:
        ages_per_seed[seed] = sorted(mine)

assert cards >= 10, cards
assert lanes["File a police report"] and lanes["Accept the loss"], lanes
assert tails["back"] and tails["lost"], tails

# Neither the next year nor the three after it can repeat the card.
for seed, years in ages_per_seed.items():
    for a, b in zip(years, years[1:]):
        assert b - a >= 4, (seed, years)

# A park-less hero never gets it.
for seed in (2, 3, 4, 5):
    doc = run(seed, zoo=False)
    assert not [e for e in doc["events"] if e.get("table") == TABLES], seed

print("cards:", cards, "| lanes:", lanes, "| tails:", tails)
print("ok")
