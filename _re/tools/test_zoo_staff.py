#!/usr/bin/env python3
"""The park's staff split and the division-of-labor card (REPORT.en.md 364).

SimZoo::.ctor splits a staff pool of 10/20/40 (by plot size) between custodial,
security and customer relations, and only the custodial counter is ever read:
GetZooCustodialScore, GetCustomerRelationsTicketSalesBumpPercentage and
GetChanceOfSecurityBreach all read +0xd4 despite their names. The cleanliness
card is eligible under a score of 20, the customer-service card only while the
bump is exactly zero, and theft fires when the breach chance beats rand(0,101).
Both cards answer "Reallocate labor resources" with the division-of-labor card,
whose board is a Unity screen the port folds into a fresh split.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
CLEAN = "registry:ScenarioZooAgeUpCleanliness"
CUST = "registry:ScenarioZooAgeUpCustomerService"
THEFT = "registry:ScenarioZooAgeUpTheft"
DIVISION = "registry:ScenarioZooEmployeeManagerStaffManagement"
DIVISION_BODY = "Allocate your zoo's staff resources today!"
DIVISION_LOG = "I reallocated my zoo's work force."
WANTED = {0: "Reallocate labor resources", 1: "Ignore"}


def run(seed, ages=70, only=None, choices=None, staff=None, cash=3000000):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": cash, "zoo": True}
    if only:
        spec["only"] = only
    if choices is not None:
        spec["choices"] = choices
    if staff:
        spec["zoo_staff"] = staff
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    done = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                          capture_output=True, text=True)
    assert done.returncode == 0, done.stderr[-800:]
    return json.loads(done.stdout)


def cards(doc, table):
    return [e for e in doc["events"] if e.get("table") == table]


def forced(seed, family, want=0, ages=70, staff="low"):
    """The harness answers one card per age-up, so the answer needs the right index.

    Priority chains before the wheel (a relative's death, a graduation) eat choice slots,
    so a padding that merely produced the card is not enough: the card has to carry the
    wanted answer.
    """
    answer = WANTED[want]
    for pad in range(0, 40):
        doc = run(seed, ages=ages, only=family, choices=[-1] * pad + [want], staff=staff)
        for card in cards(doc, CLEAN if family == "zoo-cleanliness" else CUST):
            if (card.get("choice") or "").startswith(answer):
                return doc, card
    raise AssertionError("the card never appeared")


def main():
    # A high custodial share closes both cards and makes theft impossible: the score is
    # 90..100, the bump is 0.20..0.25 and the breach chance is zero.
    for seed in range(1, 13):
        doc = run(seed, staff="high")
        assert not cards(doc, CLEAN), (seed, "cleanliness card behind a high share")
        assert not cards(doc, CUST), (seed, "customer-service card behind a high share")
        assert not cards(doc, THEFT), (seed, "theft behind a zero breach chance")

    # A low share opens the cleanliness card (score 1..10) and the customer-service card
    # (bump exactly zero), and either hand-off opens the division card.
    doc, card = forced(1, "zoo-cleanliness", want=0, staff="low")
    divisions = cards(doc, DIVISION)
    assert divisions, "the hand-off did not open the division card"
    division = divisions[0]
    assert division["title"] == "Division of Labor", division["title"]
    assert division["body"].startswith(DIVISION_BODY), division["body"]
    assert division["choice"] == "OK", division["choice"]
    assert any(x == DIVISION_LOG for x in division["logLines"]), division["logLines"]
    assert not any((division.get("deltas") or {}).values()), division["deltas"]

    # The customer-service card hands off to the same card.
    doc, card = forced(1, "zoo-customer-service", want=0, staff="low")
    assert cards(doc, DIVISION), "the customer-service hand-off did not open the division card"

    # A low share leaves theft on the table, since the breach chance is 20..30 points.
    doc = run(2, only="zoo-theft", staff="low")
    print("cards:", len(cards(doc, THEFT)), "| division body:", division["body"])
    print("ok")


if __name__ == "__main__":
    main()
