import json, os, re, subprocess, tempfile

ROOT = "/Users/Adam/Documents/bitlife/_re"
BIN = os.path.join(ROOT, "port", "build", "blsim")
THERAPY = "registry:ScenarioSportsPhysicalTherapy"
SURGERY = "registry:ScenarioSportsSurgery"
BODY_T = re.compile(r"^Go to physical therapy for a sports injury today!\n\nInjury: (?P<inj>.+)"
                    r"\nRehab Left: (?P<years>\d+) (?:year|years)$")
BODY_S = re.compile(r"^Undergo surgery for a sports injury today!\n\nInjury: (?P<inj>.+)$")


def run(spec):
    path = os.path.join(tempfile.gettempdir(), "_t394.json")
    with open(path, "w") as fh:
        json.dump(spec, fh)
    out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr[:400]
    return json.loads(out.stdout)


therapy = surgery = 0
therapy_heals = surgery_heals = 0
for seed in range(1, 13):
    doc = run({"seed": seed, "country": "United States", "gender": "male",
               "ages": 45, "sports": True, "sport_injury": 1})
    for event in (doc.get("events") or []):
        if event.get("table") != THERAPY:
            continue
        therapy += 1
        m = BODY_T.match(event.get("body") or "")
        assert m, (seed, event.get("body"))
        assert event.get("choice") == "Go to physical therapy", (seed, event.get("choice"))
        years = int(m.group("years"))
        line = "\n".join(event.get("logLines") or [])
        if years == 0:
            therapy_heals += 1
            assert "cleared up my" in line, (seed, line)
        else:
            assert "spent the year in physical therapy" in line, (seed, line)

for seed in range(1, 25):
    doc = run({"seed": seed, "country": "United States", "gender": "male",
               "ages": 45, "sports": True, "sport_injury": 2})
    for event in (doc.get("events") or []):
        if event.get("table") != SURGERY:
            continue
        surgery += 1
        m = BODY_S.match(event.get("body") or "")
        assert m, (seed, event.get("body"))
        assert event.get("choice") == "Undergo surgery", (seed, event.get("choice"))
        line = "\n".join(event.get("logLines") or [])
        if "cleared to train again" in line:
            surgery_heals += 1
        else:
            assert "did not take" in line, (seed, line)

assert therapy >= 3 and surgery >= 3, (therapy, surgery)
assert therapy_heals and surgery_heals, (therapy_heals, surgery_heals)
print("therapy cards: %d (healed %d) | surgery cards: %d (worked %d)"
      % (therapy, therapy_heals, surgery, surgery_heals))

# Control: no hook, no injury, no cards.
doc = run({"seed": 4, "country": "United States", "gender": "male", "ages": 30, "sports": True})
sports = [e for e in (doc.get("events") or []) if str(e.get("table")).startswith("registry:ScenarioSports")]
assert len(sports) <= 2, sports
print("ok")
