"""A news item (REPORT.en.md 212).

a16_news-events-enc, 157 rows of Type + Scenario. Every token in the corpus is a city,
country or numeric range the resolver already answers, so the card needed a gate only.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 70}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
bodies = set()
for seed in range(1, 31):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_news-events-enc":
            continue
        cards += 1
        body = event["body"]
        assert "<" not in body and ">" not in body, (seed, body)
        # A story may open on a quoted word: the capital check reads the first
        # letter, not the first byte.
        lead = body.lstrip(chr(34) + chr(39))
        assert lead and (lead[0].isupper() or lead[0].isdigit()), (seed, body)
        assert event.get("title"), (seed, "no title")
        assert not event.get("residue"), (seed, event.get("residue"))
        bodies.add(body)

assert cards >= 3, cards
assert len(bodies) >= 3, bodies
print("cards:", cards, "distinct:", len(bodies))
print("ok")