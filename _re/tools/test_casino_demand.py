
#!/usr/bin/env python3
"""The guest act's demand card (REPORT.en.md 356).

The celebrity in residence asks the hero to keep a perk on hand before every
performance: obliging satisfies the demand and raises the act's own popularity,
refusing earns an insult and nothing else. Neither answer moves money -- the
native only prints the yearly cost of the perk.

The native eligibility roll happens before its age/prison/year gates and keeps a
per-celebrity list of satisfied rows; the port tests the plain gates first and
keeps that list on the hero. Both are recorded in the report.
"""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLE = "ScenarioCasinoAgeUpCelebrityDemand"
FOLLOW_UP = "It's a NEED, not a  want"

BODY = re.compile(
    r"^Your casino's artist-in-residence, (?P<name>.+?), is demanding you provide "
    r"(?P<obj>him|her) with a perk, or (?P<sub>he|she) won't perform\.\n\n"
    r"(?P<cont>He's|She's) demanding you have (?P<perk>.+?) on-hand before each of "
    r"(?P<poss>his|her) performances\.\n\n"
    r"Additional Expenses: \$[\d,]+\n"
    r"Request: (?P<request>.+)\n\n"
    r"(?P<cap>His|Her) Fame: (?P<fame>\d+)\n"
    r"(?P=cap) Popularity: (?P<pop>\d+)$")
OBLIGE_LOG = re.compile(
    r"^I began providing (?P<perk>.+?) for my casino's artist-in-residence, (?P<name>.+?)\.$")
REFUSE_LOG = re.compile(
    r"^I refused to give in to my casino's artist-in-residence, "
    r"(?P<name>.+?)'s, demand for (?P<perk>.+?)\.$")
OBLIGE_FB = re.compile(
    r"^You began providing (?P<perk>.+?) for (?P<name>.+?) before each of "
    r"(?P<poss>his|her) performances\.$")
REFUSE_FB = re.compile(
    r"^(?P<sub>He|She) yelled at you for refusing (?P<poss>his|her) request\.\n\n"
    r"(?:(?P=sub)|He|She) called you (?P<insult>.+)\.\.$")


def run(seed, only="casino-celeb-demand", guest="same", casino=True, ages=70, choices=None):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": 2000000, "only": only}
    if casino:
        spec["casino"] = True
    if guest:
        spec["casino_guest"] = guest
    if choices:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    try:
        out = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                             capture_output=True, text=True, check=True).stdout
    finally:
        Path(path).unlink()
    return json.loads(out)


def demand_cards(doc):
    return [e for e in doc["events"] if (e.get("table") or "") == "registry:" + TABLE]


def follow_ups(doc):
    return [e for e in doc["events"]
            if (e.get("table") or "") == "registry:Scenario" and e.get("title") == FOLLOW_UP]


def items():
    """Every table item as a regex: a slash pool matches any of its words."""
    out = []
    with open(ROOT / "gamedata_plain" / "a16_casino-celebrity-demand-enc", newline="") as fh:
        for row in csv.DictReader(fh):
            item = (row.get("Item") or "").strip()
            if not item:
                continue
            parts = []
            for chunk in re.split(r"(<[^>]*>)", item):
                if chunk.startswith("<") and chunk.endswith(">"):
                    words = chunk[1:-1].split("/")
                    # A slash pool names its own words; a lone token is a game
                    # dictionary lookup and the resolved text is not in this file.
                    if len(words) == 1:
                        parts.append(".+")
                    else:
                        parts.append("(?:" + "|".join(re.escape(w) for w in words) + ")")
                else:
                    parts.append(re.escape(chunk))
            out.append(re.compile("".join(parts).lower()))
    return out


def bare(text):
    """The perk as the log and the follow-up print it: no article, no 'some '."""
    text = text.lower().strip()
    for prefix in ("some ", "a ", "an "):
        if text.startswith(prefix):
            text = text[len(prefix):]
    return text.rstrip(".")


