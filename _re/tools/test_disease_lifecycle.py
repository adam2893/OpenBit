"""247: the hero's own illnesses -- diagnosis, yearly cost, cure.

The played-life trace (246) logs "I have been diagnosed with anxiety." at 36,
"I have been diagnosed with depression." at 37 and "I was cured of both" at 41,
which is what fixes the shape: a list of ailments, each with its own clock, a
yearly health cost, and the table's own article rules ("the flu", "a staph
infection").
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

DIAGNOSED = re.compile(r"^I have been diagnosed with (the |a |an )?(.+)\.$")
CURED = re.compile(r"^I was cured of (the |a |an )?(.+)\.$")
ARTICLED = {"flu", "chicken pox", "common cold", "measles", "swine flu"}


def run(seed, ages=80):
    spec = {"seed": seed, "country": "United States",
            "gender": "female" if seed % 2 else "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


diagnoses = 0
cures = 0
lives_with = 0
for seed in range(1, 41):
    doc = run(seed)
    active = set()
    seen_here = False
    for event in doc["events"]:
        for line in (event.get("logLines") or []):
            m = DIAGNOSED.match(line)
            if m:
                diagnoses += 1
                seen_here = True
                name = m.group(2)
                # The table marks the rows that need an article, and the trace shows
                # the result: "the flu", never "flu".
                if name in ARTICLED:
                    assert m.group(1) == "the ", (seed, line)
                assert name not in active, (seed, "diagnosed twice at once", line)
                active.add(name)
                continue
            m = CURED.match(line)
            if m:
                cures += 1
                name = m.group(2)
                # A cure has to name something the hero actually has.
                assert name in active, (seed, "cured of nothing", line, sorted(active))
                active.discard(name)
    if seen_here:
        lives_with += 1

assert diagnoses >= 40, diagnoses           # the coin is 1-in-7 a year
assert cures >= 20, cures                   # and most rows do cure
assert lives_with >= 35, lives_with         # almost every long life catches something

print("diagnoses:", diagnoses, "cures:", cures, "lives with an illness:", lives_with)
print("ok")
