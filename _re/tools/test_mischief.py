"""318: the mischief card (ScenarioCrimeDelinquencyPush).

A friend asks the hero out to get up to something. Both branches move karma and
discipline; the accept branch draws a delinquent activity from
a16_delinquent-activities-enc and can get the hero caught, which hands over to
the 273 charge chain with one of the table's cartoon crime names.

The auto-policy always refuses, so both branches are reached by forcing the
choice. A forced step skips that year's priority chain (funeral/graduation/
school/prom/licence/...), so the padding length cannot be taken from an
unforced run: it is searched for the first step whose table is the card.
"""
import csv
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
CARD = "registry:ScenarioCrimeDelinquencyPush"
ACCEPT = "Sure, why not?"
REFUSE = "No, I'm not a troublemaker"
TABLE = ROOT / "gamedata_plain" / "a16_delinquent-activities-enc"

crimes = set()
activities = []
for row in csv.DictReader(open(TABLE)):
    crime = (row.get("Crime") or "").strip()
    if crime:
        crimes.add(crime)
    activity = (row.get("Activity") or "").strip()
    if activity:
        activities.append(activity)
assert len(crimes) >= 8, crimes
assert len(activities) >= 40, len(activities)


def run(spec):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as handle:
        json.dump(spec, handle)
        path = handle.name
    out = subprocess.run([str(PORT), "--script", path], capture_output=True, text=True)
    Path(path).unlink()
    assert out.returncode == 0, out.stderr[-500:]
    return json.loads(out.stdout)


BASE = {"country": "United States", "gender": "male", "ages": 40, "cash": 500}
PROMPT_TAIL = " wants to get up to some mischief with you.\n\nWhat will you do?"


def card_at(spec, choice, limit=60):
    """Force `choice` on the first step of a run that shows the mischief card."""
    for pad in range(1, limit):
        doc = run(dict(spec, choices=[-1] * pad + [choice]))
        events = doc["events"]
        if pad < len(events) and events[pad].get("table") == CARD:
            return pad, events[pad]
    raise AssertionError("no mischief card in %d steps" % limit)


def asked_name(body):
    """'<name> wants to get up to some mischief with you.' -> '<name>'."""
    assert body.endswith(PROMPT_TAIL), body
    return body[: -len(PROMPT_TAIL)]


# 1. Accept: the card names a friend, karma and discipline drop, and the body
#    reads "You and <name> <activity>." with no unreplaced token.
pad, event = card_at(dict(BASE, seed=7), 0)
assert event["title"] == "Mischief", event["title"]
assert event["choice"] == ACCEPT, event["choice"]
assert event["deltas"]["karma"] < 0 and event["deltas"]["discipline"] < 0, event["deltas"]
told = [line for line in event["logLines"] if line.startswith("I told ")]
assert len(told) == 1, event["logLines"]
assert told[0].endswith(" that sounds great."), told
friend = told[0][len("I told ") : -len(" that sounds great.")]
assert friend, told
assert event["body"].startswith("You and " + friend + " "), event["body"]
assert event["body"].endswith("."), event["body"]
assert "<" not in event["body"], event["body"]
stem = event["body"][len("You and " + friend + " ") : -1]
assert any(activity.split("<")[0].strip() and activity.split("<")[0].strip() in stem for activity in activities), (
    stem,
)
print("accept:", "age", event["age"], "friend", friend, "activity", stem[:60])

# 2. Refuse: the prompt is a prompt, and karma and discipline rise by exactly
#    the amount the accept branch drops them.
_, refused = card_at(dict(BASE, seed=7), 1)
assert refused["choice"] == REFUSE, refused["choice"]
assert refused["body"].endswith(PROMPT_TAIL), refused["body"]
assert refused["deltas"]["karma"] == -event["deltas"]["karma"], (refused["deltas"], event["deltas"])
assert refused["deltas"]["discipline"] == -event["deltas"]["discipline"], (
    refused["deltas"],
    event["deltas"],
)
says_no = [line for line in refused["logLines"] if line.startswith("I told ")]
assert says_no == ["I told %s that I am no troublemaker." % asked_name(refused["body"])], says_no
print("refuse:", "age", refused["age"], "deltas", refused["deltas"])

# 3. Age floor and presented cap: no card before 8, at most three per life
#    (Life::PresentedCountForScenario(name) < 3).
shown = 0
most = 0
youngest = None
for seed in range(1, 21):
    doc = run({"seed": seed, "country": "United States", "gender": "male", "ages": 30, "cash": 1000})
    ages = [e["age"] for e in doc["events"] if e.get("table") == CARD]
    assert len(ages) <= 3, (seed, ages)
    assert all(age >= 8 for age in ages), (seed, ages)
    shown += len(ages)
    most = max(most, len(ages))
    if ages:
        youngest = min(youngest, min(ages)) if youngest is not None else min(ages)
print("lives 1..20: cards", shown, "max per life", most, "youngest", youngest)

# 4. The caught branch hands the crime to the 273 chain: a charge card naming
#    one of the table's cartoony crimes, in the same year as the accept.
catches = []
for seed in range(1, 41):
    doc = run(
        {
            "seed": seed,
            "country": "United States",
            "gender": "male",
            "ages": 45,
            "cash": 500,
            "choices": [0] * 100,
        }
    )
    events = doc["events"]
    for index, entry in enumerate(events):
        if entry.get("table") != CARD:
            continue
        assert entry["choice"] == ACCEPT, (seed, entry["choice"])
        if index + 1 >= len(events):
            continue
        nxt = events[index + 1]
        if nxt.get("table") != "registry:ScenarioCrimeCharge":
            continue
        body = nxt["body"] or ""
        named = [crime for crime in crimes if crime in body]
        assert named, (seed, body)
        catches.append((seed, entry["age"], nxt["age"], named[0], body.split("\n")[0]))
assert catches, "no mischief accept was ever caught in seeds 1..40"
for seed, age, next_age, crime, line in catches:
    assert "%s and are facing" % crime in line, line
    assert next_age >= age, (age, next_age)
print("catches:", len(catches), "first", catches[0][1:4])
print("ok")
