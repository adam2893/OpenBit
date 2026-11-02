"""The zoo's merchandise order card (REPORT.en.md 363)."""

import json
import re
import subprocess
import tempfile

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"
TABLE = "a16_zoo-gift-shop-item-enc"
CARDS = "registry:ScenarioZooAgeUpMerchOrder"
FAMILY = "zoo-merch-order"
TITLES = {"Just came in!", "That new gift smell", "Flying off the the shelves!"}
BODY = re.compile(
    r"^Determine how many more (?P<plural>.+?) featuring (?P<animal>.+?) the (?P<breed>.+?) "
    r"you'd like to order from your merchandise supplier\.\n\n"
    r"Current Unit Count: (?P<units>[\d,]+)\n"
    r"Cost Per Unit: (?P<unit>[^\n]+)\n"
    r"Available Capital: (?P<capital>[^\n]+)\n"
    r"Featured Animal: (?P<featured>[^\n]+)$")
ARRIVED = re.compile(
    r"^Your merch order of additional (?P<plural>.+?) featuring (?P<animal>.+?) the (?P<breed>.+?) "
    r"just arrived!\n\nUnits Ordered: (?P<ordered>[\d,]+)\nNew Unit Count: (?P<count>[\d,]+)$")
JOURNAL = re.compile(
    r"^I ordered [\d,]+ more .+? featuring .+? the .+? to restock the shelves in my zoo's gift shop\.$")


def run(seed=3, ages=60, choices=None, zoo=True, only=None):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    if zoo:
        spec["zoo"] = True
    if only:
        spec["only"] = only
    if choices is not None:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    r = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise AssertionError(r.stderr[-400:])
    return json.loads(r.stdout)


def cards(doc):
    return [e for e in doc["events"] if (e.get("table") or "") == CARDS]


def arrived(doc):
    return [e for e in doc["events"] if (e.get("title") or "") in TITLES]


def catalogue():
    rows = [line.split(",") for line in (DATA / TABLE).read_text().splitlines()]
    return {row[1] for row in rows[1:] if len(row) > 1}, {row[5] for row in rows[1:] if len(row) > 5}


def forced(want, seed=3, ages=60):
    """The harness answers one card per age-up, so the answer needs the right index."""
    name = "Order more" if want == 0 else "Nothing"
    for pad in range(0, 60):
        doc = run(seed=seed, ages=ages, only=FAMILY, choices=[-1] * pad + [want])
        found = cards(doc)
        if found and found[0]["choice"] == name:
            return doc, found[0]
    raise AssertionError("the card never took the forced index")


def main():
    # The card is a park card: without one it never fires, and neither does it for a minor.
    assert not cards(run(seed=3, ages=40, zoo=False)), "the card fired without a zoo"
    assert not cards(run(seed=3, ages=17)), "the card fired for a minor"

    plurals, scores = catalogue()
    seen = {"Order more": 0, "Nothing": 0}
    features = set()
    for seed in range(1, 21):
        doc = run(seed=seed)
        for card in cards(doc):
            m = BODY.match(card["body"])
            assert m, card["body"]
            assert card["choice"] in seen, card["choice"]
            seen[card["choice"]] += 1
            assert m.group("plural") in plurals, m.group("plural")
            assert m.group("featured").lower() == (m.group("animal") + " the " + m.group("breed")).lower()
            assert m.group("capital").startswith("$"), m.group("capital")
            assert m.group("unit").startswith("$"), m.group("unit")
            assert int(m.group("units").replace(",", "")) <= 2, m.group("units")
            assert not any((card.get("deltas") or {}).values()), card["deltas"]
            features.add(m.group("animal"))
    assert seen["Order more"] and seen["Nothing"], seen

    # The order lane restocks the shelves, pays for it and logs the order.
    doc, card = forced(0)
    letters = [x for x in card["logLines"] if JOURNAL.match(x)]
    assert letters, card["logLines"]
    arriveds = arrived(doc)
    assert arriveds, "no arrival card followed the order"
    m = ARRIVED.match(arriveds[0]["body"])
    assert m, arriveds[0]["body"]
    assert 1 <= int(m.group("ordered").replace(",", "")) <= 500, m.group("ordered")
    assert int(m.group("count").replace(",", "")) <= 500, m.group("count")
    assert int(m.group("count").replace(",", "")) >= int(m.group("ordered").replace(",", "")), m.group("count")
    assert not any((arriveds[0].get("deltas") or {}).values()), arriveds[0]["deltas"]

    # The refusal lane leaves the shelves alone, so no arrival card follows it.
    doc, card = forced(1)
    assert not [x for x in card["logLines"] if JOURNAL.match(x)], card["logLines"]
    # Later cards in the same life may still order on their own draw, so only this answer
    # is checked: the refusal writes no order line and no arrival of its own.
    assert not [e for e in arrived(doc) if e["age"] == card["age"]], "the refusal restocked"

    print("cards:", sum(seen.values()), "| lanes:", seen, "| animals:", len(features))
    print("ok")


if __name__ == "__main__":
    main()
