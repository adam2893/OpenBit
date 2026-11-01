
import json, re, subprocess, sys, tempfile
from pathlib import Path

"""The casino's two act cards (REPORT.en.md 350).

A punter either complains that the act in residence does not fit the house theme or
compliments the booking, and both cards only ever fire for an act whose own theme is
NOT the house's. The act is pinned through the harness hook (casino_act), because a
port casino books an act only now and then: see the ceilings in REPORT 350.
"""

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TIERS = r"(?:Beginner|Amateur|Novice|Regular|High Roller)"

COMPLAINT = "registry:ScenarioCasinoAgeUpGamblerComplaintEntertainmentAct"
COMPLIMENT = "registry:ScenarioCasinoAgeUpGamblerComplimentEntertainmentAct"

BODY_COMPLAINT = re.compile(
    r"^Your casino's " + TIERS + r", (?P<punter>.+?), is complaining that "
    r"(?P<act>.+?)'s performances ruin your casino's vibe\.\n\n(?:she|he) saying their "
    r"(?P<atheme>.+?)-themed act doesn't fit the (?P<theme>.+?) theme you're trying "
    r"to establish\.$")
BODY_COMPLIMENT = re.compile(
    r"^Your casino's " + TIERS + r", (?P<punter>.+?), is complimenting you on booking "
    r"(?P<act>.+?) to perform at your casino\.\n\n(?:she|he) says it's her favorite "
    r"attraction\.$")
BAN_LOG = re.compile(
    r"^I banned my casino's gambler, (?P<punter>.+), from my casino after she "
    r"complained about (?P<act>.+)\.$")
SORRY_LOG = re.compile(
    r"^I apologized to my casino's gambler, (?P<punter>.+), for having booked "
    r"(?P<act>.+)\.$")
IGNORE_LOG = re.compile(
    r"^I ignored my casino's gambler, (?P<punter>.+)'s, complaint about (?P<act>.+)\.$")


def run(seed, which, only, choices=None, ages=70):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 5000000, "casino": True, "only": only}
    if which:
        spec["casino_act"] = which
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


# --- the complaint -----------------------------------------------------------
lanes = {}
ages = []
for seed in range(1, 21):
    for card in cards(run(seed, "diff", "casino-act-complaint"), COMPLAINT):
        assert card["title"] == "Complaint", card
        m = BODY_COMPLAINT.match(card.get("body") or "")
        assert m, (seed, card.get("body"))
        act = m.group("act")
        assert m.group("atheme").lower() != m.group("theme").lower(), (seed, m.groups())
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
        assert hit.group("act") == act, (seed, act, logged)
        assert not any((card.get("deltas") or {}).values()), (seed, card.get("deltas"))
        ages.append((seed, card["age"]))
assert len(lanes) == 3, lanes

# Neither card comes round again the next two years (native: YearsSinceScenario > 2).
for seed in sorted({s for s, _ in ages}):
    years = sorted(a for s, a in ages if s == seed)
    for a, b in zip(years, years[1:]):
        assert b - a >= 3, (seed, years)

# --- the compliment ---------------------------------------------------------
praise = 0
praise_ages = []
for seed in range(1, 21):
    for card in cards(run(seed, "diff", "casino-act-compliment"), COMPLIMENT):
        assert card["title"] == "Compliment", card
        assert BODY_COMPLIMENT.match(card.get("body") or ""), (seed, card.get("body"))
        assert card.get("choice") in ("Thank him", "Thank her",
                                      "Give him some free games", "Give her some free games",
                                      "Ignore him", "Ignore her"), card
        assert not any((card.get("deltas") or {}).values()), (seed, card.get("deltas"))
        # The card writes no journal line at all: its narrative slot is never filled.
        assert not any(l.lower().startswith(("i thanked", "i gave", "i ignored"))
                       for l in lines(card)), (seed, lines(card))
        praise += 1
        praise_ages.append((seed, card["age"]))
assert praise, "no compliment card"
for seed in sorted({s for s, _ in praise_ages}):
    years = sorted(a for s, a in praise_ages if s == seed)
    for a, b in zip(years, years[1:]):
        assert b - a >= 5, (seed, years)

# --- controls ---------------------------------------------------------------
# An act that matches the house theme satisfies neither card.
for only, table in (("casino-act-complaint", COMPLAINT),
                    ("casino-act-compliment", COMPLIMENT)):
    for seed in range(1, 6):
        assert not cards(run(seed, "same", only), table), (seed, only)

print("complaint lanes:", lanes, "| complaints:", sum(lanes.values()), "| praise:", praise)
print("ok")
