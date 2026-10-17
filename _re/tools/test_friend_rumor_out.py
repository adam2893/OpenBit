"""Row, gate, and carry regression for the friend-rumor outbound card (REPORT.en.md 107)."""
import collections
import csv
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLE = "a16_friend-rumors-enc"
ROWS = list(csv.DictReader((ROOT / "gamedata_plain" / TABLE).open(newline="")))
TITLES = ("In good faith", "We joke, we joke", "This is hilarious",
          "Didn't hear it from me", "It's a shame really", "Spread the word",
          "No bad publicity", "It's probably true", "Don't believe me?")
BODY = re.compile(r"^You started a rumor that (.+)\.$")
NAR = re.compile(r"^I started a rumor that (.+)\.$")
PICK = re.compile(r"^Start a rumor about (him|her)$")
PICKBODY = re.compile(r"^Your friend, (.+?), started a rumor that ")

seen = collections.Counter()
for seed in range(1, 41):
    for gender in ("male", "female"):
        spec = {"seed": seed, "country": "United States", "gender": gender, "ages": 80}
        run = subprocess.run([str(PORT), "--script", "/dev/stdin"],
                             input=json.dumps(spec), text=True, capture_output=True,
                             check=True, cwd=ROOT / "port")
        pending = None
        for event in json.loads(run.stdout)["events"]:
            if event.get("table") != TABLE:
                continue
            if PICK.match(str(event.get("choice") or "")):
                seen["picks"] += 1
                pending = PICKBODY.match(event["body"]).group(1)
                continue
            match = BODY.match(str(event.get("body") or ""))
            if not match:
                continue
            seen["cards"] += 1
            # The card is the answer to the inbound card's last "Start a rumor
            # about ..." pick in this life: the port queues it there because the
            # game pushes the child scenario and the port has no scenario push.
            assert pending is not None, event
            assert event["age"] >= 18, event
            row = ROWS[int(event["row"]) - 1]
            assert event["title"] in TITLES, event
            text = match.group(1)
            narrative = NAR.match(event["logLines"][-1])
            assert narrative and narrative.group(1) == text, event
            assert not event["choice"], event
            assert {k: v for k, v in event["deltas"].items() if v} == {}, event
            # The row's own gates, sender = the hero and receiver = the friend
            # (the port stands the friend at the hero's age).
            for column, floor in (("Sender Min Age", True), ("Sender Max Age", False),
                                  ("Receiver Min Age", True), ("Receiver Max Age", False)):
                value = row[column].strip()
                if value:
                    assert (event["age"] >= int(value)) if floor else (event["age"] <= int(value)), event
            # Every literal segment of the row's Outbound Scenario cell outside its
            # <a/b/c> and <token> groups survives into both texts, and where the row
            # names the friend the queued name is the one that lands.
            for segment in re.split(r"<[^<>]*>", row["Outbound Scenario"]):
                if segment.strip():
                    assert segment.strip() in text, (row["Outbound Scenario"], text)
            if "<friend>" in row["Outbound Scenario"] or "<friend-possessive>" in row["Outbound Scenario"]:
                assert pending in text, (pending, text)
            assert not re.search(r"<[^<>]*>", text), event
            # One-shot: rendering the answer clears the queue, so a second card
            # needs a second pick.
            pending = None
assert seen["cards"], seen
assert seen["cards"] <= seen["picks"], seen
print("friend rumor outbound cards:", dict(sorted(seen.items())))
