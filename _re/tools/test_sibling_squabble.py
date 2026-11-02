"""256: the sibling squabble -- topic from the table, apology in the journal."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
FEUD = re.compile(r"^I feuded with my (.+?)(?:, (.+?),)? about (.+?)\. I apologized to (him|her) for feuding\.$")

topics = set()
with open(ROOT / "gamedata_plain" / "a16_sibling-arguments-enc", newline="") as fh:
    for row in csv.DictReader(fh):
        t = (row.get("Topic") or "").strip()
        if t and "<" not in t:
            topics.add(t)


def run(seed, ages=50):
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
apologies = 0
declines = 0
for seed in range(1, 61):  # widened when REPORT 386's zoo-supplier family joined the wheel
    doc = run(seed)
    for event in doc["events"]:
        if event.get("title") != "Squabble":
            continue
        cards += 1
        lines = event.get("logLines") or []
        assert event["choice"] in ("Apologize", "Double down"), event
        if event["choice"] == "Double down":
            declines += 1
            assert not [l for l in lines if l.startswith("I feuded with my ")], (seed, lines)
            continue
        apologies += 1
        got = [l for l in lines if l.startswith("I feuded with my ")]
        assert got, (seed, lines)
        m = FEUD.match(got[0])
        assert m, (seed, got[0])
        relation, name, topic, pron = m.groups()
        assert topic in topics, (seed, topic)
        assert ("brother" in relation) != ("sister" in relation), (seed, relation)
        if name:
            # The relation word carries the gender; a named sibling must match the
            # pronoun the sentence picked.
            sibling = [p for p in doc["final"]["family"] if p.get("name") == name]
            if sibling:
                female = sibling[0]["female"]
                assert pron == ("her" if female else "him"), (seed, got[0], sibling[0])

assert cards >= 10, cards
assert apologies >= 3 and declines >= 3, (apologies, declines)
print("cards:", cards, "apologies:", apologies, "declines:", declines)
print("ok")
