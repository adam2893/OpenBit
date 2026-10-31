#!/usr/bin/env python3
"""The tuition screen and the student loan (REPORT.en.md 317).

ScenarioSchoolFund::ChoiceSelectedAtIndex 0x73f2a5b090 answers the card an accepted
university application hands to. It dispatches on a lowercased substring of the
clicked label - 'free', 'student loan', 'ask', 'scholarship', 'cash' - and the money
only moves on the cash lane; the loan lane creates a SimLoan whose principal grows by
one annual tuition per college year (AddAnotherYearOfTuitionToStudentLoan) and whose
payback starts when the hero leaves college (BeginStudentLoanPayback, #24386), with
one annuity instalment a year and a last line when it is cleared (#24196).

The port shows the four lanes it models and takes the cheapest one the hero can get:
the free lane when the country pays for college (a16_countries-enc <FreeCollege>),
otherwise a scholarship at 96 grades, otherwise its own cash, otherwise the loan. The
tests pin every lane and the money and wording each one owns.
"""
import json, os, subprocess, tempfile

RE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLSIM = os.environ.get("BLSIM", os.path.join(RE, "port", "build", "blsim"))
FUND = "registry:ScenarioSchoolFund"
BROKE = "registry:ScenarioInsufficientFinances"
UNIVERSITY = "registry:ScenarioSchoolUniversity"


def run(spec):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(spec, f)
    p = subprocess.run([BLSIM, "--script", f.name], cwd=RE, capture_output=True, text=True)
    os.unlink(f.name)
    return p.returncode, (json.loads(p.stdout) if p.returncode == 0 else None), p.stderr


def logs(d):
    out = []
    for e in d["events"]:
        out.extend(e["logLines"])
    return out


def card_at(d, prefix):
    hits = [(i, e) for i, e in enumerate(d["events"]) if str(e.get("table")) == prefix]
    return hits


def college_row():
    path = os.path.join(RE, "gamedata_plain", "a16_colleges-enc")
    with open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()
    head = lines[0].split(",")
    row = lines[1].split(",")
    return dict(zip(head, row))


row = college_row()
SCHOOL = row["School"]
TOTAL = int(row["Tuition"]) * int(row["Years"])

# 1. A country whose <FreeCollege> is set hands the hero the free lane: the card is
#    the four-lane one, the log names the country's public education system, and the
#    college rung still starts on the same answer.
rc, d, err = run({"seed": 7, "ages": 20, "country": "Germany", "cash": 0, "grades": 70})
assert rc == 0, err
fund = card_at(d, FUND)
assert len(fund) == 1, [(i, e["table"]) for i, e in enumerate(d["events"])][-6:]
assert fund[0][1]["age"] == 19, fund[0][1]["age"]
assert fund[0][1]["choice"] == "Free public university", fund[0][1]["choice"]
assert fund[0][1]["body"] == "You are now enrolled in %s." % SCHOOL, fund[0][1]["body"]
assert "I enrolled in university for free thanks to the German public education system." in logs(d), logs(d)[-4:]
assert "my tuition" not in " ".join(logs(d)), "the free lane takes no loan or cash"

# 2. The United States has no free college, so the same hero is offered the three paid
#    lanes and the simulated one takes the loan when it cannot afford the tuition.
rc, d, err = run({"seed": 7, "ages": 20, "country": "United States", "cash": 0, "grades": 70})
assert rc == 0, err
fund = card_at(d, FUND)
assert len(fund) == 1, fund
e = fund[0][1]
assert e["choice"] == "Apply for a student loan", e["choice"]
assert e["body"] == "You took out a student loan to pay for your %s tuition." % SCHOOL, e["body"]
assert "I took out a student loan to pay for my %s tuition." % SCHOOL in logs(d), logs(d)[-4:]

# 3. The loan's own years: one add per college year (four), the payback start line the
#    first year out of college (#24386), and the payoff line (#24196) once the annuity
#    has cleared the principal. The installments can take the bank balance negative.
rc, d, err = run({"seed": 7, "ages": 40, "country": "United States", "cash": 0, "grades": 70})
assert rc == 0, err
loan_lines = [(e["age"], l) for e in d["events"] for l in e["logLines"] if "student loan" in l]
start = [a for a, l in loan_lines if l.startswith("I have to start paying back")]
payoff = [a for a, l in loan_lines if l.startswith("I fully paid off")]
assert start == [23], start
assert len(payoff) == 1 and 23 < payoff[0] <= 33, payoff
assert d["final"]["money"] < 0 or d["final"]["money"] == 0, d["final"]["money"]

