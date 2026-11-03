#!/usr/bin/env python3
"""The legal name change card and its outcome (REPORT.en.md 398).

ScenarioNameChangePull asks the hero to file a request with the government, charges
the fee, and answers a year later with an approval or a rejection. Both the filing
and the outcome carry the "Name Change" title and the same registry key, so the drive
below separates them by body shape.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "port", "build", "blsim")
TABLE = "registry:ScenarioNameChangePull"
FILED = re.compile(
    r"^File a request to legally change your name today!\n\n"
    r"First Name: (?P<first>.+?)\n"
    r"Last Name: (?P<last>.+?)\n"
    r"Cost: \$(?P<cost>[\d,]+)$")
APPROVED = re.compile(
    r"^Your request to change your name has been approved by the (?P<country>.+?) government\.\n\n"
    r"You are now (?P<name>.+)\.$")
REJECTED = re.compile(
    r"^Your request to change your name to (?P<name>.+?) was rejected by the (?P<country>.+?) government\.$")


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
    filed = wins = losses = 0
    for seed in range(1, 41):
        data = run({"seed": seed, "country": "United States", "gender": "female",
                    "ages": 80, "cash": 200000, "only": "legal-name"})
        events = [e for e in data["events"] if (e.get("table") or "") == TABLE]
        for index, ev in enumerate(events):
            assert ev["title"] == "Name Change", ev["title"]
            body = ev["body"]
            m = FILED.match(body)
            if m:
                filed += 1
                assert m.group("cost") == "200", body
                assert ev["choice"] == "File a request", ev["choice"]
                assert (ev.get("logLines") or [])[-1] == (
                    "I filed a request to legally change my name to "
                    + m.group("first") + " " + m.group("last") + "."), ev.get("logLines")
                # The outcome follows on a later age-up (other pending cards can
                # come first) and names the same person; a life that ends right after
                # the filing never gets one.
                target = m.group("first") + " " + m.group("last")
                nxt = None
                for later in events[index + 1:]:
                    am = APPROVED.match(later["body"])
                    rm = REJECTED.match(later["body"])
                    if (am or rm) and (am or rm).group("name") == target:
                        nxt = later
                        break
                if nxt is None:
                    assert index + 1 >= len(events), (seed, ev["age"], "no outcome")
                    continue
                am = APPROVED.match(nxt["body"])
                rm = REJECTED.match(nxt["body"])
                assert (am or rm).group("name") == target, (nxt["body"], target)
                assert nxt["choice"] == "OK", nxt["choice"]
                if am:
                    wins += 1
                    assert am.group("country") == "United States", am.group("country")
                else:
                    losses += 1
                    assert rm.group("country") == "United States", rm.group("country")
    assert filed >= 20, filed
    assert wins and losses, (wins, losses)
    # A life that never reaches eighteen never files.
    for seed in range(1, 11):
        data = run({"seed": seed, "country": "United States", "gender": "male",
                    "ages": 17, "cash": 200000, "only": "legal-name"})
        assert not [e for e in data["events"] if (e.get("table") or "") == TABLE], seed
    print("filings: {} | approved: {} | rejected: {}".format(filed, wins, losses))
    print("ok")


if __name__ == "__main__":
    main()