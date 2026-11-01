
#!/usr/bin/env python3
"""The HR investigation card (REPORT.en.md 339).

ScenarioWorkplaceDisciplineHR has no choices: the workplace cards push it with a
reason string and a seriousness, and the card decides between a reprimand and a
termination while it is built. The port opens it from the two workplace lanes it
already models -- the unprofessional-supervisor attack lane (seriousness 5) and
the supervisor seduction's success lane (seriousness 4).
"""
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORT = ROOT / "port" / "build" / "blsim"

MIL_TITLES = {"Must tread lightly", "On thin ice", "Being watched", "Under the microscope"}
CIV_TITLES = {"Really? HR?", "Who's the narc", "A whole investigation?",
              "It's not that serious", "These people are so touchy"}
# The card rewrites the caller's '<your/my>' token: 'your' in the body, 'my' in
# the journal (String::Replace at 0x299bc50 and 0x299bdd0).
BODY_REASONS = r"(?:attacking your supervisor|trying to seduce your supervisor)"
LOG_REASONS = r"(?:attacking my supervisor|trying to seduce my supervisor)"
MIL_BODY = re.compile(r"^Your chain of command has formally reprimanded you for "
                      + BODY_REASONS + r"!$")
CIV_BODY = re.compile(r"^HR has launched an investigation into you for "
                      + BODY_REASONS + r"!$")
MIL_LOG = re.compile(r"^I was reprimanded by my chain of command for "
                     + LOG_REASONS + r"!")
CIV_LOG = re.compile(r"^HR investigated me for " + LOG_REASONS + r"!")
FIRED = "You have been let go from your position as "


def log_ok(pattern, log):
    """The first sentence is the card's own; a termination appends its own line."""
    m = pattern.match(log)
    if not m:
        return False
    rest = log[m.end():]
    return rest == "" or re.match(r"^ I was let go from my job as .+\.$", rest) is not None
DISCHARGED = "You have been dishonorably discharged from the "


def run(seed, ages=70, cash=60000, military=False):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": cash, "military": military}
    fd, path = tempfile.mkstemp(suffix=".json")
    os.write(fd, json.dumps(spec).encode())
    os.close(fd)
    try:
        out = subprocess.run([str(PORT), "--script", path], cwd=str(ROOT / "port"),
                             capture_output=True, text=True)
    finally:
        os.unlink(path)
    assert out.returncode == 0, out.stderr[-800:]
    return json.loads(out.stdout[out.stdout.find("{"):])


def main():
    cards = 0
    reprimands = 0
    firings = 0
    mil = 0
    titles = set()
    reasons = set()
    for seed in range(1, 41):
        data = run(seed)
        producer_age = None
        for e in data["events"]:
            choice = e.get("choice") or ""
            narrative = " ".join(e.get("logLines") or [])
            if choice.startswith("Attack ") or "I seduced my supervisor" in narrative:
                producer_age = e["age"]
                continue
            if e.get("table") != "registry:ScenarioWorkplaceDisciplineHR":
                continue
            cards += 1
            titles.add(e["title"])
            body = e["body"]
            logs = e.get("logLines") or []
            assert e.get("choice") is None, (seed, e["age"], e.get("choice"))
            assert producer_age is not None, ("HR card nobody opened", seed, e["age"])
            assert 0 <= e["age"] - producer_age <= 1, (
                "HR card not attached to its producer", seed, e["age"], producer_age)
            first = body.split("\n\n")[0]
            log = logs[0] if logs else ""
            if first.startswith("Your chain of command"):
                mil += 1
                assert e["title"] in MIL_TITLES, (seed, e["title"])
                m = MIL_BODY.match(first)
                assert m, (seed, first)
                assert log_ok(MIL_LOG, log), (seed, log)
            else:
                assert e["title"] in CIV_TITLES, (seed, e["title"])
                m = CIV_BODY.match(first)
                assert m, (seed, first)
                assert log_ok(CIV_LOG, log), (seed, log)
            reasons.add(m.group(0))
            if FIRED in body or DISCHARGED in body:
                firings += 1
                assert e["deltas"]["happiness"] == -6, (seed, e["deltas"])
                if FIRED in body:
                    assert body.index(FIRED) > body.index(first), seed
                    assert re.search(r"for disciplinary issues\.$", body), (seed, body)
                else:
                    assert e["deltas"]["happiness"] == -6
            else:
                reprimands += 1
                assert e["deltas"]["happiness"] == 0, (seed, e["deltas"])
                assert e["deltas"]["discipline"] == 0, (seed, e["deltas"])
    assert cards >= 3, cards
    assert firings >= 1, ("no termination lane reached", cards)
    assert reprimands >= 1, ("no reprimand lane reached", cards)
    # The military arm takes its own body and its own four titles.
    mil_cards = 0
    for seed in range(1, 31):
        for e in run(seed, cash=600000, military=True)["events"]:
            if e.get("table") != "registry:ScenarioWorkplaceDisciplineHR":
                continue
            if e["title"] in MIL_TITLES:
                mil_cards += 1
                first = e["body"].split("\n\n")[0]
                assert MIL_BODY.match(first), (seed, first)
                assert log_ok(MIL_LOG, (e.get("logLines") or [""])[0]), (seed, e.get("logLines"))
    assert mil_cards >= 1, "the military reprimand never landed"
    # A minor never reaches the workplace cards at all, so no investigation either.
    kid = run(3, ages=17)
    assert not [e for e in kid["events"]
                if e.get("table") == "registry:ScenarioWorkplaceDisciplineHR"], "minor"
    print("cards:", cards, "| reprimands:", reprimands, "| firings:", firings,
          "| military:", mil, "| military sweep:", mil_cards)
    print("titles seen:", sorted(titles))
    print("reasons seen:", sorted(reasons))
    print("ok")


if __name__ == "__main__":
    main()
