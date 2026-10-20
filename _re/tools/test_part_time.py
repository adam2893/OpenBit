"""276: the part-time job -- table row, pay arithmetic, and the line."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
LINE = re.compile(r"^A (.+) hired me as a (.+)\. I earned \$(\d+) after working (\d+) hours for (him|her) at \$(\d+) an hour\.$")

jobs = {}
with open(ROOT / "gamedata_plain" / "a16_part-time-jobs-enc", newline="") as fh:
    for row in csv.DictReader(fh):
        name = (row.get("Job") or "").strip().lower()
        if not name:
            continue
        lo = int((row.get("Min Hourly Rate") or "0").replace("$", "").strip() or 0)
        hi = int((row.get("Max Hourly Rate") or "0").replace("$", "").strip() or 0)
        jobs[name] = (lo, hi)


def run(seed, ages=30):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


count = 0
for seed in range(1, 41):
    doc = run(seed)
    for event in doc["events"]:
        if event.get("title") != "Part-time job":
            continue
        count += 1
        line = (event.get("logLines") or [""])[-1]
        m = LINE.match(line)
        assert m, (seed, line)
        employer, job, pay, hours, pronoun, rate = m.groups()
        assert job in jobs, (seed, job)
        lo, hi = jobs[job]
        assert lo <= int(rate) <= hi, (seed, job, rate, lo, hi)
        assert 8 <= int(hours) <= 16, (seed, hours)
        assert int(pay) == int(hours) * int(rate), (seed, line)
        assert pronoun in ("him", "her"), (seed, line)
        assert employer[0].isupper(), (seed, line)
        assert 13 <= event["age"] <= 22, (seed, event["age"])

assert count >= 5, count
print("part-time jobs:", count)
print("ok")
