#!/usr/bin/env python3
"""The hero's own pregnancy (REPORT.en.md 342).

The game carries the hero's pregnancy on the same SimPerson+0x48 slot the lover's uses:
SimHero::IncrementAge runs GetResultingPregnancyForMotherAndFatherAndTypeAndAssignToMother
with the hero as the mother, ScenarioPregnancy asks about it with its own two bodies, and
the next age-up resolves it into either ScenarioMiscarriage or ScenarioBirth with the
hero-mother sentences. The port now plays that lane beside the lover lane of REPORT 338.
"""
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

CARD = re.compile(r"^You are pregnant with your (boyfriend|girlfriend|fiance|fiancee|husband|wife), "
                  r"(?P<name>.+), baby!\n\nWhat will you do\?$")
CHOICES = ("Keep the baby", "Don't keep the baby")
BIRTH = re.compile(r"^You just gave birth to (a baby (girl|boy)|(twin|triplet) (girls|boys))!$")
MULTI = re.compile(r"^(twin|triplet) (girls|boys)$")
JOURNAL = re.compile(r"^I gave birth to .+ named .+\.$")
LOSS = re.compile(r"^You (suffered a .+ miscarriage and lost the baby|had .+ and lost the baby)!$")
DECLINE_LOG = "You chose not to keep the baby!"
# pointsForImpact(-6) is the port's steepest step (REPORT 338).
PTS_DECLINE = -100


def run(seed, ages=45, gender="female", cash=60000, relationship=True):
    spec = {"seed": seed, "country": "United States", "gender": gender, "ages": ages,
            "cash": cash, "relationship": relationship}
    fd, path = tempfile.mkstemp(suffix=".json")
    os.write(fd, json.dumps(spec).encode())
    os.close(fd)
    try:
        out = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                             capture_output=True, text=True)
    finally:
        os.unlink(path)
    assert out.returncode == 0, out.stderr[-800:]
    return json.loads(out.stdout[out.stdout.find("{"):])


cards = keeps = declines = births = losses = 0
for seed in range(1, 21):
    doc = run(seed)
    kept = False
    for event in doc["events"]:
        body = event.get("body") or ""
        lines = event.get("logLines") or []
        deltas = event.get("deltas") or {}
        if event.get("title") == "Pregnancy":
            cards += 1
            m = CARD.match(body)
            assert m, (seed, event["age"], body)
            assert event["age"] >= 13, (seed, event["age"])
            assert event.get("choice") in CHOICES, (seed, event.get("choice"))
            assert not event.get("residue"), (seed, event.get("residue"))
            if event["choice"] == "Keep the baby":
                keeps += 1
                kept = True
                father = m.group("name")
            else:
                declines += 1
                assert deltas["happiness"] == PTS_DECLINE, (seed, deltas)
                assert DECLINE_LOG in " ".join(lines), (seed, lines)
        elif BIRTH.match(body):
            births += 1
            assert kept, (seed, event["age"], body)
            kept = False
            # The naming lines come first and the journal line last.
            assert any(JOURNAL.match(l) for l in lines), (seed, lines)
            word = BIRTH.match(body).group(3)
            kids = 2 if word == "twin" else 3 if word == "triplet" else 1
            born = [c for c in doc["final"]["children"] if c["bornAtHeroAge"] == event["age"]]
            assert len(born) == kids, (seed, event["age"], kids, born)
            assert len({c["name"] for c in born}) == kids, (seed, born)
            for c in born:
                assert c["isHeroes"] is True, (seed, c)
                assert c["twins"] == kids - 1, (seed, c)
                assert c["otherParent"] == father, (seed, c)
        elif LOSS.match(body):
            losses += 1
            assert kept, (seed, event["age"], body)
            kept = False

# A male hero never carries the pregnancy: the lane is the hero-as-mother's only.
male = [e for e in run(3, gender="male")["events"]
        if (e.get("body") or "").startswith("You are pregnant")]
assert not male, male
assert cards >= 6, cards
assert keeps >= 2, keeps
assert declines >= 1, declines
assert births >= 1, births
print("cards:", cards, "| keeps:", keeps, "| declines:", declines, "| births:", births,
      "| losses:", losses)
print("ok")
