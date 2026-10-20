"""275: the parents' pet -- the gift line and the named death."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
GIFT = re.compile(r"^My (mother|father) got me( and my (.+?))? an? (dog|cat) named ([A-Za-z'-]+)\.$")
DEATH = re.compile(r"^My (.+?), ([A-Za-z'-]+), (died|perished) .+\.$")


def run(seed, ages=40):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


gifts = 0
deaths = 0
for seed in range(1, 41):
    doc = run(seed)
    per_life = 0
    names = set()
    for event in doc["events"]:
        for line in (event.get("logLines") or []):
            m = GIFT.match(line)
            if m:
                gifts += 1
                per_life += 1
                names.add(m.group(5))
                assert m.group(2) is None or "sister" in m.group(3) or "brother" in m.group(3), line
                assert m.group(4) in ("dog", "cat"), line
            m = DEATH.match(line)
            if m and ("died" in line or "perished" in line):
                pet_word, name, verb = m.groups()
                if any(k in pet_word.lower() for k in ("dog", "cat", "husky", "retriever", "bird", "canary", "parrot", "terrier", "poodle", "beagle", "cat", "ragdoll", "siamese", "malamute", "shepherd")):
                    deaths += 1
                    assert name, line
    assert per_life <= 1, (seed, "two pet gifts in a life", per_life)

assert gifts >= 8, gifts
assert deaths >= 1, deaths
print("gifts:", gifts, "named pet deaths:", deaths)
print("ok")
