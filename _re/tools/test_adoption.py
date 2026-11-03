"""298: adoption -- the reason snippet and the adoption line."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
LINE = re.compile(r"^I adopted (.+), a (\d+)-year old (boy|girl)\.$")

rows = list(csv.DictReader((ROOT / "gamedata_plain" / "a16_adoption-reasons-enc").open(newline="")))
snippets = [(r.get("Snippet") or "").strip() for r in rows if (r.get("Snippet") or "").strip()]


def run(seed, ages=70):
    spec = {"seed": seed, "country": "United States", "gender": "female", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = adopted = refused = 0
for seed in range(1, 41):
    doc = run(seed)
    for event in doc["events"]:
        if event.get("title") != "Adoption":
            continue
        body = event.get("body") or ""
        # The naming card of REPORT.en.md 399 carries the same title; it comes after
        # the adoption and asks about the child's surname.
        if body.startswith("Now that you've adopted "):
            continue
        cards += 1
        assert event["age"] >= 21, (seed, event["age"])
        assert body.startswith("You are thinking about adopting a child "), (seed, body)
        assert body.endswith("\n\nWhat will you do?"), (seed, body)
        middle = body[len("You are thinking about adopting a child "):-len("\n\nWhat will you do?")]
        if middle.endswith("."):
            middle = middle[:-1]
        patterns = [re.escape(sn).replace(re.escape("<his/her>"), "(his|her)")
                    for sn in snippets]
        assert any(re.fullmatch(pat, middle) for pat in patterns), (seed, body)
        choice = event.get("choice") or ""
        assert choice in ("Adopt the child", "Not now"), (seed, choice)
        if choice == "Not now":
            refused += 1
            continue
        adopted += 1
        last = (event.get("logLines") or [""])[-1]
        m = LINE.match(last)
        assert m, (seed, last)
        name, age, sex = m.groups()
        assert name and 1 <= int(age) <= 8, (seed, last)
        assert sex in ("boy", "girl"), (seed, last)

assert cards >= 4, cards
assert adopted >= 2, adopted
print("adoption cards:", cards, "adopted:", adopted, "refused:", refused)
print("ok")
