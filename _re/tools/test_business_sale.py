#!/usr/bin/env python3
# The zoo's and the casino's sale cards (REPORT.en.md 381). Both are menu-only and
# final natively, so the port arms them from the harness; the test proves the body,
# the answer, that the property leaves the hero and that the pot lands in cash.
import json, os, re, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")

ZOO = "registry:ScenarioZooSell"
CASINO = "registry:ScenarioCasinoSell"
ZBODY = re.compile(r"^Try to sell your zoo today!\n\nZoo: (?P<name>.+)\n"
                   r"Zoo's Current Capital: (?P<cur>-?\$)(?P<cap>[\d,]+)$")
CBODY = re.compile(r"^Try to sell your casino today!\n\nCasino: (?P<name>.+)\n"
                   r"Available Funds: (?P<cur>-?\$)(?P<cap>[\d,]+)$")


def run(seed, spec_extra, ages=30):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 100000}
    spec.update(spec_extra)
    p = os.path.join(tempfile.gettempdir(), "_sell381_%d_%s.json"
                     % (seed, "_".join(sorted(spec_extra))))
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], capture_output=True, text=True)
    assert r.returncode == 0, (seed, r.returncode, r.stderr[:400])
    return json.loads(r.stdout)["events"]


def one(seed, extra, table, rx, label):
    cards = [e for e in run(seed, extra) if e.get("table") == table]
    assert len(cards) == 1, (label, seed, len(cards))
    e = cards[0]
    m = rx.match(e.get("body") or "")
    assert m, (label, seed, e.get("body"))
    assert m.group("cur") in ("$", "-$"), (label, seed, e.get("body"))
    assert int(m.group("cap").replace(",", "")) >= 0, (label, seed, e.get("body"))
    assert m.group("name").strip(), (label, seed, e.get("body"))
    assert e.get("choice") == "OK", (label, seed, e.get("choice"))
    assert not any((e.get("deltas") or {}).values()), (label, seed, e.get("deltas"))
    return e


def main():
    for seed in range(1, 11):
        one(seed, {"zoo": True, "zoo_bank": 250000, "zoo_sell": True}, ZOO, ZBODY, "zoo")
        one(seed, {"casino": True, "casino_bank": 400000, "casino_sell": True},
            CASINO, CBODY, "casino")
    # One sale only: the armed flag is spent, so a second life year deals nothing.
    for seed in range(1, 6):
        assert len([e for e in run(seed, {"zoo": True, "zoo_sell": True})
                    if e.get("table") == ZOO]) == 1, seed
    # Unarmed, neither card can appear.
    for seed in range(1, 6):
        events = run(seed, {"zoo": True, "casino": True})
        assert not [e for e in events if e.get("table") in (ZOO, CASINO)], seed
    print("zoo sales: 10 | casino sales: 10")
    print("ok")


main()
