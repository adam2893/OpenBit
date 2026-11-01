"""302: the bank robbery card -- three payload tables and the heist's two outcomes."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"


def column(name, key):
    values = set()
    with open(DATA / name, newline="") as fh:
        for row in csv.DictReader(fh):
            value = (row.get(key) or "").strip()
            if value:
                values.add(value)
    return values


DISGUISES = column("a16_bank-robbery-disguises-enc", "Description")
WEAPONS = column("a16_bank-robbery-weapons-enc", "Weapon")
GETAWAYS = column("a16_bank-robbery-getaways-enc", "Description")
assert DISGUISES and WEAPONS and GETAWAYS, (len(DISGUISES), len(WEAPONS), len(GETAWAYS))

# The table's Description column already carries the 'while' clause ("disguised as
# a nun") or a noun phrase ("wearing a Fortnite costume"), so the sentence only
# adds the preposition. The weapon is a bare noun and gets its article.
WEAPON = r"(?:an? )?(.+?)"
WIN = re.compile(r"^You successfully robbed a bank with " + WEAPON + r" while (.+?)!"
                 r"\n\nYou got away (.+?) with (\$[\d,]+)\.$")
CAUGHT = re.compile(r"^You were caught trying to rob a bank with " + WEAPON +
                    r" while (.+?)!$")


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


cards = 0
wins = 0
busts = 0
charges = 0
cut = 0
for seed in range(1, 61):
    doc = run(seed)
    events = doc["events"]
    for index, event in enumerate(events):
        title = event.get("title") or ""
        if title == "Bank Robbery":
            cards += 1
            assert event.get("body") == "You're thinking about robbing a bank.", seed
            assert event.get("table") == "a16_bank-robbery-getaways-enc", seed
            choice = event.get("choice") or ""
            assert choice in GETAWAYS, (seed, choice)
            lines = event.get("logLines") or []
            # The year's family lines are written first, so the card's own two
            # lines are the tail of the list.
            assert len(lines) >= 2, (seed, lines)
            assert lines[-2] == "Bank Robbery: You're thinking about robbing a bank.", seed
            assert lines[-1].startswith("Pick your getaway -> "), seed
            assert lines[-1].endswith(choice), (seed, lines)
            # The result card answers the pick in the same year, but a pending card
            # can be drained between the two (REPORT 342, 345), so walk forward
            # over this age's events instead of trusting the very next one.
            follow = None
            for later in events[index + 1:]:
                if later.get("age") != event.get("age"):
                    break
                if (later.get("title") or "") in ("Heist: Successful", "Police"):
                    follow = later
                    break
            if follow is None:
                # The card can be the last card of the life: the hero's death ends
                # the run before the result card is dealt.
                assert index == len(events) - 1, (seed, event.get("age"))
            continue
        if title == "Heist: Successful":
            wins += 1
            log = (event.get("logLines") or [""])[-1]
            match = WIN.match(event.get("body") or "")
            assert match, (seed, event.get("body"))
            item, disguise, getaway, loot = match.groups()
            assert item in WEAPONS, (seed, item)
            assert disguise in DISGUISES, (seed, disguise)
            assert getaway in GETAWAYS, (seed, getaway)
            assert re.fullmatch(r"\$[\d,]+", loot), loot
            assert log.startswith("I robbed a bank with "), seed
            assert (event.get("deltas") or {}).get("karma") < 0, seed
            continue
        if title == "Police":
            body = event.get("body") or ""
            match = CAUGHT.match(body)
            if not match:
                continue  # an ordinary arrest, not this chain
            busts += 1
            item, disguise = match.groups()
            assert item in WEAPONS, (seed, item)
            assert disguise in DISGUISES, (seed, disguise)
            assert (event.get("logLines") or [""])[-1] == "I was arrested for armed robbery!", seed
            # The 273 chain charges the prison table's own row for an armed robbery.
            # The charge is the next pending card, so a heist caught on the last
            # event of the run has no year left to serve it (the same cut
            # test_crime_chain tolerates when a death ends a chain).
            if index + 1 >= len(events):
                cut += 1
                continue
            charged = any("armed robbery" in ((e.get("body") or "") + " ".join(e.get("logLines") or []))
                          for e in events[index + 1:index + 4])
            assert charged, seed
            charges += 1

assert cards >= 8, cards
assert wins >= 3, wins
assert busts >= 2, busts
assert charges + cut == busts, (charges, cut, busts)
print("picks:", cards, "wins:", wins, "caught:", busts)
print("ok")
