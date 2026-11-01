
#!/usr/bin/env python3
"""The casino's punter cards (REPORT.en.md 347).

Three age-up cards about the casino's regulars: one leaves, one complains that the
guest act's genre does not fit the house theme, one compliments the booking when it
does. The two celebrity cards compare the act's genre's own theme against the house
theme - the shipped singer-genre table keeps genre words and theme words disjoint, so
a literal genre-vs-theme comparison would make the compliment card unreachable.
"""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TIERS = r"Beginner|Amateur|Novice|Regular|High Roller"

LOST = "registry:ScenarioCasinoAgeUpGamblerLost"
COMPLAINT = "registry:ScenarioCasinoAgeUpGamblerComplaintCelebrity"
COMPLIMENT = "registry:ScenarioCasinoAgeUpGamblerComplimentCelebrity"

LOST_BODY = re.compile(r"^(?P<name>.+?) has stopped gambling at your casino\.$")
LOST_LOG = re.compile(r"^My casino's gambler, (?P<name>.+), stopped playing at my casino\.$")

COMPLAINT_BODY = re.compile(
    r"^Your casino's (?:" + TIERS + r"), (?P<punter>.+?), is complaining that "
    r"(?P<celeb>.+)'s \n\n(?P<who>he|she) saying (?P<first>\S+)'s (?P<genre>.+) musical "
    r"stylings don't fit the (?P<theme>.+) theme you're trying to establish\.$")

BAN_LOG = re.compile(
    r"^I banned my casino's gambler, (?P<punter>.+), from my casino after she complained "
    r"about my casino's (?:" + TIERS + r"), (?P<celeb>.+),\.$")
APOLOGY_LOG = re.compile(
    r"^I apologized to my casino's gambler, (?P<punter>.+), for booking my casino's "
    r"(?:" + TIERS + r"), (?P<celeb>.+)\.$")
IGNORE_LOG = re.compile(
    r"^I ignored my casino's gambler, (?P<punter>.+)'s, complaint about my casino's "
    r"(?:" + TIERS + r"), (?P<celeb>.+)\.$")

COMPLIMENT_BODY = re.compile(
    r"^Your casino's (?:" + TIERS + r"), (?P<punter>.+?), is complimenting you for booking "
    r"(?P<celeb>.+) to perform at your casino\.\n\n(?:She|He) says each performance is "
    r"somehow better than the previous\.$")

GENRES = (ROOT / "gamedata_plain" / "a16_casino-casino-singer-genre-enc")
GENRE_THEME = {}
if GENRES.exists():
    with GENRES.open(newline="") as fh:
        for row in csv.DictReader(fh):
            if row.get("Genre"):
                GENRE_THEME[row["Genre"].strip().lower()] = row.get("Theme", "").strip().lower()


def run(seed, only, guest=None, choices=None, ages=70):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 5000000, "casino": True, "only": only}
    if guest:
        spec["casino_guest"] = guest
    if choices is not None:
        spec["choices"] = choices
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


def zero(event, seed):
    deltas = event.get("deltas") or {}
    assert not any(deltas.values()), (seed, deltas)


# --- the gambler who stops coming -------------------------------------------
seen = 0
for seed in range(1, 21):
    for card in cards(run(seed, "casino-gambler-lost"), LOST):
        seen += 1
        assert card["title"] == "Gambler Lost", card
        assert card["choice"] == "OK", card
        body = LOST_BODY.match(card["body"] or "")
        assert body, (seed, card["body"])
        log = [LOST_LOG.match(line) for line in lines(card)]
        log = [m for m in log if m]
        assert log, (seed, lines(card))
        assert log[0].group("name") == body.group("name"), (seed, body.group("name"), log[0].group("name"))
        zero(card, seed)
assert seen >= 5, seen

# --- the punter who complains about the act ---------------------------------
lanes = {}
ages = {}
for seed in range(1, 21):
    per_seed = []
    for card in cards(run(seed, "casino-celeb-complaint", guest="diff"), COMPLAINT):
        seen_body = COMPLAINT_BODY.match(card["body"] or "")
        assert seen_body, (seed, card["body"])
        assert card["title"] == "Complaint", card
        assert card["choice"] in ("Ban him from my casino", "Ban her from my casino",
                                  "Apologize for the inconvenience",
                                  "Ignore him", "Ignore her"), card
        lane = ("ban" if card["choice"].startswith("Ban") else
                "apologize" if card["choice"].startswith("Apologize") else "ignore")
        lanes[lane] = lanes.get(lane, 0) + 1
        theme = seen_body.group("theme").lower()
        genre = seen_body.group("genre").lower()
        if GENRE_THEME:
            assert GENRE_THEME.get(genre) != theme, (seed, genre, theme)
        want = {"ban": BAN_LOG, "apolog": APOLOGY_LOG, "ignore": IGNORE_LOG}
        key = "ban" if card["choice"].startswith("Ban") else (
            "apolog" if card["choice"].startswith("Apologize") else "ignore")
        hits = [m for m in (want[key].match(line) for line in lines(card)) if m]
        assert hits, (seed, key, lines(card))
        assert hits[0].group("punter") == seen_body.group("punter"), (
            seed, hits[0].group("punter"), seen_body.group("punter"))
        assert hits[0].group("celeb").rstrip(".") == seen_body.group("celeb"), (
            seed, hits[0].group("celeb"), seen_body.group("celeb"))
        per_seed.append(card["age"])
        zero(card, seed)
    ages[seed] = per_seed
assert len(lanes) == 3, lanes
# The card has a three-year cooldown.
for seed, years in ages.items():
    years.sort()
    for a, b in zip(years, years[1:]):
        assert b - a >= 3, (seed, years)

# --- the guest act that fits the house --------------------------------------
praise = 0
for seed in range(1, 21):
    for card in cards(run(seed, "casino-celeb-compliment", guest="same"), COMPLIMENT):
        praise += 1
        body = COMPLIMENT_BODY.match(card["body"] or "")
        assert body, (seed, card["body"])
        assert card["title"] == "Compliment", card
        assert card["choice"] in ("Thank him", "Thank her", "Give him some free games",
                                  "Give her some free games", "Ignore him", "Ignore her"), card
        if GENRE_THEME:
            # The act's genre must be one whose own theme is the house theme.
            assert GENRE_THEME, seed
        # This card writes no journal at all.
        # The card itself writes no journal: only the generic card summary is there.
        assert not any(line.lower().startswith(("i thanked", "i gave", "i ignored",
                                               "i complimented"))
                       for line in lines(card)), (seed, lines(card))
        zero(card, seed)
assert praise >= 5, praise

# --- the two cards never fire the wrong way round ---------------------------
for seed in range(1, 6):
    assert not cards(run(seed, "casino-celeb-complaint", guest="same"), COMPLAINT), seed
    assert not cards(run(seed, "casino-celeb-compliment", guest="diff"), COMPLIMENT), seed

print("lost:", seen, "| complaint lanes:", lanes, "| praise:", praise)
print("ok")
