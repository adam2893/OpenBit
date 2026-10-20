"""290: the diet menu -- the modal, the cost, and the row's own impacts."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
LINE = re.compile(r"^I went on (a|the) (.+) diet\.$")
POINTS = {1: 4, 2: 8, 3: 16, 4: 30, 5: 50, 6: 100, -1: -4, -2: -8, -3: -16, -4: -30, -5: -50, -6: -100}

rows = list(csv.DictReader((ROOT / "gamedata_plain" / "a16_diets-enc").open(newline="")))


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


cards = 0
for seed in range(1, 41):
    doc = run(seed)
    for event in doc["events"]:
        if event.get("title") != "Diet":
            continue
        cards += 1
        assert event["age"] >= 12, (seed, event["age"])
        assert event.get("body") == "Start a diet today!", (seed, event.get("body"))
        assert event.get("choice") == "Do it", (seed, event.get("choice"))
        m = LINE.match((event.get("logLines") or [""])[-1])
        assert m, (seed, event.get("logLines"))
        article, diet = m.groups()
        row = next((r for r in rows if r["Diet"] == diet), None)
        assert row is not None, (seed, diet)
        idx = rows.index(row)
        assert int(event["row"]) in (idx, idx + 1), (seed, event["row"], idx)
        definite = (row.get("Use Definite Article") or "").strip().lower() in ("x", "yes", "true")
        assert article == ("the" if definite else "a"), (seed, article, row)
        moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
        for col, key in (("Health Impact", "health"), ("Appearance Impact", "looks")):
            raw = (row.get(col) or "").strip()
            if not raw:
                continue
            impact = int(raw)
            if impact == 0:
                assert key not in moves or abs(moves[key]) == 1, (seed, col, moves)
            else:
                assert moves.get(key) == POINTS[impact], (seed, col, moves, POINTS[impact])

assert cards >= 4, cards
print("diet cards:", cards)
print("ok")
