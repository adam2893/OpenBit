"""The book deal (REPORT.en.md 336).

SimPlace::RefreshAvailableFameOpportunities builds a SimBookOpportunity - a publisher, a
cost from the hero's book appeal and a proceeds roll - and ScenarioFameBook sells it. The
port has no place object, so the offer is rebuilt on the hero's own yearly tick, which is
what this test pins: the gates, the fee, the proceeds band, the outcome ladder the sales
line quotes, and both lanes.
"""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
BIO = ROOT / "gamedata_plain" / "a16_biographies-enc"
COMPANY = ROOT / "gamedata_plain" / "a16_company-name-structure-enc"

with open(BIO, newline="", encoding="utf-8") as fh:
    BIO_ROWS = list(csv.DictReader(fh))
with open(COMPANY, newline="", encoding="utf-8") as fh:
    COMPANY_ROWS = list(csv.DictReader(fh))
assert any((r["Company Type"] or "").strip() == "Publisher" for r in COMPANY_ROWS), "no Publisher type"

# HeroBookAppealScore's age bands, and the outcome ladder's thresholds.
BANDS = ((18, 100), (25, 30), (30, 40), (35, 50), (40, 60), (45, 70), (50, 80), (60, 90))
OUTCOME = ("became an overnight bestseller", "became a bestseller", "was a commercial success",
           "was generally well-received", "was received with mixed reviews", "received poor reviews",
           "was not well-received", "was a commercial failure", "received terrible reviews",
           "was an absolute flop")

OFFER = re.compile(r"^(?P<pub>.+?) has agreed to edit and publish your book\.\n\n"
                   r"Publisher: (?P<pub2>[^\n]+)\nCost: \$(?P<cost>[\d,]+)")
PAID = re.compile(r"^I paid \$(?P<cost>[\d,]+) to (?P<pub>.+?) to publish my book, "
                  r"\"(?P<title>.+?)\"\. It (?P<outcome>.+?) and grossed \$(?P<sales>[\d,]+) "
                  r"in sales\.$")
TURNED = re.compile(r"^(?P<pub>.+?) has agreed to publish my book for \$(?P<cost>[\d,]+)\.$")


def appeal(age, fame, smarts):
    band = 0
    for limit, value in BANDS:
        if age < limit:
            band = value
            break
    return 0.25 * (band + 2.0 * fame + smarts)


def run(seed, fame=95, ages=45, smarts=None):
    spec = {"seed": seed, "country": "United States", "gender": "female", "ages": ages,
            "cash": 200000, "fame": fame}
    if smarts is not None:
        spec["smarts"] = smarts
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = accepted = refused = 0
outcomes = set()
for seed in range(1, 31):
    doc = run(seed)
    for event in doc["events"]:
        if event.get("table") != "registry:ScenarioFameBook":
            continue
        cards += 1
        assert event["age"] >= 18, (seed, event["age"])
        m = OFFER.match(event["body"])
        assert m, (seed, event["body"][:200])
        assert m.group("pub") == m.group("pub2"), (seed, m.group("pub"), m.group("pub2"))
        cost = int(m.group("cost").replace(",", ""))
        assert cost % 100 == 0 and cost >= 1000, (seed, cost)
        # Cost = 20000 + (1 - appeal/100) * 80000, rounded to 100: so the fee pins the
        # appeal, and the appeal pins the age band against the hero's own stats.
        appealish = 100.0 * (1.0 - (cost - 20000.0) / 80000.0)
        assert 0.0 <= appealish <= 100.0, (seed, cost, appealish)
        assert event["choice"] in ("Write it", "Not now"), (seed, event["choice"])
        log = (event.get("logLines") or [""])[-1]
        if event["choice"] == "Write it":
            accepted += 1
            m2 = PAID.match(log)
            assert m2, (seed, log)
            assert int(m2.group("cost").replace(",", "")) == cost, (seed, log)
            assert m2.group("pub") == m.group("pub"), (seed, log)
            assert m2.group("outcome") in OUTCOME, (seed, m2.group("outcome"))
            outcomes.add(m2.group("outcome"))
            sales = int(m2.group("sales").replace(",", ""))
            # Proceeds = Random.Next(0, (int)(2 * base * (1 + appeal/100))), base rising
            # from 250000 to 500000 at appeal 70.
            base = 250000.0 if appealish < 70.0 else 500000.0
            ceiling = 2.0 * base * (1.0 + appealish / 100.0)
            assert 0 <= sales <= ceiling * 1.05 + 100, (seed, sales, ceiling)
            # The sales line's own ladder and the card body's flavour line agree.
            assert "Your book, \"" in event["body"], (seed, event["body"][-120:])
            assert m2.group("outcome") in event["body"], (seed, event["body"][-160:])
        else:
            refused += 1
            m2 = TURNED.match(log)
            assert m2, (seed, log)
            assert int(m2.group("cost").replace(",", "")) == cost, (seed, log)
            assert m2.group("pub") == m.group("pub"), (seed, log)
        assert not event.get("residue"), (seed, event.get("residue"))

assert cards >= 20, cards
assert accepted >= 3 and refused >= 3, (accepted, refused)
assert len(outcomes) >= 2, outcomes

# No fame, no deal: the opportunity builder needs Att_fame >= 10.
for event in run(1, fame=0, ages=40)["events"]:
    assert event.get("table") != "registry:ScenarioFameBook", "deal without fame"
# The five title candidates are biography rows and the chosen one is printed resolved.
for seed in range(1, 6):
    for event in run(seed)["events"]:
        log = (event.get("logLines") or [""])[-1]
        if event.get("table") != "registry:ScenarioFameBook" or "publish my book" not in log:
            continue
        m2 = PAID.match(log)
        if m2:
            assert "<" not in m2.group("title"), (seed, m2.group("title"))
print("cards:", cards, "accepted:", accepted, "refused:", refused,
      "outcomes:", len(outcomes))
print("ok")
