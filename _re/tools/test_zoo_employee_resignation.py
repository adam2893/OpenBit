

import csv, json, re, subprocess, tempfile
from pathlib import Path

"""The zoo keeper's resignation (REPORT.en.md 353).

The keeper hands in notice once the park has been open six years and they have not been
spoken to for three. The three answers are: wish them well (they leave, the hero loses a
little happiness), offer a raise (they stay), or insult them one last time (they leave and
the hero loses discipline and karma).
"""

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"
TABLE = "ScenarioZooAgeUpEmployeeResignation"
TITLES = {"And stay out", "Useless mutt", "You dirty, mangy, ugly-", "Only animals could love you"}

BODY = re.compile(r"^Your (?P<role>keeper|veterinarian), (?P<name>.+?), is resigning from "
                  r"(?:her|his) position at your zoo\.\n\n(?P<colour>.+?)\.\n\nWhat will you do\?$")
LETTER = re.compile(r"^My (?P<role>keeper|veterinarian), (?P<name>.+?), handed me a letter of "
                    r"resignation\.$")
WISH = re.compile(r"^I wished (?:her|him) well\.$")
CALLED = re.compile(r"^You called (?:her|him) .+\.$")
INSULT_LOG = re.compile(r"^I called (?:her|him) .+\.$")


def run(seed, ages=90, cash=5000000, zoo=True, only=None, choices=None):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "cash": cash}
    if zoo:
        spec["zoo"] = True
    if only:
        spec["only"] = only
    if choices:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    done = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                          capture_output=True, text=True)
    Path(path).unlink()
    assert done.returncode == 0, done.stderr[-600:]
    return json.loads(done.stdout)


def cards(doc):
    return [e for e in doc["events"] if e.get("table") == "registry:" + TABLE]


def colours():
    with open(DATA / "a16_zoo-employee-resignation-color-enc", newline="") as fh:
        return [row["String"].strip() for row in csv.DictReader(fh)]


def lines(event):
    out = []
    for entry in event.get("logLines") or []:
        out.extend(part for part in entry.split("\n") if part)
    return out


def colour_hits(cell, text):
    parts = re.split(r"<[^>]+>", cell)
    pattern = r".+?".join(re.escape(part) for part in parts)
    return re.fullmatch(pattern, text + ".", re.IGNORECASE) is not None


CELLS = colours()


seen = 0
lanes = {}
ages = {}
feedback = []
appearances = {}


for seed in range(1, 31):
    doc = run(seed, only="zoo-employee-resign")
    for event in doc["events"]:
        table = event.get("table") or ""
        if table == "registry:Scenario" and event.get("title") in TITLES:
            # The insult lane ends on a plain feedback card, so it is counted here.
            feedback.append((event.get("title"), event.get("body") or ""))
            lanes["insult"] = lanes.get("insult", 0) + 1
            log = lines(event)
            assert any(INSULT_LOG.match(x) for x in log), (seed, log)
            assert event["deltas"].get("discipline", 0) == -2, (seed, event["deltas"])
            assert event["deltas"].get("karma", 0) == -3, (seed, event["deltas"])
            continue
        if table != "registry:" + TABLE:
            continue
        seen += 1
        body = event.get("body") or ""
        m = BODY.match(body)
        assert m, (seed, body)
        assert any(colour_hits(cell, m.group("colour")) for cell in CELLS), (seed, body)
        choice = event.get("choice") or ""
        # An insulting answer never shows on this card: it resolves to the feedback card
        # above, so only the two parting favours are read from this one.
        lane = ("wish" if choice.startswith("Wish") else
                "offer" if choice.startswith("Offer") else None)
        assert lane, (seed, choice)
        lanes[lane] = lanes.get(lane, 0) + 1
        ages.setdefault(seed, []).append(event["age"])
        log = lines(event)
        assert any(LETTER.match(x) for x in log), (seed, log)
        if lane == "wish":
            assert any(WISH.match(x) for x in log), (seed, log)
            assert event["deltas"].get("happiness", 0) == -2, (seed, event["deltas"])
        else:
            # The raise keeps the keeper, so no farewell line is written at all.
            assert not any(WISH.match(x) or INSULT_LOG.match(x) for x in log), (seed, log)
        appearances.setdefault(seed, []).append((event["age"], lane, m.group("name")))


assert seen, "no resignation card"
assert set(lanes) == {"wish", "offer", "insult"}, lanes
for seed, years in ages.items():
    years.sort()
    for a, b in zip(years, years[1:]):
        assert b - a >= 4, (seed, years)


for title, body in feedback:
    assert title in TITLES, title
    assert CALLED.match(body), body


# An offer leaves the same keeper in place, so that seed can see the same name twice.
kept = [(seed, seq) for seed, seq in appearances.items()
        if any(lane == "offer" for _, lane, _ in seq)]
assert kept, "no offer lane"


for seed in range(1, 4):
    assert not cards(run(seed, only="zoo-employee-resign", zoo=False)), seed
    assert not cards(run(seed, ages=17, only="zoo-employee-resign")), seed


print("cards:", seen, "| lanes:", lanes, "| feedback:", len(feedback))
print("ok")
