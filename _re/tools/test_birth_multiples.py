#!/usr/bin/env python3
"""A multiple birth (REPORT.en.md 340).

The game rolls a lifetime denominator of 50 for a multiple pregnancy and then a
1-in-10 roll for three children instead of two; every child of the birth shares the
roll's gender. The port plays the same rolls, names each child from the game's pool
without repeating a name, and writes SimChild.Twins on each of them.
"""
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORT = ROOT / "port" / "build" / "blsim"
SINGLE = re.compile(r"^Your (wife|husband|girlfriend|boyfriend|fiance|fiancee), ([^,]+), "
                    r"just gave birth to a baby (girl|boy)!$")
MULTI = re.compile(r"^Your (wife|husband|girlfriend|boyfriend|fiance|fiancee), ([^,]+), "
                   r"just gave birth to (twin|triplet) (boys|girls)!$")


def run(seed, ages=60):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 60000, "relationship": True}
    fd, path = tempfile.mkstemp(suffix=".json")
    os.write(fd, json.dumps(spec).encode())
    os.close(fd)
    try:
        out = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                             capture_output=True, text=True)
    finally:
        os.unlink(path)
    assert out.returncode == 0, out.stderr[-800:]
    return json.loads(out.stdout[out.stdout.find("{"):])


multiples = 0
triplets = 0
singles = 0
for seed in range(1, 121):
    doc = run(seed)
    for event in doc["events"]:
        body = event.get("body") or ""
        m = SINGLE.match(body)
        if m:
            singles += 1
            continue
        m = MULTI.match(body)
        if not m:
            continue
        multiples += 1
        kind, plural = m.group(3), m.group(4)
        kids = 2 if kind == "twin" else 3
        if kind == "triplet":
            triplets += 1
        # Every child of the birth is in the array, at the same age, same gender,
        # with a distinct name and the shared sibling count.
        born = [c for c in doc["final"]["children"]
                if c["bornAtHeroAge"] == event["age"]]
        assert len(born) == kids, (seed, event["age"], kind, [c["name"] for c in born])
        assert len({c["name"] for c in born}) == kids, (seed, [c["name"] for c in born])
        girls = plural == "girls"
        for c in born:
            assert c["female"] is girls, (seed, c)
            assert c["twins"] == kids - 1, (seed, c)
            assert c["otherParent"] == m.group(2), (seed, c)
        # The journal names the birth and every child the port named.
        narrative = " ".join(event["logLines"])
        assert ("gave birth to " + kind + " " + plural) in narrative, (seed, narrative)
        for c in born:
            assert c["name"] in narrative, (seed, c["name"], narrative)

assert singles > 0, "no single birth"
assert multiples >= 1, "no multiple birth in 120 lives"
print("births:", singles + multiples, "| singles:", singles, "| multiples:", multiples,
      "| triplets:", triplets)
print("ok")
