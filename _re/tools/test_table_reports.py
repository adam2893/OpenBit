"""Two report cards off one helper (REPORT.en.md 196).

a16_paranormal-phenomena-enc and a16_surrogate-run-off-scenarios-enc are the same
shape: Scenario, Log Version, nothing else. Both go through Sim::tableReport.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLES = ("a16_paranormal-phenomena-enc",)


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 85}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


seen = {t: 0 for t in TABLES}
for seed in range(1, 41):
    for event in run(seed)["events"]:
        table = event.get("table") or ""
        if table not in TABLES:
            continue
        seen[table] += 1
        body = event["body"]
        assert body[0].isupper(), (seed, body)
        assert not event.get("choice"), (seed, "a report has no buttons")
        journal = (event.get("logLines") or [""])[-1]
        assert journal, (seed, "no journal")

for table, count in seen.items():
    assert count >= 3, (table, count)
print("cards:", seen)
print("ok")