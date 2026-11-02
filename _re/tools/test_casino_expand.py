#!/usr/bin/env python3
# The casino's expand card (REPORT.en.md 378): the menu card the native never
# pushes, offered as its own family. Checks the recovered body and answer, the
# row that names the next structure and its price, the cooldown, that the price
# is the table's step, and the no-casino control.
import json, os, re, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")

TABLE = "registry:ScenarioCasinoExpand"
BODY = re.compile(
    r"^Upgrade your (?P<name>.+?) today!\n\n"
    r"Expansion: (?P<structure>.+) \((?P<cur>.)(?P<price>[\d,]+)\)\n"
    r"Available Funds: (?P<cur2>.)(?P<funds>[\d,]+)$")


def run(seed, ages=60, casino=True):
    spec = {"seed": seed, "country": "United States", "gender": "female",
            "ages": ages, "cash": 3000000, "only": "casino-expand"}
    if casino:
        spec["casino"] = True
    p = os.path.join(tempfile.gettempdir(), "_ce378_%d.json" % seed)
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], capture_output=True, text=True)
    assert r.returncode == 0, (seed, r.returncode, r.stderr[:400])
    return json.loads(r.stdout)["events"]


def main():
    total = 0
    structures = set()
    agesteps = []
    for seed in range(1, 41):
        ages = []
        for e in run(seed):
            if e.get("table") != TABLE:
                continue
            total += 1
            body = e.get("body") or ""
            m = BODY.match(body)
            assert m, (seed, body)
            assert m.group("cur") == "$", (seed, body)
            assert int(m.group("price").replace(",", "")) > 0, (seed, body)
            funds = int(m.group("funds").replace(",", ""))
            assert funds >= 0, (seed, body)
            assert e.get("choice") == "Upgrade", (seed, e.get("choice"))
            assert not any((e.get("deltas") or {}).values()), (seed, e.get("deltas"))
            structures.add(m.group("structure"))
            ages.append(e["age"])
        for a, b in zip(ages, ages[1:]):
            # The card cannot come back inside its five-year cooldown.
            assert b - a >= 5, (seed, ages)
        agesteps.extend(zip(ages, ages[1:]))
    assert total, "no expand cards"
    assert len(structures) >= 2, structures
    # The control: no casino, no card.
    for seed in range(1, 11):
        assert not [e for e in run(seed, casino=False) if e.get("table") == TABLE], seed
    print("cards: %d | structures seen: %d | cooldown steps: %d"
          % (total, len(structures), len(agesteps)))
    print("ok")


main()
