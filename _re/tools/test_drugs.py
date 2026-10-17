"""Card, lane, and journal regression for the drug find (REPORT.en.md 122).

The card is the second wired Citizen Scenario and the port's second card that can
kill the hero: it names one of the six containers RefreshScenario draws, one drug
whose Illegal column is marked, and then runs one of the four things the game
offers -- turn it in, sell it, pretend it is not there, or swallow the lot. The
test checks the row it names, the sentence it assembles, every lane's stat move,
and the two journals whose wording depends on a roll.
"""
import collections
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TITLE = "Drugs"
TABLE = "a16_drugs-enc"
BOXES = ("duffle bag", "satchel", "backpack", "shopping bag", "grocery cart", "briefcase")
CHOICES = ("Turn it in to authorities", "Sell it", "Pretend like I didn't see it",
           "Consume all of it")
SRC = (ROOT / "port" / "src" / "sim.cpp").read_text(encoding="utf-8")
SCENES = set(re.findall(
    r'"([^"]*)"',
    SRC.split("static const char* const kCircumstancePublicScenes[] = {", 1)[1].split("};", 1)[0],
))

rows = (ROOT / "gamedata_plain" / TABLE).read_text(encoding="utf-8").splitlines()
head = rows[0].split(",")
DRUG = {}
for i, line in enumerate(rows[1:], start=1):
    if line.strip():
        cells = line.split(",")
        if cells[head.index("Illegal")].strip():
            DRUG[i] = cells[head.index("Drug")]
assert len(DRUG) == 14, len(DRUG)


def run(seed, gender, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": gender, "ages": ages}
    proc = subprocess.run([str(PORT), "--script", "/dev/stdin"], input=json.dumps(spec),
                          text=True, capture_output=True, cwd=str(ROOT / "port"))
    assert proc.returncode == 0, (seed, gender, proc.stderr[-400:])
    return json.loads(proc.stdout)


lanes = collections.Counter()
choices = collections.Counter()
per_life = collections.Counter()
rows_seen = set()
cards = 0
for seed in range(1, 41):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        n = 0
        for event in doc["events"]:
            if event.get("title") != TITLE:
                continue
            cards += 1
            n += 1
            assert event["table"] == TABLE, (seed, event.get("table"))
            assert event["age"] >= 21, (seed, event["age"])
            assert event["choice"] in CHOICES, (seed, event.get("choice"))
            choices[event["choice"]] += 1
            row = int(event["row"])
            assert row in DRUG, (seed, row)
            rows_seen.add(row)
            drug = DRUG[row]
            match = re.match(
                r"^(.+?), you come across a (.+?) filled with what appears to be (.+?)\.$",
                event["body"])
            assert match, (seed, event["body"])
            scene, box, named = match.groups()
            assert scene in SCENES or scene == "While on your walk", (seed, scene)
            assert box in BOXES, (seed, box)
            assert named == drug, (seed, named, drug)
            assert not re.search(r"<[a-zA-Z][^<>]*>", event["body"] + " ".join(event["logLines"])), seed
            found = "I found a " + box + " full of " + drug + "."
            got = {k: v for k, v in (event.get("deltas") or {}).items() if v}
            choice = event["choice"]
            if choice == "Turn it in to authorities":
                base = {"discipline": 50, "karma": 50}
                if got.get("happiness") == -100:
                    base["happiness"] = -100
                    want = found + " I tried to turn it into authorities but they arrested me!"
                    lanes["turn_busted"] += 1
                else:
                    want = found + " I turned it in."
                    lanes["turn_clean"] += 1
                assert got == base, (seed, choice, got)
            elif choice == "Sell it":
                if got.get("happiness") == 30:
                    base = {"discipline": -16, "karma": -50, "happiness": 30}
                    want = found + " I earned "
                    lanes["sell_ok"] += 1
                else:
                    base = {"discipline": -16, "karma": -50, "happiness": -100}
                    want = found + " I was caught trying to sell it!"
                    lanes["sell_caught"] += 1
                assert got == base, (seed, choice, got)
            elif choice == "Pretend like I didn't see it":
                want = found + " I left it alone."
                lanes["pretend"] += 1
                assert got == {"discipline": 8, "karma": 8}, (seed, choice, got)
            else:
                want = found + " I consumed it."
                lanes["consume"] += 1
                assert got == {"discipline": -50, "karma": -30, "happiness": -50,
                               "health": -50}, (seed, choice, got)
            hits = [line for line in event["logLines"] if line.startswith(want)]
            assert hits, (seed, want, event["logLines"][-3:])
            if choice == "Sell it" and got.get("happiness") == 30:
                assert re.match(r"^I found a .+ I earned \$[\d,]+ by selling it\.$", hits[0]), hits[0]
        per_life[n] += 1
        assert n <= 3, (seed, gender, n)   # PresentedCountForScenario < 3

print("drug cards:", cards)
print("rows drawn:", len(rows_seen), "of", len(DRUG))
print("lanes:", dict(lanes))
print("choices:", dict(choices))
print("cards per life:", dict(sorted(per_life.items())))
print("ok")
