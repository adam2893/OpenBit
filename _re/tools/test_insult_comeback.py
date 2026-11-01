"""343: the insult's comeback card.

The discipline card's insult button hands the follow-up card its own sentence and a
comeback title (#39989, #40899, #41445, #48825) and the card carries #69007 as its
journal. The insult word comes from a16_insults-enc filtered by the *supervisor's*
gender, so this test reads the pronoun back out of the journal and checks the word
belongs to that pool.
"""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"

DISCIPLINE = "Workplace Discipline"
COMEBACKS = ("Stay classy", "Take that", "That's right", "You heard me")
BODY = re.compile(r"^You called your supervisor, (?P<sup>.+), (?P<insult>.+)$")
LOG = re.compile(r"^I called (?P<them>him|her) (?P<insult>.+)$")


def run(seed, ages=60, choices=None):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 50000, "only": "workplace-discipline"}
    spec["office"] = True
    if choices is not None:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-400:]
    return json.loads(proc.stdout)


def insults_by_gender():
    pools = {"male": [], "female": []}
    with open(DATA / "a16_insults-enc", newline="") as fh:
        for row in csv.DictReader(fh):
            word = (row.get("Insult") or "").strip()
            if not word:
                continue
            gender = (row.get("Gender") or "").strip().lower()
            for key in ("male", "female"):
                if gender in ("", "both", "all", key):
                    # The pool keeps the bare word so the comparison ignores the article.
                    pools[key].append(re.sub(r"^(a|an) ", "", word.lower()))
    return pools


def bare(word):
    # The sentence ends in a full stop and the article rides along with the word.
    word = word.strip().lower().rstrip(".")
    for article in ("a ", "an "):
        if word.startswith(article):
            return word[len(article):]
    return word


pools = insults_by_gender()
seen = 0
titles = set()
for seed in range(1, 31):
    doc = run(seed, choices=[2] * 60)
    events = doc["events"]
    for index, event in enumerate(events):
        if event.get("title") not in COMEBACKS:
            continue
        seen += 1
        titles.add(event["title"])
        assert event.get("age", 0) >= 18, event
        match = BODY.match(event.get("body") or "")
        assert match, (seed, event.get("body"))
        word = match.group("insult")
        assert not re.search(r"<[^<>]+>", word), (seed, word)
        # The card is dismiss-only: no choice, no stat movement of its own.
        assert not event.get("choice"), (seed, event.get("choice"))
        for key, value in (event.get("deltas") or {}).items():
            assert not value, (seed, key, value)
        # The journal repeats the insult word with the supervisor's object pronoun,
        # and the word belongs to the pool for the supervisor's own gender.
        # The journal entry is the whole first-person narrative, so the line it starts
        # with is the one to read.
        logs = []
        for entry in event.get("logLines") or []:
            for part in entry.split("\n"):
                match = LOG.match(part)
                if match:
                    logs.append((part, match))
        assert len(logs) == 1, (seed, event.get("logLines"))
        log = logs[0][1]
        them = log.group("them")
        assert bare(log.group("insult")) == bare(word), (seed, word, logs[0][0])
        gender = "female" if them == "her" else "male"
        assert bare(word) in pools[gender], (seed, word, gender)
        # The line before the card is the discipline card that wrote it, or an earlier
        # card of the same age-up (a pending card can drain in the middle of the year).
        earlier = [e for e in events[:index] if e.get("age") == event.get("age")]
        assert any(e.get("title") == DISCIPLINE for e in earlier), (seed, event)
assert seen, "no comeback cards in 30 lives"
print("comeback cards:", seen, "| titles:", len(titles))
for title in sorted(titles):
    print("  ", title)
print("ok")
