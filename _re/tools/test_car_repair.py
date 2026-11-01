"""323: the car's condition, the repair card and the scrap push."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

REPAIR = "registry:ScenarioCarRepair"
SCRAP = "registry:ScenarioCarScrapPush"
# 0x3559924 / 0x355a730: the card only knows three labels, and the Golden Wrench is an
# in-app purchase this port does not carry, so the two free lanes are all it offers.
REPAIR_LABEL = "Repair it"
DECLINE_LABEL = "I might fix it later"
SCRAP_LABEL = "Scrap it"
REPAIR_LOG = re.compile(r"^I repaired the (.+) on my (.+)\.$")
SCRAP_LOG = re.compile(r"^I had to scrap my (.+)\.$")
BODY = re.compile(r"^Your (?P<car>.+) car has (?P<terms>.+)\.\n\nRepair: \$(?P<cost>[0-9,]+)\nVehicle Condition: (?P<cond>\d+)\n\nWhat will you do\?$")
SCRAP_BODY = re.compile(r"^Your car will no longer start and is beyond repair\.\n\nAge: (?P<years>\d+) Years$")


def run(seed=7, ages=24, cash=50000, car_cond=50, car_rel=1, choices=None, car=True,
        only="car"):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": cash}
    if car:
        spec.update({"car": True, "carCondition": car_cond, "carReliability": car_rel})
    if only:
        spec["only"] = only
    if choices is not None:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


def cards(doc, table=REPAIR):
    return [e for e in doc["events"] if e.get("table") == table]


def forced(table, want, seed=7, ages=24, cash=50000, car_cond=50, car_rel=1):
    """Pad the harness's per-year vote until the wanted card takes the forced index."""
    for pad in range(1, 60):
        for idx in range(2):
            doc = run(seed=seed, ages=ages, cash=cash, car_cond=car_cond,
                      car_rel=car_rel, choices=[-1] * pad + [idx])
            found = cards(doc, table)
            if found and found[0]["choice"] == want:
                return doc, found[0], pad
    raise AssertionError("card never took the forced index")


def main():
    # 1. The repair card: the title is the car, the body quotes the part and the price.
    doc, card, pad = forced(REPAIR, REPAIR_LABEL)
    body = BODY.match(card["body"])
    assert body, card["body"]
    car = body.group("car")
    assert card["title"] == car, (card["title"], car)
    assert card["age"] >= 16
    cost = int(body.group("cost").replace(",", ""))
    assert cost >= 1
    assert 0 <= int(body.group("cond")) <= 100
    logs = [l for l in card["logLines"] if REPAIR_LOG.match(l)]
    assert logs, card["logLines"]
    assert logs[0].endswith(" on my %s." % car)
    assert card["deltas"] == {"karma": 0, "discipline": 0, "happiness": 0, "health": 0,
                              "smarts": 0, "looks": 0, "fame": 0}, card["deltas"]
    print("repair:", card["age"], card["title"], cost, repr(card["choice"]))

    # 2. The repair charges exactly the row it printed: stop both lives at the card's own
    # year, so the only difference between them is the vote in front of that card.
    year = card["age"]
    spent_run = run(ages=year, choices=[-1] * pad + [0])
    kept_run = run(ages=year, choices=[-1] * pad + [1])
    spent = kept_run["final"]["money"] - spent_run["final"]["money"]
    assert spent == cost, (spent, cost)
    assert cards(spent_run)[-1]["choice"] == REPAIR_LABEL
    assert cards(kept_run)[-1]["choice"] == DECLINE_LABEL
    declined_logs = [l for e in cards(kept_run) for l in (e.get("logLines") or [])
                     if l == "I decided not to repair it."]
    assert declined_logs, "the decline line is missing"
    print("charged:", spent, "at age", year, "- declined lives keep it")

    # 3. A car the hero cannot afford offers one lane only.
    broke = run(cash=100, ages=20, choices=[-1] * 40)
    offers = [e["choice"] for e in cards(broke)]
    assert offers and set(offers) == {DECLINE_LABEL}, offers[:5]
    print("broke:", len(offers), "cards, all", DECLINE_LABEL)

    # 4. A car with no condition left is scrapped instead, once, and pays nothing.
    scrap = run(ages=20, car_cond=0)
    got = cards(scrap, SCRAP)
    assert len(got) == 1, [(e["age"], e["title"]) for e in got]
    one = got[0]
    m = SCRAP_BODY.match(one["body"])
    assert m, one["body"]
    assert int(m.group("years")) >= 1
    assert one["choice"] == SCRAP_LABEL
    assert one["title"] is None, one["title"]
    assert any(SCRAP_LOG.match(l) and SCRAP_LOG.match(l).group(1) for l in one["logLines"]), \
        one["logLines"]
    assert not cards(scrap), "the scrap push and the repair card must not both fire"
    assert scrap["final"]["money"] == 50000, scrap["final"]["money"]
    print("scrap:", one["age"], repr(one["logLines"][-1]))

    # 5. A hero with no car never sees either card.
    for seed in range(1, 6):
        plain = run(seed=seed, ages=24, car=False, only="")
        assert not cards(plain) and not cards(plain, SCRAP), seed
    print("no car, no card")
    print("ok")


main()
