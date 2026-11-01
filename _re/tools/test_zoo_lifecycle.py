"""344: the zoo's animals, pregnant, born and dead."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLE = ROOT / "gamedata_plain" / "a16_zoo-animals-enc"

PREGNANT = re.compile(r"^Your zoo's veterinarian, (?P<vet>.+), is reporting that "
                      r"(?P<name>.+) the (?P<breed>.+?) (?P<what>laid some eggs!|is pregnant!)$")
PREGNANT_LOG = re.compile(r"^I discovered (?P<name>.+) the (?P<breed>.+?) "
                          r"(?P<what>laid some eggs\.|is pregnant\.)$")
BIRTH = re.compile(r"^(?P<lead>A newborn (?P<eggs_gender>male|female) "
                   r"(?P<eggs_breed>.+?) just hatched from one of (?P<eggs_parent>.+) the "
                   r"(?P<eggs_pbreed>.+?)'s? eggs\.|(?P<live_parent>.+) the (?P<live_pbreed>.+?) "
                   r"just gave birth to a baby (?P<live_gender>male|female) (?P<live_breed>.+?)!)"
                   r"\n\nWhat will you name (him|her)\?$")
BIRTH_LOG_EGGS = re.compile(r"^A baby (male|female) (?P<breed>.+?) hatched from one of "
                            r"(?P<parent>.+) the (?P<pbreed>.+?)'s? eggs\. I named (him|her) "
                            r'"(?P<name>.+)"\.$')
BIRTH_LOG_LIVE = re.compile(r"^(?P<parent>.+) the (?P<pbreed>.+?) gave birth to a (male|female) "
                            r'(?P<breed>.+?), whom I named "(?P<name>.+)"\.$')
FAILURE = re.compile(r"^(None of (?P<eggs_name>.+) the (?P<eggs_breed>.+?)'s eggs hatched\.|"
                     r"(?P<live_name>.+) the (?P<live_breed>.+?)'s newborn (?P<newborn>.+?) "
                     r"died during childbirth\.)$")
FAILURE_LOG_EGGS = re.compile(r"^I discovered that all of (?P<name>.+) the (?P<breed>.+?)'s "
                              r"eggs were duds\.$")
FAILURE_LOG_LIVE = re.compile(r"^I discovered that (?P<name>.+) the (?P<breed>.+?)'s baby was "
                              r"stillborn\.$")
DEATH = re.compile(r"^(?P<name>.+) the (?P<breed>.+?) died at the age of (?P<age>\d+)\.\n\n"
                   r"(?P<what>.+)$")
MEMORIAL = re.compile(r"^Hold a memorial for (him|her)$")
TAXIDERMY = re.compile(r"^Have (him|her) taxidermied$")

LIFECYCLE_TABLES = ("registry:ScenarioZooAgeUpAnimalPregnancy",
                    "registry:ScenarioZooAgeUpAnimalBirth",
                    "registry:ScenarioZooAgeUpAnimalBirthFailure",
                    "registry:ScenarioZooAnimalDeath")


def animals():
    rows = []
    with open(TABLE, newline="", encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            rows.append(row)
    return rows


ROWS = animals()
EGG_BREEDS = {r["Breed"].strip().lower() for r in ROWS if (r.get("Lays Eggs") or "").strip()}
LIVE_BREEDS = {r["Breed"].strip().lower() for r in ROWS if not (r.get("Lays Eggs") or "").strip()}


def run(seed=3, ages=60, zoo=True, choices=None):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 3000000, "zoo": zoo, "only": "zoo-lifecycle"}
    if choices is not None:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True, capture_output=True,
                          cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


def cards(doc):
    return [e for e in doc["events"] if (e.get("table") or "") in LIFECYCLE_TABLES]


def journal(event):
    return list(event.get("logLines") or [])


def zero(event):
    deltas = event.get("deltas") or {}
    return all(v == 0 for k, v in deltas.items() if k != "happiness") and \
        (deltas.get("happiness") or 0) == 0


def main():
    seen = {"pregnancy": 0, "birth": 0, "failure": 0, "death": 0, "eggs": 0, "live": 0}
    preg_ages = {}
    for seed in range(1, 61):
        doc = run(seed=seed)
        got = cards(doc)
        # The family takes the wheel's turn, so a quiet life is allowed; what
        # must never happen is a malformed card on the ones that do fire.
        if not got:
            continue
        for event in got:
            table = event["table"].split(":")[-1]
            body = event.get("body") or ""
            lines = journal(event)
            if table == "ScenarioZooAgeUpAnimalPregnancy":
                seen["pregnancy"] += 1
                m = PREGNANT.match(body)
                assert m, (seed, body)
                assert event["title"] in ("Eggs", "Pregnancy"), (seed, event["title"])
                breed = m.group("breed").lower()
                if m.group("what") == "laid some eggs!":
                    seen["eggs"] += 1
                    assert event["title"] == "Eggs", (seed, event["title"])
                    assert breed in EGG_BREEDS, (seed, breed)
                else:
                    seen["live"] += 1
                    assert event["title"] == "Pregnancy", (seed, event["title"])
                    assert breed in LIVE_BREEDS, (seed, breed)
                logs = [journal_line for journal_line in lines
                        if PREGNANT_LOG.match(journal_line)]
                assert logs, (seed, lines)
                log = PREGNANT_LOG.match(logs[-1])
                assert log.group("name") == m.group("name"), (seed, log.group("name"), m.group("name"))
                assert log.group("breed") == m.group("breed"), (seed, log.group("breed"))
                assert zero(event), (seed, event.get("deltas"))
                preg_ages.setdefault(seed, []).append(event["age"])
                assert event["age"] >= 18, (seed, event["age"])
            elif table == "ScenarioZooAgeUpAnimalBirth":
                seen["birth"] += 1
                m = BIRTH.match(body)
                assert m, (seed, body)
                if m.group("eggs_breed"):
                    breed = m.group("eggs_breed")
                    assert m.group("eggs_pbreed") == breed, (seed, m.group("eggs_pbreed"), breed)
                    assert breed.lower() in EGG_BREEDS, (seed, breed)
                else:
                    breed = m.group("live_breed")
                    assert m.group("live_pbreed") == breed, (seed, m.group("live_pbreed"), breed)
                    assert breed.lower() in LIVE_BREEDS, (seed, breed)
                assert event["title"] == "Baby " + " ".join(
                    word[:1].upper() + word[1:] for word in breed.split()), (seed, event["title"], breed)
                births = [line for line in lines
                          if BIRTH_LOG_EGGS.match(line) or BIRTH_LOG_LIVE.match(line)]
                assert births, (seed, lines)
                log = BIRTH_LOG_EGGS.match(births[-1]) or BIRTH_LOG_LIVE.match(births[-1])
                assert log.group("breed") == breed, (seed, log.group("breed"), breed)
                assert zero(event), (seed, event.get("deltas"))
            elif table == "ScenarioZooAgeUpAnimalBirthFailure":
                seen["failure"] += 1
                m = FAILURE.match(body)
                assert m, (seed, body)
                if m.group("eggs_breed"):
                    assert event["title"] == "Duds", (seed, event["title"])
                    assert m.group("eggs_breed").lower() in EGG_BREEDS, (seed, m.group("eggs_breed"))
                    assert any(FAILURE_LOG_EGGS.match(line) for line in lines), (seed, lines)
                else:
                    assert event["title"] == "Death", (seed, event["title"])
                    assert m.group("live_breed").lower() in LIVE_BREEDS, (seed, m.group("live_breed"))
                    assert any(FAILURE_LOG_LIVE.match(line) for line in lines), (seed, lines)
                assert zero(event), (seed, event.get("deltas"))
            else:
                seen["death"] += 1
                m = DEATH.match(body)
                assert m, (seed, body)
                choice = event.get("choice") or ""
                assert MEMORIAL.match(choice) or TAXIDERMY.match(choice), (seed, choice)
                assert m.group("what").startswith(m.group("name") + " the " + m.group("breed") +
                                                  " died from natural causes."), (seed, m.group("what"))
                assert int(m.group("age")) >= 1, (seed, m.group("age"))
                deltas = event.get("deltas") or {}
                assert (deltas.get("happiness") or 0) == -8, (seed, deltas)
    for seed, ages in preg_ages.items():
        for a, b in zip(ages, ages[1:]):
            assert b - a >= 4, (seed, ages)
    # A hero without a zoo never gets one of these cards.
    doc = run(seed=5, zoo=False)
    assert not cards(doc), ("card without a zoo", cards(doc)[:2])
    # No card before the hero is an adult.
    for seed in range(1, 9):
        for event in cards(run(seed=seed, ages=17)):
            assert event["age"] >= 18, (seed, event["age"])
    assert seen["pregnancy"] and seen["birth"] and seen["failure"] and seen["death"], seen
    assert seen["eggs"] and seen["live"], seen
    print("cards:", json.dumps(seen, sort_keys=True))
    print("ok")


main()
