"""The hero trolls back (REPORT.en.md 203).

a16_social-media-troll-outbounds-enc. The text carries <insult>, which the shared
resolver already answers from the §138 pool; rows that need a platform or a streaming
career are skipped rather than rendered with a hole.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 40}
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
        if event.get("table") != "a16_social-media-troll-outbounds-enc":
            continue
        cards += 1
        body = event["body"]
        journal = (event.get("logLines") or [""])[-1]
        assert "<" not in body and ">" not in body, (seed, body)
        assert body[0].isupper(), (seed, body)
        assert journal and "<" not in journal, (seed, journal)
        moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
        assert moves.get("karma", 0) == -4, (seed, moves)   # pointsForImpact(-1)

# Two in forty lives is what the current card mix gives: the family wheel fills most
# years and the dumb pool -- where this card lives -- is squeezed (REPORT.en.md 267).
# The bar stays low on purpose; the composition fix is its own section.
assert cards >= 2, cards
print("cards:", cards)
print("ok")