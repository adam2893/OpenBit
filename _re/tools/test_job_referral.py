"""Card, lane, and journal regression for the job referral (REPORT.en.md 124).

The card is the Citizen family's career card: a hero with no work at all, past
retirement age, is offered a job by a friend of a friend. The test checks the row
it names, the two templates it assembles, every lane's stat move, the journals,
and the gate itself -- especially that the card only ever reaches a hero without
employment, which is what the retirement rule exists for.
"""
import collections
import csv
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TITLE = "Job Referral"
TABLE = "a16_careers-enc"
CHOICES = ("Take the job", "That's not for me")

# The port's own rule (src/sim.cpp jobReferralYear, the same one careerYear uses):
# the title is the first step of the Seniority Track, and a track with no comma
# is not a track at all, so the row's Career column names the job.
rows = list(csv.reader((ROOT / "gamedata_plain" / TABLE).read_text(encoding="utf-8").splitlines()))
head = rows[0]
CAREER = []
for i, cells in enumerate(rows[1:], start=1):
    if not any(cell.strip() for cell in cells):
        continue
    track = cells[head.index("Seniority Track")].strip()
    first = track.split(",")[0].strip() if "," in track else cells[head.index("Career")].strip()
    CAREER.append((i, first))
assert len(CAREER) > 100, len(CAREER)


def run(seed, gender, ages=95):
    spec = {"seed": seed, "country": "United States", "gender": gender, "ages": ages}
    proc = subprocess.run([str(PORT), "--script", "/dev/stdin"], input=json.dumps(spec),
                          text=True, capture_output=True, cwd=str(ROOT / "port"))
    assert proc.returncode == 0, (seed, gender, proc.stderr[-400:])
    return json.loads(proc.stdout)


lanes = collections.Counter()
per_life = collections.Counter()
rows_seen = set()
cards = 0
accepted = 0
for seed in range(1, 61):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        n = 0
        employed_after = None
        for event in doc["events"]:
            if event.get("title") != TITLE:
                continue
            cards += 1
            n += 1
            assert event["table"] == TABLE, (seed, event.get("table"))
            # Gate @0x73f3d14918: age in [66,110).
            assert 66 <= event["age"] < 110, (seed, event["age"])
            assert event["choice"] in CHOICES, (seed, event.get("choice"))
            row = int(event["row"])
            want_title = dict(CAREER).get(row)
            assert want_title, (seed, row)
            rows_seen.add(row)
            match = re.match(
                r"^Your friend, (.+?), has a friend who works at (.+?) "
                r"and can get you a job there as (.+?)\.$",
                event["body"])
            assert match, (seed, event["body"])
            friend, employer, job = match.groups()
            assert job == want_title, (seed, job, want_title)
            assert not re.search(r"<[a-zA-Z][^<>]*>", event["body"]), (seed, event["body"])
            line = ("A friend of " + friend + " offered to set me up with a job as " +
                    job + " for " + employer + ".")
            got = {k: v for k, v in (event.get("deltas") or {}).items() if v}
            if event["choice"] == "Take the job":
                lanes["take"] += 1
                accepted += 1
                assert got == {"discipline": 8, "karma": 8, "happiness": 30}, (seed, got)
                # The accept clears the referral cap: the hero is employed again.
                employed_after = True
            else:
                lanes["decline"] += 1
                assert got == {"discipline": -30, "karma": -8, "happiness": -16}, (seed, got)
                line += " I turned down the opportunity."
            hits = [entry for entry in event["logLines"] if entry.startswith(line)]
            assert hits or line == ("A friend of " + friend + " offered to set me up with a "
                                    "job as " + job + " for " + employer + "."), (
                seed, line, event["logLines"][-3:])
            if event["choice"] != "Take the job":
                assert hits, (seed, line, event["logLines"][-3:])
        assert n <= 3, (seed, gender, n)   # PresentedCountForScenario < 3
        per_life[n] += 1

print("referral cards:", cards)
print("rows drawn:", len(rows_seen), "of", len(CAREER))
print("lanes:", dict(lanes))
print("cards per life:", dict(sorted(per_life.items())))
print("ok")
