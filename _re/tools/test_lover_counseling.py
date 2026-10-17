"""The counseling demand (REPORT.en.md 152).

ScenarioLoverCounselingPush asks the hero to attend relationship counseling. Its
handler classifies the chosen label by text: Contains("agree") runs the session and
moves discipline, Contains("refuse") only writes the journal.
"""
import collections
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
BODY = re.compile(r"^Your (girlfriend|boyfriend|wife|husband), ([^,]+), is demanding "
                   r"that you visit a relationship counselor with (him|her)\.\n\n"
                   r"What will you do\?$")
AGREE = "Agree to go"
REFUSE = "Refuse to go"


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 60,
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


lanes = collections.Counter()
for seed in range(1, 31):
    doc = run(seed)
    for event in doc["events"]:
        body = event.get("body") or ""
        # Another card also mentions a relationship counselor ("The relationship counselor
        # says that ..."), so the filter is the demand's own wording (REPORT 193).
        if "is demanding that you visit a relationship counselor" not in body:
            continue
        match = BODY.match(body)
        assert match, (seed, body)
        word, partner, them = match.groups()
        assert (them == "her") == (word in ("girlfriend", "wife")), (seed, body)
        assert event["age"] >= 18, (seed, event["age"])
        choice = event["choice"]
        assert choice in (AGREE, REFUSE), (seed, choice)
        lanes[choice] += 1
        journal = (event.get("logLines") or [""])[-1]
        moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
        if choice == AGREE:
            assert journal == ("I agreed to attend relationship counseling with my " +
                               word + ", " + partner + ","), (seed, journal)
            assert moves == {"discipline": 8}, (seed, moves)
        else:
            assert journal.startswith("My " + word + ", " + partner +
                                      ", tried to make me attend"), (seed, journal)
            assert journal.endswith("but I refused."), (seed, journal)
            assert not moves, (seed, moves)

assert lanes[AGREE] and lanes[REFUSE], dict(lanes)

# No partner, no card.
for seed in range(1, 16):
    doc = run(seed, relationship=False)
    hits = [e for e in doc["events"] if "relationship counselor" in (e.get("body") or "")]
    assert not hits, (seed, hits[:1])

print("lanes:", dict(lanes))
print("ok")