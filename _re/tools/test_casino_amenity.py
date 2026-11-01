
import json, re, subprocess, tempfile
from pathlib import Path

"""The casino's two amenity cards (REPORT.en.md 351).

One punter complains that a tacky amenity ruins the floor, another praises an amenity
that fits the house theme. The complaint wants an amenity whose theme does NOT match the
house's; the compliment wants one that does. The amenity is pinned through the harness
hook (casino_amenity), because a port casino only builds what the patron visit rolls.
"""

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TIERS = r"(?:Beginner|Amateur|Novice|Regular|High Roller)"

COMPLAINT = "registry:ScenarioCasinoAgeUpGamblerComplaintAmenity"
COMPLIMENT = "registry:ScenarioCasinoAgeUpGamblerComplimentAmenity"

BODY_COMPLAINT = re.compile(
    r"^Your casino's " + TIERS + r", (?P<punter>.+?), is complaining that your casino "
    r"is tacky\.\n\n(?:she|he) saying your casino's (?P<amenity>.+) is the worst part\.$")
BODY_COMPLIMENT = re.compile(
    r"^Your casino's " + TIERS + r", (?P<punter>.+?), is complimenting you on the quality "
    r"of your casino's (?P<amenity>.+?)\.\n\n(?:She|He) says it's (?:her|his) favorite "
    r"place to go when (?:she's|he's) not playing .+\.$")
BAN_LOG = re.compile(
    r"^I banned my casino's gambler, (?P<punter>.+), from my casino after she complained "
    r"about my casino's (?P<amenity>.+)\.$")
SORRY_LOG = re.compile(
    r"^I apologized to my casino's gambler, (?P<punter>.+), for having a (?P<amenity>.+) "
    r"in my casino\.$")
IGNORE_LOG = re.compile(
    r"^I ignored my casino's gambler, (?P<punter>.+)'s, complaint about my casino's "
    r"(?P<amenity>.+)\.$")


def run(seed, which, only, ages=70):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 5000000, "casino": True, "only": only}
    if which:
        spec["casino_amenity"] = which
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    done = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                          capture_output=True, text=True)
    assert done.returncode == 0, done.stderr[-800:]
    return json.loads(done.stdout)


def cards(doc, table):
    return [e for e in doc["events"] if e.get("table") == table]


def lines(event):
    out = []
    for entry in event.get("logLines") or []:
        out.extend(part for part in entry.split("\n") if part)
    return out


lanes = {}
ages = []
for seed in range(1, 21):
    for card in cards(run(seed, "diff", "casino-amenity-complaint"), COMPLAINT):
        assert card["title"] == "Complaint", card
        m = BODY_COMPLAINT.match(card.get("body") or "")
        assert m, (seed, card.get("body"))
        amenity = m.group("amenity")
        choice = card.get("choice") or ""
        key = ("ban" if choice.startswith("Ban") else
               "apologize" if choice.startswith("Apologize") else
               "ignore" if choice.startswith("Ignore") else None)
        assert key, (seed, choice)
        lanes[key] = lanes.get(key, 0) + 1
        logged = lines(card)
        want = {"ban": BAN_LOG, "apologize": SORRY_LOG, "ignore": IGNORE_LOG}[key]
        hit = next((lm for lm in (want.match(l) for l in logged) if lm), None)
        assert hit, (seed, key, logged)
        assert hit.group("punter") == m.group("punter"), (seed, logged)
        assert hit.group("amenity") == amenity, (seed, amenity, logged)
        assert not any((card.get("deltas") or {}).values()), (seed, card.get("deltas"))
        ages.append((seed, card["age"]))
assert len(lanes) == 3, lanes

for seed in sorted({s for s, _ in ages}):
    years = sorted(a for s, a in ages if s == seed)
    for a, b in zip(years, years[1:]):
        assert b - a >= 3, (seed, years)

praise = 0
praise_ages = []
for seed in range(1, 21):
    for card in cards(run(seed, "same", "casino-amenity-compliment"), COMPLIMENT):
        assert card["title"] == "Compliment", card
        assert BODY_COMPLIMENT.match(card.get("body") or ""), (seed, card.get("body"))
        assert card.get("choice") in ("Thank him", "Thank her",
                                      "Give him some free games", "Give her some free games",
                                      "Ignore him", "Ignore her"), card
        assert not any((card.get("deltas") or {}).values()), (seed, card.get("deltas"))
        # No journal line: this card's narrative slot is never written.
        assert not any(l.lower().startswith(("i thanked", "i gave", "i ignored"))
                       for l in lines(card)), (seed, lines(card))
        praise += 1
        praise_ages.append((seed, card["age"]))
assert praise, "no compliment card"
for seed in sorted({s for s, _ in praise_ages}):
    years = sorted(a for s, a in praise_ages if s == seed)
    for a, b in zip(years, years[1:]):
        assert b - a >= 5, (seed, years)

# A pinned amenity that matches the house theme never draws the complaint.
# (The other direction cannot be a control: the port's own casino also builds amenities
# as it ages, so a pinned mismatched one does not exclude a matching one existing.)
for seed in range(1, 6):
    assert not cards(run(seed, "same", "casino-amenity-complaint"), COMPLAINT), seed

print("complaint lanes:", lanes, "| complaints:", sum(lanes.values()), "| praise:", praise)
print("ok")
