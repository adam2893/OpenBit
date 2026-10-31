"""319: the casino's books -- the yearly pass and the Casino Ledger card."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"

LEDGER = "Casino Ledger"
ROW = re.compile(r"^(Wagers Placed|Payouts|Gambling Revenue|Performance Revenue|"
                 r"Amenity Revenue|Taxes|Annual Expenses|Net Income): (-?\$[\d,]+)$")
REVLOG = re.compile(r"^My casino made (-?\$[\d,]+) in revenue last year\.$")


def money(s):
    return int(s.replace("$", "").replace(",", ""))


def structure_types():
    """The Type column the tier multipliers index."""
    with open(DATA / "a16_casino-casino-structure-enc", newline="") as fh:
        rows = list(csv.DictReader(fh))
    return len(rows), rows[0]


def run(seed, ages=80, cash=3000000, only="casino", bank=1000000000):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": cash, "casino": True,
            # A solvent pot keeps the house open: the bankruptcy card (320) closes
            # it once the books have bled for six years, and a closed casino deals
            # no ledgers. The pot never touches the hero's wallet, so the rows this
            # test reads are the same either way.
            "casinoBank": bank}
    if only:
        spec["only"] = only
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


def ledgers(doc):
    return [e for e in doc["events"] if e.get("title") == LEDGER]


def parse(event):
    """The labelled rows of one ledger body, in print order."""
    rows, order = {}, []
    body = event["body"]
    for line in body.split("\n"):
        m = ROW.match(line.strip())
        if m:
            rows[m.group(1)] = money(m.group(2))
            order.append(m.group(1))
    return rows, order


def main():
    n_struct, first = structure_types()
    assert n_struct >= 5, n_struct
    # The tier ladder the multipliers index has to cover every multiplier branch.
    assert "Structure Type" in first or "StructureType" in " ".join(first), list(first)[:6]

    found = 0
    fields = 0
    multi = 0
    sample = None
    for seed in range(1, 25):
        doc = run(seed)
        for event in ledgers(doc):
            found += 1
            rows, order = parse(event)
            assert rows, (seed, event["body"][:200])
            if sample is None:
                sample = (seed, event["age"], event["body"], event["logLines"], rows)
            # The row order is the native refresh order.
            assert order == [k for k in ["Wagers Placed", "Payouts", "Gambling Revenue",
                                         "Performance Revenue", "Amenity Revenue",
                                         "Taxes", "Annual Expenses", "Net Income"]
                             if k in order], (seed, order)
            assert "Popularity:" in event["body"]
            assert "The annual report for your casino is now available." in event["body"]
            # Wagers - payouts is the gambling revenue the card prints.
            assert rows["Wagers Placed"] - rows["Payouts"] == rows["Gambling Revenue"], rows
            gross = (rows["Gambling Revenue"] + rows["Performance Revenue"] +
                     rows["Amenity Revenue"])
            taxable = gross - rows["Annual Expenses"]
            # The tax is clamped at zero and never exceeds the profit.
            assert rows["Taxes"] >= 0, rows
            if taxable <= 0:
                assert rows["Taxes"] == 0, (seed, rows)
            assert rows["Net Income"] == gross - rows["Taxes"] - rows["Annual Expenses"],                 (seed, rows)
            # The year's narrative repeats the gross.
            rev = None
            for line in event["logLines"]:
                m = REVLOG.match(line)
                if m:
                    rev = money(m.group(1))
            assert rev == gross, (seed, rev, gross, rows)
            # Money moves only through the casino pot, never the hero's wallet: the
            # ledger card itself has no stat deltas.
            assert event["deltas"] == {"karma": 0, "discipline": 0, "happiness": 0,
                                       "health": 0, "smarts": 0, "looks": 0,
                                       "fame": 0}, event["deltas"]
            fields += 1
            if "Most Popular Room" in event["body"]:
                multi += 1

    assert found >= 8, found
    assert fields == found
    # The books are simulated every year, so no two ledgers may match: a static
    # ledger would mean the pass never ran. (How often the review card is dealt is
    # a presentation shortcut, so the check spans lives rather than one life.)
    bodies = []
    per_life = {}
    for seed in range(1, 41):
        got = [e["body"] for e in ledgers(run(seed, ages=60))]
        per_life[seed] = len(got)
        bodies.extend(got)
    assert len(bodies) >= 6, per_life
    assert len(set(bodies)) == len(bodies), per_life
    assert max(per_life.values()) >= 1, per_life

    # A hero without a casino never sees the card.
    plain = run(1, ages=60, only="")
    plain_spec = {"seed": 3, "country": "United States", "gender": "male", "ages": 60}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(plain_spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    assert not ledgers(json.loads(proc.stdout))

    print("ledgers:", found, "of seeds 1..24")
    print("rooms listed:", multi)
    print("sample seed/age:", sample[0], sample[1])
    print("sample rows:", sample[4])
    print("ok")


main()
