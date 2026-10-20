"""273: the arrest -> charge -> plea -> sentence chain."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
ARREST = re.compile(r"^I was arrested for (.+)!$")
CHARGE = re.compile(r"^You've been charged with (.+?) and are facing (\d+) (year|years)!\n")
CONVICT = re.compile(r"^I have been convicted of (.+) and sentenced to (\d+|1) (year|years) in prison!$")

crimes = set()
for name in ("a16_prisoner-crimes-enc", "a16_prisoner-juvenile-crimes-enc"):
    with open(ROOT / "gamedata_plain" / name, newline="") as fh:
        for row in csv.DictReader(fh):
            c = (row.get("Crime") or "").strip()
            if c:
                crimes.add(c)


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


chains = 0
acquittals = 0
for seed in range(1, 41):
    doc = run(seed)
    pending = None
    for event in doc["events"]:
        lines = event.get("logLines") or []
        text = chr(10).join(lines)
        m = ARREST.match(lines[-1]) if lines else None
        if m:
            assert pending is None, (seed, "second arrest before the first ended", event)
            assert m.group(1) in crimes, (seed, m.group(1))
            pending = {"crime": m.group(1), "age": event["age"], "stage": 0}
            continue
        if event.get("title") == "Criminal Charges":
            assert pending, (seed, "charge without an arrest", event)
            m = CHARGE.search(event.get("body") or "")
            assert m, (seed, event.get("body"))
            assert m.group(1) == pending["crime"], (seed, m.group(1), pending)
            pending["stage"] = 1
            continue
        if event.get("title") == "Plea":
            assert pending and pending["stage"] == 1, (seed, "plea out of order", event)
            assert event.get("choice") in ("Plead guilty", "Plead not guilty"), (seed, event)
            pending["plea"] = event["choice"]
            pending["stage"] = 2
            continue
        m = CONVICT.match(lines[-1]) if lines else None
        if m:
            assert pending and pending["stage"] == 2, (seed, "sentence out of order", event)
            assert m.group(1) == pending["crime"], (seed, m.group(1), pending)
            assert event.get("title") == "Guilty", event
            assert event["age"] >= pending["age"], (seed, event, pending)
            assert event["age"] - pending["age"] <= 3, (seed, "the chain drifted", event, pending)
            if pending.get("plea") == "Plead not guilty":
                acquittals += 0
            chains += 1
            pending = None
        elif pending and pending["stage"] == 2 and event["age"] > pending["age"] + 1:
            # A not-guilty plea that won: the chain ends with no sentence.
            assert pending.get("plea") == "Plead not guilty", (seed, event, pending)
            acquittals += 1
            pending = None
    if pending is not None:
        # A death in the year after the plea cuts the chain: the queue never drains
        # for a hero who is no longer there (REPORT.en.md 275.2).
        if doc["final"]["alive"]:
            assert pending["stage"] < 2, (seed, "a plea with no verdict", pending)

assert chains >= 5, chains
print("chains:", chains, "acquittals:", acquittals)
print("ok")
