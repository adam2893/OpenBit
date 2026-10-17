"""Row, gate, and carry regression for the friend-status card (REPORT.en.md 109)."""
import collections
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
BODY = re.compile(r"^Your friend, (.+?), (wants to become best friends with you|has made you (his|her) enemy)\.$")
REQUEST_NAR = re.compile(r"^My friend, (.+?), asked me to become best friends with (him|her)\.$")
ENEMY_NAR = re.compile(r"^My friend, (.+?), and I became enemies\.$")
AGREE = "Agree to become best friends"
KEEP = "Keep the status quo"
# AttributeAdjustmentPointsForImpactScore (src/sim.cpp pointsForImpact): -3 -> -16,
# +5 -> 50, so the card's raw scores land on those points.
ENEMY_DELTAS = {"happiness": -16}
AGREE_DELTAS = {"happiness": 50}

seen = collections.Counter()
asked_at = collections.defaultdict(list)
for seed in range(1, 41):
    for gender in ("male", "female"):
        spec = {"seed": seed, "country": "United States", "gender": gender, "ages": 80}
        run = subprocess.run([str(PORT), "--script", "/dev/stdin"],
                             input=json.dumps(spec), text=True, capture_output=True,
                             check=True, cwd=ROOT / "port")
        for event in json.loads(run.stdout)["events"]:
            match = BODY.match(str(event.get("body") or ""))
            if not match:
                continue
            seen["cards"] += 1
            name, tail = match.group(1), match.group(2)
            text = " ".join(str(event.get(k) or "") for k in
                            ("title", "body", "choice", "logLines"))
            assert "<" not in text, event
            assert event["age"] >= 12, event
            assert not str(event.get("title") or ""), event
            narrative = event["logLines"][-1]
            deltas = {k: v for k, v in event["deltas"].items() if v}
            if tail == "wants to become best friends with you":
                seen["request"] += 1
                assert event["choice"] in (AGREE, KEEP), event
                nar = REQUEST_NAR.match(narrative)
                assert nar and nar.group(1) == name, event
                if event["choice"] == AGREE:
                    assert deltas == AGREE_DELTAS, event
                else:
                    assert deltas == {}, event
                asked_at[(seed, gender)].append(event["age"])
            else:
                seen["enemy"] += 1
                assert tail.endswith("enemy"), event
                assert not str(event.get("choice") or ""), event
                nar = ENEMY_NAR.match(narrative)
                assert nar and nar.group(1) == name, event
                assert deltas == ENEMY_DELTAS, event
            seen["agree" if event["choice"] == AGREE else
                 "keep" if event["choice"] == KEEP else "dismiss"] += 1

for key, ages in asked_at.items():
    for before, after in zip(ages, ages[1:]):
        assert after - before >= 11, (key, ages)

print("friend status cards:", dict(seen))
