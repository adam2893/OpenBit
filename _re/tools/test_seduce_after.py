"""Card and pool regression for the morning after the hookup (REPORT.en.md 123).

ScenarioSupervisorSeduceSuccess is the only card in the supervisor family the hero
never chooses: the game pushes it the moment a hookup choice resolves, so it just
reports what happened, and its title comes from one of two pools -- the hero's own
attempt or the boss's move. The test checks that every one of those cards follows a
hookup that earned one, that the pool matches the branch, that the cheater's stat
move is the -4 pair, and that the card writes no journal line of its own.
"""
import collections
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
HERO_TITLES = {"Chasing my bag", "I'm the captain now", "And about my pay...",
               "Let's play favorites"}
OTHER_TITLES = {"Nobody can know", "If we're on company time",
                "Let's shoot for a promotion soon", "Securing my position here",
                "I need lots of supervision"}
TITLES = HERO_TITLES | OTHER_TITLES
BODY = re.compile(r"^You (?:seduced your supervisor, (.+?)!|let your supervisor, (.+?), seduce you!)$")
HOOKUP = {"Seducer", "Seductress"}
ADVANCE = ("Get at it", "Let me get a piece of that", "Give it to him", "Give it to her",
           "Hit that")


def run(seed, gender, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": gender, "ages": ages}
    proc = subprocess.run([str(PORT), "--script", "/dev/stdin"], input=json.dumps(spec),
                          text=True, capture_output=True, cwd=str(ROOT / "port"))
    assert proc.returncode == 0, (seed, gender, proc.stderr[-300:])
    return json.loads(proc.stdout)


cards = 0
who = collections.Counter()
titles = collections.Counter()
deltas = collections.Counter()
delays = collections.Counter()
per_life = collections.Counter()

for seed in range(1, 41):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        pending = []
        n = 0
        for event in doc["events"]:
            title = event.get("title") or ""
            log = " ".join(event.get("logLines") or [])
            if title in HOOKUP:
                if event.get("choice") in ADVANCE or "I seduced my supervisor" in log:
                    pending.append(event["age"])
                continue
            if title not in TITLES:
                continue
            cards += 1
            n += 1
            titles[title] += 1
            assert not event.get("table"), (seed, event.get("table"))
            assert not event.get("choice"), (seed, event.get("choice"))
            assert not event.get("narrative"), (seed, event.get("narrative"))
            assert event["age"] >= 18, (seed, event["age"])
            match = BODY.match(event["body"])
            assert match, (seed, event["body"])
            hero_said, boss_said = match.groups()
            by_hero = hero_said is not None
            who["hero" if by_hero else "boss"] += 1
            assert (title in HERO_TITLES) == by_hero, (seed, title, by_hero)
            got = {k: v for k, v in (event.get("deltas") or {}).items() if v}
            assert got in ({}, {"karma": -30, "discipline": -30}), (seed, got)
            deltas[tuple(sorted(got.items()))] += 1
            assert pending, (seed, gender, event["age"], title)   # no card without a hookup
            delays[min(event["age"] - pending.pop(0), 9)] += 1
        per_life[n] += 1

print("morning cards:", cards)
print("who:", dict(who))
print("titles:", dict(sorted(titles.items())))
print("deltas:", {str(k): v for k, v in sorted(deltas.items())})
print("delay in years:", dict(sorted(delays.items())))
print("cards per life:", dict(sorted(per_life.items())))
print("ok")
