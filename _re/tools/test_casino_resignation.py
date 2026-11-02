
import json, re, subprocess, tempfile
from pathlib import Path

"""The casino's two resignations (REPORT.en.md 352).

The guest act and the act in residence each hand in their notice, and the three answers
are the same: offer more money, beg them to stay, or wish them well. Begging is a
relationship toss (the guest) or a popularity toss (the act); a failure removes the act and
costs popularity, and a beg's outcome arrives as a follow-up card.
"""

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TIER = r"(?:Beginner|Amateur|Novice|Regular|High Roller)"

CELEB = "ScenarioCasinoAgeUpCelebrityResignation"
ACT = "ScenarioCasinoAgeUpEntertainmentActResignation"

BODY_C = re.compile(
    r"^Your casino's " + TIER + r", (?P<name>.+?), is informing you that (?:she's|he's) "
    r"resigning from performing at your casino\.\n\n(?:She|He) says (?:she's|he's) booked "
    r"a better gig at a bigger casino across town\.$")
BODY_A = re.compile(
    r"^(?P<act>.+?)'s manager is informing you that the act will no longer perform at "
    r"your casino\.\n\nThey booked a better gig at a bigger casino across town\.$")
JOURNAL_C = re.compile(r"^My casino's " + TIER + r", (?P<name>.+?), put in (?:her|his) "
                       r"resignation from (?:her|his) position at my casino\.$")
JOURNAL_A = re.compile(r"^(?P<act>.+?) put in their resignation from their position at my casino\.$")
WISH_C = re.compile(r"^I wished (?:her|him) well\.$")
WISH_A = re.compile(r"^I wished them well\.$")
BEG_OK_C = re.compile(r"^I begged (?:her|him) to stay and (?:she|he) agreed\.$")
BEG_NO_C = re.compile(r"^(?:She|He) refused to stay, even though I begged\.$")

FB_OK_C = re.compile(r"^(?:She|He) agreed to stay on as your casino's " + TIER + r"!$")
FB_NO_C = re.compile(r"^(?:She|He) turned you down and resigned from (?:her|his) position "
                     r"as your casino's " + TIER + r"\.$")
FB_OK_A = re.compile(r"^They agreed to continue performing at your casino!$")
FB_NO_A = re.compile(r"^They turned you down and resigned anyway\.$")


def run(seed, only, guest=None, act=None, casino=True):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 70,
            "cash": 5000000, "only": only}
    if casino:
        spec["casino"] = True
    if guest:
        spec["casino_guest"] = guest
    if act:
        spec["casino_act"] = act
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    done = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                          capture_output=True, text=True)
    assert done.returncode == 0, done.stderr[-800:]
    return json.loads(done.stdout)


def cards(doc, key):
    return [e for e in doc["events"] if e.get("table") and key in e["table"]]


def lines(event):
    out = []
    for entry in event.get("logLines") or []:
        out.extend(part for part in entry.split("\n") if part)
    return out


seen = {"celeb": 0, "act": 0}
lanes = {"celeb": {}, "act": {}}
ages = {"celeb": {}, "act": {}}
feedback = {"celeb": set(), "act": set()}


def lane_of(choice, seed):
    if choice.startswith("Offer"):
        return "offer"
    if choice.startswith("Beg"):
        return "beg"
    if choice.startswith("Wish"):
        return "wish"
    raise AssertionError((seed, choice))


def scan_feedback(doc, seed):
    for event in doc["events"]:
        if event.get("table") != "registry:Scenario":
            continue
        title = event.get("title")
        body = event.get("body") or ""
        if title == "Fine, I guess":
            assert FB_OK_A.match(body) or FB_OK_C.match(body), (seed, body)
            feedback["act" if FB_OK_A.match(body) else "celeb"].add("ok")
        elif title == "No dice":
            assert FB_NO_A.match(body) or FB_NO_C.match(body), (seed, body)
            feedback["act" if FB_NO_A.match(body) else "celeb"].add("no")


for seed in range(1, 21):
    doc = run(seed, "casino-celeb-resign", guest="same")
    scan_feedback(doc, seed)
    for card in cards(doc, CELEB):
        seen["celeb"] += 1
        assert card["title"] == "Resignation", card.get("title")
        m = BODY_C.match(card.get("body") or "")
        assert m, (seed, card.get("body"))
        key = lane_of(card.get("choice") or "", seed)
        lanes["celeb"][key] = lanes["celeb"].get(key, 0) + 1
        logged = lines(card)
        base = next((lm for lm in (JOURNAL_C.match(l) for l in logged) if lm), None)
        assert base and base.group("name") == m.group("name"), (seed, logged)
        if key == "wish":
            assert any(WISH_C.match(l) for l in logged), (seed, logged)
        elif key == "offer":
            assert not any(WISH_C.match(l) or BEG_OK_C.match(l) or BEG_NO_C.match(l)
                           for l in logged), (seed, logged)
        ages["celeb"].setdefault(seed, []).append(card["age"])

    doc = run(seed, "casino-act-resign", act="same")
    scan_feedback(doc, seed)
    for card in cards(doc, ACT):
        seen["act"] += 1
        assert card["title"] == "Resignation", card.get("title")
        m = BODY_A.match(card.get("body") or "")
        assert m, (seed, card.get("body"))
        key = lane_of(card.get("choice") or "", seed)
        lanes["act"][key] = lanes["act"].get(key, 0) + 1
        logged = lines(card)
        base = next((lm for lm in (JOURNAL_A.match(l) for l in logged) if lm), None)
        assert base and base.group("act") == m.group("act"), (seed, logged)
        if key == "wish":
            assert any(WISH_A.match(l) for l in logged), (seed, logged)
        ages["act"].setdefault(seed, []).append(card["age"])

assert seen["celeb"] and seen["act"], seen
for kind in ("celeb", "act"):
    assert len(lanes[kind]) == 3, lanes[kind]
    for seed, years in ages[kind].items():
        years.sort()
        for a, b in zip(years, years[1:]):
            assert b - a >= 3, (kind, seed, years)

assert feedback["celeb"] == {"ok", "no"}, feedback["celeb"]

for seed in range(1, 6):
    assert not cards(run(seed, "casino-celeb-resign", casino=False), CELEB), seed
    assert not cards(run(seed, "casino-act-resign", casino=False), ACT), seed

print("celeb cards:", seen["celeb"], "| act cards:", seen["act"])
print("lanes:", lanes, "| feedback:", feedback)
print("ok")
