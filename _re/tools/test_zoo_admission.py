#!/usr/bin/env python3
# The zoo's admission fee card (REPORT.en.md 373): the menu card the native never
# pushes, offered as its own family with its own stream. Checks the recovered body
# rows, the single OK answer, the five-year cooldown and that the fee actually moves.
import json, os, re, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")

TABLE = "registry:ScenarioZooAdmissionFee"
BODY = re.compile(
    r"^Adjust the admission fee for your zoo today!\n\n"
    r"Current Ticket Price: \$(?P<price>[\d,]+)\n"
    r"Ticket Sales Last Year: (?P<visits>[\d,]+)\n"
    r"Zoo: (?P<zoo>.+)$")


def run(seed, ages=45, zoo=True):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 3000000, "only": "zoo-admission"}
    if zoo:
        spec["zoo"] = True
    p = os.path.join(tempfile.gettempdir(), "_adm373_%d.json" % seed)
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], capture_output=True, text=True)
    assert r.returncode == 0, (seed, r.returncode, r.stderr[:400])
    return json.loads(r.stdout)["events"]


def cards(seed, **kw):
    out = []
    for e in run(seed, **kw):
        if (e.get("table") or "") == TABLE:
            out.append(e)
    return out


def main():
    total = 0
    moved = 0
    seen_prices = set()
    for seed in range(1, 21):
        found = cards(seed)
        total += len(found)
        ages = [e["age"] for e in found]
        for i in range(1, len(ages)):
            assert ages[i] - ages[i - 1] >= 5, (seed, ages)
        prices = []
        for e in found:
            m = BODY.match(e.get("body") or "")
            assert m, (seed, e.get("age"), e.get("body"))
            assert e.get("title") == "Admission Fee", e.get("title")
            assert (e.get("choice") or "") == "OK", e.get("choice")
            price = int(m.group("price").replace(",", ""))
            assert 1 <= price <= 200, (seed, price)
            assert int(m.group("visits").replace(",", "")) > 0, e.get("body")
            prices.append(price)
            seen_prices.add(price)
        if len(set(prices)) > 1:
            moved += 1
    assert total >= 10, total
    assert moved >= 5, moved
    assert len(seen_prices) >= 5, sorted(seen_prices)

    # A hero with no zoo is never offered the fee card.
    assert cards(3, zoo=False) == []
    print("cards: %d | seeds whose fee moved: %d | prices seen: %d"
          % (total, moved, len(seen_prices)))
    print("ok")


if __name__ == "__main__":
    main()
