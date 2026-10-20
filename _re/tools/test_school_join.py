"""261: the school's start and the activity the hero joins."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
JOIN = re.compile(r"^I joined the (.+?) at my (elementary school|middle school|high school|college)\.$")
START = re.compile(r"^(?:I started (elementary school|middle school|high school)"
                  r"(?: at .+, an? (?:private|public) school)?\.|I started college\.)$")

activities = set()
with open(ROOT / "gamedata_plain" / "a16_school-activities-enc", newline="") as fh:
    for row in csv.DictReader(fh):
        a = (row.get("Activity") or "").strip()
        if a:
            activities.add(a)


def run(seed, ages=22):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


joins = 0
starts = 0
for seed in range(1, 31):
    doc = run(seed)
    announced = []
    for event in doc["events"]:
        for line in (event.get("logLines") or []):
            m = JOIN.match(line)
            if m:
                joins += 1
                assert m.group(1) in activities, (seed, line)
                assert announced, (seed, "joined before the school started", line)
                continue
            m = START.match(line)
            if m:
                starts += 1
                announced.append(m.group(1) or "college")
    # A level starts at most once in a life and only in the school years.
    assert len(announced) == len(set(announced)), (seed, "a level started twice", announced)
    for level in announced:
        assert level in ("elementary school", "middle school", "high school", "college"), level

assert starts >= 30, starts
assert joins >= 10, joins
print("starts:", starts, "joins:", joins)
print("ok")
