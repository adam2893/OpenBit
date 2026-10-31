"""301: the hitman chain -- found card, quote, and all four outcomes."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

FOUND = re.compile(r"^You found a hitman going by the name of (.+?) who is willing to "
                   r"pull off the hit on (.+?)\.\n\nWhat will you do\?$")
QUOTE = re.compile(r"^I contacted a hitman named (.+?) to explore the possibility of "
                   r"wacking out (.+?)\. (He|She) quoted me (\$[\d,]+) to get the job done\.$")
SUCCESS = re.compile(r"^Your hitman, (.+?), pulled off the hit on (.+?) using an? (.+?)!$")
RANOFF = re.compile(r"^Your hitman, (.+?), ran off with your money without pulling off "
                    r"the hit on (.+?)!$")
COP = re.compile(r"^Your hitman, (.+?), turned out to be an undercover cop!$")
APOLOGY = re.compile(r"^Your hitman, (.+?), apologized and explained that (.+?) has "
                     r"difficulty behaving rationally sometimes\.$")
PAY = re.compile(r"^You paid your hitman, (.+?), an extra \$[\d,]+ to leave you alone\.$")
POLICE = re.compile(r"^You called the police on your hitman, (.+?)!\n\nThe police arrived "
                    r"and arrested both of you!$")
ARREST = re.compile(r"^I was arrested for (.+)!$")
CHARGE = re.compile(r"^You've been charged with (.+?) and are facing (\d+) (year|years)!\n")

hitmen = set()
with open(ROOT / "gamedata_plain" / "a16_hitmen-enc", newline="") as fh:
    for row in csv.DictReader(fh):
        name = (row.get("Name") or "").strip()
        if name:
            hitmen.add(name)
assert hitmen, "the hitman table is empty"


def run(seed, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


kinds = {}
found_cards = 0
arrests = 0
arrested = {}
for seed in range(1, 121):
    doc = run(seed)
    cards = {}   # age -> target, so the outcome can be checked against its hire
    arrested = {k: v for k, v in arrested.items() if False}  # per seed state
    for event in doc["events"]:
        body = event.get("body") or ""
        lines = event.get("logLines") or []
        text = body + chr(10) + chr(10).join(lines)
        m = FOUND.match(body)
        if m:
            name, target = m.group(1), m.group(2)
            found_cards += 1
            assert event.get("title") == "Hitman", event
            assert name in hitmen, (seed, name)
            assert event.get("choice") == "Hire " + name, event
            q = None
            for ln in lines:
                q = QUOTE.match(ln)
                if q:
                    break
            assert q, (seed, "no quote line", lines[:4])
            assert q.group(1) == name and q.group(2) == target, (seed, q.groups(), name, target)
            assert (event.get("deltas") or {}).get("karma") == -4, event
            cards[event["age"]] = target
            continue
        for kind, rx in (("success", SUCCESS), ("ranoff", RANOFF), ("cop", COP),
                         ("apology", APOLOGY), ("pay", PAY), ("police", POLICE)):
            mm = re.search(rx.pattern, text, re.M)  # body and log lines are separate lines
            if not mm:
                continue
            kinds[kind] = kinds.get(kind, 0) + 1
            assert event.get("title") in ("Hitman", "Police"), event
            if kind in ("success", "ranoff", "apology", "pay", "police"):
                # Same life as a hire, and the hit resolves within a couple of years.
                assert any(age <= event["age"] <= age + 2 for age in cards), (seed, kind, event)
            if kind == "success":
                # The quoted target is the name in the hit line.
                assert any(t == mm.group(2) for t in cards.values()), (seed, mm.groups(), cards)
            if kind in ("cop", "police"):
                # The cop and police outcomes hand right off to the 273 pipeline.
                arrested[seed] = event["age"]
            break
        m = ARREST.match(lines[-1]) if lines else None
        if m and seed in arrested and event["age"] - arrested[seed] <= 2:
            arrests += 1
            assert m.group(1) == "attempted murder", (seed, event)
            arrested[seed] = -1
        if event.get("title") == "Criminal Charges" and arrested.get(seed) == -1:
            m = CHARGE.search(body)
            assert m and m.group(1) == "attempted murder", (seed, body)
            assert m.group(2) == "5", (seed, body)

assert found_cards >= 4, found_cards
assert arrests >= 1, "no arrest reached"
assert kinds.get("success", 0) >= 1, kinds
assert kinds.get("ranoff", 0) >= 1, kinds
assert len(kinds) >= 5, kinds
print("ok: %d hitman cards, %d arrests, %s" % (found_cards, arrests, kinds))
