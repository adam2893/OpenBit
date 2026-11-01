#!/usr/bin/env python3
"""The coworker's compliment card (REPORT.en.md 331)."""
import json, re, subprocess, sys
from pathlib import Path

ROOT = Path("/Users/Adam/Documents/bitlife/_re")
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"

CALLED = re.compile(r"^You called your co-worker, (?P<name>.+?), (?P<what>.+)\.$")
TOLD = re.compile(r"^You told your co-worker, (?P<name>.+?), that (?P<pron>he's|she's) (?P<what>.+)\.$")
THINK = re.compile(r"^You told your co-worker, (?P<name>.+?), that you think (?P<pron>he's|she's) (?P<what>.+)\.$")
IGNORED = re.compile(r"\n\n(?P<pron>He|She) ignored you\.$")
MCALLED = re.compile(r"^I called my co-worker, (?P<name>.+?), (?P<what>.+)\.$")
MTOLD = re.compile(r"^I told my co-worker, (?P<name>.+?), that (?P<pron>he's|she's) (?P<what>.+)\.$")
MCALLED_ME = re.compile(r"^(?P<pron>He|She) called me (?P<what>.+)\.$")
MTOLD_ME = re.compile(r"^(?P<pron>He|She) told me that I'm (?P<what>.+)\.$")

TITLES = {"A joy to be around", "Office butterfly", "Compliments to the office",
          "There's that smile", "Good natured"}


def rows():
    with open(DATA / "a16_compliments-enc", encoding="utf-8", newline="") as fh:
        import csv
        return list(csv.DictReader(fh))


def run(seed, ages=80, cash=50000):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": cash}
    path = Path("/tmp/qdir/cw_spec.json")
    path.write_text(json.dumps(spec), encoding="utf-8")
    out = subprocess.run([str(PORT), "--script", str(path)], cwd=str(ROOT / "port"),
                         capture_output=True, text=True, check=True).stdout
    return json.loads(out).get("events") or []


def cards(events):
    return [e for e in events if "your co-worker, " in (e.get("body") or "")]


def main():
    table = rows()
    seen = 0
    lanes = {"called": 0, "told": 0, "think": 0, "back": 0, "ignored": 0}
    for seed in range(1, 81):
        events = run(seed)
        found = cards(events)
        for e in found:
            seen += 1
            assert e["choice"] is None, e
            assert e["table"] == "a16_compliments-enc", e
            assert e["title"] in TITLES, e["title"]
            assert e["age"] >= 18, e
            for key, value in e["deltas"].items():
                assert value == 0, (key, value, e)
            head = e["body"].split("\n\n")[0]
            body_match = None
            for kind, pattern in (("called", CALLED), ("told", TOLD), ("think", THINK)):
                m = pattern.match(head)
                if m:
                    body_match = (kind, m)
                    break
            assert body_match, head
            kind, m = body_match
            lanes[kind] += 1
            index = int(e["row"]) - 1
            assert 0 <= index < len(table), e["row"]
            assert table[index]["Compliment"].strip() == m.group("what"), (table[index], m.group("what"))
            pronoun = m.groupdict().get("pron")
            if pronoun:
                assert pronoun in {"he's", "she's"}, pronoun
            # A snapshot carries the whole year's log, so the card's own line is
            # found by shape rather than by position.
            lines = [part for line in e["logLines"] for part in line.split("\n")]
            mirrors = [l for l in lines if MCALLED.match(l) or MTOLD.match(l)]
            assert len(mirrors) == 1, lines
            mm = MCALLED.match(mirrors[0]) or MTOLD.match(mirrors[0])
            assert mm.group("name") == m.group("name"), (mm.group("name"), m.group("name"))
            tail = [l for l in lines if MCALLED_ME.match(l) or MTOLD_ME.match(l)]
            ignored = IGNORED.search(head)
            if ignored:
                lanes["ignored"] += 1
                assert not tail, lines
            if tail:
                lanes["back"] += 1
                assert len(tail) == 1, lines
            else:
                assert not ignored, lines
            assert e["body"].count("ignored you") <= 1, e["body"]
    assert seen >= 8, seen
    for key in ("called", "told"):
        assert lanes[key] >= 1, lanes
    print(f"cards {seen} | lanes {lanes} | titles ok")
    print("ok")


if __name__ == "__main__":
    main()
