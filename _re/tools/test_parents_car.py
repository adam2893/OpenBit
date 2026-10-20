"""280: the parents' car gift -- the reject-and-demand pair and the handed keys."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DEMAND = re.compile(r"^I rejected the (.+?) my (mother|father) offered to buy me and demanded something more expensive\. (She|He) agreed to buy me a more expensive car\.$")
GIFT = re.compile(r"^My (mother|father) bought me a (gray|burgundy|silver|white|blue|yellow|seafoam) (.+)\.$")

models = set()
with open(ROOT / "gamedata_plain" / "a16_car-models-enc", newline="") as fh:
    for row in csv.DictReader(fh):
        make = (row.get("Make") or "").strip()
        model = (row.get("Model") or "").strip()
        if make and model:
            models.add(make + " " + model)


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


gifts = 0
demands = 0
accepts = 0
for seed in range(1, 41):
    doc = run(seed)
    for event in doc["events"]:
        if event.get("title") != "A new car":
            continue
        gifts += 1
        choice = event.get("choice")
        lines = event.get("logLines") or []
        assert event["age"] in (16, 17, 18), (seed, event["age"])
        if choice == "Ask for a more expensive car":
            demands += 1
            m = next((DEMAND.match(l) for l in lines if DEMAND.match(l)), None)
            assert m, (seed, lines)
            name, parent, pron = m.groups()
            assert name in models, (seed, name)
            assert (parent == "mother") == (pron == "She"), (seed, lines)
            # The gift the demand buys is the card's own extra line.
            g = next((GIFT.match(l) for l in lines if GIFT.match(l)), None)
            assert g, (seed, lines)
            assert g.group(1) == parent, (seed, lines)
            assert g.group(3) in models, (seed, g.group(3))
        else:
            assert choice == "Take it", (seed, choice)
            accepts += 1
            g = GIFT.match((lines or [""])[-1])
            assert g, (seed, lines)
            assert g.group(3) in models, (seed, g.group(3))

assert gifts >= 5, gifts
assert demands and accepts, (demands, accepts)
print("car gifts:", gifts, "asked for more:", demands, "took it:", accepts)
print("ok")
