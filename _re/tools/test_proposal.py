"""303: the proposal card -- the ring's cells, the setting, the two lanes."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"
SETTING_ROW = re.compile(r"^Setting: (.+)$")
YEARS_ROW = re.compile(r"^Years Together: (\d+)$")


def table(name):
    with open(DATA / (name + "-enc"), newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def number(text):
    return float("".join(ch for ch in (text or "") if ch.isdigit() or ch == ".") or 0)


TYPES = {r["Category"].strip().lower(): r for r in table("a16_jewelry-types")}
GEMS = {r["Gemstone"].strip().lower(): r for r in table("a16_jewelry-gemstones")}
SETTINGS = {r["Place"].strip().lower() for r in table("a16_proposal-settings")}

# Gemstones ("black opal") and categories ("gemstone watch") both carry spaces, so
# the split anchors on a known category instead of on word count.
TYPE_ALT = "|".join(
    re.escape(t) for t in sorted(TYPES, key=len, reverse=True))
BODY = re.compile(
    r"^Your (boyfriend|girlfriend), (.+?), proposed to you with a ([\d.]+)-carat "
    r"(.+?) (" + TYPE_ALT + r") (.+)!$", re.IGNORECASE)
RING_ROW = re.compile(
    r"^Ring: (.+?) (" + TYPE_ALT + r") \(([\d.]+)ct\)$", re.IGNORECASE)


def run(seed, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": "female",
            "ages": ages, "dating": True}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    try:
        out = subprocess.run([str(PORT), "--script", path], capture_output=True,
                             text=True, cwd=str(ROOT / "port"), check=True)
    finally:
        Path(path).unlink()
    return json.loads(out.stdout)["events"]


cards = accept = refuse = 0
for seed in range(1, 41):
    for event in run(seed):
        if event.get("title") != "Proposal":
            continue
        cards += 1
        body = event.get("body") or ""
        lines = body.split("\n")
        m = BODY.match(lines[0])
        assert m, (seed, body)
        kind, name, carats, gem, jewel, setting = m.groups()
        assert kind in ("boyfriend", "girlfriend"), (seed, kind)
        assert name and name[0].isupper(), (seed, name)

        ring = RING_ROW.match(lines[2])
        assert ring, (seed, lines[2])
        row_gem, row_type, row_carats = ring.groups()
        assert row_carats == carats, (seed, carats, row_carats)
        assert row_gem.lower() == gem.lower(), (seed, gem, row_gem)
        assert row_type.lower() == jewel.lower(), (seed, jewel, row_type)
        assert "ring" in row_type.lower(), (seed, row_type)

        meta = GEMS[gem.lower()]
        lo, hi = number(meta["Min Carats"]), number(meta["Max Carats"])
        assert lo <= float(carats) <= hi, (seed, carats, lo, hi)
        assert TYPES["ring"]["Category"].strip().lower() == "ring"

        hit = SETTING_ROW.match(lines[3])
        assert hit, (seed, lines[3])
        assert hit.group(1).strip().lower() in SETTINGS, (seed, hit.group(1))

        years = YEARS_ROW.match(lines[4])
        assert years, (seed, lines[4])
        assert int(years.group(1)) >= 0, (seed, lines[4])
        # The fixture pair starts at age 0, so its "years together" is the whole age;
        # a lover the sim picked up later has a shorter count (the card is genuine).
        if name == "Sam Ellery":
            assert int(years.group(1)) == event["age"], (seed, years.group(1), event["age"])
        assert body.endswith("\n\nWhat will you do?"), (seed, body)

        choice = event.get("choice") or ""
        assert choice in ("Accept his proposal", "Accept her proposal",
                          "I'm not ready for marriage"), (seed, choice)
        journal = (event.get("logLines") or [""])[-1]
        if choice.startswith("Accept "):
            accept += 1
            assert journal == "I accepted " + choice.split(" ", 1)[1] + "!", (seed, journal)
        else:
            refuse += 1
            assert journal == "I refused to accept " + (
                "his proposal." if kind == "boyfriend" else "her proposal."), (seed, journal)
        assert event.get("table") == "registry:ScenarioLoverProposeInbound", (seed, event.get("table"))

assert cards >= 2, cards
assert accept >= 1, accept
assert refuse >= 1, refuse
print("cards: %d accepted: %d refused: %d ok" % (cards, accept, refuse))
