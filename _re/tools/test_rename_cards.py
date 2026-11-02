#!/usr/bin/env python3
# The zoo's and the casino's rename cards (REPORT.en.md 376): the two menu cards the
# native never pushes, offered as their own families. Checks both recovered bodies,
# the two name rows, the single OK answer, that a card never prints the same name
# twice, the chain from one card's Name to the next card's Current Name, the
# five-year cooldown and both no-ownership controls.
import json, os, re, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")

ZOO = "registry:ScenarioZooName"
CASINO = "registry:ScenarioCasinoName"
ZOO_BODY = "Decide on a name for your new zoo."
CASINO_BODY = "Give your casino a new name today!"
BODY = re.compile(r"^(?P<body>.+)\n\nCurrent Name: (?P<cur>.+)\nName: (?P<new>.+)$")


def run(seed, only, table, owned=True, ages=55):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 3000000, "only": only}
    if owned:
        spec[table] = True
    p = os.path.join(tempfile.gettempdir(), "_rn376_%d_%s.json" % (seed, only))
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], capture_output=True, text=True)
    assert r.returncode == 0, (seed, r.returncode, r.stderr[:400])
    return json.loads(r.stdout)["events"]


def cards(seed, only, table, key, **kw):
    return [e for e in run(seed, only, table, **kw) if (e.get("table") or "") == key]


def check(label, seed, found, body_text, title):
    ages = [e["age"] for e in found]
    for i in range(1, len(ages)):
        assert ages[i] - ages[i - 1] >= 5, (label, seed, ages)
    last = None
    for e in found:
        m = BODY.match(e.get("body") or "")
        assert m, (label, seed, e.get("age"), e.get("body"))
        assert m.group("body") == body_text, (label, e.get("body"))
        assert e.get("title") == title, (label, e.get("title"))
        assert (e.get("choice") or "") == "OK", (label, e.get("choice"))
        assert m.group("cur") != m.group("new"), (label, e.get("body"))
        if last is not None:
            assert m.group("cur") == last, (label, seed, last, e.get("body"))
        last = m.group("new")


def main():
    zoo_cards = 0
    casino_cards = 0
    zoo_chains = 0
    for seed in range(1, 21):
        found = cards(seed, "zoo-rename", "zoo", ZOO)
        zoo_cards += len(found)
        zoo_chains += 1 if len(found) > 1 else 0
        check("zoo", seed, found, ZOO_BODY, "Zoo")
        found = cards(seed, "casino-rename", "casino", CASINO)
        casino_cards += len(found)
        check("casino", seed, found, CASINO_BODY, "Casino")

    assert zoo_cards >= 10, zoo_cards
    assert casino_cards >= 3, casino_cards
    assert zoo_chains >= 1, zoo_chains
    assert cards(4, "zoo-rename", "zoo", ZOO, owned=False) == []
    assert cards(4, "casino-rename", "casino", CASINO, owned=False) == []
    print("zoo cards: %d (chains %d) | casino cards: %d | ok"
          % (zoo_cards, zoo_chains, casino_cards))


if __name__ == "__main__":
    main()
