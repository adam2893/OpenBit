"""248: the working week -- the overtime request and the year it is let go.

The played-life trace (246) is the evidence: at 32 "My supervisor, Cheyenne, asked me
to start putting in 46 hours a week at Impact Racetrack. I told her that would be no
problem." and at 55 "My supervisor told me I can go back to working 40 hours a week
again." Four requests in sixty years, one release, and the release has no card.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
ASK = re.compile(r"^My supervisor, (.+?), asked me to start putting in (\d+) hours a week at (.+?)\. "
                 r"I told (her|him) that would be no problem\.")
RELEASE = "My supervisor told me I can go back to working 40 hours a week again."


def run(seed, ages=80):
    spec = {"seed": seed, "country": "United States",
            "gender": "female" if seed % 2 else "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
accepted = 0
refused = 0
released = 0
for seed in range(1, 41):
    doc = run(seed)
    live = None  # age an overtime week was accepted at
    for event in doc["events"]:
        lines = event.get("logLines") or []
        if RELEASE in lines:
            released += 1
            assert live is not None, (seed, "released without accepting", event)
            # 2 + below(4) years, so the release lands 2..5 years after the ask.
            assert 2 <= event["age"] - live <= 5, (seed, "release after", live, event["age"])
            live = None
            continue
        if event.get("title") != "Overtime":
            continue
        cards += 1
        assert event["age"] >= 18, (seed, event)
        assert event["choice"] in ("It's no problem", "I'd rather not"), (seed, event)
        ask = [line for line in lines if line.startswith("My supervisor, ")]
        if event["choice"] == "I'd rather not":
            refused += 1
            assert not ask, (seed, "a refusal wrote a sentence", lines)
            continue
        accepted += 1
        assert ask, (seed, lines)
        sentence = ask[0].split(chr(10))[0]
        m = ASK.match(sentence)
        assert m, (seed, ask[0])
        sup, hours, place, pron = m.groups()
        hours = int(hours)
        assert hours % 2 == 0 and 42 <= hours <= 52, (seed, hours)
        # The card and the log name the same supervisor.
        assert sup in (event.get("body") or ""), (seed, event.get("body"))
        assert place in (event.get("body") or ""), (seed, event.get("body"))
        assert pron in ("her", "him"), (seed, pron)
        live = event["age"]
    # A week never stays above forty past the release line.
    if live is not None:
        assert doc["final"]["hoursPerWeek"] > 40, (seed, doc["final"]["hoursPerWeek"])
    else:
        assert doc["final"]["hoursPerWeek"] == 40, (seed, doc["final"]["hoursPerWeek"])

assert cards >= 20, cards
assert accepted >= 10, accepted
assert released >= 8, released
print("cards:", cards, "accepted:", accepted, "refused:", refused, "released:", released)
print("ok")
