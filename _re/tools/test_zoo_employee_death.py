

import json, re, subprocess, tempfile
from pathlib import Path

"""The zoo keeper's death (REPORT.en.md 354).

The keeper is hired with an age and a lifespan. Once the years they have served take them
past that lifespan, a card reports the death and offers the hero the funeral. The two
literals the native concatenates are not the same shape, so an old keeper's card reads
"died died of old age" -- an upstream duplication this port reproduces.
"""

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLE = "registry:ScenarioZooEmployeeDeath"

BODY = re.compile(r"^Your zoo's (?P<role>keeper|veterinarian), (?P<name>.+?), passed away at "
                  r"the age of (?P<age>\d+)\.\n\n(?P<who>He|She) died "
                  r"(?P<why>died of old age|of natural causes)\.$")
JOURNAL = re.compile(r"^My zoo's (?P<role>keeper|veterinarian), (?P<name>.+?), died "
                     r"(?P<why>died of old age|of natural causes)\.$")


def run(seed, ages=95, cash=5000000, zoo=True, only=None, choices=None):
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


def lines(event):
    out = []
    for entry in event.get("logLines") or []:
        out.extend(part for part in entry.split("\n") if part)
    return out


seen = 0
lanes = {}


for seed in range(1, 21):
    events = run(seed, only="zoo-employee-death")["events"]
    seen_in_life = 0
    for event in events:
        table = event.get("table") or ""
        if table != TABLE:
            continue
        seen += 1
        seen_in_life += 1
        body = event.get("body") or ""
        m = BODY.match(body)
        assert m, (seed, body)
        keep_age = int(m.group("age"))
        assert keep_age >= 55, (seed, body)
        if keep_age > 69:
            assert m.group("why") == "died of old age", (seed, body)
        else:
            assert m.group("why") == "of natural causes", (seed, body)
        assert (m.group("who") == "She") == bool(re.search(r"her", event.get("choice") or "")), \
            (seed, event.get("choice"))
        choice = event.get("choice") or ""
        lane = "attend" if choice.startswith("Attend") else "skip" if choice.startswith("Skip") else None
        assert lane, (seed, choice)
        lanes[lane] = lanes.get(lane, 0) + 1
        log = lines(event)
        letter = [x for x in log if JOURNAL.match(x)]
        assert len(letter) == 1, (seed, log)
        j = JOURNAL.match(letter[0])
        assert j.group("why") == m.group("why"), (seed, letter[0], body)
        assert j.group("name") == m.group("name"), (seed, letter[0], body)
        if lane == "attend":
            assert any(re.fullmatch(r"I attended (?:her|his) funeral\.", x) for x in log), (seed, log)
        else:
            assert any(re.fullmatch(r"I skipped (?:her|his) funeral\.", x) for x in log), (seed, log)
        assert event["deltas"].get("happiness", 0) == -2, (seed, event["deltas"])


assert seen, "no death card"
assert set(lanes) == {"attend", "skip"}, lanes


# A life that never owns a zoo never sees the card, and neither does a minor with one.
for seed in range(1, 4):
    assert not [e for e in run(seed, zoo=False)["events"] if e.get("table") == TABLE], seed
    assert not [e for e in run(seed, ages=17)["events"] if e.get("table") == TABLE], seed


print("cards:", seen, "| lanes:", lanes)
print("ok")
