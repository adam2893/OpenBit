"""Per-country retirement (REPORT.en.md 137).

DataCountry keeps RetirementAgeMale/Female and the port now reads them, so a hero
stops starting careers at his own country's age instead of a single invented 66.
The test pins the data path (the age in the script JSON equals the country row) and
the behaviour (no new career or promotion on or after that age).
"""
import collections
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
COUNTRIES = json.loads((ROOT / "gamedata_json" / "a16_countries-enc.json").read_text())
RETIRE = {c['<Name>k__BackingField']: (c['<RetirementAgeMale>k__BackingField'],
                                          c['<RetirementAgeFemale>k__BackingField'])
          for c in COUNTRIES}
START = re.compile(r'^I started a new position as ')
PROMO = re.compile(r'^I have been promoted to ')


def run(seed, country, gender, ages):
    spec = {'seed': seed, 'country': country, 'gender': gender, 'ages': ages}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), '--script', path], text=True,
                          capture_output=True, cwd=str(ROOT / 'port'))
    Path(path).unlink()
    assert proc.returncode == 0, (seed, country, proc.stderr[-300:])
    return json.loads(proc.stdout)


# Four countries with four different pairs, so a fixed 66 cannot pass.
CASES = [("Afghanistan", "male", 60), ("Afghanistan", "female", 55),
         ("Norway", "male", 67), ("Brazil", "female", 60), ("Japan", "male", 62)]
seen = collections.Counter()
for country, gender, want in CASES:
    assert RETIRE[country][0 if gender == 'male' else 1] == want, (country, gender)
    for seed in range(1, 16):
        doc = run(seed, country, gender, want + 12)
        final = doc['final']
        assert final['retirementAge'] == want, (country, gender, final['retirementAge'])
        if final['age'] >= want:
            assert final['employed'] is False, (country, gender, seed, final)
            seen[(country, gender)] += 1
        for event in doc['events']:
            for line in (event.get('logLines') or []):
                if START.match(line) or PROMO.match(line):
                    assert event['age'] < want, (country, gender, seed, event['age'], line)

missing = [c for c in CASES if seen[(c[0], c[1])] == 0]
assert not missing, ('no life reached retirement', missing)
for (country, gender), n in sorted(seen.items()):
    print('%-12s %-6s retired in %d lives' % (country, gender, n))
print('ok')