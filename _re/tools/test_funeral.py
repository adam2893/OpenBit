"""307: parent funeral planning, affordable dispositions, and ashes from table rows."""
import csv
import json
import re
import subprocess
import tempfile
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
with (ROOT / "gamedata_plain/a16_funeral-ash-locations-enc").open(newline="") as fh:
    PLACES = list(csv.DictReader(fh))
TOKEN = re.compile(r"<([^<>]*)>")


def place_matches(row, place, female, age):
    gender = row["Gender"].strip()
    if gender and gender != ("female" if female else "male"):
        return False
    if row["Min Age"] and age < int(row["Min Age"]):
        return False
    if row["Max Age"] and age > int(row["Max Age"]):
        return False
    text = row["Place"].strip().replace("<his/her>", "her" if female else "his")
    text = text.replace("<he/she>", "she" if female else "he")
    parts, last = [], 0
    for match in TOKEN.finditer(text):
        parts.extend((re.escape(text[last:match.start()]),
                      "(?:" + "|".join(map(re.escape, match[1].split("/"))) + ")"))
        last = match.end()
    parts.append(re.escape(text[last:]))
    return re.fullmatch("".join(parts), place) is not None


def run(seed, cash):
    spec = dict(seed=seed, country="United States", gender="male", ages=90, cash=cash)
    with tempfile.NamedTemporaryFile("w", suffix=".json") as fh:
        json.dump(spec, fh)
        fh.flush()
        proc = subprocess.run([str(PORT), "--script", fh.name], text=True,
                              capture_output=True, cwd=ROOT / "port", check=True)
    return json.loads(proc.stdout)["events"]


def check_events(seed, events, counts):
    for index, event in enumerate(events):
        title = event.get("title")
        lines = event.get("logLines") or []
        if title == "Death" and (event.get("choice") or "").startswith("Plan "):
            counts["plan choices"] += 1
            # The queued screen must be produced, not merely marked pending.
            if index + 1 < len(events):
                assert events[index + 1]["title"] == "Funeral Planning", (seed, event)
                assert events[index + 1]["age"] == event["age"], (seed, event)
        elif title == "Funeral Planning":
            match = re.fullmatch(
                r"It's time to decide what to do with your (father|mother)'s remains\.\n\n"
                r"(Her|His) wish was to be cremated\.\n\n"
                r"Burial: \$7,710\nCremation: \$990\nTaxidermy: \$45,360\nDonate to Science: \$0",
                event["body"])
            assert match, (seed, event)
            rel, cap = match.groups()
            female = rel == "mother"
            assert cap == ("Her" if female else "His"), (seed, event)
            poss, obj = ("her", "her") if female else ("his", "him")
            choices = {
                "Bury " + obj: "arranged for " + obj + " to be buried",
                "Cremate " + obj: "arranged for " + obj + " to be cremated",
                "Have " + obj + " taxidermied": "had " + obj + " taxidermied",
                "Donate " + poss + " body to science": "donated " + poss + " body to science",
            }
            choice = event["choice"]
            assert choice in choices, (seed, event)
            counts["planning"] += 1
            broke = [line for line in lines if line.startswith("You don't have enough money")]
            if broke:
                action = ("have your " + rel + " taxidermied" if choice.startswith("Have ")
                          else choice.split()[0].lower() + " your " + rel)
                assert broke == ["You don't have enough money to " + action +
                                 ".\n\nYou are broke."], (seed, event)
                assert not any(line.startswith("I planned ") for line in lines), (seed, event)
                counts["broke"] += 1
                continue
            expected = "I planned " + poss + " funeral and " + choices[choice] + "."
            assert expected in lines, (seed, expected, lines)
            counts[choice.split()[0]] += 1
            if choice.startswith("Cremate "):
                counts["cremations"] += 1
                if index + 1 < len(events):
                    follow = events[index + 1]
                    assert follow["title"] == "Ashes", (seed, event, follow)
                    assert follow["age"] == event["age"], (seed, event, follow)
                    assert follow["body"].startswith("You've received your " + rel + "'s ashes"), follow
        elif title == "Ashes":
            match = re.fullmatch(
                r"You've received your (father|mother)'s ashes back from the crematorium\.\n\n"
                r"What will you do\?", event["body"])
            assert match, (seed, event)
            rel = match[1]
            poss = "my " + rel + "'s"
            choice = event["choice"]
            if choice == "Sprinkle them somewhere special":
                prefix = "I sprinkled " + poss + " ashes "
                hit = [line for line in lines if line.startswith(prefix) and line.endswith(".")]
                assert len(hit) == 1, (seed, event)
                place = hit[0][len(prefix):-1]
                assert any(place_matches(row, place, rel == "mother", event["age"])
                           for row in PLACES), (seed, place, event)
                counts["sprinkled"] += 1
            elif choice == "Keep them in an urn":
                assert "I put " + poss + " ashes in an urn and stored it in a safe place." in lines, event
                counts["urn"] += 1
            else:
                assert choice == "Throw them in the trash", event
                assert "I threw " + poss + " ashes in the trash." in lines, event
                counts["trash"] += 1
            assert index and events[index - 1]["choice"].startswith("Cremate "), (seed, event)
            counts["ashes"] += 1


# Public family entry points must honor every displayed button, including index 2.
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "funeral.cpp"
    binary = Path(directory) / "funeral"
    source.write_text(r'''#include "bl/sim.h"
#include <cassert>
int main(int argc, char** argv) {
  bl::Sim sim(argv[1], 1);
  for (bool female : {false, true}) {
    bl::Life life;
    life.hero.pendingFuneral = true;
    life.hero.funeralFemale = female;
    life.hero.funeralRelation = female ? "mother" : "father";
    bl::AgeUp out;
    assert(sim.funeralYear(life, out, 2));
    assert(out.choice == out.options[2]);
    assert(life.hero.pendingFuneralPlan);
    assert(life.pendingCards.size() == 1);
    assert(life.pendingCards[0].kind == bl::kPendingFuneralPlan);
    bl::Life poor;
    poor.hero.pendingFuneralPlan = true;
    poor.hero.funeralFemale = female;
    poor.hero.funeralRelation = female ? "mother" : "father";
    assert(sim.funeralPlanningYear(poor, out, 2));
    const std::string rel = female ? "mother" : "father";
    assert(out.narrative == "You don't have enough money to have your " + rel +
                            " taxidermied.\n\nYou are broke.");
    assert(poor.hero.money == 0 && poor.pendingCards.empty());
    assert(!poor.hero.pendingFuneralPlan && !poor.hero.pendingFuneralAshes);
  }
}
''')
    subprocess.run(["c++", "-std=c++17", "-I" + str(ROOT / "port/include"), str(source),
                    str(PORT.parent / "libblport.a"), "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(ROOT)], check=True)

counts = Counter()
for cash in (50000, 0):
    for seed in range(1, 41):
        check_events(seed, run(seed, cash), counts)
assert counts["planning"] >= 10 and counts["plan choices"] >= counts["planning"], counts
assert counts["cremations"] >= 3 and counts["ashes"] == counts["cremations"], counts
assert all(counts[key] for key in ("broke", "Bury", "Have", "Donate", "sprinkled", "urn", "trash")), counts
print(dict(counts))
print("ok")
