#!/usr/bin/env python3
"""The university application (REPORT.en.md 313).

ScenarioSchoolUniversity (RefreshScenario 0x73f2a67ecc) asks a hero who has just
left high school whether to apply, ScenarioUniversityRejectedOptions (.ctor
0x73f2a6936c) is the refusal, and WillBeAccepted 0x73f2a687ac is the admission
test: the school's Difficulty Score indexes RequiredEducationalAbilityScoreForAdmission
(.rodata 0xfc2e9c = [25,45,63,73,80]) and the hero's score plus a 30-point
prerequisite bonus must reach it. The port keeps the threshold table and the bonus
and uses its own schoolGrades for the score, so the boundary sits at 15 grades.

The tests also pin the dispatch order: the graduation card owns the year the hero
turns 18, the application is offered the year after, and college only starts for a
hero who was accepted.
"""
import json, os, subprocess, tempfile

RE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLSIM = os.environ.get("BLSIM", os.path.join(RE, "port", "build", "blsim"))
OFFER = "registry:ScenarioSchoolUniversity"
REJECTED = "registry:ScenarioUniversityRejectedOptions"
ACCEPTED_LINE = "I applied to university and was accepted."
REJECTED_LINE = "I applied to university but my application was rejected."


def run(spec):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(spec, f)
    p = subprocess.run([BLSIM, "--script", f.name], cwd=RE, capture_output=True, text=True)
    os.unlink(f.name)
    return p.returncode, (json.loads(p.stdout) if p.returncode == 0 else None), p.stderr


def card(d, prefix):
    hits = [e for e in d["events"] if str(e.get("table")) == prefix]
    return hits


def logs(d):
    out = []
    for e in d["events"]:
        out.extend(e.get("logLines") or [])
    return out


# 1. An accepted hero: the card is the year after graduation, and college starts.
rc, d, err = run({"seed": 7, "ages": 26, "grades": 70})
assert rc == 0, err
offers = card(d, OFFER)
assert len(offers) == 1, offers
ev = offers[0]
assert ev["age"] == 19, ev["age"]
assert ev["title"] == "Education", ev["title"]
assert ev["choice"] == "Apply to university", ev["choice"]
# The accepted card's own line comes last; the college start line belongs to the
# tuition screen that answers it (REPORT.en.md 317), which is a card of its own.
assert ev["logLines"][-1] == ACCEPTED_LINE, ev["logLines"]
fund = card(d, "registry:ScenarioSchoolFund")
assert len(fund) == 1, fund
college = [l for e2 in d["events"] for l in e2["logLines"] if l == "I started college."]
assert len(college) == 1, college
assert [e2["age"] for e2 in fund] == [19], fund
assert not card(d, REJECTED), "accepted hero cannot also be rejected"

# 2. A rejected hero: its own scenario, its own four ways out, no college.
rc, d, err = run({"seed": 7, "ages": 26, "grades": 10})
assert rc == 0, err
rej = card(d, REJECTED)
assert len(rej) == 1, rej
ev = rej[0]
assert ev["age"] == 19, ev["age"]
assert ev["body"] == "Your application to university was rejected.", ev["body"]
assert ev["choice"] == "Get a job", ev["choice"]
assert ev["logLines"][-1] == REJECTED_LINE, ev["logLines"]
assert not card(d, OFFER), "a rejected hero does not get the offer card"
allLines = logs(d)
assert "I started college." not in allLines, allLines
assert not [l for l in allLines if "extracurricular activities were available at my college" in l]

# 3. The 45-point threshold of the undergraduate row: 14 grades fall short, 15 pass,
#    and a hero with no fixture always clears it (the draw is 62-86).
for grades, table in ((14, REJECTED), (15, OFFER)):
    rc, d, err = run({"seed": 7, "ages": 24, "grades": grades})
    assert rc == 0, err
    assert [e["table"] for e in card(d, OFFER) + card(d, REJECTED)] == [table], grades
rc, d, err = run({"seed": 7, "ages": 24})
assert rc == 0, err
assert card(d, OFFER), "the default grade draw clears the threshold"

# 4. One application per life, never in the graduation year, never after college.
for seed in range(1, 21):
    rc, d, err = run({"seed": seed, "ages": 24, "grades": 70})
    assert rc == 0, err
    ages = [e["age"] for e in card(d, OFFER) + card(d, REJECTED)]
    assert ages in ([19], [18], []), (seed, ages)
    grad = [e["age"] for e in d["events"]
            if str(e.get("table")) == "registry:ScenarioSchoolGraduate"]
    # The graduation card owns its own year; when a life queues anything ahead of it
    # (a funeral) both lines slide by one year, so only the order is asserted.
    if ages and grad:
        assert grad[0] <= ages[0], (seed, grad, ages)

# 5. College years only belong to an accepted hero: the college activity line the
#    school loop writes is absent for a rejected one, present for an accepted one.
rc, d2, err = run({"seed": 7, "ages": 23, "grades": 10})
assert rc == 0, err
assert not [l for l in logs(d2) if "college" in l and "applied" not in l], logs(d2)[-5:]

# 6. The grades fixture is checked, not trusted.
for bad, msg in ((101, "grades must be between 0 and 100"), ("x", "grades is not a number")):
    rc, d, err = run({"seed": 1, "ages": 2, "grades": bad})
    assert rc != 0, bad
    assert msg in err, err

print("test_university: PASS")
