"""The ex sends a picture (REPORT.en.md 173).

ScenarioExLoverInboundSext stopped at a hole in §172: {2} -- what was sent -- came
from DataHub::GetRandomSexyTextItemForGender, and the pool behind it is
a16_sexy-text-items-enc (Item, Gender, Clean), which the port had never read.
"""
import collections
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 60,
            "relationship": True, "ex": True}
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
items = set()
for seed in range(1, 31):
    for event in run(seed)["events"]:
        body = event.get("body") or ""
        # Another family has a "just sent you" card (a prison one), so the filter is the
        # ex-lover wording, not the verb.
        if not body.startswith("Your ex-") or "just sent you" not in body:
            continue
        assert body.endswith("."), (seed, body)
        assert "<pronoun>" not in body, (seed, body)
        assert event.get("table") == "a16_sexy-text-items-enc", (seed, event.get("table"))
        items.add(body.split(" just sent you ", 1)[1].rstrip("."))
        choice = event.get("choice")
        assert choice in ("Forward it to my friends", "Do these save to the cloud?") \
               or "a txxxt back" in choice, (seed, choice)
        lanes[choice.split(" a txxxt")[0] if "txxxt" in choice else choice] += 1
        journal = (event.get("logLines") or [""])[-1]
        assert journal.startswith("My ex-"), (seed, journal)

assert len(lanes) >= 2, lanes
assert len(items) >= 4, items   # the item really is drawn from the pool
print("lanes:", dict(lanes))
print("distinct items:", len(items))
print("ok")