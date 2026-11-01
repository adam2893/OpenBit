"""The aircraft's own life after purchase (REPORT.en.md 334).

SimAircraft ages and wears every year, costs twelve months of its MonthlyExpense,
breaks (ScenarioAircraftRepair) and is finally scrapped (ScenarioAircraftScrapPush).
Everything measured here comes from the played cards: the repair card quotes the part
its own parts-table row names, and the scrap push removes the aircraft.
"""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
PLAIN = ROOT / "gamedata_plain"


def rows(name):
    with open(PLAIN / name, newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


AIRCRAFT = rows("a16_aircraft-enc")
PARTS = rows("a16_aircraft-parts-enc")
MODELS = set()
for r in AIRCRAFT:
    override = (r["Descriptive Name Override"] or "").strip()
    name = override or " ".join([(r["Make"] or "").strip(), (r["Model"] or "").strip()]).strip()
    suffix = (r["Descriptive Name Suffix"] or "").strip()
    MODELS.add((name + " " + suffix).strip() if suffix else name)

PART_NAMES = {(r["Part"] or "").strip() for r in PARTS if (r["Part"] or "").strip()}
BROKEN = set()
for r in PARTS:
    for term in (r["Broken Terms"] or "").split(","):
        if term.strip():
            BROKEN.add(term.strip())


def run(seed, ages=70):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 40000000, "luxury": True}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
    proc = subprocess.run([str(PORT), "--script", fh.name], cwd=str(ROOT / "port"),
                          capture_output=True, text=True)
    assert proc.returncode == 0, proc.stderr[-400:]
    return json.loads(proc.stdout)


REPAIR = re.compile(r"^Your (?P<desc>.+?) (?:needs a repair\.|has an? (?P<broken>.+?)\.)")
SCRAP = re.compile(r"^Your (?P<desc>.+?) will no longer fly and is beyond repair\.\n\n"
                   r"Age: (?P<age>\d+) Years")

repairs = 0
scraps = 0
repaired = 0
declined = 0
for seed in range(1, 41):
    data = run(seed)
    owned = None
    for event in data["events"]:
        table = event.get("table") or ""
        body = event.get("body") or ""
        log = (event.get("logLines") or [""])[-1]
        choice = event.get("choice")
        if table == "a16_aircraft-enc" and choice == "Buy it":
            owned = log[len("I bought a "):].split(" for $")[0]
        elif table == "registry:ScenarioAircraftRepair":
            repairs += 1
            assert owned is not None, (seed, "repair without an aircraft")
            m = REPAIR.match(body)
            assert m, (seed, body[:200])
            # The card names the aircraft the hero owns, and the part it quotes is a
            # row of a16_aircraft-parts-enc that carries that broken term.
            # The card names the row's Descriptive Name (its override when it has one),
            # while the purchase log and the '<aircraft>' token use Make + Model.
            assert m.group("desc") in MODELS, (seed, m.group("desc"))
            if m.group("broken"):
                term, part = m.group("broken").split(" ", 1)
                assert term in BROKEN, (seed, term)
                assert part in PART_NAMES, (seed, part)
                assert any((row["Part"] or "").strip() == part
                           and term in [x.strip() for x in (row["Broken Terms"] or "").split(",")]
                           for row in PARTS), (seed, part, term)
            assert "What will you do?" in body, (seed, body[-80:])
            assert "Repair: $" in body, (seed, body[-80:])
            assert choice in ("Repair it", "I might fix it later"), (seed, choice)
            assert not event.get("residue"), (seed, event.get("residue"))
            if choice == "Repair it":
                repaired += 1
                assert log.endswith("on my " + m.group("desc") + "."), (seed, log)
                assert "is back in service." in body or not body.endswith("?"), (seed, body[-80:])
            else:
                declined += 1
                assert log.endswith("I decided not to repair it."), (seed, log)
                # The pushed card's refusal costs a point of discipline, per REPORT 325.
                assert event["deltas"]["discipline"] < 0, (seed, event["deltas"])
        elif table == "registry:ScenarioAircraftScrapPush":
            scraps += 1
            m = SCRAP.match(body)
            assert m, (seed, body[:200])
            assert m.group("desc") in MODELS, (seed, m.group("desc"))
            assert choice == "Scrap it", (seed, choice)
            assert log.startswith(m.group("desc")[-20:]) or "beyond repair" in log, (seed, log)
            owned = None
            break  # the aircraft is gone; nothing further is owed in this life
assert repairs >= 2, repairs
assert scraps >= 2, scraps
print("repairs:", repairs, "repaired:", repaired, "declined:", declined, "scraps:", scraps)
print("ok")
