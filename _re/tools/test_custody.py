"""The custody chain (REPORT.en.md 223).

a16_custody-case-enc opens it, and the arguments land the following year through the
payload queue (REPORT.en.md 184): a16_custody-ex-argument-enc and
a16_custody-child-argument-enc.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 50,
            "children": 1, "ex": True, "cash": 80000}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cases = 0
trials = 0
for seed in range(1, 41):
    pending = 0
    for event in run(seed)["events"]:
        table = event.get("table") or ""
        if table == "a16_custody-case-enc":
            cases += 1
            pending += 1
            body = event["body"]
            assert " is taking you to court for custody, claiming that " in body, (seed, body)
            assert "<" not in body, (seed, body)
            assert event["choice"] in ("Fight it", "Let them have custody"), (seed, event["choice"])
        elif table == "a16_custody-ex-argument-enc":
            trials += 1
            assert pending > 0, (seed, "a trial without a case")
            pending -= 1
            body = event["body"]
            assert "<" not in body and ">" not in body, (seed, body)
            assert event.get("narrative"), (seed, "no verdict")
            assert not event.get("residue"), (seed, event.get("residue"))

assert cases >= 5, cases
assert trials >= cases - 3, (cases, trials)
print("cases:", cases, "trials:", trials)
print("ok")