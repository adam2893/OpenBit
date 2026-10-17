"""A child asks for money (REPORT.en.md 181).

ScenarioChildDemands reads a row of a16_child-demands-enc: Age, Amount, and two
comma-separated option lists. The row supplies both the ask and the choices, which is a
shape the last two cards did not have.
"""
import collections
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
BODY = re.compile(r"^Your (son|daughter), ([^,]+), is asking you for \$(\d+) to (.+)\.$")


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 30,
            "relationship": True, "children": 2, "cash": 5000}
    spec.update(extra)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


lanes = collections.Counter()
asks = set()
for seed in range(1, 31):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_child-demands-enc":
            continue
        body = event["body"]
        match = BODY.match(body)
        assert match, (seed, body)
        word, name, amount, demand = match.groups()
        asks.add(demand)
        choice = event["choice"]
        lanes[choice.split(",")[0]] += 1
        journal = (event.get("logLines") or [""])[-1]
        assert demand in journal, (seed, journal, demand)
        assert ("gave my" in journal) != ("refused to give" in journal), (seed, journal)
        if "gave my" in journal:
            assert choice.startswith("Yes, give "), (seed, choice)
        else:
            assert choice.startswith("No, "), (seed, choice)

assert len(lanes) >= 1, lanes
assert len(asks) >= 2, asks
print("lanes:", dict(lanes))
print("distinct asks:", len(asks))
print("ok")