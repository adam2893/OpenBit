"""The partner ends it (REPORT.en.md 164).

ScenarioLoverLeaves is the code-built break-up with buttons: beg, wish well, and --
for a vampire -- hypnotize. Both leaving lanes hand off to the announcement and the
ex-lover record the port already has, so there is one exit path, not two.
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
            "relationship": True}
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
for seed in range(1, 21):
    for event in run(seed)["events"]:
        body = event.get("body") or ""
        if not (body.endswith(" wants a divorce.") or body.endswith(" wants to break up with you.")):
            continue
        assert body[0].isupper(), (seed, body)
        choice = event["choice"]
        assert choice.startswith(("Beg ", "Wish ", "Hypnotize ")), (seed, choice)
        lanes[choice.split()[0]] += 1
        journal = (event.get("logLines") or [""])[-1]
        assert journal, (seed, "no journal")

assert lanes["Beg"] and lanes["Wish"], dict(lanes)

# A life that wished the partner well has an ex-lover on record (REPORT 134).
for seed in range(1, 21):
    doc = run(seed)
    wished = [e for e in doc["events"]
              if (e.get("body") or "").endswith(" wants to break up with you.")
              and (e.get("choice") or "").startswith("Wish ")]
    if wished:
        assert doc["final"]["exLovers"], (seed, "wished well but no ex recorded")

print("lanes:", dict(lanes))
print("ok")