"""255: the driver's licence -- offered at 16, tested, passed or failed."""
import csv
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
PASS = "I passed my driving test and got a driving license."
FAIL = "I failed my driving test."

rows = []
with open(ROOT / "gamedata_plain" / "a16_license-car-questions-enc", newline="") as fh:
    rows = list(csv.DictReader(fh))
correct = {r["Correct"] for r in rows}


def run(seed, ages=22):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


offers = 0
tests = 0
passes = 0
fails = 0
for seed in range(1, 81):
    doc = run(seed)
    licensed = False
    for event in doc["events"]:
        title = event.get("title")
        lines = event.get("logLines") or []
        if title == "Driving License":
            offers += 1
            assert event["age"] >= 16, (seed, event["age"])
            assert event["choice"] in ("Take the driving test", "I couldn't be bothered"), event
            if event["choice"] == "I couldn't be bothered":
                assert "I couldn't be bothered to get a driving license." in lines, (seed, lines)
        elif title == "Driving Test":
            tests += 1
            assert event["table"] == "a16_license-car-questions-enc", event
            row = rows[int(event["row"]) - 1]
            answers = {row["Correct"], row["Incorrect Funny"], row["Incorrect Decoy"]}
            assert event["choice"] in answers, (seed, event["choice"], answers)
            if PASS in lines:
                passes += 1
                licensed = True
                assert event["choice"] in correct, (seed, event["choice"])
            elif FAIL in lines:
                fails += 1
                assert event["choice"] not in correct, (seed, event["choice"])
            else:
                raise AssertionError((seed, "a test with no outcome", lines))
    if licensed:
        assert doc["final"]["license"] is True, (seed, doc["final"]["license"])

assert offers >= 20, offers
assert tests >= 8, tests
assert passes >= 1 and fails >= 1, (passes, fails)
print("offers:", offers, "tests:", tests, "passed:", passes, "failed:", fails)
print("ok")
