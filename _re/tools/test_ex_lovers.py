"""The ex-lover array (REPORT.en.md 134).

The game keeps exes as a SimLover array (Life.ExLoverArray) and every ExLover* card
is constructed with one entry. The port used to keep only the most recent ex as two
fields; the test pins the array against the only other evidence of a break-up the
port emits -- the See ya announcement -- so the two cannot drift apart.
"""
import collections
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
STATUSES = {"dating", "engaged", "married"}


def run(seed, gender):
    spec = {'seed': seed, 'country': 'United States', 'gender': gender, 'ages': 95,
            'relationship': True}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), '--script', path], text=True,
                          capture_output=True, cwd=str(ROOT / 'port'))
    Path(path).unlink()
    assert proc.returncode == 0, (seed, gender, proc.stderr[-400:])
    return json.loads(proc.stdout)


lives = 0
with_ex = 0
entries = 0
statuses = collections.Counter()
for seed in range(1, 61):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        lives += 1
        exes = doc['final']['exLovers']
        cards = [e for e in doc['events'] if e.get('title') == 'See ya']
        # One entry per break-up, and the break-up card is the only place a
        # relationship ever ends in the port. A run can end between the break-up
        # and its announcement -- the announcement rides the same year's queue
        # (REPORT.en.md 133) and the loop stops at `ages` -- so one trailing entry
        # without a card is legal, and it has to be the last year.
        gap = len(exes) - len(cards)
        assert gap in (0, 1), (seed, gender, len(exes), len(cards))
        if gap == 1:
            assert exes[-1]['year'] == doc['final']['age'], (seed, gender, exes[-1], doc['final'])
        if exes:
            with_ex += 1
        years = [c['age'] for c in cards]
        for ex in exes:
            entries += 1
            assert ex['name'], (seed, gender, ex)
            assert ex['status'] in STATUSES, (seed, gender, ex)
            assert 0 <= ex['relationship'] <= 100, (seed, gender, ex)
            if ex is exes[-1] and gap == 1:
                continue  # its announcement never drained, asserted above
            assert ex['year'] in years, (seed, gender, ex, years)
            statuses[ex['status']] += 1

assert entries, 'no ex-lover was recorded in 120 lives'
print('lives with an ex:', with_ex, 'of', lives)
print('ex entries:', entries)
print('statuses:', dict(sorted(statuses.items())))
print('ok')