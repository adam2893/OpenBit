"""306: the horse races -- the friends' invitation, the picker and the result."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"


def horses():
    scores = {}
    with open(DATA / "a16_horses-enc", newline="") as fh:
        for row in csv.DictReader(fh):
            name = (row.get("Horse") or "").strip()
            if name:
                scores[name] = int((row.get("Success Score") or "1").strip() or 1)
    return scores


HORSES = horses()
assert len(HORSES) > 100, len(HORSES)

INVITE = "Some of your friends are going to the horse races.\n\nWill you join them?"
WIN = re.compile(r"^(.+?) won the race!\n\nYou won (\$[\d,]+)!$")
LOSS = re.compile(r"^(.+?) won the race!\n\nYou lost (\$[\d,]+) by betting on (.+?)\.$")
WONLOG = re.compile(r"^I won (\$[\d,]+) after betting on (.+?)!$")
LOSTLOG = re.compile(r"^I lost (\$[\d,]+) after betting on a losing horse named (.+?)!$")


def run(seed, ages=80, cash=500000):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": cash}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


invites = 0
joins = 0
declines = 0
broke = 0
picks = 0
wins = 0
losses = 0
# The invite is one card among many on a rotating wheel; after 315 removed a seat it
# shows up in about a fifth of lives, so scan twice the lives to keep the lanes fed.
# 300 seeds, not 200: the wheel turned wider when the hero's own attack and the other
# families joined the list (REPORT 337), so a given life meets the races less often.
for seed in range(1, 301):
    doc = run(seed)
    events = doc["events"]
    picked = ""
    for index, event in enumerate(events):
        title = event.get("title") or ""
        body = event.get("body") or ""
        lines = event.get("logLines") or []
        if title != "Horse Races":
            continue
        if body.startswith("Some of your friends are going to the horse races."):
            invites += 1
            assert body == INVITE, (seed, body)
            assert event.get("table") == "registry:ScenarioOpportunityHorseTrack", seed
            choice = event.get("choice") or ""
            assert choice in ("Join them", "I'd rather stay home"), (seed, choice)
            last = lines[-1] if lines else ""
            if choice == "Join them":
                joins += 1
                if "enough money" in last:
                    broke += 1
                    assert last == "I didn't have enough money to go to the horse races.", (seed, lines)
                else:
                    assert last == "I went with some friends to the horse races.", (seed, lines)
            else:
                declines += 1
                assert last == "I turned down an opportunity to go to the horse races with friends.", (seed, lines)
            continue
        if body == "Pick your horse":
            picks += 1
            assert event.get("table") == "a16_horses-enc", seed
            choice = event.get("choice") or ""
            assert choice in HORSES, (seed, choice)
            picked = choice
            # A life that ends on the picker's own year has no outcome card: the pick
            # is the year's last event when the hero dies, so there is nothing after it.
            if index + 1 >= len(events):
                continue
            follow = events[index + 1]
            assert (follow.get("title") or "") == "Horse Races", (seed, follow.get("title"))
            assert (follow.get("table") or "") == "registry:ScenarioHorseRacesOutcome", seed
            continue
        win = WIN.match(body)
        loss = LOSS.match(body)
        if win:
            wins += 1
            winner, amount = win.group(1), win.group(2)
            assert winner in HORSES, (seed, winner)
            journal = [ln for ln in lines if WONLOG.match(ln)]
            assert len(journal) == 1, (seed, lines)
            jm = WONLOG.match(journal[0])
            assert jm.group(1) == amount and jm.group(2) == winner, (seed, journal[0])
            if picked:
                assert winner == picked, (seed, winner, picked)
            picked = ""
            continue
        if loss:
            losses += 1
            winner, amount, loser = loss.group(1), loss.group(2), loss.group(3)
            assert winner in HORSES and loser in HORSES, (seed, winner, loser)
            assert winner != loser, (seed, winner)
            if picked:
                assert loser == picked, (seed, loser, picked)
            journal = [ln for ln in lines if LOSTLOG.match(ln)]
            assert len(journal) == 1, (seed, lines)
            jm = LOSTLOG.match(journal[0])
            assert jm.group(1) == amount and jm.group(2) == loser, (seed, journal[0])
            picked = ""
            continue
        raise AssertionError(("unclassified Horse Races card", seed, body))

assert invites >= 20, invites
assert joins >= 10, (invites, joins)
assert declines >= 5, declines
assert picks >= 10, picks
assert wins >= 3, wins
assert losses >= 3, losses
print("invites: %d joins: %d declines: %d broke: %d picks: %d wins: %d losses: %d" %
      (invites, joins, declines, broke, picks, wins, losses))
print("ok")
