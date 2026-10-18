"""Arrest and a child's refusal (REPORT.en.md 209).

a16_arrest-scenarios-enc (<insult>, <numN-M>) and
a16_child-pressure-rejections-enc (<child>, <him/her>): both were blocked only on a
token the resolver already answers.
"""
import collections
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLES = ("a16_arrest-scenarios-enc", "a16_child-pressure-rejections-enc")


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 80,
            "relationship": True, "children": 2, "cash": 20000}
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
        journal = (event.get("logLines") or [""])[-1]
        for text in (body, journal):
            assert "<" not in text and ">" not in text, (seed, text)
        assert body[0].isupper(), (seed, body)
        assert not event.get("residue"), (seed, event.get("residue"))

for table in TABLES:
    assert seen[table] >= 3, (table, seen[table])
print("cards:", dict(seen))
print("ok")