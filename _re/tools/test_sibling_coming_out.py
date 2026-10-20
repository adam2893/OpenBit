"""274: a sibling's coming out -- the confession card and the announcement."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
PROMPT = re.compile(r"^Your (brother|sister|half brother|half sister|big brother|big sister|little brother|little sister), ([A-Za-z-]+), has confessed to you that (she|he) is considering coming out of the closet\.\n\nWhat will you do\?$")
IGNORE = re.compile(r"^My (.+?), ([A-Za-z-]+), confessed to me that (she|he) is thinking about coming out of the closet\. I ignored (her|him)\.$")
RIDICULE = re.compile(r"^I ridiculed my (.+?), ([A-Za-z-]+), for thinking about coming out of the closet\.$")
OUT = re.compile(r"^My (.+?), ([A-Za-z-]+), moved out\. (She|He) came out of the closet as (gay|bi)\.$")


def run(seed, ages=60):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
out_lines = 0
for seed in range(1, 41):
    doc = run(seed)
    considered = set()
    for event in doc["events"]:
        if event.get("title") == "Coming Out":
            cards += 1
            m = PROMPT.match(event.get("body") or "")
            assert m, (seed, event.get("body"))
            relation, name, pron = m.groups()
            assert relation in event["body"], (seed, event["body"])
            considered.add((relation, name))
            choice = event.get("choice") or ""
            journal = (event.get("logLines") or [""])[-1]
            if choice.startswith("Ignore "):
                mi = IGNORE.match(journal)
                assert mi, (seed, journal)
                assert mi.group(1) == relation and mi.group(2) == name, (seed, journal)
                assert mi.group(3) == pron, (seed, journal)
                assert mi.group(4) == ("her" if pron == "she" else "him"), (seed, journal)
            elif choice.startswith("Ridicule "):
                mr = RIDICULE.match(journal)
                assert mr, (seed, journal)
                assert mr.group(1) == relation and mr.group(2) == name, (seed, journal)
            else:
                assert choice.startswith("Support "), (seed, choice)
                assert name in journal, (seed, journal)
        for line in (event.get("logLines") or []):
            m = OUT.match(line)
            if m:
                out_lines += 1
                relation, name, pron, sex = m.groups()
                assert (relation, name) in considered, (seed, "an announcement with no confession", line)
                assert (pron == "She") == ("sister" in relation), (seed, line)
                assert sex in ("gay", "bi"), (seed, line)

assert cards >= 5, cards
assert out_lines >= 3, out_lines
print("confessions:", cards, "announcements:", out_lines)
print("ok")
