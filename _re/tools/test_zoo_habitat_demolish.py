#!/usr/bin/env python3
# The zoo's demolish card (REPORT.en.md 380): the build card's mirror. A seeded
# park (the zoo_habitats hook) is taken apart one habitat at a time, so the test
# can see the list empty out and the card stop offering itself.
import json, os, re, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")

TABLE = "registry:ScenarioZooHabitatDemolish"
BODY = re.compile(
    r"^Demolish one of your zoo's habitats today!\n\n"
    r"Zoo: (?P<zoo>.+)\n"
    r"Habitat: (?P<hab>.+ \((?P<type>[a-z ]+)\))\n"
    r"Open Habitat Slots: (?P<slots>\d+)$")


def run(seed, habitats=3, ages=40, zoo=True):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 3000000, "only": "zoo-demolish"}
    if zoo:
        spec["zoo"] = True
        spec["zoo_habitats"] = habitats
    p = os.path.join(tempfile.gettempdir(), "_zhd380_%d.json" % seed)
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], capture_output=True, text=True)
    assert r.returncode == 0, (seed, r.returncode, r.stderr[:400])
    return json.loads(r.stdout)["events"]


def main():
    total = 0
    seen = set()
    for seed in range(1, 31):
        cards = [e for e in run(seed) if e.get("table") == TABLE]
        if not cards:
            # The card is drawn per year, so an unlucky life can miss it entirely.
            continue
        # Three seeded habitats, so the card can fire three times and no more.
        assert len(cards) <= 3, (seed, len(cards))
        last = None
        for e in cards:
            total += 1
            body = e.get("body") or ""
            m = BODY.match(body)
            assert m, (seed, body)
            assert int(m.group("slots")) >= 0, (seed, body)
            if last is not None:
                assert m.group("hab") != last, (seed, last, body)
            last = m.group("hab")
            seen.add(m.group("type"))
            assert e.get("choice") == "OK", (seed, e.get("choice"))
            assert not any((e.get("deltas") or {}).values()), (seed, e.get("deltas"))
        for a, b in zip(cards, cards[1:]):
            assert b["age"] - a["age"] >= 4, (seed, [c["age"] for c in cards])
    assert total, "no demolish cards"
    for seed in range(1, 11):
        assert not [e for e in run(seed, habitats=0) if e.get("table") == TABLE], seed
    print("cards: %d | habitat types seen: %d" % (total, len(seen)))
    print("ok")


main()
