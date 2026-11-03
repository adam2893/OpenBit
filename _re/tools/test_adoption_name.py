#!/usr/bin/env python3
"""The adoption naming card (REPORT.en.md 399).

An adoption now records the child in the family and asks, one age-up later, whether the
child takes the hero's surname. ScenarioNameChangeAdoption carries the title "Adoption",
two rows and two buttons, and the port draws the child's birth surname so the two names
differ.
"""
import json
import os
import re
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "port", "build", "blsim")
TABLE = "registry:ScenarioNameChangeAdoption"
ADOPTED = re.compile(r"^I adopted (?P<name>.+?), a (?P<age>\d+)-year old (?P<kind>girl|boy)\.$")
CARD = re.compile(
    r"^Now that you've adopted (?P<first>.+?), you're wondering if you should change "
    r"(?P<poss>his|her) last name to match yours\.\n\nWhat will you do\?\n\n"
    r"Current Name: (?P<current>.+?)\nPossible Name: (?P<possible>.+)$")


def run(spec):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    try:
        out = subprocess.run([BIN, "--script", path], capture_output=True, text=True)
    finally:
        os.unlink(path)
    if out.returncode != 0:
        raise SystemExit("blsim failed: " + out.stderr[-400:])
    return json.loads(out.stdout)


def main():
    adoptions = kept = changed = 0
    for seed in range(1, 101):
        data = run({"seed": seed, "country": "United States", "gender": "male",
                    "ages": 70, "cash": 100000})
        events = data["events"]
        for index, ev in enumerate(events):
            if (ev.get("table") or "") != TABLE:
                continue
            m = CARD.match(ev["body"])
            assert m, ev["body"]
            assert ev["title"] == "Adoption", ev["title"]
            first, poss = m.group("first"), m.group("poss")
            current, possible = m.group("current"), m.group("possible")
            assert current.startswith(first + " "), current
            assert possible.startswith(first + " "), possible
            assert current != possible, (seed, current)
            # The adoption itself was journaled a few events earlier with the same name.
            earlier = []
            for e in events[:index]:
                for line in e.get("logLines") or []:
                    if ADOPTED.match(line):
                        earlier.append(line)
            assert earlier, (seed, "naming card without an adoption")
            adopted = ADOPTED.match(earlier[-1])
            assert adopted.group("name") == current, (adopted.group("name"), current)
            assert adopted.group("kind") == ("girl" if poss == "her" else "boy"), adopted.group(0)
            adoptions += 1
            choice = ev["choice"] or ""
            if choice == "Keep " + first + "'s current name":
                kept += 1
            elif choice == "Change it to " + possible:
                changed += 1
            else:
                raise AssertionError(choice)
            assert (ev.get("logLines") or []), ev
    assert adoptions >= 10, adoptions
    assert kept and changed, (kept, changed)
    # A life that never adopts never sees the card.
    for seed in range(1, 21):
        data = run({"seed": seed, "country": "United States", "gender": "female",
                    "ages": 20, "cash": 100000})
        assert not [e for e in data["events"] if (e.get("table") or "") == TABLE], seed
    print("adoptions: {} | kept: {} | changed: {}".format(adoptions, kept, changed))
    print("ok")


if __name__ == "__main__":
    main()