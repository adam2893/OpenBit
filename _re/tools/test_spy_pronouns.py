"""Honeypot briefings and stings must use the mark's own pronouns.

The target table a16_secret-agent-mission-target-people-enc carries a Target
Gender column (female: '<his/her> mistress', male: "a male stripper"), so 'work
the fact that he cannot resist' and the sting's object/possessive pronouns have
to agree with the drawn mark, not with the opposite of the hero (REPORT.en.md
309).
"""
import json
import re
import subprocess
import sys
import tempfile
from multiprocessing import Pool
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
SEEDS = 1500  # per gender; card yield is ~0.115/life, so this lands ~350 cards

# Explicit-gender honeypot rows (Target Gender column, Honeypot flag 'x').
MARKERS = [
    (re.compile(r"best bro|gym buddy|fraternity brother"), "male"),
    (re.compile(r"billionaire playboy"), "male"),
    (re.compile(r"male stripper"), "male"),
    (re.compile(r"mistress"), "female"),
]

BRIEF_PRONOUN = re.compile(r"work the fact that (he|she) cannot resist")
STING = re.compile(r"Close with ([^\n]+?)")


def run(seed_gender):
    seed, gender = seed_gender
    spec = {"seed": seed, "country": "United States", "gender": gender,
            "ages": 80, "relationship": True, "spy": True}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr[-300:])
    out = json.loads(proc.stdout)
    cards = []
    for event in out["events"]:
        body = event.get("body") or ""
        m = BRIEF_PRONOUN.search(body)
        if not m or "Meet " not in body:
            continue
        target = body.split("Meet ", 1)[1].split(" at the ", 1)[0]
        hit = next(((g, rx) for rx, g in MARKERS if rx.search(target)), None)
        if not hit:
            continue
        sting = STING.search(body)
        pron = sting and re.search(r"\b(him|his|her)\b", sting.group(1))
        cards.append({"gender": gender, "seed": seed, "age": event["age"],
                      "mark": hit[0], "brief": m.group(1),
                      "sting": pron.group(1) if pron else None, "target": target})
    return cards


brief_bad, sting_bad = [], []
seen = {"male": 0, "female": 0}
checked = 0
if __name__ == "__main__":
    jobs = [(s, g) for g in ("male", "female") for s in range(1, SEEDS + 1)]
    with Pool(8) as pool:
        for cards in pool.imap_unordered(run, jobs, chunksize=16):
            for c in cards:
                checked += 1
                seen[c["mark"]] += 1
                want_brief = "he" if c["mark"] == "male" else "she"
                if c["brief"] != want_brief:
                    brief_bad.append(c)
                if c["sting"]:
                    want_sting = "him" if c["mark"] == "male" else "her"
                    if c["sting"] != want_sting:
                        sting_bad.append(c)
    assert seen["male"] > 0, "no male-marker honeypot card drawn in %d lives" % (2 * SEEDS)
    assert seen["female"] > 0, "no female-marker honeypot card drawn in %d lives" % (2 * SEEDS)
    assert not brief_bad, "briefing pronoun wrong on %d/%d cards, first: %r" % (
        len(brief_bad), checked, brief_bad[0])
    assert not sting_bad, "sting pronoun wrong on %d/%d cards, first: %r" % (
        len(sting_bad), checked, sting_bad[0])
    print("cards checked:", checked, "per marker:", seen)
    print("briefing pronouns: 0 wrong; sting pronouns: 0 wrong")
    print("ok")
