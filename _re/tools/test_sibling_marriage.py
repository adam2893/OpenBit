"""270: a sibling's marriage and children, in the game's own sentences."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
MARRIAGE = re.compile(
    r"^(My [a-z ]+, ([A-Za-z]+),) married (.+), a (\d+)-year old (.+)\. (She|He) changed (her|his) name to (.+)\.$")
BIRTH = re.compile(r"^(My [a-z ]+, ([A-Za-z]+),) gave birth to a (girl|boy) named (.+)$")


def run(seed, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


marriages = 0
births = 0
for seed in range(1, 21):
    doc = run(seed)
    married_at = {}
    kids = {}
    for event in doc["events"]:
        for line in (event.get("logLines") or []):
            m = MARRIAGE.match(line)
            if m:
                marriages += 1
                who, name, spouse, age, job, pron, poss, new_name = m.groups()
                assert name.split()[0] in new_name, (seed, line)
                assert int(age) >= 18, (seed, line)
                assert " " in spouse, (seed, line)
                assert pron in ("She", "He"), (seed, line)
                assert (pron == "She") == (poss == "her"), (seed, line)
                assert job == "unemployed person" or not job.endswith(" person"), (seed, line)
                married_at[name.split()[0]] = event["age"]
            m = BIRTH.match(line)
            if m:
                births += 1
                who, name, girl, child = m.groups()
                assert girl in ("girl", "boy"), (seed, line)
                kids.setdefault(name.split()[0], []).append((event["age"], child))
    # A first name can repeat among siblings, and a line cannot then be tied to one
    # of them by name alone (REPORT.en.md 272). Ordering is checked only where the
    # name is unique in that life.
    parent_counts = {}
    for parent in list(married_at) + list(kids):
        parent_counts[parent] = parent_counts.get(parent, 0) + 1
    for parent, records in kids.items():
        if parent_counts[parent] > 1:
            continue
        assert parent in married_at, (seed, "a birth without a marriage", parent)
        for age, child in records:
            assert age >= married_at[parent], (seed, "a birth before the marriage", parent)
            first = child.split()[0]
            assert any(p.get("name") == first for p in doc["final"]["family"]), (seed, child)

assert marriages >= 5, marriages
assert births >= 1, births
print("marriages:", marriages, "births:", births)
print("ok")
