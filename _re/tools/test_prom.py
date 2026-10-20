"""283: the prom -- nomination, the vote, and the trace's own line."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
LOSE = re.compile(r"^I was nominated for prom (king|queen) at (.+ High School), but lost to (.+)\.$")
WIN = re.compile(r"^I was elected prom (king|queen)\.$")
DECLINE = re.compile(r"^You decided you didn't care about being nominated for prom (king|queen)\.$")


def run(seed, ages=20):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = wins = loses = declines = 0
for seed in range(1, 41):
    doc = run(seed)
    for event in doc["events"]:
        if event.get("title") not in ("Prom king", "Prom queen"):
            continue
        cards += 1
        crown = event["title"].split()[1]
        assert 16 <= event["age"] <= 18, (seed, event["age"])
        body = event.get("body") or ""
        assert body.startswith("You've been nominated for prom " + crown), (seed, body)
        choice = event.get("choice") or ""
        assert choice in ("Run for prom " + crown, "I don't care"), (seed, choice)
        lines = event.get("logLines") or []
        if choice == "I don't care":
            declines += 1
            m = next((DECLINE.match(l) for l in lines if DECLINE.match(l)), None)
            assert m and m.group(1) == crown, (seed, lines)
            continue
        m = next((WIN.match(l) for l in lines if WIN.match(l)), None)
        if m:
            wins += 1
            assert m.group(1) == crown, (seed, lines)
            continue
        m = next((LOSE.match(l) for l in lines if LOSE.match(l)), None)
        assert m, (seed, lines)
        loses += 1
        assert m.group(1) == crown, (seed, lines)
        assert len(m.group(3).split()) == 2, (seed, lines)

assert cards >= 8, cards
assert wins and loses and declines, (wins, loses, declines)
print("prom cards:", cards, "won:", wins, "lost:", loses, "declined:", declines)
print("ok")
