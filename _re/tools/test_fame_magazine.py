"""The magazine photo shoot (REPORT.en.md 335).

SimPlace::RefreshAvailableFameOpportunities builds a SimMagazineOpportunity each year and
ScenarioFameMagazine asks for the answer: a title from a16_magazines-enc, a pose the appeal
ladder picked, and a fee from SimMagazineOpportunity::GetPaymentAmount. The port has no place
object, so the offer is rebuilt on the hero's own tick - which is what this test pins, along
with the fee formula and both lanes.
"""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLE = ROOT / "gamedata_plain" / "a16_magazines-enc"

with open(TABLE, newline="", encoding="utf-8") as fh:
    ROWS = list(csv.DictReader(fh))
TITLES = {(r["Gender"].strip(), r["Title"].strip()) for r in ROWS if (r["Title"] or "").strip()}

# SimMagazineOpportunity::GetPaymentAmount's per-pose {A, B} pair.
POSE = {1: (15000.0, 100000.0), 2: (50000.0, 150000.0), 3: (50000.0, 250000.0),
        4: (250000.0, 1000000.0), 5: (750000.0, 2500000.0)}
APPEAR = {"clothed": 1, "in a swimsuit": 2, "topless": 3, "nude": 4,
          "as the nude centerfold": 5}
MONTHS = {"January", "February", "March", "April", "May", "June", "July", "August",
          "September", "October", "November", "December"}
CARD = re.compile(r"^(?P<title>.+?) magazine has offered you a deal to appear "
                  r"(?P<appear>.+?) in its (?P<month>[A-Za-z]*) issue\.\n\n"
                  r"What will you do\?")
EARNED = re.compile(r"^I earned \$(?P<fee>[\d,]+) by appearing (?P<appear>.+?) in the "
                    r"(?P<month>[A-Za-z]*) issue of (?P<title>.+?) magazine\.$")
TURNED = re.compile(r"^I turned down an opportunity to appear (?P<appear>.+?) in the "
                    r"(?P<month>[A-Za-z]*) issue of (?P<title>.+?) magazine\.$")
# pointsForImpact: 2 -> 8, 3 -> 16, 4 -> 30 (REPORT.en.md 121).
POINTS = {2: 8, 3: 16, 4: 30}


def run(seed, fame=95, ages=50):
    spec = {"seed": seed, "country": "United States", "gender": "female", "ages": ages,
            "cash": 50000, "fame": fame}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = accepted = refused = 0
poses = set()
for seed in range(1, 31):
    doc = run(seed)
    last_age = {}
    for event in doc["events"]:
        if event.get("table") != "registry:ScenarioFameMagazine":
            continue
        cards += 1
        age = event["age"]
        assert age >= 18, (seed, age)
        # One offer a year at most: the place's refresh is an age-equality gate.
        assert last_age.get(seed) != age, (seed, age)
        last_age[seed] = age
        body = event["body"]
        m = CARD.match(body)
        assert m, (seed, body[:200])
        appear, month, title = m.group("appear"), m.group("month"), m.group("title")
        assert appear in APPEAR, (seed, appear)
        pose = APPEAR[appear]
        poses.add(pose)
        # Poses 4 and 5 decode but nothing assigns them in this build.
        assert pose <= 3, (seed, pose)
        assert ("female", title) in TITLES, (seed, title)
        assert month == "" or month in MONTHS, (seed, month)
        choice = event["choice"]
        assert choice in ("I can't do that", "Sign me up"), (seed, choice)
        log = (event.get("logLines") or [""])[-1]
        deltas = event["deltas"]
        a, b = POSE[pose]
        if choice == "Sign me up":
            accepted += 1
            m2 = EARNED.match(log)
            assert m2, (seed, log)
            assert m2.group("appear") == appear and m2.group("title") == title, (seed, log)
            fee = int(m2.group("fee").replace(",", ""))
            assert fee % 100 == 0 and fee >= 1000, (seed, fee)
            # The appeal moves the amount along A..B, but its penalties can drag B below
            # A (a third, then a half, at worst B/6), so the fee is bounded by both ends.
            assert min(a, b / 6.0) - 100 <= fee <= max(a, b) + 100, (seed, pose, fee)
            assert deltas["fame"] > 0, (seed, deltas)
            assert deltas["discipline"] == -POINTS[2], (seed, deltas)
            assert deltas["karma"] == -POINTS[2], (seed, deltas)
            # The follow-up card's headline is one of the pose's three lines.
            assert body.count("\n\n") >= 2, (seed, body[-160:])
        else:
            refused += 1
            m2 = TURNED.match(log)
            assert m2, (seed, log)
            assert m2.group("appear") == appear and m2.group("title") == title, (seed, log)
            assert deltas["discipline"] == POINTS[4], (seed, deltas)
            assert deltas["karma"] == 0 and deltas["fame"] == 0, (seed, deltas)
        assert not event.get("residue"), (seed, event.get("residue"))

assert cards >= 30, cards
assert accepted >= 5 and refused >= 5, (accepted, refused)
assert poses <= {1, 2, 3}, poses
# A less famous hero is asked for the clothed and swimsuit shoots, so the ladder is walked.
faint = set()
for seed in range(1, 26):
    for event in run(seed, fame=45)["events"]:
        if event.get("table") != "registry:ScenarioFameMagazine":
            continue
        m = CARD.match(event["body"])
        assert m, (seed, event["body"][:200])
        faint.add(APPEAR[m.group("appear")])
assert faint and faint <= {1, 2, 3}, faint
assert len(faint | poses) >= 2, (faint, poses)
# A hero with no fame is never offered a shoot.
for event in run(1, fame=0)["events"]:
    assert event.get("table") != "registry:ScenarioFameMagazine", "offer without fame"
print("cards:", cards, "accepted:", accepted, "refused:", refused, "poses:", sorted(poses))
print("ok")
