#!/usr/bin/env python3
# The product card's two answer cards (REPORT.en.md 370): the "Order more units" card with
# its arrival card, and the "Adjust the price" card. Heavy: ~80-year lives on five seeds.
import json, os, re, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")

PROFILE = "registry:ScenarioZooGiftShopProductProfile"
ORDER = "registry:ScenarioZooGiftShopProductOrderUnits"
ADJUST = "registry:ScenarioZooGiftShopProductAdjustPrice"
ARRIVED = {"Just came in!", "That new gift smell", "Flying off the the shelves!"}

ORDER_BODY = re.compile(
    r"^Determine how many more (?P<plural>.+?) featuring (?P<animal>.+?) the (?P<breed>.+?) "
    r"you'd like to order from your merchandise supplier\.\n\n"
    r"Current Unit Count: (?P<count>[\d,]+)\n"
    r"Cost Per Unit: (?P<unit>\$[\d,]+)\n"
    r"Available Capital: (?P<capital>\$[\d,]+)\n"
    r"Featured Animal: (?P<featured>.+?) the (?P<breed2>.+?)\n"
    r"Popularity: (?P<pop>-?[\d,]+)$")
ADJUST_BODY = re.compile(
    r"^Adjust the price of your gift shop's (?P<plural>.+?)\.\n\n"
    r"Featured Animal: (?P<animal>.+?) the (?P<breed>.+?)\n"
    r"Cost Per Unit: (?P<unit>\$[\d,]+)\n"
    r"Current Sale Price: (?P<price>\$[\d,]+)\n"
    r"Units Sold Last Year: (?P<sold>[\d,]+)\n"
    r"Units Remaining: (?P<left>[\d,]+)\n"
    r"Popularity: (?P<pop>-?[\d,]+)$")
PRICE_LOG = re.compile(r"^You adjusted the price for your (?P<plural>.+?) from "
                       r"(?P<old>\$[\d,]+) to (?P<new>\$[\d,]+)\.$")
ORDER_LOG = re.compile(r"^I ordered (?P<count>[\d,]+) more (?P<plural>.+?) featuring "
                       r"(?P<animal>.+?) the (?P<breed>.+?) to restock the shelves in my "
                       r"zoo's gift shop\.$")
ARRIVED_BODY = re.compile(
    r"^Your merch order of additional (?P<plural>.+?) featuring (?P<animal>.+?) the "
    r"(?P<breed>.+?) just arrived!\n\n"
    r"Units Ordered: (?P<ordered>[\d,]+)\n"
    r"New Unit Count: (?P<count>[\d,]+)$")


def run(seed, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 500000, "zoo": True, "only": "zoo-shop-profile"}
    p = os.path.join(tempfile.gettempdir(), "_shop370_%d.json" % seed)
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], cwd=os.path.join(ROOT, "port"),
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("run failed %d: %s" % (seed, r.stderr[-400:]))
    return json.loads(r.stdout)


def cards(doc, table):
    return [e for e in doc["events"] if (e.get("table") or "") == table]


def lines(event):
    return [x for x in (event.get("logLines") or []) if x.strip()]


def main():
    orders = adjusts = placed = refused = arrivals = 0
    for seed in range(1, 6):
        doc = run(seed)
        rows = cards(doc, ORDER)
        fixes = cards(doc, ADJUST)
        for e in rows:
            orders += 1
            body = (e.get("body") or "").rstrip("\n")
            assert e.get("title") == "Merch Order", (seed, e.get("title"))
            assert e.get("choice") in ("Place order", "Never mind"), (seed, e.get("choice"))
            m = ORDER_BODY.match(body)
            assert m, (seed, "order body", body[:160])
            assert int(m.group("pop").lstrip("-")) >= 0
            assert not any((e.get("deltas") or {}).values()), (seed, "delta", e["title"])
            assert not (e.get("residue") or "").strip(), (seed, "residue", e.get("residue"))
            if e["choice"] == "Never mind":
                refused += 1
                assert not [x for x in lines(e) if ORDER_LOG.match(x)], (seed, lines(e))
                continue
            placed += 1
            got = [x for x in lines(e) if ORDER_LOG.match(x)]
            assert got, (seed, "order without its journal", lines(e))
            lg = ORDER_LOG.match(got[0])
            assert lg.group("plural").lower() == m.group("plural").lower()
            # The arrival card is queued by the same answer and drains at the same age.
            arrived = [x for x in doc["events"]
                       if x["age"] == e["age"] and (x.get("title") or "") in ARRIVED]
            assert arrived, (seed, e["age"], "no arrival card")
            arrivals += 1
            am = ARRIVED_BODY.match((arrived[0].get("body") or "").rstrip("\n"))
            assert am, (seed, "arrival body", arrived[0].get("body"))
            assert am.group("plural").lower() == m.group("plural").lower()
        for e in fixes:
            adjusts += 1
            body = (e.get("body") or "").rstrip("\n")
            assert e.get("title") == "Merch Price", (seed, e.get("title"))
            assert e.get("choice") == "OK", (seed, e.get("choice"))
            m = ADJUST_BODY.match(body)
            assert m, (seed, "price body", body[:160])
            got = [x for x in lines(e) if PRICE_LOG.match(x)]
            assert got, (seed, "price without its journal", lines(e))
            lg = PRICE_LOG.match(got[0])
            assert lg.group("old") != lg.group("new"), (seed, "the price never moved", got[0])
            assert lg.group("plural").lower() == m.group("plural").lower()
            assert not any((e.get("deltas") or {}).values()), (seed, "delta", e["title"])
    assert orders and adjusts, (orders, adjusts)
    assert placed and refused, (placed, refused)
    assert arrivals >= placed, (arrivals, placed)
    print("order cards: %d (placed %d, refused %d) | arrival cards: %d | price cards: %d"
          % (orders, placed, refused, arrivals, adjusts))
    print("ok")


main()
