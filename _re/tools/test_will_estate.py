"""252: the will and the hero"s estate line.

The played-life trace: "I created a will. I"m leaving everything to my son,
Cleveland." at 50, and "I died while sleeping peacefully. My net worth at the time
of my death was $388,327." at 60.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

WILL = re.compile(r"^I created a will\. I'm leaving everything to my (son|daughter|girlfriend|boyfriend), "
                  r"(.+)\.$")
WORTH = re.compile(r"My net worth at the time of my death was -?\$[\d,]+\.$")


def run(seed, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "relationship": True}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


wills = 0
deaths = 0
for seed in range(1, 41):
    doc = run(seed)
    made = 0
    earlier = []
    for event in doc["events"]:
        for line in (event.get("logLines") or []):
            m = WILL.match(line)
            if m:
                wills += 1
                made += 1
                kind, name = m.groups()
                # The beneficiary has to be somebody the life has already met:
                # the partner can change afterwards, so the final state is no
                # witness, but a name that never appeared is a bug.
                assert any(name in old for old in earlier), (seed, line)
            earlier.append(line)
    assert made <= 1, (seed, "two wills in one life", made)
    if not doc["final"]["alive"]:
        deaths += 1
        sentence = (doc["final"].get("death") or {}).get("sentence") or ""
        assert WORTH.search(sentence), (seed, sentence)

assert wills >= 5, wills
assert deaths >= 20, deaths
print("wills:", wills, "deaths:", deaths)
print("ok")
