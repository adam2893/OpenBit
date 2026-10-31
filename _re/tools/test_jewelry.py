"""304: the jewelry shop card -- the jewel's cells, its price band and the buy line."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"
BODY = re.compile(r"^Buy this (?:([\d.]+)-carat )?(.+?) today!$")


def table(name):
    with open(DATA / (name + "-enc"), newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def number(text):
    return float("".join(ch for ch in (text or "") if ch.isdigit() or ch == ".") or 0)


def title(text):
    return " ".join(word[:1].upper() + word[1:] for word in text.split(" "))


TYPES = {r["Category"].strip().lower(): r for r in table("a16_jewelry-types")}
GEMS = {r["Gemstone"].strip().lower(): r for r in table("a16_jewelry-gemstones")}
METALS = {r["Metal"].strip().lower() for r in table("a16_jewelry-metals")}


def run(seed, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 500000}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    try:
        out = subprocess.run([str(PORT), "--script", path], capture_output=True,
                             text=True, cwd=str(ROOT / "port"), check=True)
    finally:
        Path(path).unlink()
    return json.loads(out.stdout)["events"]


cards = 0
for seed in range(1, 41):
    for event in run(seed):
        if event.get("table") != "registry:ScenarioJewelShop":
            continue
        cards += 1
        lines = (event.get("body") or "").split("\n")
        match = BODY.match(lines[0])
        assert match, (seed, lines[0])
        carats, item = match.groups()

        cards_rows = {}
        for line in lines[1:]:
            if not line.strip():
                continue
            key, _, value = line.partition(": ")
            cards_rows[key] = value

        # The body's item is lowercase '<gem> <type>'; find that pair in the tables.
        type_row = None
        for category, row in TYPES.items():
            if not item.lower().endswith(" " + category):
                continue
            gem = item[: -len(category) - 1].lower()
            if gem in GEMS:
                type_row, gem_row = row, GEMS[gem]
                break
        assert type_row is not None, (seed, item)
        assert event.get("title") == title(item), (seed, event.get("title"), item)

        assert not type_row["Is Plural"].strip(), (seed, item)
        if type_row["Show Carats"].strip():
            assert carats, (seed, lines[0])
            value = float(carats)
            lo, hi = number(gem_row["Min Carats"]), number(gem_row["Max Carats"])
            assert lo <= value <= hi, (seed, value, lo, hi)
            assert cards_rows["Carats"] == carats, (seed, cards_rows)
        else:
            assert carats is None, (seed, lines[0])
            assert "Carats" not in cards_rows, (seed, cards_rows)

        assert cards_rows["Gemstone"].strip().lower() == gem_row["Gemstone"].strip().lower(), (seed, cards_rows)
        assert cards_rows["Metal"].strip().lower() in METALS, (seed, cards_rows)

        price = int(cards_rows["Price"].replace("$", "").replace(",", ""))
        assert price >= number(gem_row["Min Cost"]), (seed, price)
        assert price <= number(gem_row["Max Cost"]), (seed, price)

        assert event.get("choice") == "Buy it", (seed, event.get("choice"))
        journal = (event.get("logLines") or [""])[-1]
        bought = re.match(r"^I bought (a|an) (.+) for \$([\d,]+)\.$", journal)
        assert bought, (seed, journal)
        article, bought_item, paid = bought.groups()
        assert bought_item == title(item), (seed, bought_item, item)
        assert article == ("an" if title(item)[0] in "AEIOU" else "a"), (seed, journal)
        assert int(paid.replace(",", "")) == price, (seed, journal, price)

assert cards >= 8, cards
print("jewels: %d ok" % cards)
