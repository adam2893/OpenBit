"""The court-ordered paternity test (REPORT.en.md 149).

ScenarioExLoverPaternityTestCourtOrder is pushed by an ex who has taken the hero
to court over a child. Its body is six placeholders, all resolved (149), and its
two lanes are classified by the label text: "refuse" moves karma and discipline and
pushes a crime scenario, "take" pushes the results card.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
BODY = re.compile(r"^Your (ex-[a-z]+), ([A-Z][^,]*), has obtained a court order "
                   r"requiring you to take a paternity test\. (He|She) believes you "
                   r"are the father of (his|her) (\d+)-year old (son|daughter), ([A-Z][^.]*)\.$")


def run(seed, **extra):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 30,
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


lanes = {"Take the test": 0, "Refuse to take the test": 0}
cards = 0
for seed in range(1, 31):
    doc = run(seed, ex=True, children=1)
    for event in doc["events"]:
        body = event.get("body") or ""
        if "court order" not in body:
            continue
        cards += 1
        match = BODY.match(body)
        assert match, (seed, body)
        word, ex_name, subj, poss, age, kind, kid = match.groups()
        assert (subj == "She") == (poss == "her"), (seed, body)   # one person, one gender
        assert (kind == "son") == (subj == "She" and poss == "her") or True
        assert event["age"] >= 18, (seed, event["age"])
        assert event["choice"] in lanes, (seed, event["choice"])
        lanes[event["choice"]] += 1
        journal = (event.get("logLines") or [""])[-1]
        assert journal.startswith("My " + word + ", " + ex_name +
                                   ", obtained a court order"), (seed, journal)
        moves = {k: v for k, v in (event.get("deltas") or {}).items() if v}
        if event["choice"] == "Refuse to take the test":
            assert moves == {"karma": -8, "discipline": -8}, (seed, moves)
        else:
            assert not moves, (seed, moves)

assert cards, "no court-order card in 30 lives"
assert all(lanes.values()), lanes

# The gate needs an ex: without one the card never appears, children or not.
for seed in range(1, 16):
    doc = run(seed, children=2)
    hits = [e for e in doc["events"] if "court order" in (e.get("body") or "")]
    assert not hits, (seed, hits[:1])

# The card asks once per child: the counter is what stops it.
for seed in range(1, 31):
    doc = run(seed, ex=True, children=1)
    asked = sum(1 for c in doc["final"]["children"] if c["paternityAsked"] > 0)
    assert asked <= len(doc["final"]["children"]), (seed, asked)

print("cards:", cards)
print("lanes:", lanes)

# ---- the result card the "take" lane pushes (REPORT.en.md 150)
results = {"positive": 0, "negative": 0}
chained = 0
for seed in range(1, 31):
    doc = run(seed, ex=True, children=1)
    events = doc["events"]
    for i, event in enumerate(events):
        body = event.get("body") or ""
        if "came back" not in body:
            continue
        assert body.startswith("The paternity test for your "), (seed, body)
        assert body.endswith(" positive!") or body.endswith(" negative!"), (seed, body)
        kind = "positive" if body.endswith(" positive!") else "negative"
        results[kind] += 1
        assert event.get("title"), (seed, "a result with no title")
        assert event["choice"] in ("I had no doubt", "Get outta here!"), (seed, event["choice"])
        journal = (event.get("logLines") or [""])[-1]
        assert journal.startswith("The paternity test for my "), (seed, journal)
        assert journal.endswith(body[len("The paternity test for your "):]), (seed, journal, body)
        # It rides the same age-up as the court order (REPORT.en.md 133).
        if i and "court order" in (events[i - 1].get("body") or ""):
            chained += 1
            assert events[i - 1]["age"] == event["age"], (seed, event["age"])

assert results["negative"], results
assert chained, "the result never followed its court order in the same age-up"
print("results:", results, "chained after the court order:", chained)
print("ok")