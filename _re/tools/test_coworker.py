"""Row, gate, and outcome regression for the coworker cards (REPORT.en.md 105)."""
import collections
import csv
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLES = {
    "a16_coworker-annoy-scenarios-enc": ("Hero Gender", "Coworker Gender", 5),
    "a16_coworker-behavior-scenarios-enc": ("Recipient Gender", "Sender Gender", 4),
    "a16_coworker-pranks-enc": ("", "", 3),
    "a16_coworker-rumors-enc": ("Victim Gender", "", 3),
}
ROWS = {
    name: list(csv.DictReader((ROOT / "gamedata_plain" / name).open(newline="")))
    for name in TABLES
}
seen = collections.Counter()
for seed in range(1, 41):
    for gender in ("male", "female"):
        spec = {"seed": seed, "country": "United States", "gender": gender, "ages": 80}
        run = subprocess.run([str(PORT), "--script", "/dev/stdin"],
                             input=json.dumps(spec), text=True, capture_output=True,
                             check=True, cwd=ROOT / "port")
        counts = collections.Counter()
        coworker = None
        for event in json.loads(run.stdout)["events"]:
            name = event.get("table")
            if name not in TABLES:
                continue
            row = ROWS[name][int(event["row"]) - 1]
            hero_col, colleague_col, cap = TABLES[name]
            counts[name] += 1
            seen[name] += 1
            assert counts[name] <= cap, (name, counts[name])
            assert event["age"] >= 18, event
            assert not hero_col or row[hero_col].strip().lower() in ("", gender), event
            if name.endswith("pranks-enc"):
                assert not row["Vampire"].strip(), event
            text = event["body"] + " ".join(event["logLines"])
            match = re.search(r"[Yy]our coworker, (.+?)(?:'s)?,", event["body"])
            if match:
                if coworker is None:
                    coworker = match[1]
                assert coworker == match[1], event
            assert event["choice"] and event["logLines"], event
            assert not re.search(r"<[a-zA-Z][^<>]*>", text), event
            choice = event["choice"]
            expected = ({"happiness": -8, "discipline": 8}
                        if choice in ("Laugh it off", "Pay no attention", "It doesn't bother me")
                        or choice.startswith("Ignore ") else
                        {"happiness": 16, "discipline": -16}
                        if choice.startswith("Start a rumor about ") else {})
            assert {key: value for key, value in event["deltas"].items() if value} == expected, event
            if colleague_col and row[colleague_col].strip().lower() in ("male", "female"):
                pronoun = "him" if row[colleague_col].strip().lower() == "male" else "her"
                if choice.startswith(("Attack ", "Have a talk with ", "Report ")):
                    assert re.search(r"\b" + pronoun + r"\b", choice), event
            if choice.startswith("Start a rumor about "):
                assert coworker and choice == "Start a rumor about " + coworker, event
assert set(seen) == set(TABLES), seen
print("coworker cards by table:", dict(sorted(seen.items())))
