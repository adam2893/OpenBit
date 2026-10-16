"""Card, lane, and journal regression for the witness card (REPORT.en.md 121).

The card is the port's first wired street Scenario: it names a passer-by out of
a16_street-persons-enc, a crime out of the game's own 33-entry array, and then
runs one of the three things a bystander can do. The test checks the row it
names, the sentence it assembles from that row, all three statement journals and
the life's presentation cap of four.
"""
import collections
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TITLE = "Witness"
TABLE = "a16_street-persons-enc"
CHOICES = ("Look the other way", "Call the police", "Try to intervene")
SRC = (ROOT / "port" / "src" / "sim.cpp").read_text(encoding="utf-8")
SCENES = set(re.findall(
    r'"([^"]*)"',
    SRC.split("static const char* const kCircumstancePublicScenes[] = {", 1)[1].split("};", 1)[0],
))
CRIMES = set(re.findall(r'"([^"]*)"', SRC.split("kCrimes[] = {", 1)[1].split("};", 1)[0]))
assert len(CRIMES) == 33, len(CRIMES)

rows = (ROOT / "gamedata_plain" / TABLE).read_text(encoding="utf-8").splitlines()
head = rows[0].split(",")
DESC = {}
for i, line in enumerate(rows[1:], start=1):
    if line.strip():
        DESC[i] = line.split(",")[head.index("Description")]


def article(word):
    return "an " if word and word[0] in "aeiouAEIOU" else "a "


def run(seed, gender, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": gender, "ages": ages}
    proc = subprocess.run([str(PORT), "--script", "/dev/stdin"], input=json.dumps(spec),
                          text=True, capture_output=True, cwd=str(ROOT / "port"))
    assert proc.returncode == 0, (seed, gender, proc.stderr[-400:])
    return json.loads(proc.stdout)


seen = collections.Counter()
lanes = collections.Counter()
per_life = collections.Counter()
rows_seen = set()
lives = 0
for seed in range(1, 41):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        lives += 1
        n = 0
        for event in doc["events"]:
            if event.get("title") != TITLE:
                continue
            seen["cards"] += 1
            n += 1
            assert event["table"] == TABLE, (seed, event.get("table"))
            assert event["age"] >= 18, (seed, event["age"])
            assert event["choice"] in CHOICES, (seed, event.get("choice"))
            row = int(event["row"])
            assert row in DESC, (seed, event.get("row"))
            rows_seen.add(row)
            desc = DESC[row]
            prefix = article(desc) + desc + " "
            crime = None
            for cand in CRIMES:
                for scene in list(SCENES) + ["While on your walk"]:
                    if event["body"] == scene + ", you witness " + prefix + cand + "!":
                        crime = cand
            assert crime, (seed, event["body"])
            assert not re.search(r"<[a-zA-Z][^<>]*>", event["body"] + " ".join(event["logLines"])), seed
            witnessed = "I witnessed " + prefix + crime + "."
            got = {k: v for k, v in (event.get("deltas") or {}).items() if v}
            if event["choice"] == "Look the other way":
                assert got == {"karma": -8, "discipline": -4}, (seed, got)
                assert witnessed + " I looked the other way." in event["logLines"], event["logLines"]
                lanes["look"] += 1
            elif event["choice"] == "Call the police":
                assert got == {"discipline": 8}, (seed, got)
                hit = [x for x in event["logLines"] if x.startswith(witnessed + " I called the police on ")]
                assert len(hit) == 1 and hit[0].endswith((" on him.", " on her.")), (seed, event["logLines"])
                lanes["police"] += 1
            else:
                assert got == {"karma": 16, "discipline": 16}, (seed, got)
                assert witnessed + " I stopped the " + desc + "!" in event["logLines"], event["logLines"]
                lanes["intervene"] += 1
        per_life[n] += 1
        assert n <= 4, (seed, gender, n)

print("lives", lives)
print("witness cards", seen["cards"])
print("lanes", dict(lanes))
print("rows drawn", len(rows_seen), "of", len(DESC))
print("cards per life", dict(sorted(per_life.items())))
assert seen["cards"], "no witness card was drawn"
assert len(rows_seen) > 10, len(rows_seen)
assert all(lanes[k] for k in ("look", "police", "intervene")), lanes
assert max(per_life) >= 3, per_life
print("ok")
