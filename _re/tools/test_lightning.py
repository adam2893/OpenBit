"""Card, lane, and pending-death regression for the lightning strike
(REPORT.en.md 119).

The card is the port's first wired Scenario that can kill the hero, so the test
checks three things the other card tests do not need: the scene sentence comes
out of the port's own copy of the game's 43-slot circumstance switch, the four
stat floats land on either 0 or 100, and a strike that rolls the death flag ends
the life with the death table's own 'Fluke' row.
"""
import collections
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TITLE = "Lightning Strike"
BODY = re.compile(r"^(.+?), you were struck by lightning!$")
LOG = "I was struck by lightning!"
STATS = ("happiness", "health", "smarts", "looks")
SCENES = set(re.findall(
    r'"([^"]*)"',
    (ROOT / "port" / "src" / "sim.cpp").read_text(encoding="utf-8")
    .split("static const char* const kCircumstancePublicScenes[] = {", 1)[1]
    .split("};", 1)[0],
))


def run(seed, gender, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": gender, "ages": ages}
    proc = subprocess.run([str(PORT), "--script", "/dev/stdin"], input=json.dumps(spec),
                          text=True, capture_output=True, cwd=str(ROOT / "port"))
    assert proc.returncode == 0, (seed, gender, proc.stderr[-400:])
    return json.loads(proc.stdout)


seen = collections.Counter()
per_life = collections.Counter()
deaths = 0
lives = 0
for seed in range(1, 41):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        lives += 1
        now = dict((doc.get("newborn") or {}).get("stats") or {})
        n = 0
        for e in doc["events"]:
            pre = dict(now)
            for k in ("karma", "discipline") + STATS:
                now[k] = max(0, min(100, now.get(k, 0) + (e.get("deltas") or {}).get(k, 0)))
            # The hero's own ailments cost health outside the card deltas (REPORT 247),
            # so the reconstructed stats have to charge it too.
            now["health"] = max(0, min(100, now["health"] - int(e.get("ailmentCost") or 0)))
            if e.get("title") != TITLE:
                continue
            n += 1
            seen["cards"] += 1
            assert e["table"] == "" and e["row"] is None and e["choice"] is None, (seed, e)
            assert e["age"] >= 18, (seed, e["age"])
            m = BODY.match(e["body"])
            assert m and m.group(1) in SCENES, (seed, e["body"])
            assert LOG in e["logLines"], (seed, e["logLines"])
            assert not re.search(r"<[a-zA-Z][^<>]*>", e["body"]), (seed, e["body"])
            d = e.get("deltas") or {}
            got = {k: v for k, v in d.items() if v}
            assert got and set(got) <= set(STATS), (seed, got)
            # The card zeroes the four stat floats or restores them to 100.0f,
            # so its deltas are exactly the negatives or the complements of the
            # values the strike found (the runner clamps after every year).
            wipe = {k: v for k, v in ((k, -pre.get(k, 0)) for k in STATS) if v}
            fill = {k: v for k, v in ((k, 100 - pre.get(k, 0)) for k in STATS) if v}
            assert got == wipe or got == fill, (seed, pre, got)
            seen["damage" if got == wipe else "unharmed"] += 1
        per_life[n] += 1
        assert n <= 1, (seed, gender, n)
        last = doc["events"][-1] if doc["events"] else None
        if last and last.get("title") == TITLE and doc["final"]["alive"] is False:
            death = doc["final"]["death"]
            # The strike hands DeathManager its own cause only when the card's fatal
            # lane fires; a hero who survives the card can still die later of the
            # ordinary annual roll (REPORT.en.md 228).
            if "struck by lightning" in death["sentence"]:
                deaths += 1
                assert death["type"] == "Fluke", (seed, gender, death)

print("lives", lives)
print("lightning cards", seen["cards"])
print("lanes", {"damage": seen["damage"], "unharmed": seen["unharmed"]})
print("lightning deaths", deaths)
print("cards per life", dict(sorted(per_life.items())))
assert seen["cards"], "no lightning card was drawn"
assert deaths, "no lightning death was rolled"
print("ok")
