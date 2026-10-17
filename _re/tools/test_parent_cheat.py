"""The hero walks in on a parent (REPORT.en.md 188).

a16_parent-cheat-scenarios-enc, 36 rows. The body carries an alternative group
'<a/b/c>' and the journal refers back to whichever the body chose ('<rb>'), so the
card picks once and uses it twice.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 20}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
for seed in range(1, 61):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_parent-cheat-scenarios-enc":
            continue
        cards += 1
        body = event["body"]
        assert "<" not in body and ">" not in body, (seed, body)
        journal = (event.get("logLines") or [""])[-1]
        assert "<rb>" not in journal, (seed, journal)
        # Not every row reuses the alternative in its journal -- some say 'having sex'
        # outright -- so the check is one-way: when a journal DOES carry the word, the
        # body must have chosen it too. A left-over '<rb>' is caught above.
        for alt in ("having intercourse", "fornicating", "copulating"):
            if alt in journal:
                assert alt in body, (seed, alt, body, journal)

assert cards >= 2, cards
print("cards:", cards)
print("ok")