"""Row, gate, and outcome regression for the friend spend-time card (REPORT.en.md 106)."""
import collections
import csv
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLE = "a16_pastimes-enc"
ROWS = list(csv.DictReader((ROOT / "gamedata_plain" / TABLE).open(newline="")))
YES = ("Good times", "Chillin' with my villain", "Loving the crew", "What a trip",
       "Always a blast")
NO = ("Next time...", "With friends like these...", "Busy buddy", "Preoccupied pal",
      "Alright well lmk")
ACCEPT = re.compile(r"^You and your friend, (.+?), went (.+)\.$")
REFUSE = re.compile(r"^You asked your friend, (.+?), to go (.+?) but (she|he) refused\.$")
N_ACCEPT = re.compile(r"^I went (.+) with my friend, (.+?)\.$")
N_REFUSE = re.compile(r"^My friend, (.+?), refused to go (.+) with me\.$")

seen = collections.Counter()
for seed in range(1, 41):
    for gender in ("male", "female"):
        spec = {"seed": seed, "country": "United States", "gender": gender, "ages": 80}
        run = subprocess.run([str(PORT), "--script", "/dev/stdin"],
                             input=json.dumps(spec), text=True, capture_output=True,
                             check=True, cwd=ROOT / "port")
        friend = None
        for event in json.loads(run.stdout)["events"]:
            if event.get("table") != TABLE or event["title"] not in YES + NO:
                continue
            seen["cards"] += 1
            assert event["age"] >= 5, event
            row = ROWS[int(event["row"]) - 1]
            # Every literal segment of the row's Pastime cell outside its <a/b/c>
            # groups survives into the shown text (the groups themselves are
            # replaced by one alternative or by a dictionary token).
            accept = ACCEPT.match(event["body"])
            refuse = REFUSE.match(event["body"])
            assert bool(accept) != bool(refuse), event
            name, pastime = (accept.group(1), accept.group(2)) if accept else \
                (refuse.group(1), refuse.group(2))
            # The port draws the friend's name per card (the family's own shortcut:
            # the game's friend is a real SimPerson with one name, the port's is a
            # drawn slot), so only the body and the narrative must agree.
            assert event["title"] in (YES if accept else NO), event
            assert not re.search(r"<[^<>]*>", pastime), event
            for seg in re.split(r"<[^<>]*>", row["Pastime"]):
                if seg.strip():
                    assert seg.strip() in pastime, (row["Pastime"], pastime)
            narrative = event["logLines"][-1]
            if accept:
                seen["accept"] += 1
                assert N_ACCEPT.match(narrative) and N_ACCEPT.match(narrative).group(1) == pastime, event
                assert N_ACCEPT.match(narrative).group(2) == name, event
                assert not event["choice"], event
                assert {k: v for k, v in event["deltas"].items() if v} == \
                    {"happiness": 8, "karma": 4}, event
            else:
                seen["refuse"] += 1
                assert N_REFUSE.match(narrative) and N_REFUSE.match(narrative).group(1) == name, event
                assert N_REFUSE.match(narrative).group(2) == pastime, event
                assert not event["choice"], event
                assert {k: v for k, v in event["deltas"].items() if v} == \
                    {"happiness": -4}, event
assert seen["cards"], seen
assert seen["accept"] and seen["refuse"], seen
print("friend spend-time cards:", dict(sorted(seen.items())))
