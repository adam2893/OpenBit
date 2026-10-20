"""249: a parent's death, its cause from the table, and the funeral the same year.

The played-life trace: "My father died of complications from old age. / I couldn't be
bothered to plan or even attend his funeral." at 54, "My mother died of natural
causes. / I let someone else plan her funeral but I found time to attend it." at 56.
"""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

DEATH = re.compile(r"^My (father|mother) (.+)$")
FUNERAL = re.compile(r"^(I couldn't be bothered to attend (his|her) funeral\.|"
                     r"I let someone else plan (his|her) funeral but I found time to attend it\.)$")

# The table's own sentences, with the leading subject token removed. A row can carry
# another token ("<body part>"), which the port resolves at render time, so the
# comparison is a pattern with those tokens as wildcards.
sentence_patterns = []
with open(ROOT / "gamedata_plain" / "a16_death-types-enc", newline="") as fh:
    for row in csv.DictReader(fh):
        s = (row.get("Sentence") or "").strip()
        if not s:
            continue
        close = s.find(">")
        if s.startswith("<pronoun>") and close != -1:
            s = s[close + 1:].lstrip()
        if not s:
            continue
        parts = re.split(r"<[^>]+>", s)
        sentence_patterns.append(re.compile(".+?".join(re.escape(p) for p in parts) + r"\Z"))


def is_death_sentence(text):
    return any(p.fullmatch(text) for p in sentence_patterns)


def run(seed, ages=80):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


deaths = 0
funerals = 0
for seed in range(1, 41):
    for event in run(seed)["events"]:
        lines = event.get("logLines") or []
        # A death line is a parent"s name followed by one of the table"s own
        # sentences; the relation plus an unrelated sentence (a diagnosis, a
        # promotion) is not one.
        death_lines = [l for l in lines
                       if (m := DEATH.match(l)) and is_death_sentence(m.group(2))]
        funeral_lines = [l for l in lines if FUNERAL.match(l)]
        if death_lines:
            deaths += 1
            cause = DEATH.match(death_lines[0]).group(2)
            assert is_death_sentence(cause), (seed, death_lines[0], cause)
        if funeral_lines:
            funerals += 1
            # The funeral card is the same year as the death line, which is what
            # the trace shows: both sentences belong to one event's log.
            assert death_lines, (seed, "funeral without a death in the same event", event)
            assert event["choice"] in ("I couldn't be bothered", "Let someone else plan it"), event

assert deaths >= 30, deaths
assert funerals >= 20, funerals
print("deaths:", deaths, "funerals:", funerals)
print("ok")
