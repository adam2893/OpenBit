"""257: safe sex, owed by a hookup."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
BODY = re.compile(r"^Things are getting hot with (.+?) and you're thinking about safe sex\.$")
SAFE = "I practiced safe sex."
UNSAFE = "I practiced unsafe sex."


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
safe = 0
unsafe = 0
for seed in range(1, 61):
    doc = run(seed)
    want = None  # the partner name the card named
    for event in doc["events"]:
        if event.get("title") != "Safe Sex":
            continue
        cards += 1
        assert event["age"] >= 18, (seed, event["age"])
        body = event.get("body") or ""
        m = BODY.match(body)
        assert m, (seed, body)
        name = m.group(1)
        assert name, (seed, body)
        lines = event.get("logLines") or []
        if event["choice"] == "Use a condom":
            safe += 1
            assert SAFE in lines, (seed, lines)
        elif event["choice"] == "Don't use a condom":
            unsafe += 1
            assert UNSAFE in lines, (seed, lines)
        else:
            raise AssertionError((seed, event["choice"]))

assert cards >= 3, cards
assert safe >= 1 and unsafe >= 1, (safe, unsafe)
print("cards:", cards, "safe:", safe, "unsafe:", unsafe)
print("ok")
