"""291: martial arts -- the belt ladder and the course line."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
LINE = re.compile(r"^I took 10 (.+) lessons and earned my (.+) belt!$")

belts = {}
with open(ROOT / "gamedata_plain" / "a16_martial-arts-enc", newline="") as fh:
    for row in csv.DictReader(fh):
        art = (row.get("Martial Art") or "").strip().lower()
        if not art:
            continue
        belts[art] = [b.strip().lower() for b in (row.get("Belt Hierarchy") or "").split(",") if b.strip()]


def run(seed, ages=60):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = accepted = declined = 0
for seed in range(1, 41):
    doc = run(seed)
    seen = {}
    for event in doc["events"]:
        title = (event.get("title") or "").lower()
        if title not in belts:
            continue
        cards += 1
        body = event.get("body") or ""
        assert body == f"Enroll in a course of {title} lessons today!", (seed, body)
        choice = event.get("choice") or ""
        if choice == "Not now":
            declined += 1
            continue
        assert choice == f"Take {title} lessons", (seed, choice)
        accepted += 1
        m = LINE.match((event.get("logLines") or [""])[-1])
        assert m, (seed, event.get("logLines"))
        art, belt = m.groups()
        assert art == title, (seed, art, title)
        ladder = belts[title]
        assert belt.lower() in ladder, (seed, belt, ladder)
        previous = seen.get(title)
        if previous is not None:
            assert ladder.index(belt.lower()) == ladder.index(previous) + 1, (seed, belt, previous)
        seen[title] = belt.lower()

assert cards >= 5, cards
assert accepted >= 3, accepted
assert declined >= 1, declined
print("arts cards:", cards, "accepted:", accepted, "declined:", declined)
print("ok")
