"""333: the vampire hunt's animal card (ScenarioVampireHuntAnimal)."""
import csv
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"


def rows(name):
    with open(DATA / name, newline="") as fh:
        return list(csv.DictReader(fh))


ANIMALS = rows("a16_animals-enc")
LOCS = {r["String Version"].strip() for r in rows("a16_vampire-solo-hunt-location-enc")}
TITLES = {r["Scenario Tag"].strip(): {v.strip() for k, v in r.items() if k != "Scenario Tag" and v and v.strip()}
          for r in rows("a16_vampire-titles-enc")}
PARTS = {r["Body Part"].strip() for r in rows("a16_body-parts-enc") if r["Gender"].strip() in ("both", "male")}
WIN = {"Big heart", "Jane Goodall mode", "Star citizen", "It lives to see another day"}
MISS = {"Big heart", "Animal magnetism", "Brave but naive", "Hands to yourself"}
PTS = {1: 4, 2: 8, 3: 16, 4: 30, 5: 50, 6: 100}


def run(seed, ages=60, vampire=True):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "vampire": vampire, "only": "vampire-hunt-animal"}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True, capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


def title_case(s):
    return " ".join(w[:1].upper() + w[1:] for w in s.split(" "))


kinds = {}
for seed in range(1, 81):
    for e in run(seed)["events"]:
        if e.get("table") != "a16_animals-enc":
            continue
        row = ANIMALS[int(e["row"]) - 1]
        animal = row["Animal"].strip()
        danger = int(row["Danger Score"])
        verb = row["Attack String"].strip()
        cont = row["Continent"].lower()
        assert "north america" in cont or "all 7 continents" in cont, (seed, cont)
        assert e["title"] == title_case(animal), e
        paras = e["body"].split("\n\n")
        head = "A juicy-looking " + animal + " you discovered while hunting "
        assert paras[0].startswith(head) and paras[0].endswith(" is tempting you."), e
        assert paras[0][len(head):-len(" is tempting you.")] in LOCS, e
        assert paras[1] == "What will you do?", e
        follow = [p.split("\n", 1) for p in paras[2:]]
        log = "\n".join(e.get("logLines") or [])
        d = e["deltas"]
        assert not e.get("residue"), e
        ch = e["choice"]
        if ch == "Let it go":
            k = "let go"
            assert not follow and log.endswith(" " + animal + "."), e
            assert all(v == 0 for v in d.values()), e
        elif ch == "Feed on it":
            t, b = follow[0]
            if b.startswith("You drained "):
                k = "fed"
                assert t in TITLES["BLOOD_IS_BLOOD_1"] and b.endswith(" and discarded its carcass."), e
                assert log.endswith("\nI fed on it for some essence.") and d["happiness"] == 4, e
            elif b == "It fled the scene after you attacked.":
                k = "fled"
                assert t in TITLES["LATER"] and log.endswith("\nIt fled after I failed to attack it."), e
            else:
                k = "hit"
                assert t in TITLES["BLOOD_IS_BLOOD_2"] and b == "You tried to attack the " + animal + ", but missed!", e
                t2, b2 = follow[1]
                pre = "The " + animal + " charged you!"
                assert t2 == "Ouch" and b2.startswith(pre), e
                part = paras[-1][len("It " + verb + " your "):-1]
                assert paras[-1].startswith("It " + verb + " your ") and part in PARTS, (e, part)
                assert d["health"] == -(8 if danger >= 5 else 4) and d["happiness"] == -8, e
                assert log.endswith("\nI attacked and missed.\nIt " + verb + " my " + part + "."), e
        else:
            assert ch == "Attempt to rescue it", e
            if follow and follow[0][1] == "You successfully rescued the " + animal + "!":
                k = "rescued"
                assert follow[0][0] in WIN and d["karma"] == 3 and d["happiness"] == 16, e
            elif follow and follow[0][1] == "Your attempt to rescue the " + animal + " was unsuccessful.":
                k = "missed"
                assert follow[0][0] == "Are you freaking crazy" if danger > 3 else follow[0][0] in MISS, e
                assert log.endswith("I encountered " + log.split("I encountered ")[-1].split(" and ")[0] +
                                    " and tried unsuccessfully to rescue it."), e
            else:
                k = "attacked"
                assert log.endswith(" and tried to rescue it. The " + animal + " " + verb + " me!"), e
                assert d["health"] == -PTS[min(danger + 1, 6)], e
                if follow:
                    assert follow[-1] == ["Help!", "The " + animal + " " + verb + " you!"], e
        kinds[k] = kinds.get(k, 0) + 1

for k in ("let go", "fed", "fled", "hit", "rescued", "missed", "attacked"):
    assert kinds.get(k), (k, kinds)
assert not [e for e in run(3, vampire=False)["events"] if e.get("table") == "a16_animals-enc"]
print("kinds:", kinds)
print("ok")
