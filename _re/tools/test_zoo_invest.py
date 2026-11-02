#!/usr/bin/env python3
# The zoo's invest card (REPORT.en.md 377): the menu card the native never pushes,
# offered as its own family. Checks the recovered body, the three rows, the Invest
# answer, the cooldown, that the amount is a whole thousand and never exceeds what
# the zoo already holds, and the no-zoo control.
import json, os, re, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")

TABLE = "registry:ScenarioZooInvest"
BODY = re.compile(
    r"^Invest in your zoo today!\n\n"
    r"Zoo: (?P<zoo>.+)\n"
    r"Zoo's Current Capital: (?P<cur>[^\d]*)(?P<capital>[\d,]+)\n"
    r"Amount: (?P<cur2>[^\d]*)(?P<amount>[\d,]+)$")


def run(seed, ages=60, zoo=True):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 3000000, "only": "zoo-invest"}
    if zoo:
        spec["zoo"] = True
    p = os.path.join(tempfile.gettempdir(), "_zi377_%d.json" % seed)
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], capture_output=True, text=True)
    assert r.returncode == 0, (seed, r.returncode, r.stderr[:400])
    return json.loads(r.stdout)["events"]


def main():
    total = 0
    for seed in range(1, 21):
        found = [e for e in run(seed) if (e.get("table") or "") == TABLE]
        total += len(found)
        ages = [e["age"] for e in found]
        for i in range(1, len(ages)):
            assert ages[i] - ages[i - 1] >= 4, (seed, ages)
        for e in found:
            m = BODY.match(e.get("body") or "")
            assert m, (seed, e.get("age"), e.get("body"))
            assert e.get("title") == "Zoo", e.get("title")
            assert (e.get("choice") or "") == "Invest", e.get("choice")
            assert m.group("cur") == m.group("cur2"), e.get("body")
            assert m.group("zoo").strip(), e.get("body")
            amount = int(m.group("amount").replace(",", ""))
            capital = int(m.group("capital").replace(",", ""))
            assert amount > 0 and amount % 1000 == 0, (seed, amount)
            assert amount <= max(capital, 0) or capital < 0, (seed, amount, capital)

    assert total >= 10, total
    assert [e for e in run(4, zoo=False) if (e.get("table") or "") == TABLE] == []
    print("cards: %d | ok" % total)


if __name__ == "__main__":
    main()
