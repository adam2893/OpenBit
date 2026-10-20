"""The family model (REPORT.en.md 135): siblings carry the game's own state.

The game keeps a SimSibling per brother/sister with MovedOut (+0x1fd), Spouse
(+0x210) and AgeAtCurrentSpouseMarriage (+0x218). The port mirrors those three and
emits the two lines they drive, so a sibling is an actor with a history instead of
a row that only ages.
"""
import collections
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
MOVED = re.compile(r"^My (.+?),? (?:, )?moved out\.$|^My (.+) moved out\.$")
MARRIED = re.compile(r"^My (.+?) married (.+)\.$")


def run(seed, gender, ages=80):
    spec = {'seed': seed, 'country': 'United States', 'gender': gender, 'ages': ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), '--script', path], text=True,
                          capture_output=True, cwd=str(ROOT / 'port'))
    Path(path).unlink()
    assert proc.returncode == 0, (seed, gender, proc.stderr[-400:])
    return json.loads(proc.stdout)


counts = collections.Counter()
for seed in range(1, 61):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        lines = []
        for event in doc['events']:
            lines.extend(event.get('logLines') or [])
            for extra in event.get('extra') or []:
                lines.append(extra)
        siblings = [f for f in doc['final']['family']
                    if 'brother' in f['relation'] or 'sister' in f['relation']]
        counts['lives'] += 1
        counts['siblings'] += len(siblings)
        for sib in siblings:
            who = 'My ' + sib['relation'] + (', ' + sib['name'] + ',' if sib['name'] else '')
            if sib['movedOut']:
                counts['movedOut'] += 1
                assert who + ' moved out.' in lines, (seed, gender, sib, lines[-6:])
            else:
                assert who + ' moved out.' not in lines, (seed, gender, sib)
            if sib['spouse']:
                counts['married'] += 1
                # The marriage sentence now carries the spouse's age and occupation
                # and the name change (REPORT.en.md 270).
                assert (who + ' married ' + sib['spouse'] + '.' in lines or
                        any(l.startswith(who + ' married ' + sib['spouse'] + ',')
                            for l in lines)), (seed, gender, sib)
                # SimSibling.ThirdPersonMarries records the age it happened at.
                assert sib['ageAtMarriage'] > 0, (seed, gender, sib)
                assert sib['ageAtMarriage'] <= sib['age'], (seed, gender, sib)
                assert not sib['spouse'].startswith(sib['name'] or '\x00'), (seed, sib)
            else:
                assert sib['ageAtMarriage'] == 0, (seed, gender, sib)
        # A moved-out sibling is the one that marries, so the order holds.
        for sib in siblings:
            if sib['spouse']:
                assert sib['movedOut'], (seed, gender, sib)

assert counts['movedOut'] and counts['married'], dict(counts)
print('lives:', counts['lives'])
print('siblings:', counts['siblings'], 'moved out:', counts['movedOut'],
      'married:', counts['married'])
print('ok')