def main():
    pool = items()
    assert pool, "table has no items"
    lanes = {}
    cards = 0
    gaps_ok = True
    fame_seen = set()
    for seed in range(1, 21):
        doc = run(seed)
        ages = []
        for event in demand_cards(doc):
            cards += 1
            body = event["body"]
            m = BODY.match(body)
            assert m, (seed, event["age"], body)
            assert m.group("request") == m.group("request").strip(), (seed, "request padded")
            # The Request row is the perk with every major word capitalised; the
            # log and follow-up print the raw parsed item.
            assert bare(m.group("request")) == bare(m.group("perk")), (seed, m.groupdict())
            perk = m.group("perk")
            assert any(p.fullmatch(x) for p in pool
                       for x in (perk.lower(), bare(perk))), (seed, perk)
            assert m.group("obj") == ("her" if m.group("poss") == "her" else "him"), (seed, "pronoun")
            assert m.group("sub") == ("she" if m.group("poss") == "her" else "he"), (seed, "subject")
            assert m.group("cont") == ("She's" if m.group("poss") == "her" else "He's"), (seed, "cont")
            assert int(m.group("fame")) >= 20, (seed, "fame")
            fame_seen.add(int(m.group("fame")))
            lanes[event["choice"]] = lanes.get(event["choice"], 0) + 1
            ages.append(event["age"])
            # Both answers chain the same follow-up card, and every demand writes a
            # narrative line.
            assert event["logLines"], (seed, "no log")
            tail = event["logLines"][-1]
            if event["choice"] == "Refuse":
                r = REFUSE_LOG.match(tail)
                assert r, (seed, tail)
                assert bare(r.group("perk")) == bare(m.group("perk")), (seed, "refuse perk")
            else:
                assert event["choice"] == "Oblige " + m.group("obj"), (seed, event["choice"])
                o = OBLIGE_LOG.match(tail)
                assert o, (seed, tail)
                assert bare(o.group("perk")) == bare(m.group("perk")), (seed, "oblige perk")
            # Neither answer touches money.
            assert not (event.get("deltas") or {}).get("money"), (seed, event.get("deltas"))
        gaps = [b - a for a, b in zip(ages, ages[1:])]
        if any(g < 2 for g in gaps):
            gaps_ok = False
        if seed <= 5:
            for e in follow_ups(doc):
                if OBLIGE_FB.match(e["body"]):
                    continue
                REFUSE_FB.match(e["body"]) or (_ for _ in ()).throw(
                    AssertionError((seed, "follow-up", e["body"])))
    assert gaps_ok, "a demand landed in the year right after another"
    # The card addresses the punter by pronoun, so the oblige lane has two spellings.
    lender = sorted(k for k in lanes if k.startswith("Oblige"))
    assert lender, lanes
    assert set(lanes) == {"Refuse", *lender} or len(lanes) == 2, lanes
    assert lanes.get("Refuse"), lanes
    # The two follow-up bodies, one lane each, checked on the seeds where they occur.
    oblige_fb = refuse_fb = 0
    for seed in range(1, 21):
        doc = run(seed)
        for event in follow_ups(doc):
            if OBLIGE_FB.match(event["body"]):
                oblige_fb += 1
            elif REFUSE_FB.match(event["body"]):
                refuse_fb += 1
            else:
                raise AssertionError((seed, "follow-up", event["body"]))
    assert oblige_fb and refuse_fb, (oblige_fb, refuse_fb)
    # A hero who owns no casino never sees the card.
    assert not demand_cards(run(3, casino=False)), "demand card without a casino"
    print("cards: %d | lanes: %s | follow-ups: %d oblige / %d refuse | fame: %s"
          % (cards, lanes, oblige_fb, refuse_fb, sorted(fame_seen)[:4]))
    print("ok")


if __name__ == "__main__":
    main()
