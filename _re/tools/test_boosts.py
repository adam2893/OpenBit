"""A chance to improve an attribute (REPORT.en.md 198).

a16_boosts-enc, 125 rows carrying the whole card: body, conclusion, both buttons, the
accept feedback and the journal. The Attribute names the stat the accept lane moves.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
STATS = {"Happiness", "Health", "Smarts", "Looks"}


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 60}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
for seed in range(1, 31):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_boosts-enc":
            continue
        cards += 1
        body = event["body"]
        assert body[0].isupper(), (seed, body)
        choice = event["choice"]
        assert choice and choice[0].isupper(), (seed, choice)
        journal = (event.get("logLines") or [""])[-1]
        assert journal, (seed, "no journal")
        moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
        # The accept lane moves exactly one attribute by 4; the reject lane moves none.
        if moves:
            assert list(moves) == ["happiness"] or len(moves) == 1, (seed, moves)
            assert list(moves.values()) == [4], (seed, moves)

# The dumb pool this card lives in is squeezed by every family added since
# (REPORT 285); six in forty lives is the current floor.
assert cards >= 5, cards
print("cards:", cards)
print("ok")