"""295: the pilot test -- lesson, question, and the pass line."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
PASS = "I passed my pilot test and earned a pilot's license."
FAIL = "I failed my pilot test."

rows = list(csv.DictReader((ROOT / "gamedata_plain" / "a16_license-plane-questions-enc").open(newline="")))
def rows_for(question):
    return [r for r in rows if (r.get("Question") or "").strip() == question]


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


lessons = tests = passed = failed = 0
for seed in range(1, 41):
    doc = run(seed)
    had_test = False
    for event in doc["events"]:
        title = event.get("title")
        last = (event.get("logLines") or [""])[-1]
        if title == "Flying Lesson":
            lessons += 1
            assert event["age"] >= 16, (seed, event["age"])
            assert event.get("body") == "Take a flying lesson today!", (seed, event.get("body"))
            assert event.get("choice") in ("Take a flying lesson", "Not now"), (seed, event.get("choice"))
            if event.get("choice") == "Take a flying lesson":
                assert last == "I took 10 hours of flying lessons.", (seed, last)
            had_test = True
        elif title == "Pilot Test":
            tests += 1
            assert had_test, (seed, "test before the lesson")
            question = event.get("body") or ""
            # The table repeats some questions once per marker image, so the
            # card's own row is the one that lists the chosen answer.
            cand = rows_for(question)
            assert cand, (seed, question)
            row = next((r for r in cand
                        if event.get("choice") in [
                            (r.get("Correct Answer") or "").strip(),
                            (r.get("Incorrect Funny") or "").strip(),
                            (r.get("Incorrect Decoy") or "").strip()]), None)
            assert row is not None, (seed, event.get("choice"), cand)
            if last == PASS:
                passed += 1
                assert event.get("choice") == row["Correct Answer"].strip(), (seed, last)
            else:
                failed += 1
                assert last == FAIL, (seed, last)

assert lessons >= 4, lessons
assert tests >= 3, tests
assert passed + failed == tests, (passed, failed, tests)
print("lessons:", lessons, "tests:", tests, "passed:", passed, "failed:", failed)
print("ok")
