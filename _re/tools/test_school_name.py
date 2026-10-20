"""282: the school's name and its private/public tag."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
START = re.compile(r"^I started (elementary|middle|high) school at (.+), a (private|public) school\.$")
TIERS = {"elementary": "Elementary School", "middle": "Middle School", "high": "High School"}


def run(seed, ages=20):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


names = set()
kinds = {"private": 0, "public": 0}
count = 0
for seed in range(1, 41):
    doc = run(seed)
    per_life = 0
    for event in doc["events"]:
        for line in (event.get("logLines") or []):
            m = START.match(line)
            if not m:
                continue
            count += 1
            per_life += 1
            tier, name, kind = m.groups()
            kinds[kind] += 1
            names.add(name)
            assert name.endswith(" " + TIERS[tier]), (seed, line)
            head = name[: -len(TIERS[tier])].strip()
            assert head, (seed, line)
            if head.startswith("St. "):
                assert "'s " in name, (seed, line)
            else:
                tokens = head.split()
                assert 1 <= len(tokens) <= 3, (seed, line)
                # A surname can carry a particle ('del Slay'); the name itself is
                # the last token and must be capitalised.
                assert tokens[-1][0].isupper() or tokens[-1][0].isdigit(), (seed, line)
    assert per_life <= 3, (seed, "a life started more than three schools", per_life)

assert count >= 30, count
assert len(names) >= 10, len(names)
assert kinds["private"] and kinds["public"], kinds
print("school lines:", count, "distinct names:", len(names), kinds)
print("ok")