# 4. The cash lane pays the whole tuition up front and never creates a loan.
rc, d, err = run({"seed": 7, "ages": 20, "country": "United States", "cash": 70000, "grades": 70})
assert rc == 0, err
e = card_at(d, FUND)[0][1]
assert e["choice"] == "Pay with cash", e["choice"]
# The native branch's own body and log were not recovered; the port leaves them empty
# and the test pins that ceiling instead of inventing wording.
assert e["body"] is None, e["body"]
# The screen is the last card of that age, so the year's own money is the cash lane's:
# it pays Tuition * Years in one go.
rc, d19, err = run({"seed": 7, "ages": 19, "country": "United States", "cash": 70000, "grades": 70})
assert rc == 0, err
assert d19["final"]["money"] == 70000 - TOTAL, (d19["final"]["money"], TOTAL)

# 5. A scholarship is offered and awarded on ability, and the rejected form is the
#    card the hero gets when the application falls short.
rc, d, err = run({"seed": 7, "ages": 20, "country": "United States", "cash": 0, "grades": 96})
assert rc == 0, err
e = card_at(d, FUND)[0][1]
assert e["choice"] == "Apply for a scholarship", e["choice"]
assert e["body"] == "Your application for a scholarship to %s has been awarded!" % SCHOOL, e["body"]
assert "I was awarded a scholarship to %s." % SCHOOL in logs(d), logs(d)[-4:]

# 6. A forced lane answers the card: padding the earlier cards with -1 leaves their own
#    answers alone, so the life up to the tuition screen is the unforced one.
rc, d, err = run({"seed": 7, "ages": 20, "country": "United States", "cash": 0, "grades": 70})
assert rc == 0, err
idx = card_at(d, FUND)[0][0]
rc, d, err = run({"seed": 7, "ages": 20, "country": "United States", "cash": 100, "grades": 70,
                  "choices": [-1] * idx + [2]})
assert rc == 0, err
broke = card_at(d, BROKE)
assert len(broke) == 1 and broke[0][1]["age"] == 19, broke
assert broke[0][1]["body"] == "You don't have enough money to pay your %s tuition." % SCHOOL, \
    broke[0][1]["body"]
assert not [l for l in logs(d) if l.startswith("I started college")], logs(d)[-4:]

rc, d, err = run({"seed": 7, "ages": 20, "country": "United States", "cash": 0, "grades": 70,
                  "choices": [-1] * idx + [1]})
assert rc == 0, err
e = card_at(d, FUND)[0][1]
assert e["choice"] == "Apply for a scholarship", e["choice"]
assert e["body"] == "Your application for a scholarship to %s has been rejected." % SCHOOL, e["body"]
assert not [l for l in logs(d) if l.startswith("I started college")], logs(d)[-4:]

# 7. The free lane is index 0 only in a country that pays: the same forced index in the
#    United States is the loan lane.
rc, d, err = run({"seed": 7, "ages": 20, "country": "United States", "cash": 0, "grades": 70,
                  "choices": [-1] * idx + [0]})
assert rc == 0, err
assert card_at(d, FUND)[0][1]["choice"] == "Apply for a student loan", card_at(d, FUND)[0][1]["choice"]

# 8. One tuition screen per life, and only for a hero the university accepted. The
#    application is offered in the two years after high school (REPORT.en.md 313), so a
#    seed whose graduation card lands at 19 never sees either card.
for grades in (70, 10):
    for seed in range(1, 11):
        rc, d, err = run({"seed": seed, "ages": 24, "country": "United States", "cash": 0,
                          "grades": grades})
        assert rc == 0, err
        accepted = [e for _, e in card_at(d, UNIVERSITY) if e["choice"] == "Apply to university"]
        assert len(card_at(d, FUND)) <= 1, (seed, grades, card_at(d, FUND))
        assert len(card_at(d, FUND)) == len(accepted), (seed, grades, accepted, card_at(d, FUND))

# 9. The cash lane and the free lane leave no debt behind: no payback or payoff line.
for country, cash in (("United States", 70000), ("Germany", 0)):
    rc, d, err = run({"seed": 7, "ages": 40, "country": country, "cash": cash, "grades": 70})
    assert rc == 0, err
    assert not [l for l in logs(d) if "student loan" in l], (country, logs(d)[-4:])

print("test_university_fund: PASS")
