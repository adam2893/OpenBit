"""286: the headhunter -- the prompt's four openings and the hiring line."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
OPENERS = (
    "Just as you're finishing your workout, a headhunter from the ",
    "Just as you're leaving the library, a headhunter from the ",
    "Just as your modeling lessons end, a headhunter from the ",
    "While on your walk, a headhunter from the ",
)
HIRED = re.compile(r"^I was hired for the position of (.+) for (.+) with a salary of \$([\d,]+)\.$")


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


cards = hired = declined = 0
for seed in range(1, 41):
    doc = run(seed)
    for event in doc["events"]:
        if event.get("title") != "When can you start?":
            continue
        cards += 1
        assert event["age"] >= 18, (seed, event["age"])
        body = event.get("body") or ""
        assert body.startswith(OPENERS), (seed, body)
        assert body.endswith("wants to offer you a job.\n\nWhen can you start?"), (seed, body)
        choice = event.get("choice") or ""
        assert choice in ("Accept the job", "I turned them down"), (seed, choice)
        journal = (event.get("logLines") or [""])[-1]
        if choice == "Accept the job":
            hired += 1
            m = HIRED.match(journal)
            assert m, (seed, journal)
            title, company, amount = m.groups()
            salary = int(amount.replace(",", ""))
            assert 60000 <= salary <= 250000, (seed, salary)
            assert title and title not in ("None", "Abbreviated", "Working Class", "Corporate"), (seed, journal)
            assert company and company[0].isupper(), (seed, journal)
        else:
            declined += 1
            assert journal == "I turned them down.", (seed, journal)

assert cards >= 5, cards
assert hired >= 3, hired
assert declined >= 1, declined
print("cards:", cards, "hired:", hired, "declined:", declined)
print("ok")
