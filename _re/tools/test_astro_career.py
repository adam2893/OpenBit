#!/usr/bin/env python3
"""The astronaut's promotion and forced-retirement cards (REPORT.en.md 367).

SimSpecialJobAstronaut's own age-up chain builds ScenarioAstronautRankUp once the
rank row's gates pass, and ScenarioAstronautAgencyRetire once the rank's own Too Old
Age is reached. Neither card offers a choice: the native pair is dismiss-only, so the
harness answers the single "OK" the port renders. The rank ladder, its pay windows
and the Too Old Age column all come from a16_astronaut-career-rank-enc; the agency
name comes from a16_astronaut-countries-enc.
"""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
RANKS = ROOT / "gamedata_plain" / "a16_astronaut-career-rank-enc"
COUNTRIES = ROOT / "gamedata_plain" / "a16_astronaut-countries-enc"
RANK_CARD = "registry:ScenarioAstronautRankUp"
RETIRE_CARD = "registry:ScenarioAstronautAgencyRetire"
TITLES = {"Gone for good", "Now I have...free time?", "Clocked out", "Unshackled",
          "Chasing a different bag", "Thanks for the memories", "Nothing for me here",
          "On to bigger & better things", "Spreading my wings"}
BODY = re.compile(r"^You have been promoted to the rank of (?P<rank>.+?) in (?P<agency>.+?)\."
                  r"\n\nNew Rank: (?P<nrank>.+?)\nNew Salary: (?P<sal>\$[\d,]+) "
                  r"\(\+(?P<pct>\d+\.\d)%\)$")
NARR = re.compile(r"^(?P<agency>.+?) promoted me to (?P<rank>.+?) with a salary of "
                  r"(?P<sal>\$[\d,]+)\.$")
RET_BODY = re.compile(r"^You have been forced to retire from your job at (?P<agency>.+?) "
                      r"due to old age\.$")
RET_LOG = re.compile(r"^I retired from (?P<agency>.+?) after being told that I am too old "
                     r"to keep doing this type of work\.$")


def column(path, name):
    with open(path, newline="", encoding="utf-8") as fh:
        return [r[name].strip() for r in csv.DictReader(fh) if r.get(name, "").strip()]


RANK_NAMES = column(RANKS, "Rank")
AGENCIES = set(column(COUNTRIES, "Space Agency Name"))
TOO_OLD = {r["Rank"].strip(): int(r["Too Old Age"]) for r in
           csv.DictReader(open(RANKS, newline="", encoding="utf-8"))}


def lines(event):
    out = []
    for entry in event.get("logLines") or []:
        out.extend(part for part in str(entry).split("\n") if part.strip())
    return out


def run(seed, ages=70, astronaut=True, missions=3):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    if astronaut:
        spec["astronaut"] = True
        spec["astro_missions"] = missions
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    done = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                          capture_output=True, text=True)
    assert done.returncode == 0, done.stderr[-2000:]
    return json.loads(done.stdout)


def cards(doc, table):
    return [e for e in doc["events"] if e.get("table") == table]


def check_promotion(event):
    m = BODY.match(event.get("body") or "")
    assert m, ("body", event.get("body"))
    assert event.get("title") == "Rank up", event.get("title")
    assert event.get("choice") == "OK", event.get("choice")
    assert m.group("agency") in AGENCIES, m.group("agency")
    assert m.group("rank") in RANK_NAMES, m.group("rank")
    assert m.group("nrank").lower() == m.group("rank").lower(), (m.group("nrank"), m.group("rank"))
    assert float(m.group("pct")) > 0.0, m.group("pct")
    logs = lines(event)
    assert any(NARR.match(x) for x in logs), logs
    for x in logs:
        n = NARR.match(x)
        if n:
            assert n.group("sal") == m.group("sal"), (n.group("sal"), m.group("sal"))
            assert n.group("rank") == m.group("rank"), (n.group("rank"), m.group("rank"))
    return RANK_NAMES.index(m.group("rank")) + 1


def check_retirement(event):
    m = RET_BODY.match(event.get("body") or "")
    assert m, ("retirement body", event.get("body"))
    assert event.get("title") in TITLES, event.get("title")
    assert event.get("choice") == "OK", event.get("choice")
    assert m.group("agency") in AGENCIES, m.group("agency")
    logs = lines(event)
    assert any(RET_LOG.match(x) for x in logs), logs
    assert event["age"] >= min(TOO_OLD.values()), event["age"]


def main():
    ranks = retires = 0
    lives = 0
    for seed in range(1, 17):
        doc = run(seed)
        lives += 1
        seen = []
        retired_at = None
        for e in doc["events"]:
            table = e.get("table") or ""
            if table == RANK_CARD:
                assert retired_at is None, (seed, "a promotion after the retirement")
                level = check_promotion(e)
                seen.append(level)
                ranks += 1
            elif table == RETIRE_CARD:
                assert retired_at is None, (seed, "two retirements in one life")
                check_retirement(e)
                retired_at = e["age"]
                retires += 1
        assert seen == sorted(seen), (seed, seen)
        assert len(set(seen)) == len(seen), (seed, seen)
        assert all(level <= len(RANK_NAMES) for level in seen), (seed, seen)
        if retired_at is not None:
            after = [e for e in doc["events"]
                     if e["age"] > retired_at and (e.get("table") or "") in (RANK_CARD, RETIRE_CARD)]
            assert not after, (seed, after)
    print(f"lives: {lives} | promotion cards: {ranks} | retirements: {retires}")
    assert ranks > 0, "no promotion card in sixteen lives"
    assert retires > 0, "no retirement card in sixteen lives"

    plain = run(5, astronaut=False)
    assert not cards(plain, RANK_CARD) and not cards(plain, RETIRE_CARD)
    print("no-agency control ok")


if __name__ == "__main__":
    main()
