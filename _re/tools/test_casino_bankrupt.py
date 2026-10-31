"""320: the casino's sixth losing year closes it for good."""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

BANKRUPT = "Bankrupt"
LEDGER = "Casino Ledger"
BODY = ("Your casino has been bankrupt for 5 years.\n\n"
        "You were forced to close it for good.")


def run(seed=7, ages=30, cash=3000000, casino_bank=-1000000000):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": cash, "casino": True, "only": "casino"}
    if casino_bank:
        spec["casinoBank"] = casino_bank
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


def cards(doc, title):
    return [e for e in doc["events"] if e.get("title") == title]


def main():
    # Six losing years: the pass starts the year the hero turns 18, so the card
    # lands on the sixth one. (The literal says five, the code counts six.)
    got = cards(run(ages=30), BANKRUPT)
    assert len(got) == 1, [(e["age"], e["title"]) for e in got]
    card = got[0]
    assert card["body"] == BODY, card["body"]
    assert card["choice"] == "Ok", card["choice"]
    assert 23 <= card["age"] <= 25, card["age"]
    for stat in ("karma", "discipline", "happiness", "health", "smarts", "looks"):
        assert card["deltas"].get(stat, 0) == 0, (stat, card["deltas"])

    # Five losing years (ages 18..22) are not enough; the sixth closes the casino.
    # In "only" mode the harness reads "ages" as the hero's age cap.
    early = run(ages=22)
    assert cards(early, BANKRUPT) == [], [(e["age"], e["title"]) for e in cards(early, BANKRUPT)]

    # Once the house is shut down the books stop: no ledger after the closing card.
    doc = run(ages=40)
    shut = cards(doc, BANKRUPT)
    assert len(shut) == 1, len(shut)
    later = [e for e in cards(doc, LEDGER) if e["age"] > shut[0]["age"]]
    assert later == [], [(e["age"], e["title"]) for e in later]
    assert [e for e in cards(doc, BANKRUPT)] == shut, "the card fired twice"

    # The closing costs the hero nothing: the casino pot was never his money (319).
    # Same seed, same years, pot untouched -- the only difference is the card, so
    # the two lives must end with the same money.
    control = run(ages=40, casino_bank=1000000000)
    assert cards(control, BANKRUPT) == [], "the control life went bankrupt"
    assert control["final"]["money"] == doc["final"]["money"], (
        control["final"]["money"], doc["final"]["money"])

    # An untouched pot may drown on its own (the default house runs at a loss), but
    # the card closes the casino once: never twice in a life.
    for seed in range(1, 7):
        seen = cards(run(seed=seed, ages=40, casino_bank=0), BANKRUPT)
        assert len(seen) <= 1, (seed, [(e["age"], e["title"]) for e in seen])

    print("card:", card["age"], card["title"], "->", card["choice"])
    print("five losing years: no card; six: closed; ledgers after closing: 0")
    print("hero cash after closing:", doc["final"]["money"])
    print("ok")


main()
