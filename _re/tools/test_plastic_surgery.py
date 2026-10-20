"""297: plastic surgery -- the procedure, the cost, and the two outcomes."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
UNDERWENT = re.compile(r"^I underwent surgery on my (.+)\.$")
BOTCHED = re.compile(r"^I had a botched (.+)!$")

rows = list(csv.DictReader((ROOT / "gamedata_plain" / "a16_plastic-surgeries-enc").open(newline="")))
parts = set()
for r in rows:
    procedure = (r.get("Procedure") or "").strip()
    desc = (r.get("Description") or "").strip()
    if procedure:
        parts.add(desc or procedure)


def run(seed, gender, ages=70):
    spec = {"seed": seed, "country": "United States", "gender": gender, "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = done = botched = refused = 0
for seed in range(1, 41):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        for event in doc["events"]:
            if event.get("title") != "Plastic Surgery":
                continue
            cards += 1
            assert event["age"] >= 18, (seed, gender, event["age"])
            assert event.get("body") == "Get plastic surgery.", (seed, event.get("body"))
            choice = event.get("choice") or ""
            assert choice in ("Get the surgery", "Not now"), (seed, choice)
            if choice == "Not now":
                refused += 1
                continue
            last = (event.get("logLines") or [""])[-1]
            m = UNDERWENT.match(last)
            if m:
                done += 1
                assert m.group(1) in parts, (seed, gender, last)
                continue
            m = BOTCHED.match(last)
            assert m, (seed, gender, last)
            botched += 1
            assert m.group(1) in parts, (seed, gender, last)

assert cards >= 6, cards
assert done >= 2, done
print("surgery cards:", cards, "done:", done, "botched:", botched, "refused:", refused)
print("ok")
