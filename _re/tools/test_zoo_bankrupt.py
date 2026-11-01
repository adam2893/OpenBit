"""321: the zoo's books and the federal inspector's bankruptcy warning."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

BANKRUPT = "Bankrupt"
BODY_HEAD = ("The federal inspector is informing you that your zoo has accrued too "
             "much debt to keep it operational.")
AFFORDABLE_TAIL = ("\n\nThey said if you don't do something about it right now, the "
                   "government will confiscate your zoo.\n\nWhat will you do?")
INVEST_LOG = re.compile(r"^I invested (-?\$[\d,]+) to save my zoo\.$")
FOLLOW_UP = "Your investment kept your zoo afloat for at least another year!"


def run(seed=7, bank=-1000000000, ages=30, choices=None, cash=3000000):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": cash, "zoo": True, "only": "zoo",
            "zooBank": bank}
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


def cards(doc, title=BANKRUPT):
    return [e for e in doc["events"] if e.get("title") == title]


def zoo_cards(doc):
    return [e for e in doc["events"] if (e.get("table") or "").startswith("a16_zoo")]


def forced(seed, want, bank, ages=40):
    """The harness answers one card a year; pad until the wanted card takes the vote."""
    for pad in range(1, 60):
        for idx in range(2):
            doc = run(seed=seed, bank=bank, ages=ages, choices=[-1] * pad + [idx])
            found = cards(doc)
            if found and found[0]["choice"] == want:
                return doc
    raise AssertionError("card never took the forced index")


def main():
    # A debt no life can pay: one single-button card, and the park is gone.
    doc = run()
    got = cards(doc)
    assert len(got) == 1, [(e["age"], e["title"]) for e in got]
    card = got[0]
    assert card["choice"] == "OK", card["choice"]
    assert card["body"].startswith(BODY_HEAD), card["body"]
    # Ceiling: the recovered native concatenation reads awkwardly; the pieces are the
    # native literals, so the test pins the shape rather than a smooth sentence.
    assert " said because you don't have the funds in your bank account to bring it " \
           "out of debt, " in card["body"], card["body"]
    assert card["body"].endswith(" immediately confiscating your zoo."), card["body"]
    assert "he's" in card["body"] or "she's" in card["body"], card["body"]
    assert card["deltas"] == {"karma": 0, "discipline": 0, "happiness": 0,
                              "health": 0, "smarts": 0, "looks": 0, "fame": 0}, \
        card["deltas"]
    assert not [e for e in zoo_cards(doc) if e["age"] > card["age"]], "zoo survived"
    assert len(cards(doc)) == 1, "the inspector came twice"

    # An affordable debt: two buttons, and the harness default lets the zoo go.
    default = run(bank=-1000000, ages=30)
    got = cards(default)
    assert len(got) == 1, [(e["age"], e["title"]) for e in got]
    assert got[0]["choice"] == "Let it go", got[0]["choice"]
    assert got[0]["body"] == BODY_HEAD + AFFORDABLE_TAIL, repr(got[0]["body"])
    assert not [e for e in zoo_cards(default) if e["age"] > got[0]["age"]], \
        "a closed zoo still shows cards"

    # Closing charges the hero nothing and does not depend on the depth of the debt:
    # both debts drown on the third losing year, the park closes on the same age and
    # the two wallets match.
    assert got[0]["age"] == card["age"], (got[0]["age"], card["age"])
    assert default["final"]["money"] == doc["final"]["money"], (
        default["final"]["money"], doc["final"]["money"])

    # The investment lane: the shortfall is charged and the park stays open.
    invested = forced(7, "Invest more money", -1000000)
    card = cards(invested)[0]
    assert card["body"].startswith(BODY_HEAD), card["body"]
    assert card["body"].endswith(FOLLOW_UP), repr(card["body"][-80:])
    logs = [l for l in card["logLines"] if INVEST_LOG.match(l)]
    assert logs, card["logLines"]
    amount = int(INVEST_LOG.match(logs[0]).group(1).replace("$", "").replace(",", ""))
    assert amount > 0, logs
    # The park survives the card, so zoo cards keep coming afterwards.
    assert [e for e in zoo_cards(invested) if e["age"] > card["age"]], \
        "the investment did not keep the zoo open"
    # Investing costs the hero more than letting it go (the later zoo cards can earn
    # some of it back, so the check is a direction, not an equality).
    assert invested["final"]["money"] < default["final"]["money"], (
        invested["final"]["money"], default["final"]["money"])

    # A debt the books can clear never calls the inspector in.
    for seed in range(1, 7):
        assert cards(run(seed=seed, bank=-5000, ages=40)) == [], seed

    print("unaffordable: one OK card, zoo closed, hero money untouched")
    print("affordable default: Let it go")
    print("invested:", logs[0], "-> park stays open")
    print("ok")


main()
