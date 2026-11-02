#!/usr/bin/env python3
# The casino's withdraw card (REPORT.en.md 374): the menu card the native never
# pushes, offered as its own family. Checks the recovered body rows, the single OK
# answer, the half-the-balance shortcut the rows imply, the cooldown and the
# no-casino control.
import json, os, re, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")

TABLE = "registry:ScenarioCasinoWithdrawFunds"
BODY = re.compile(
    r"^Take money out of your casino today!\n\n"
    r"Amount: (?P<cur1>[^\d]*)(?P<amount>[\d,]+)\n"
    r"Available Funds: (?P<cur2>[^\d]*)(?P<funds>[\d,]+)$")


def run(seed, ages=50, casino=True):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 2000000, "only": "casino-withdraw"}
    if casino:
        spec["casino"] = True
    p = os.path.join(tempfile.gettempdir(), "_wd374_%d.json" % seed)
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], capture_output=True, text=True)
    assert r.returncode == 0, (seed, r.returncode, r.stderr[:400])
    return json.loads(r.stdout)["events"]


def main():
    total = 0
    for seed in range(1, 21):
        found = [e for e in run(seed)
                 if (e.get("table") or "") == TABLE]
        total += len(found)
        ages = [e["age"] for e in found]
        for i in range(1, len(ages)):
            assert ages[i] - ages[i - 1] >= 4, (seed, ages)
        for e in found:
            m = BODY.match(e.get("body") or "")
            assert m, (seed, e.get("age"), e.get("body"))
            assert e.get("title") == "Casino", e.get("title")
            assert (e.get("choice") or "") == "OK", e.get("choice")
            assert m.group("cur1") == m.group("cur2"), e.get("body")
            amount = int(m.group("amount").replace(",", ""))
            funds = int(m.group("funds").replace(",", ""))
            want = (funds // 2) // 1000 * 1000
            if want <= 0:
                want = funds
            assert amount == want, (seed, amount, funds, e.get("body"))
            assert amount <= funds, (seed, amount, funds)

    assert total >= 5, total
    assert [e for e in run(4, casino=False) if (e.get("table") or "") == TABLE] == []
    print("cards: %d | ok" % total)


if __name__ == "__main__":
    main()
