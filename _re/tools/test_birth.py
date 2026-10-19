"""A child is born (REPORT.en.md 151).

The game runs birth through SimPregnancy and two cards; the port carries the
pregnancy as a flag and reports the arrival with the class's own sentence, naming
the child from the game's pool. The test ties the card, the journal and the child
array together, and pins that a hero with no partner has no children.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
BODY = re.compile(r"^Your (wife|husband|girlfriend|boyfriend), ([^,]+), just gave "
                   r"birth to a baby (girl|boy)!$")


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


births = 0
lives_with_children = 0
for seed in range(1, 31):
    doc = run(seed)
    kids = doc["final"]["children"]
    if kids:
        lives_with_children += 1
    for event in doc["events"]:
        body = event.get("body") or ""
        if "gave birth" not in body:
            continue
        births += 1
        match = BODY.match(body)
        assert match, (seed, body)
        word, partner, kind = match.groups()
        assert event["age"] >= 18, (seed, event["age"])
        lines = event.get("logLines") or []
        named = [l for l in lines if l.startswith("I named ")]
        assert named, (seed, lines)
        # A child name can be two words ("Ray Lee"), so it is the whole tail, not the
        # last token (REPORT.en.md 242).
        child_name = re.sub(r"^I named (?:him|her) ", "", named[0]).rstrip(".").strip()
        # The child is on the life, with that name and that partner as its parent.
        found = [c for c in kids if c["name"] == child_name]
        assert found, (seed, child_name, [c["name"] for c in kids])
        kid = found[0]
        assert kid["otherParent"] == partner, (seed, kid, partner)
        assert kid["age"] == doc["final"]["age"] - event["age"], (seed, kid, event["age"])
        assert (kind == "girl") == kid["female"], (seed, kid, kind)
        assert kid["isHeroes"] is True, (seed, kid)

assert births, "no birth in 30 lives with partners"
assert lives_with_children, "no life ended with a child"

# A hero with no relationship has no children: the card needs a partner.
for seed in range(1, 16):
    doc = run(seed, relationship=False)
    assert doc["final"]["children"] == [], seed

print("births:", births, "lives with children:", lives_with_children)
print("ok")