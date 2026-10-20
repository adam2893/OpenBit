"""289: the bookshelf -- the modal's text and the table's own numbers."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
PAGES = re.compile(r"^I read it from cover to cover\. It was (\d+) pages\.$")

rows = list(csv.DictReader((ROOT / "gamedata_plain" / "a16_books-enc").open(newline="")))


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
        if event.get("title") != "Book":
            continue
        cards += 1
        assert event.get("body") == "Curl up with a good book today!", (seed, event.get("body"))
        assert event.get("choice") == "Read it", (seed, event.get("choice"))
        m = PAGES.match((event.get("logLines") or [""])[-1])
        assert m, (seed, event.get("logLines"))
        # The script reports the table index 0-based over the data rows.
        row = rows[int(event["row"])]
        assert int(m.group(1)) == int(row["Pages"]), (seed, m.group(1), row["Pages"])
        assert row["Book"] and row["Author"], (seed, row)

assert cards >= 3, cards
print("book cards:", cards)
print("ok")
