"""Two report cards from tables with no entity tokens (REPORT.en.md 208).

a16_surrogate-rejections-enc and a16_time-machine-scenarios-enc are pure Scenario
tables; the only thing standing between them and the port was a filter each.
"""
import collections
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLES = ("a16_surrogate-rejections-enc", "a16_time-machine-scenarios-enc")


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 80}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


seen = collections.Counter()
for seed in range(1, 41):
    for event in run(seed)["events"]:
        table = event.get("table") or ""
        if table not in TABLES:
            continue
        seen[table] += 1
        body = event["body"]
        assert body[0].isupper(), (seed, body)
        assert "<" not in body, (seed, body)
        # These tables carry no choice column, so a button here would be a card
        # writing into a shared field; checked loosely for now (REPORT 208).
        assert not event.get("residue"), (seed, event.get("residue"))

for table in TABLES:
    assert seen[table] >= 3, (table, seen[table])
print("cards:", dict(seen))
print("ok")