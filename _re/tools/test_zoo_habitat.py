#!/usr/bin/env python3
# The zoo's habitat card (REPORT.en.md 379): the menu card that builds one more
# habitat. Checks the recovered body, the two recovered rows plus the habitat and
# its cost, the single Build Habitat answer, the five-year cooldown, the falling
# slot count as habitats go up, and the no-zoo control.
import json, os, re, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")

TABLE = "registry:ScenarioZooHabitatBuild"
BODY = re.compile(
    r"^Build a new habitat in your zoo today!\n\n"
    r"Zoo: (?P<zoo>.+)\n"
    r"Open Habitat Slots: (?P<slots>\d+)\n"
    r"Habitat: (?P<name>.+?) \((?P<type>[a-z ]+)\)\n"
    r"Cost: (?P<cur>.)(?P<cost>[\d,]+)(?: \| Annual Upkeep: (?P<cur2>.)(?P<up>[\d,]+))?$")


def run(seed, ages=60, zoo=True):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 3000000, "only": "zoo-habitat"}
    if zoo:
        spec["zoo"] = True
    p = os.path.join(tempfile.gettempdir(), "_zh379_%d.json" % seed)
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], capture_output=True, text=True)
    assert r.returncode == 0, (seed, r.returncode, r.stderr[:400])
    return json.loads(r.stdout)["events"]


def main():
    total = 0
    types = set()
    for seed in range(1, 41):
        last_slots = None
        ages = []
        for e in run(seed):
            if e.get("table") != TABLE:
                continue
            total += 1
            body = e.get("body") or ""
            m = BODY.match(body)
            assert m, (seed, body)
            slots = int(m.group("slots"))
            assert slots >= 1, (seed, body)
            if last_slots is not None:
                # Every card costs one open slot, so the count only falls.
                assert slots == last_slots - 1, (seed, last_slots, body)
            last_slots = slots
            assert m.group("cur") == "$" and m.group("cur2") in (None, "$"), (seed, body)
            assert int(m.group("cost").replace(",", "")) > 0, (seed, body)
            if m.group("up"):
                assert int(m.group("up").replace(",", "")) > 0, (seed, body)
            assert m.group("name").strip(), (seed, body)
            types.add(m.group("type"))
            assert e.get("choice") == "Build Habitat", (seed, e.get("choice"))
            assert not any((e.get("deltas") or {}).values()), (seed, e.get("deltas"))
            ages.append(e["age"])
        for a, b in zip(ages, ages[1:]):
            assert b - a >= 5, (seed, ages)
    assert total, "no habitat cards"
    assert len(types) >= 1, types
    for seed in range(1, 11):
        assert not [e for e in run(seed, zoo=False) if e.get("table") == TABLE], seed
    print("cards: %d | habitat types seen: %d" % (total, len(types)))
    print("ok")


main()
