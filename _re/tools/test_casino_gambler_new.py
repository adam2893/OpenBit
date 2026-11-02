
#!/usr/bin/env python3
"""The new regular's card (REPORT.en.md 357).

A gambler starts frequenting the casino: the card names them, prints their age,
occupation, gambling style and favourite games as table rows and their money and
addiction as the two bar values, and logs the arrival. The native adds the
gambler to the casino and raises its popularity by two; the port counts the
regular and pushes its style onto the same list the other casino cards draw
from.

Eligibility draws nothing (casino, hero age >= 18, not in prison, more than four
years since the last one), so the card lands every fifth year of an owned casino.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TYPES = r"casual gambler|high roller|Mafia member"
STYLES = {"Beginner", "Amateur", "Novice", "Regular", "High Roller"}

BODY = re.compile(
    r"^A (?P<type>" + TYPES + r") named (?P<name>.+?) "
    r"is now frequently gambling at your casino\.\n\n"
    r"Age: (?P<age>\d+)\n"
    r"Occupation: (?P<occ>[^\n]+)\n"
    r"Gambling Style: (?P<style>Beginner|Amateur|Novice|Regular|High Roller)\n"
    r"Favorite Games: (?P<games>[^\n]+)\n\n"
    r"Money: \$(?P<money>[\d,]+)\n"
    r"Addiction: (?P<addiction>\d+)$")
JOURNAL = re.compile(
    r"^A (?P<style>Beginner|Amateur|Novice|Regular|High Roller) gambler named "
    r"(?P<name>.+?) began frequenting my casino\.$")


def run(seed, only="casino-gambler-new", casino=True, ages=70):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 2000000, "only": only}
    if casino:
        spec["casino"] = True
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    try:
        out = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                             capture_output=True, text=True, check=True).stdout
    finally:
        Path(path).unlink()
    return json.loads(out)


def cards(doc):
    return [e for e in doc["events"]
            if (e.get("table") or "") == "registry:ScenarioCasinoAgeUpGamblerNew"]


def main():
    total = 0
    styles = set()
    names = set()
    gaps_ok = True
    for seed in range(1, 11):
        doc = run(seed)
        ages = []
        for event in cards(doc):
            total += 1
            m = BODY.match(event["body"])
            assert m, (seed, event["age"], event["body"])
            assert event["title"] == "New Gambler", event["title"]
            assert event["choice"] == "OK", event["choice"]
            age = int(m.group("age"))
            assert 21 <= age <= 65, (seed, age)
            assert m.group("style") in STYLES, (seed, m.group("style"))
            styles.add(m.group("style"))
            # GamblerTypeString calls a five-style regular a high roller and
            # everybody else a casual gambler; the port has no mafia regulars.
            expected = "high roller" if m.group("style") == "High Roller" else "casual gambler"
            assert m.group("type") == expected, (seed, m.groupdict())
            assert m.group("occ").strip() == m.group("occ"), (seed, "occupation padded")
            assert m.group("games").strip(), (seed, "no favourite games")
            assert int(m.group("addiction")) <= 100, (seed, "addiction")
            assert int(m.group("money").replace(",", "")) >= 500, (seed, "money")
            names.add(m.group("name"))
            j = JOURNAL.match(event["logLines"][-1])
            assert j, (seed, event["logLines"][-1])
            assert j.group("style") == m.group("style"), (seed, "journal style")
            assert j.group("name") == m.group("name"), (seed, "journal name")
            ages.append(event["age"])
        if any(b - a < 5 for a, b in zip(ages, ages[1:])):
            gaps_ok = False
    assert gaps_ok, "two new regulars walked in inside the same five years"
    assert total >= 40, total
    assert styles == STYLES, styles
    assert len(names) > 1, names
    assert not cards(run(3, casino=False)), "new regular without a casino"
    print("cards: %d | styles: %s | names: %d" % (total, sorted(styles), len(names)))
    print("ok")


if __name__ == "__main__":
    main()
