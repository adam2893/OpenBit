import json, os, re, subprocess, tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
TABLE = "registry:ScenarioRaise"
BODY = re.compile(r"^Adjust your salary today!\n\nCurrent Salary: \$(?P<was>[\d,]+)"
                  r"(?:\nNew Salary: \$(?P<next>[\d,]+))?$")


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t393.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


cards = raised = refused = 0
last_new = {}
for seed in range(1, 21):
    doc = run({"seed": seed, "country": "United States", "gender": "female",
               "ages": 45, "cash": 20000})
    prev_age = -1
    prev = None
    for event in (doc.get("events") or []):
        if event.get("table") != TABLE:
            continue
        cards += 1
        body = event.get("body") or ""
        m = BODY.match(body)
        assert m, (seed, body)
        assert event.get("choice") == "Ask for a raise", (seed, event.get("choice"))
        assert event["age"] - prev_age >= 4, (seed, prev_age, event["age"])
        prev_age = event["age"]
        was = int(m.group("was").replace(",", ""))
        if prev is not None:
            # The last raise the hero won is the salary the next card opens on.
            assert was == prev, (seed, prev, was)
        if m.group("next"):
            raised += 1
            nxt = int(m.group("next").replace(",", ""))
            assert nxt > was, (seed, was, nxt)
            prev = nxt
        else:
            refused += 1
            prev = was

assert cards >= 6, cards
assert raised and refused, (raised, refused)
print("cards: %d | raised: %d | refused: %d" % (cards, raised, refused))

# Control: a hero who never held a job never sees the card.
doc = run({"seed": 3, "country": "United States", "gender": "female", "ages": 17})
assert not [e for e in (doc.get("events") or []) if e.get("table") == TABLE]
print("ok")
