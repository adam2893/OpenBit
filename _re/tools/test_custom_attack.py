"""337: the hero attacking somebody (ScenarioCustomAttack)."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"

TABLE = "a16_assaults-enc"
BODY = re.compile(r"^How will you attack your (?P<who>.+)\?$")
CHOICE = re.compile(r"^Attack (him|her)$")
PTS = -50   # ImpactKarma(-5) and ImpactDiscipline(-5) through pointsForImpact


def run(seed=5, ages=70, choices=None):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 50000, "only": "custom-attack"}
    if choices is not None:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


def load():
    with open(DATA / TABLE, newline="") as fh:
        data = list(csv.reader(fh))
    head = data[0]
    oi, ci = head.index("Outbound Scenario"), head.index("Clean")
    return [r[oi] for r in data[1:] if len(r) > max(oi, ci) and r[ci].strip() and r[oi].strip()]


FILL = {"you/I": "I", "your/my": "my", "himself/herself": "(?:himself|herself)",
        "his/her": "(?:his|her)", "him/her": "(?:him|her)", "he/she": "(?:he|she)"}


def pattern(cell):
    out, i = "", 0
    while i < len(cell):
        if cell[i] == "<":
            close = cell.find(">", i)
            token = cell[i + 1:close]
            out += FILL.get(token, re.escape(cell[i:close + 1]))
            i = close + 1
        else:
            out += re.escape(cell[i])
            i += 1
    return re.compile("^" + out + "$")


ROWS = load()
PATTERNS = [pattern(cell) for cell in ROWS]


def cards(doc):
    return [e for e in doc["events"] if e.get("table") == TABLE]


def check(doc, where):
    for e in cards(doc):
        assert e["title"] == "Attack", (where, e["title"])
        m = BODY.match(e["body"] or "")
        assert m, (where, e["body"])
        assert CHOICE.match(e["choice"] or ""), (where, e["choice"])
        logs = e["logLines"]
        assert logs, (where, logs)
        assert not any("<" in line for line in logs), (where, logs)
        assert any(hit for hit in (p.match(line) for line in logs for p in PATTERNS)), (where, logs)
        assert e["deltas"]["karma"] == PTS, (where, e["deltas"])
        assert e["deltas"]["discipline"] == PTS, (where, e["deltas"])
        assert not e["residue"], (where, e["residue"])


def main():
    total, gaps, targets = 0, [], set()
    for seed in range(1, 41):
        doc = run(seed=seed)
        check(doc, seed)
        mine = cards(doc)
        total += len(mine)
        for e in mine:
            targets.add(BODY.match(e["body"]).group("who").split(",")[0])
        ages = sorted(e["age"] for e in mine)
        gaps.extend(b - a for a, b in zip(ages, ages[1:]))
    assert total >= 8, total
    assert gaps and min(gaps) >= 3, gaps
    print("attack cards:", total)
    print("targets seen:", sorted(targets))
    print("min cooldown:", min(gaps))

    minor = run(seed=3, ages=17, choices=[0] * 17)
    assert not cards(minor), ("minor card", cards(minor)[:1])
    print("minor: no card")

    forced = run(seed=9, choices=[0] * 70)
    check(forced, "forced")
    print("forced cards:", len(cards(forced)))
    print("ok")


main()
