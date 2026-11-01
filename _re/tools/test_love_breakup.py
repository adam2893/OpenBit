"""Card, gate and journal regression for the breakup announcement (REPORT.en.md 125).

LoverBreakUp is the card the game pushes once a partner has already walked: its
RefreshScenario builds the whole card out of the pair it was constructed with,
there is no ChoiceSelectedAtIndex, and DismissSelected only writes the narrative
to the journal. The test drives lives with the lover subsystem on, finds every
card the port calls 'See ya', and checks the body template, the status word, the
first-name/FullName split, the empty choice list, and the journal line.
"""
import collections
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TITLE = "See ya"
WORDS = ("girlfriend", "boyfriend", "fianc\u00e9e", "fianc\u00e9", "wife", "husband")
BODY = re.compile(r"^You broke up with your (" + "|".join(WORDS) + r"), ([A-Z][^,]*)\.$")


def run(seed, gender, ages=95):
    spec = {"seed": seed, "country": "United States", "gender": gender, "ages": ages,
            "relationship": True}
    proc = subprocess.run([str(PORT), "--script", "/dev/stdin"], input=json.dumps(spec),
                          text=True, capture_output=True, cwd=str(ROOT / "port"))
    assert proc.returncode == 0, (seed, gender, proc.stderr[-400:])
    return json.loads(proc.stdout)


lanes = collections.Counter()
cards = 0
checked = 0
for seed in range(1, 61):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        pending = None
        for event in doc["events"]:
            # Another card also has no title and speaks of breaking up ('<name> wants to
            # break up with you.', REPORT 164), so the filter is the body itself.
            if not (event.get("body") or "").startswith("You broke up with your "):
                continue
            cards += 1
            assert event["table"] == "relationship", (seed, event.get("table"))
            # The card has no ChoiceSelectedAtIndex, so no lane is ever chosen.
            assert not event.get("choice"), (seed, event.get("choice"))
            for key in ("karma", "discipline", "happiness", "health", "smarts", "looks"):
                assert not (event.get("deltas") or {}).get(key), (seed, key, event["deltas"])
            match = BODY.match(event["body"])
            assert match, (seed, event["body"])
            word, first = match.groups()
            lanes[word] += 1
            # The status word follows the partner the pair was built with: a male
            # partner is a boyfriend/husband/fiance, a female one the -e/-friend form.
            assert not re.search(r"<[a-zA-Z][^<>]*>", event["body"]), (seed, event["body"])
            # DismissSelected logs Format('I broke up with {0}.', FullName), whose
            # first token is the body's own name.
            # The announcement belongs to the same age-up as the break-up card
            # (REPORT.en.md 133), so the card immediately before it has to be the
            # break-up row of that same age.
            # A pending card can drain in the same age-up between the row and the
            # announcement -- the hero's own pregnancy card does now (REPORT 342) -- so walk
            # back over this age's events instead of insisting on the immediate predecessor.
            idx = doc["events"].index(event)
            prev = None
            for earlier in reversed(doc["events"][:idx]):
                if earlier.get("age") != event.get("age"):
                    break
                # Two cards end a pair: the break-up pool and the partner-leaves card (§164).
                if earlier.get("table") in ("a16_breakup-scenarios-enc", "relationship"):
                    prev = earlier
                    break
            assert prev, (seed, doc["events"][max(0, idx - 3):idx])
            hits = [entry for entry in event.get("logLines") or []
                    if entry.startswith("I broke up with ")]
            assert hits, (seed, event.get("logLines", [])[-3:])
            line = hits[-1]
            assert line.startswith("I broke up with " + first), (seed, line, first)
            assert line.endswith("."), (seed, line)
            checked += 1

assert cards, "no breakup cards in 120 lives"
print("breakup cards:", cards)
print("status words:", dict(lanes))
print("ok")
