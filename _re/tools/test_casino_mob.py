"""The casino's mob favours (REPORT.en.md 345).

A made man who owns a house gets asked to launder the family's take and to lend
out an empty storage room for an interrogation. Both cards are pushed from
SimCasino::IncrementAge in the native game; the port offers them from the family
wheel while the hero is both made and a casino owner.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

LAUNDER = "registry:ScenarioCasinoAgeUpMafiaMemberLaunder"
INTERROG = "registry:ScenarioCasinoAgeUpMafiaMemberInterrogation"
TIERS = "Mafia member"  # REPORT 360

BODY_LAUNDER = re.compile(
    r'^"(?P<nick>.+)" (?P<name>.+?) is asking for a favor on behalf of '
    r'(?P<poss>his|her) Mafia family\.\n\n(He|She) asks if you could launder some '
    r'money through your casino and promises to give you a cut if you agree\.$')
BODY_INTERROG = re.compile(
    r'^(?P<name>.+?) is asking for a favor on behalf of (his|her) Mafia family\.'
    r'\n\n(He|She) asks if (his|her) Mafia family members could "interrogate" '
    r'someone in one of your casino\'s empty storage rooms and offers you some '
    r'cash if you agree\.$')
LOG_LAUNDER = re.compile(
    r'^My casino\'s (?:%s), "(?P<nick>.+)" (?P<name>.+?), asked me to launder '
    r'\$(?P<amount>\d+) through my casino and offered me \$(?P<cut>\d+) to do so\.$'
    % TIERS)
LOG_INTERROG = re.compile(
    r'^(?P<name>.+?) asked me to let (?P<obj>him|her) interrogate someone in my '
    r'casino\'s empty storage room\. (He|She) offered me \$(?P<offer>\d+) to let '
    r'(?P=obj)\.$')


def run(seed, cash=2000000, casino=True, mafia=True, ages=60, choices=None,
        only=None):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": cash, "casino": casino, "mafia": mafia}
    if choices is not None:
        spec["choices"] = choices
    if only is not None:
        spec["only"] = only
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-400:]
    return json.loads(proc.stdout)


def lines(event):
    out = []
    for line in event.get("logLines") or []:
        out.extend(line.split("\n"))
    return out


seen = {"launder": 0, "interrog": 0}
lanes = {"Do it": 0, "Refuse": 0}
ages = {"launder": {}, "interrog": {}}
for seed in range(1, 21):
    doc = run(seed)
    mine = {"launder": [], "interrog": []}
    for event in doc["events"]:
        table = event.get("table")
        if table not in (LAUNDER, INTERROG):
            continue
        kind = "launder" if table == LAUNDER else "interrog"
        seen[kind] += 1
        mine[kind].append(event["age"])
        assert event["choice"] in ("Do it", "Refuse"), (seed, event["choice"])
        lanes[event["choice"]] += 1
        assert event["residue"] == [], (seed, event["residue"])
        assert not any(v for v in event["deltas"].values()), (seed, event["deltas"])
        assert event["title"] == "Favor", (seed, event["title"])
        body = event["body"]
        # The card's own journal line is not necessarily the first line of the
        # age-up's log: other families write theirs first, so hunt for the line
        # this card owns and read the answer straight after it.
        log = lines(event)
        if kind == "launder":
            m = BODY_LAUNDER.match(body)
            assert m, (seed, body)
            at = next((i for i, line in enumerate(log) if LOG_LAUNDER.match(line)), None)
            assert at is not None, (seed, log)
            n = LOG_LAUNDER.match(log[at])
            assert n.group("name") == m.group("name"), (seed, body, log[at])
            assert n.group("nick") == m.group("nick"), (seed, body, log[at])
            tail = "I accepted the bribe." if event["choice"] == "Do it" else "I refused the bribe."
            assert log[at + 1] == tail, (seed, log)
        else:
            m = BODY_INTERROG.match(body)
            assert m, (seed, body)
            at = next((i for i, line in enumerate(log) if LOG_INTERROG.match(line)), None)
            assert at is not None, (seed, log)
            n = LOG_INTERROG.match(log[at])
            assert n.group("name") == m.group("name"), (seed, body, log[at])
            tail = "I accepted the bribe" if event["choice"] == "Do it" else "I refused the bribe"
            assert log[at + 1:] == ["", tail], (seed, log)
    ages["launder"][seed] = mine["launder"]
    ages["interrog"][seed] = mine["interrog"]

assert seen["launder"] and seen["interrog"], seen
assert lanes["Do it"] and lanes["Refuse"], lanes

# Neither favour comes round again the next year.
for kind, perSeed in ages.items():
    for seed, years in perSeed.items():
        years.sort()
        for a, b in zip(years, years[1:]):
            assert b - a >= 2, (kind, seed, years)

# The money: a hero who owns the house and is made is offered both favours and no
# other life ever is.
for spec in ({"casino": True, "mafia": False}, {"casino": False, "mafia": True}):
    doc = run(3, **spec)
    assert not [e for e in doc["events"] if e.get("table") in (LAUNDER, INTERROG)], spec

# Accepting pays the cut or the bribe; refusing does not. Same seed, same family,
# so the only difference between the two lives is the answer.
accept = run(7, only="casino-mafia-launder", choices=[0] * 60)["final"]["money"]
refuse = run(7, only="casino-mafia-launder", choices=[1] * 60)["final"]["money"]
assert accept > refuse, (accept, refuse)

print("cards:", seen, "| lanes:", lanes, "| money %d vs %d" % (accept, refuse))
print("ok")
