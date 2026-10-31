"""300: the murder card -- a16_murders-enc's method rows, its three outcome strings."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLE = ROOT / "gamedata_plain" / "a16_murders-enc"
PREFIX = "Decide how you will attempt to murder "
TOKEN = re.compile(r"<[^<>]+>")


def rows():
    out = {}
    with open(TABLE, newline="") as fh:
        for row in csv.DictReader(fh):
            out[row["Method"]] = row
    return out


def template(s):
    """The table's own string, with every placeholder widened to a wildcard."""
    parts = TOKEN.split(s)
    return re.compile("^" + ".+?".join(re.escape(p) for p in parts) + "$")


def run(seed, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run(
        [str(PORT), "--script", path], capture_output=True, cwd=str(ROOT / "port")
    )
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


methods = rows()
cards = success = failed = 0
for seed in range(1, 81):
    doc = run(seed)
    for event in doc["events"]:
        if event.get("title") != "Murder":
            continue
        cards += 1
        assert event["age"] >= 16, (seed, event["age"])
        body = event.get("body") or ""
        assert body.startswith(PREFIX), (seed, body)
        victim = body[len(PREFIX):]
        assert victim.endswith(".") and len(victim) > 1, (seed, body)
        choice = event.get("choice") or ""
        assert choice in methods, (seed, choice)
        row = methods[choice]
        line = (event.get("logLines") or [""])[-1]
        assert "<" not in line, (seed, line)
        landed = []
        for kind in ("Success String", "Attempt String", "Retaliatory String"):
            if template(row[kind]).match(line):
                landed.append(kind)
        assert landed, (seed, choice, line)
        deltas = event.get("deltas") or {}
        if "Success String" in landed:
            success += 1
            assert deltas.get("karma") == -30, (seed, deltas)
        else:
            failed += 1
            assert deltas.get("karma") <= 0, (seed, deltas)
        assert deltas.get("health") in (0, -20, -30), (seed, deltas)

# The 302 robbery seat took one rotation slot, so the same lane now needs the
# wider scan the hitman test already uses (14 cards over 80 seeds).
assert cards >= 8, cards
assert success >= 3, success
assert failed >= 2, failed
print("cards:", cards, "success:", success, "failed:", failed)
print("ok")
