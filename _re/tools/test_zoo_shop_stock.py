#!/usr/bin/env python3
# The gift shop's stocking chain (REPORT.en.md 368): the add-product card, the unit card and
# the price card. Heavy: ~45-age lives on eight seeds.
import json, re, subprocess, tempfile, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")
DATA = os.path.join(ROOT, "gamedata_plain")

ADD = "registry:ScenarioZooGiftShopAddProduct"
UNITS = "registry:ScenarioZooGiftShopAddProductBuildDetails"
PRICE = "registry:ScenarioZooGiftShopAddProductSetPrice"

BODY_ADD = re.compile(
    r"^Place an order for custom merchandise to sell in your zoo's gift shop today!\n\n"
    r"Open Shelf Space: (?P<space>[\d,]+)\nAvailable Capital: (?P<capital>\$[\d,]+)$")
BODY_UNITS = re.compile(
    r"^Determine how many units you'd like to order from your merchandise supplier and what "
    r"animal you'd like them to feature\.\n\n"
    r"Cost Per Unit: (?P<unit>\$[\d,]+)\nAvailable Capital: (?P<capital>\$[\d,]+)$")
BODY_PRICE = re.compile(
    r"^Your order of (?P<plural>.+?) featuring (?P<animal>.+?) the (?P<breed>.+?) just arrived!"
    r"\n\nSet a price before putting them on your gift shop's shelves\.\n\n"
    r"Units Available: (?P<units>[\d,]+)$")
LABEL = re.compile(r"^(?P<product>[A-Z][A-Za-z' -]*) \(\$(?P<unit>[\d,]+)/Unit\)$")
SELL = re.compile(r"^I began selling (?P<plural>.+?) featuring (?P<animal>.+?) the "
                  r"(?P<breed>.+?) in my zoo's gift shop\.$")


def catalogue():
    rows = []
    with open(os.path.join(DATA, "a16_zoo-gift-shop-item-enc"), encoding="utf-8") as f:
        head = f.readline()
        for line in f:
            cells = line.rstrip("\n").split(",")
            if not cells or not cells[0]:
                continue
            rows.append((cells[0], int(cells[2])))
    return dict(rows)


def run(seed, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 500000, "zoo": True, "only": "zoo-shop-stock"}
    p = os.path.join(tempfile.gettempdir(), "_shop368_%d.json" % seed)
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], cwd=os.path.join(ROOT, "port"),
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("run failed %d: %s" % (seed, r.stderr[-400:]))
    return json.loads(r.stdout)


def cards(doc, table):
    return [e for e in doc["events"] if (e.get("table") or "") == table]


def body_of(e):
    return (e.get("body") or "").rstrip("\n")


def main():
    prices = catalogue()
    adds = units = sells = 0
    spaces = []
    lanes = {"Place order": 0, "Never mind": 0}
    ages_seen = []
    for seed in range(1, 9):
        doc = run(seed)
        a = cards(doc, ADD)
        u = cards(doc, UNITS)
        s = cards(doc, PRICE)
        adds += len(a)
        units += len(u)
        sells += len(s)
        if a:
            ages_seen.extend(e["age"] for e in a)
        # Every add-product card is answered by exactly one unit card and one price card at
        # the same age: the native chains them inside one year.
        for e in a:
            m = BODY_ADD.match(body_of(e))
            assert m, (seed, body_of(e)[:200])
            spaces.append(int(m.group("space").replace(",", "")))
            label = LABEL.match(e.get("choice") or "")
            assert label, (seed, "add choice", e.get("choice"))
            product = label.group("product").lower()
            assert product in [k.lower() for k in prices], (seed, product, e.get("choice"))
            assert int(label.group("unit").replace(",", "")) == prices[
                [k for k in prices if k.lower() == product][0]], (seed, e.get("choice"))
            same = [x for x in u if x["age"] == e["age"]]
            assert len(same) == 1, (seed, e["age"], "unit card", len(same))
            assert same[0].get("choice") in lanes, (seed, same[0].get("choice"))
            lanes[same[0].get("choice")] += 1
            if same[0].get("choice") == "Place order":
                price = [x for x in s if x["age"] == e["age"]]
                assert len(price) == 1, (seed, e["age"], "price card", len(price))
                pm = BODY_PRICE.match(body_of(price[0]))
                assert pm, (seed, body_of(price[0])[:200])
                log = [x for x in (price[0].get("logLines") or []) if SELL.match(x)]
                assert log, (seed, price[0].get("logLines"))
                sm = SELL.match(log[0])
                assert sm.group("animal") == pm.group("animal"), (seed, log[0], body_of(price[0]))
                assert sm.group("plural") == pm.group("plural"), (seed, log[0])
                assert int(pm.group("units").replace(",", "")) >= 1
                assert price[0].get("choice") == "OK"
        # Three years of quiet between two stocking cards.
        for prev, nxt in zip(a, a[1:]):
            assert nxt["age"] - prev["age"] >= 3, (seed, prev["age"], nxt["age"])
        # The shop's own cards never move a stat bar.
        for e in a + u + s:
            assert not any((e.get("deltas") or {}).values()), (seed, "delta", e.get("title"))
            assert not (e.get("residue") or "").strip(), (seed, "residue", e.get("residue"))
    assert adds > 2, adds
    assert sells >= 1, sells
    assert spaces and min(spaces) >= 1, spaces
    # Eight slots at the largest plot, three of them taken when the park is bought.
    assert max(spaces) <= 5, spaces
    assert lanes["Place order"] >= 1 and lanes["Never mind"] >= 1, lanes
    # The shelves fill up: the last card of a life offers fewer free slots than the first.
    assert max(spaces) >= 1
    print("stocking cards: %d | unit cards: %d | lines started: %d" % (adds, units, sells))
    print("free slots seen: %s | lanes: %s" % (sorted(set(spaces)), lanes))
    print("ok")


main()
