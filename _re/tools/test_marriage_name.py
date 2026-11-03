"""396: the identity card after a wedding."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BLSIM = ROOT / "port" / "build" / "blsim"
TABLE = "registry:ScenarioNameChangeMarriage"
BODY = re.compile(
    r"^Now that you're married, you're wondering if you should change your last name\."
    r"\n\nWhat last name will you choose\?\n\n"
    r"Your Last Name: (?P<own>.+)\n(?P<first>\S+) Last Name: (?P<spouse>.+)\n"
    r"Name: (?P<full>.+)$")
KEPT = re.compile(r"^Keep (?P<first>\S+) (?P<own>.+)$")
TAKEN = re.compile(r"^All the way: (?P<first>\S+) (?P<spouse>.+)$")


def run(spec):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(spec, f)
    p = subprocess.run([str(BLSIM), "--script", f.name], cwd=str(ROOT),
                       capture_output=True, text=True)
    return p


seen = {"cards": 0}
lanes = {}
for seed in range(1, 41):
    spec = {"seed": seed, "country": "united states", "gender": "male", "ages": 40,
            "engaged": True, "cash": 500000}
    p = run(spec)
    assert p.returncode == 0, p.stderr
    doc = json.loads(p.stdout)
    for e in doc["events"]:
        if e.get("table") != TABLE:
            continue
        seen["cards"] += 1
        m = BODY.match(e["body"])
        assert m, (seed, e["body"])
        assert e["title"] == "Identity", (seed, e["title"])
        first = m.group("first")
        own = m.group("own")
        spouse = m.group("spouse")
        choice = e["choice"]
        head = choice.split(" ")[0]
        lanes[head] = lanes.get(head, 0) + 1
        if choice.startswith("Keep "):
            k = KEPT.match(choice)
            assert k and k.group("own") == own and k.group("first") == first, (seed, choice)
            assert e["logLines"][-1] == "I kept the last name " + own + ".", (seed, e["logLines"])
        elif choice.startswith("All the way: "):
            k = TAKEN.match(choice)
            assert k and k.group("spouse") == spouse, (seed, choice)
            assert e["logLines"][-1] == "I took the last name " + spouse + ".", (seed, e["logLines"])
        else:
            # The fixture partner is female; native helper puts her first dash-part first.
            surname = spouse.split("-")[0] + "-" + own.split("-")[0]
            assert choice == "Change to " + first + " " + surname, (seed, choice)
            assert e["logLines"][-1] == ("I took both last names and became " + first + " " +
                                         surname + "."), (seed, e["logLines"])
        assert m.group("full") == first + " " + own, (seed, m.group("full"))

assert seen["cards"] >= 3, seen
assert set(lanes) == {"Keep", "Change", "All"}, lanes

# A hero who never marries never sees the card.
for seed in (1, 2, 3):
    p = run({"seed": seed, "country": "united states", "gender": "female", "ages": 40})
    assert p.returncode == 0, p.stderr
    doc = json.loads(p.stdout)
    assert not [e for e in doc["events"] if e.get("table") == TABLE], seed

print("cards:", seen["cards"], "| lanes:", lanes)
print("ok")
