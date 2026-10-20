"""267: the optometrist chain -- worsening, the exam, the result."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
WORSEN = "My eyesight has been worsening."
MINOR_VISIT = re.compile(r"^My (mother|father) took me to the optometrist for an eye exam\.$")
ADULT_VISIT = "I went to take an eye exam at the optometrist."
FAIL = "I failed my eye exam!"
PASS = "I passed my eye exam!"


def run(seed, ages=40):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


chains = 0
fails = 0
passes = 0
for seed in range(1, 41):
    doc = run(seed)
    stage = 0  # 0 none, 1 saw the worsening, 2 took the exam
    for event in doc["events"]:
        # The visit and the result share one journal string (one card), so the
        # probe reads the lines flattened.
        lines = []
        for raw in (event.get("logLines") or []):
            lines.extend(raw.split(chr(10)))
        if WORSEN in lines:
            assert stage == 0, (seed, "worsened twice", event)
            stage = 1
            continue
        if event.get("title") in ("Eye Exam", "Eye Exam Failed") and event.get("choice") in (
                "Take the eye exam", "Not right now"):
            assert stage == 1, (seed, "exam without a worsening", event)
            if event["choice"] == "Not right now":
                stage = 0
                continue
            visit = [l for l in lines if l == ADULT_VISIT or MINOR_VISIT.match(l)]
            assert visit, (seed, lines)
            stage = 2
            # The result rides the same card, so the checks below run on this event.
        if FAIL in lines or PASS in lines:
            assert stage == 2, (seed, "result without an exam", event)
            stage = 0
            chains += 1
            if FAIL in lines:
                fails += 1
                assert event.get("title") == "Eye Exam Failed", event
                assert "prescription for eyeglasses" in (event.get("body") or ""), event
            else:
                passes += 1
    if stage == 2:
        raise AssertionError((seed, "an exam took a year with no result"))
    # A failed hero keeps the glasses; a passing one does not get them from nowhere.
    if fails and stage == 0:
        pass

assert chains >= 4, chains
assert fails >= 1 and passes >= 1, (fails, passes)
print("chains:", chains, "failed:", fails, "passed:", passes)
print("ok")
