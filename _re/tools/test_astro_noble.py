#!/usr/bin/env python3
"""The Noble Prize nomination card (REPORT.en.md 372).

SimSpecialJobAstronaut::IncrementAge walks the hero's publications and offers a
nomination for any whose peer review score is 95 or better, rolling four for each
(RandomNumber(0,4) == 0). ScenarioAstronautNoblePrizePush prints the publication,
a nominee count drawn from RandomNumber(0, trunc(peer review +0x1c)) + 100, and the
score as a bar, then offers #10545 'Accept the nomination' or #44245 'Turn it down'.
The turn-down lane wraps #49674/#4752 in a bare Scenario titled #48947 'You lost'.
The nomination latches the publication (field +0x28) when the card is built, so it
is offered once. The accept card's own prose and effects are unrecovered (ceiling in
the report), so accepting records the nomination and grants nothing.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

PUSH = "registry:ScenarioAstronautNoblePrizePush"
BODY = re.compile(
    r"^You have been nominated for a Noble Prize in Physics for your publication, "
    r"'(?P<name>.+?)'!\n\n"
    r"Publication: (?P=name)\n"
    r"Number of Nominees: (?P<nominees>\d+)\n\n"
    r"Peer Review: (?P<score>\d+)$")
LOST = re.compile(r"^You turned down the nomination for '(?P<name>.+?)'\.$")
LOG = re.compile(
    r"^I have been nominated for a Noble Prize in Physics for my publication, "
    r"'(?P<name>.+?)'!$")


def run(spec):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    done = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                          capture_output=True, text=True)
    assert done.returncode == 0, done.stderr[-2000:]
    return json.loads(done.stdout)


def spec(seed, pub, ages=70):
    out = {"seed": seed, "country": "United States", "gender": "male",
           "ages": ages, "astronaut": True}
    if pub is not None:
        out["astro_pub"] = pub
    return out


def events(doc):
    return doc["events"]


noms = accepts = declines = 0
seen_scores = set()
for seed in range(1, 13):
    doc = run(spec(seed, 100))
    evs = events(doc)
    mine = [i for i, e in enumerate(evs) if e.get("table") == PUSH]
    # The latch: one nomination per publication, and the port keeps one publication.
    assert len(mine) == 1, (seed, len(mine))
    for i in mine:
        event = evs[i]
        assert event.get("title") == "Noble Prize Nomination", event.get("title")
        m = BODY.match(event.get("body") or "")
        assert m, ("body", event.get("body"))
        assert event.get("choice") in ("Accept the nomination", "Turn it down"), \
            (seed, event.get("choice"))
        assert int(m.group("nominees")) >= 100, m.group("nominees")
        assert int(m.group("score")) == 100, m.group("score")
        seen_scores.add(m.group("score"))
        lines = [LOG.match(x) for x in event.get("logLines") or []]
        assert any(x and x.group("name") == m.group("name") for x in lines), \
            (seed, event.get("logLines"))
        noms += 1
        nxt = [e for e in evs[i + 1:i + 2] if e.get("age") == event.get("age")]
        if event.get("choice") == "Accept the nomination":
            accepts += 1
            # The accept card's prose is unrecovered, so nothing marks the award
            # yet: the nomination is simply over (ceiling, REPORT.en.md 372).
            assert not [e for e in nxt if e.get("title") == "You lost"], seed
        else:
            declines += 1
            assert nxt, (seed, "turn-down wrote no card")
            lost = nxt[0]
            assert lost.get("title") == "You lost", (seed, lost.get("title"))
            lm = LOST.match(lost.get("body") or "")
            assert lm, (seed, lost.get("body"))
            assert lm.group("name") == m.group("name"), (seed, lm.group("name"))
            assert lost.get("choice") == "OK", lost.get("choice")

assert noms >= 10, noms
assert accepts and declines, (accepts, declines)

# Control: a publication that misses the 95 gate is never nominated, even though the
# hero is an astronaut and the publication exists.
control = 0
for seed in range(1, 13):
    doc = run(spec(seed, 60, ages=30))
    control += len([e for e in events(doc) if e.get("table") == PUSH])
assert control == 0, control

# Control: a hero with no publication and no hook never sees the card.
plain = 0
for seed in range(1, 7):
    doc = run(spec(seed, None, ages=24))
    plain += len([e for e in events(doc) if e.get("table") == PUSH])
assert plain == 0, plain

print("nominations: {} | accept: {} | turn down: {} | scores: {}".format(
    noms, accepts, declines, sorted(seen_scores)))
print("ok")
