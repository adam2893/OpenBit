#!/usr/bin/env python3
# The gift shop's product card (REPORT.en.md 369): the shelf profile, its two hand-offs to
# the stocking chain's cards, the retire answer and the capacity line. Heavy: ~80-year
# lives on six seeds plus three forced lanes.
import json, os, re, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")

PROFILE = "registry:ScenarioZooGiftShopProductProfile"
UNITS = "registry:ScenarioZooGiftShopProductOrderUnits"    # REPORT 370
PRICE = "registry:ScenarioZooGiftShopProductAdjustPrice"   # REPORT 370

BODY = re.compile(
    r"Merch Type: (?P<product>.+?)\n"
    r"Featured Animal: (?P<animal>.+?) the (?P<breed>.+?)\n"
    r"Sale Price: (?P<price>\$[\d,]+)\n"
    r"Last Year's Revenue: (?P<revenue>\$[\d,]+)\n"
    r"Units Sold Last Year: (?P<sold>[\d,]+)\n"
    r"Units Remaining: (?P<left>[\d,]+)\n"
    r"Popularity: (?P<pop>-?[\d,]+)$")
GONE = re.compile(
    r"^This product's sales have diminished because (?P<animal>.+?) the (?P<breed>.+?) "
    r"is no longer in your zoo\.\n\n")
CAPACITY = re.compile(
    r"^You can't order more (?P<plural>.+?) because your gift shop is already at capacity "
    r"for this product\.\n\nCurrent Unit Count: (?P<count>[\d,]+)$")
CHOICES = {"Adjust the price", "Order more units", "Retire this product"}


def run(seed, ages=80, choices=None, only="zoo-shop-profile"):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 500000, "zoo": True}
    if only:
        spec["only"] = only
    if choices is not None:
        spec["choices"] = choices
    p = os.path.join(tempfile.gettempdir(), "_shop369_%d.json" % seed)
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], cwd=os.path.join(ROOT, "port"),
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("run failed %d: %s" % (seed, r.stderr[-400:]))
    return json.loads(r.stdout)


def cards(doc, table):
    return [e for e in doc["events"] if (e.get("table") or "") == table]


def at(doc, age, table):
    return [e for e in doc["events"]
            if e["age"] == age and (e.get("table") or "") == table]


def main():
    seen = 0
    lanes = {}
    gone = capacity = 0
    for seed in range(1, 7):
        doc = run(seed)
        rows = cards(doc, PROFILE)
        assert rows, (seed, "no product card")
        for e in rows:
            seen += 1
            body = (e.get("body") or "").rstrip("\n")
            assert e.get("title"), (seed, "untitled", body[:80])
            words = e["title"].split()
            assert words[-1][0].isupper(), (seed, "title case", e["title"])
            assert e.get("choice") in CHOICES, (seed, e.get("choice"))
            lanes[e["choice"]] = lanes.get(e["choice"], 0) + 1
            m = CAPACITY.match(body)
            if m:
                capacity += 1
                assert int(m.group("count").replace(",", "")) >= 500, (seed, m.group("count"))
                continue
            g = GONE.match(body)
            if g:
                gone += 1
                body = body[g.end():]
            m = BODY.match(body)
            assert m, (seed, "body", body[:160])
            assert m.group("product")[0].isupper(), (seed, m.group("product"))
            assert m.group("animals" if False else "breed"), (seed, "breed")
            assert m.group("price").startswith("$") and m.group("revenue").startswith("$")
            assert int(m.group("left").replace(",", "")) >= 0
            assert m.group("pop").lstrip("-").isdigit(), (seed, m.group("pop"))
            # The shelf's own cards never move a stat bar.
            assert not any((e.get("deltas") or {}).values()), (seed, "delta", e["title"])
            assert not (e.get("residue") or "").strip(), (seed, "residue", e.get("residue"))
        # A wheel ticket rations itself: two quiet years follow every card.
        for prev, nxt in zip(rows, rows[1:]):
            assert nxt["age"] - prev["age"] >= 3, (seed, prev["age"], nxt["age"])

    # The three answers, forced on the first card of the first life.
    # choices[] is indexed by call, and a cardless year swallows the value, so the index
    # that reaches the product card is found by walking the first few dozen calls and
    # keeping the run whose card actually took the lane.
    LANES = {"Adjust the price": 0, "Order more units": 1, "Retire this product": 2}

    def forced(lane):
        for c in range(0, 44):
            d = run(1, ages=44, choices=[-1] * c + [LANES[lane]])
            hits = [e for e in d["events"]
                    if (e.get("table") or "") == PROFILE and e.get("choice") == lane]
            if hits:
                return d, hits[0]
        return None, None

    d, ev = forced("Adjust the price")
    assert ev, "the adjust answer could not be forced"
    assert at(d, ev["age"], PRICE), (ev["age"], "adjust opened no price card")
    assert at(d, ev["age"], PRICE)[0]["title"] == "Merch Price"

    d, ev = forced("Order more units")
    assert ev, "the order answer could not be forced"
    assert at(d, ev["age"], UNITS), (ev["age"], "order opened no unit card")
    assert at(d, ev["age"], UNITS)[0]["title"] == "Merch Order"

    d, ev = forced("Retire this product")
    assert ev, "the retire answer could not be forced"
    after = [c["title"] for c in cards(d, PROFILE) if c["age"] > ev["age"]]
    assert ev["title"] not in after, (ev["title"], "the retired shelf came back", after[:4])

    # A park that is not the hero's offers nothing.
    doc = run(1, ages=20, only=None)
    assert not cards(doc, PROFILE), "a card without a zoo"
    print("product cards: %d | gone-animal bodies: %d | capacity lines: %d"
          % (seen, gone, capacity))
    print("lanes: %s" % lanes)
    print("forced lanes: adjust, order, retire all took")
    print("ok")


main()
