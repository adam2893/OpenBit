import json, os, re, subprocess, tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
REJECTED = "registry:ScenarioUniversityRejectedOptions"
COMMUNITY = "registry:ScenarioCommunityCollegeEnroll"
ENLIST = re.compile(r"I enlisted in the (Air Force|Army|Coast Guard|Marines|Navy) with a salary of"
                    r" \$[\d,]+\.\nI am .+\.$")
LANES = ("Enlist in the military", "Enroll in community college",
         "Get a job", "Take some time off")


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t392.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


lanes = {}
enlisted = enrolled = job = off = 0
for seed in range(1, 41):
    doc = run({"seed": seed, "country": "United States", "gender": "female",
               "ages": 24, "grades": 10, "cash": 20000})
    events = doc.get("events") or []
    for i, event in enumerate(events):
        if event.get("table") != REJECTED:
            continue
        choice = event.get("choice")
        assert choice in LANES, (seed, choice)
        lanes[choice] = lanes.get(choice, 0) + 1
        line = (event.get("logLines") or [])[-1]
        if choice == "Enlist in the military":
            # The refusal line always heads the log; the enlistment sentence follows it.
            assert ENLIST.search(line), (seed, line)
            enlisted += 1
        elif choice == "Enroll in community college":
            later = [e for e in events[i:] if e.get("table") == COMMUNITY]
            assert later, (seed, [e.get("table") for e in events[i:]][:5])
            card = later[0]
            assert card["title"] == "Community College", (seed, card["title"])
            assert card["body"] == "Enroll in community college today!", (seed, card["body"])
            assert card["choice"] == "Enroll in community college", (seed, card["choice"])
            assert (card.get("logLines") or [])[-1] == "I enrolled in community college.", seed
            enrolled += 1
        elif choice == "Get a job":
            assert line.endswith("I decided to get a job."), (seed, line)
            job += 1
        else:
            assert line.endswith("I decided to take some time off."), (seed, line)
            off += 1

assert set(lanes) == set(LANES), lanes
assert enlisted and enrolled and job and off, (enlisted, enrolled, job, off)
print("rejections: %d | lanes: %s" % (sum(lanes.values()), lanes))
print("enlisted: %d | community: %d | job: %d | time off: %d" % (enlisted, enrolled, job, off))

# Control: an accepted hero never sees the rejection card, and the community card
# never fires on its own.
clean = run({"seed": 7, "country": "United States", "gender": "female", "ages": 24, "grades": 70})
seen = [e.get("table") for e in (clean.get("events") or [])]
assert COMMUNITY not in seen, seen
print("ok")
