import json, os, re, subprocess, tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
TABLE = "registry:ScenarioZooAnimalName"
BODY = re.compile(
    r"^Choose a new name for your animal today!\n\nCurrent Name: (?P<was>.+?)\nName: (?P<next>.+)$")
LOG = re.compile(r"^I renamed (?P<was>.+) the (?P<breed>.+?) to (?P<next>.+)\.$")


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t391.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


cards = 0
renames = 0
chains = 0
pairs = []
for seed in range(1, 13):
    data = run({"seed": seed, "country": "United States", "gender": "female",
                "ages": 40, "cash": 400000, "zoo": True, "zoo_bank": 1500000,
                "only": "zoo-animal-rename"})
    mine = [e for e in (data.get("events") or []) if e.get("table") == TABLE]
    last = None
    prev_age = -1
    for event in mine:
        cards += 1
        body = event.get("body") or ""
        m = BODY.match(body)
        assert m, (seed, body)
        assert m.group("was") != m.group("next"), (seed, m.groupdict())
        assert event.get("choice") == "OK", (seed, event.get("choice"))
        # The card has one answer, so it must land at least three years after the last one.
        assert event["age"] - prev_age >= 3, (seed, prev_age, event["age"])
        prev_age = event["age"]
        if last is not None and m.group("was") == last:
            chains += 1
        last = m.group("next")
        pairs.append((m.group("was"), m.group("next")))
        want = "I renamed " + m.group("was") + " the "
        hits = [t for t in (event.get("logLines") or []) if t.startswith(want)]
        assert hits, (seed, event["age"], event.get("logLines"))
        lg = LOG.match(hits[0])
        assert lg and lg.group("next") == m.group("next"), (seed, hits[0])
        renames += 1

assert cards >= 8, cards
assert renames == cards, (renames, cards)
print("cards: %d | renames: %d | chains: %d | sample: %s" %
      (cards, renames, chains, pairs[:4]))
# Control: without a zoo the card never appears.
plain = run({"seed": 3, "country": "United States", "gender": "female", "ages": 40,
             "cash": 400000, "only": "zoo-animal-rename"})
assert not [e for e in (plain.get("events") or []) if e.get("table") == TABLE]
print("ok")
