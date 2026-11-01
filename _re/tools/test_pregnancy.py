#!/usr/bin/env python3
"""The pregnancy card and the miscarriage (REPORT.en.md 338).

The card is ScenarioPregnancy: body #50740 over the wrapper #68715, two stored
choices, the decline clearing the pregnancy and firing an unnamed -6 impact.
The resolution is next year's ShouldSufferMiscarriage: a kept pregnancy either
loses the baby (ScenarioMiscarriage) or arrives (ScenarioBirth, REPORT 151).
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

WORD = r"girlfriend|boyfriend|fiancee|fiance|wife|husband"
CARD = re.compile(
    r"^Your (?P<word>" + WORD + r"), (?P<name>.+?), is pregnant with your baby!"
    r"\n\nWhat will you do\?$")
MISC = re.compile(
    r"^Your (?P<word>" + WORD + r"), (?P<name>.+?), suffered a "
    r"(?P<desc>first-trimester|second-trimester|blighted ovum) miscarriage and lost the baby!$")
LOSS = re.compile(
    r"^Your (?P<word>" + WORD + r"), (?P<name>.+?), had (?:a|an) "
    r"(?P<type>ectopic pregnancy|molar pregnancy|stillbirth|chemical pregnancy) and lost the baby!$")
PREG_LOG = re.compile(r"^(?P<name>.+?) became pregnant with my baby!$")
MISC_LOG = re.compile(
    r"^(?P<name>.+?) suffered a (?:first-trimester|second-trimester|blighted ovum) miscarriage!$")
LOSS_LOG = re.compile(
    r"^(?P<name>.+?) had (?:a|an) "
    r"(?:ectopic pregnancy|molar pregnancy|stillbirth|chemical pregnancy) and lost the baby!$")
# The multiple-birth form ("just gave birth to twin boys!") is REPORT 340; both forms
# close the pregnancy, so both must clear the flag this walk keeps.
BIRTH_BODY = re.compile(
    r"^(?P<word>[A-Za-z]+) (?P<name>.+?), just gave birth to "
    r"(?:a baby (?:girl|boy)|(?:twin|triplet) (?:boys|girls))!$")

CHOICES = ("Keep the baby", "Don't keep the baby")
DECLINE = "We chose not to keep the baby!"
# The decline lane fires an unnamed -6 impact through vtable+0x508; the point scale
# maps -6 to the 100-point step (REPORT.en.md 338).
DECLINE_HAPPINESS = -100


def run(seed, ages=50):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "relationship": True}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-400:]
    return json.loads(proc.stdout)


cards = 0
kept = 0
declined = 0
miscarriages = 0
pregnancy_losses = 0
births = 0
for seed in range(1, 61):
    doc = run(seed)
    # One pregnancy in flight at a time: a kept card sets the flag, the resolution
    # clears it, and no second pregnancy card may appear while it is set.
    open_pregnancy = False
    for event in doc["events"]:
        title = event.get("title") or ""
        body = event.get("body") or ""
        lines = event.get("logLines") or []
        deltas = event.get("deltas") or {}
        if title == "Pregnancy":
            cards += 1
            assert not open_pregnancy, (seed, event["age"])
            match = CARD.match(body)
            assert match, (seed, body)
            name = match.group("name")
            assert event["age"] >= 18, (seed, event["age"])
            assert event.get("choice") in CHOICES, (seed, event.get("choice"))
            assert not event.get("residue"), (seed, event.get("residue"))
            journals = [l for l in lines if PREG_LOG.match(l)]
            assert journals, (seed, lines)
            assert PREG_LOG.match(journals[0]).group("name") == name, (seed, name, lines)
            # The Golden Pacifier option is IAP-gated on state the port cannot read.
            assert "Pacifier" not in body, (seed, body)
            if event["choice"] == "Keep the baby":
                kept += 1
                assert all(v == 0 for v in deltas.values()), (seed, deltas)
                open_pregnancy = True
            else:
                declined += 1
                assert DECLINE in lines, (seed, lines)
                assert deltas.get("happiness") == DECLINE_HAPPINESS, (seed, deltas)
        elif title == "Miscarriage":
            miscarriages += 1
            assert open_pregnancy, (seed, event["age"])
            open_pregnancy = False
            assert MISC.match(body), (seed, body)
            assert event.get("choice") in (None, ""), (seed, event.get("choice"))
            assert any(MISC_LOG.match(l) for l in lines), (seed, lines)
        elif title == "Pregnancy Loss":
            pregnancy_losses += 1
            assert open_pregnancy, (seed, event["age"])
            open_pregnancy = False
            assert LOSS.match(body), (seed, body)
            assert any(LOSS_LOG.match(l) for l in lines), (seed, lines)
        elif BIRTH_BODY.match(body):
            births += 1
            assert open_pregnancy, (seed, event["age"])
            open_pregnancy = False

assert cards >= 20, cards
assert births >= 10, births
assert kept >= 5, kept
assert declined >= 2, declined
assert miscarriages + pregnancy_losses >= 1, (miscarriages, pregnancy_losses)

print("pregnancy cards:", cards, "kept:", kept, "declined:", declined)
print("miscarriages:", miscarriages, "pregnancy losses:", pregnancy_losses, "births:", births)
print("ok")
