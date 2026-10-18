"""No token may reach a rendered line (REPORT.en.md 207).

Until now the residue check lived in 68 probes and a poll script, so a leak was
reported as one number for a whole family and attribution took a round. This runs a
matrix of family switches, reads the `residue` list each event now carries, and names
the table and the token.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

# One spec per family that has ever reported residue.
FAMILIES = [
    {}, {"prison": True}, {"actor": True}, {"business": True}, {"dealer": True},
    {"mafia": True}, {"cult": True}, {"spy": True}, {"outdoor": True},
    {"casino": True}, {"astronaut": True}, {"finance": True}, {"school": True},
    {"vampire": True}, {"fighting": True}, {"politician": True}, {"music": True},
]


def run(seed, extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 70,
            "relationship": True}
    spec.update(extra)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


offenders = {}
for extra in FAMILIES:
    for seed in range(1, 7):
        for event in run(seed, extra)["events"]:
            for item in event.get("residue") or []:
                offenders.setdefault(item, []).append((extra, seed, event["age"]))

if offenders:
    for item, where in sorted(offenders.items()):
        print("%-60s %d  first: %s" % (item[:60], len(where), where[0]))
    raise SystemExit("residue: %d distinct tokens" % len(offenders))

print("families:", len(FAMILIES), "lives:", len(FAMILIES) * 6)
print("residue: 0")
print("ok")