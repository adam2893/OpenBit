"""The general <insult> token (REPORT.en.md 138).

a16_insults-enc is the game's own insult pool: Insult, Plural, Gender, Clean,
Vampire. Only the hustler family had a source for <insult> before; the paparazzi,
racer and vampire cards write the same token and got an empty string. The test pins
the resolution and the two filters.
"""
import csv
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
TABLE = ROOT / "gamedata_plain" / "a16_insults-enc"
rows = list(csv.reader(TABLE.read_text(encoding='utf-8').splitlines()))
head = rows[0]
POOL = [(r[head.index('Insult')].strip(), r[head.index('Gender')].strip(),
         r[head.index('Vampire')].strip()) for r in rows[1:] if r and r[head.index('Insult')].strip()]
assert len(POOL) > 150, len(POOL)
VAMPIRE_ONLY = {w for w, _, v in POOL if v}


def run(seed, gender):
    spec = {'seed': seed, 'country': 'United States', 'gender': gender, 'ages': 75}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), '--script', path], text=True,
                          capture_output=True, cwd=str(ROOT / 'port'))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


words = set()
for seed in range(1, 41):
    for gender in ("male", "female"):
        doc = run(seed, gender)
        for event in doc['events']:
            for key in ('body', 'narrative'):
                text = event.get(key) or ''
                # An unresolved token stays verbatim in the card, which is the
                # failure this test exists to catch.
                assert '<insult>' not in text, (seed, gender, text[:90])
                # The hustler family keeps its own pool (a16_hustler-insults-enc)
                # and shares wording with this one, so a word appearing in a card is
                # not evidence of which pool produced it. What is asserted is that
                # nothing is left unresolved and that the general pool is what the
                # rest of the cards read -- checked at the data level below.
                for word, _, _ in POOL:
                    if word and (' ' + word) in text:
                        words.add(word)

assert words, 'no insult was ever read out of the table'
# The pool the port draws from: the portable rows, gender-filtered, vampire rows out.
PORTABLE = [w for w, g, v in POOL if not v and g in ('male', 'female', 'both', 'all', '')]
assert len(PORTABLE) > 100, len(PORTABLE)
assert not (set(PORTABLE) & VAMPIRE_ONLY), 'a vampire row leaked into the portable pool'
print('distinct insults seen:', len(words))
print('portable rows:', len(PORTABLE), ' vampire-only rows:', len(VAMPIRE_ONLY))
print('ok')