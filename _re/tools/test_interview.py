"""296: the interview -- the table's questions, choices and the two outcomes."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
HIRED = re.compile(r"^I was hired for the position of (.+) for (.+) with a salary of \$([\d,]+)\.$")
REFUSED = re.compile(r"^Due to your performance in the interview, you were not offered a position at (.+)\.$")
CLASS_WORDS = {"None", "Abbreviated", "Working Class", "Corporate"}

rows = list(csv.DictReader((ROOT / "gamedata_plain" / "a16_interview-questions-enc").open(newline="")))


def run(seed, ages=70):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = hired = refused = 0
for seed in range(1, 41):
    doc = run(seed)
    for event in doc["events"]:
        if event.get("title") != "Interview":
            continue
        cards += 1
        assert event["age"] >= 18, (seed, event["age"])
        body = event.get("body") or ""
        cand = [r for r in rows if (r.get("Question") or "").strip() == body]
        assert cand, (seed, body)
        cta = event.get("cta") if "cta" in event else None
        choice = event.get("choice") or ""
        allowed = set()
        for row in cand:
            for i in range(1, 15):
                text = (row.get(f"Choice {i}") or "").strip()
                if text:
                    allowed.add(text)
        assert choice in allowed, (seed, choice, body)
        last = (event.get("logLines") or [""])[-1]
        m = HIRED.match(last)
        if m:
            hired += 1
            title, company, amount = m.groups()
            assert title not in CLASS_WORDS, (seed, last)
            assert title and company, (seed, last)
            salary = int(amount.replace(",", ""))
            assert 40000 <= salary <= 139000, (seed, salary)
            # The hired answer is the best-scoring one.
            best = 0
            pick_score = None
            for row in cand:
                for i in range(1, 15):
                    text = (row.get(f"Choice {i}") or "").strip()
                    score = (row.get(f"Choice {i} Score") or "").strip()
                    if text and score:
                        best = max(best, int(score))
                        if text == choice:
                            pick_score = int(score)
            assert pick_score == best, (seed, last)
        else:
            assert REFUSED.match(last), (seed, last)
            refused += 1

assert cards >= 4, cards
assert hired >= 1, hired
print("interviews:", cards, "hired:", hired, "refused:", refused)
print("ok")
