#!/usr/bin/env python3
"""REPORT.en.md 362: the artist-in-residence contract renewal card."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / 'port' / 'build' / 'blsim'

BODY = re.compile(
    r"^Your casino's artist-in-residence, (?P<name>.+?)'s contract has ended\.\n\n"
    r"(?:His|Her) agent tells you (?:he|she)'ll renew (?:his|her) contract for the "
    r"right price\.$")
CONTRACT = re.compile(
    r"^Decide the terms of your new contract with (?P<name>.+?)\n\n"
    r"Current Salary: \$(?P<salary>[\d,]+)\nLength: (?P<length>2|3|5|7|10) Years\n"
    r"Salary: \$(?P<offer>[\d,]+)$")

DEAL = re.compile(
    r"^(?P<name>.+?) agreed to the terms of (?:his|her) new contract and will continue "
    r"performing at your casino\.\n\nResidency Duration: (?:2|3|5|7|10) Years\n"
    r"New Salary: \$[\d,]+$")
CRAZY = re.compile(
    r"^(?P<name>.+?) refused your new contract offer and will not perform at your "
    r"casino anymore\.$")
LATER = re.compile(
    r"^(?P<name>.+?) ended (?:his|her) residency at your casino\.\n\n"
    r"Years Booked: \d+$")
EXPIRED = re.compile(
    r"^My casino's artist-in-residence, (?P<name>.+?)'s, contract expired and I "
    r"decided not to renew it\.$")
STAYED = re.compile(
    r"^My casino's artist-in-residence, (?P<name>.+?)'s, contract came up for renewal "
    r"and (?:he|she) decided to stay after I offered (?:him|her) a salary of \$[\d,]+\.$")
REFUSED = re.compile(
    r"^My casino's artist-in-residence, (?P<name>.+?)'s, contract came up for renewal "
    r"and (?:he|she) refused the terms of the new contract\.$")


def run(seed, casino=True, ages=45):
    spec = {
        'seed': seed,
        'country': 'United States',
        'gender': 'male',
        'ages': ages,
        'cash': 2000000,
        'casino': casino,
        'casino_guest': 'same',
        'only': 'casino-celeb-renew',
    }
    with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    try:
        done = subprocess.run([str(PORT), '--script', path], capture_output=True,
                              text=True, cwd=str(ROOT / 'port'))
    finally:
        Path(path).unlink()
    assert done.returncode == 0, done.stderr
    return json.loads(done.stdout)


lanes = {}
results = {}
seeds = 0
for seed in range(1, 21):
    doc = run(seed)
    cards = [e for e in doc['events']
             if (e.get('table') or '') == 'registry:ScenarioCasinoAgeUpCelebrityContractRenewal']
    if not cards:
        continue
    seeds += 1
    for card in cards:
        m = BODY.match(card.get('body') or '')
        assert m, (seed, card.get('body'))
        choice = card.get('choice') or ''
        lane = 'offer' if choice.endswith('an offer') else 'go' if choice.endswith('go') else None
        assert lane, (seed, choice)
        lanes[lane] = lanes.get(lane, 0) + 1
        # The answer's own text arrives on a follow-up card, the way the native chains it.
        follow = [e for e in doc['events'] if e['age'] in (card['age'], card['age'] + 1)]
        titles = {e.get('title') for e in follow}
        if lane == 'go':
            assert 'Laterrrr' in titles, (seed, titles)
            tail = [x for x in card.get('logLines') or [] if EXPIRED.match(x)]
            assert tail, (seed, card.get('logLines'))
            later = next(e for e in follow if e.get('title') == 'Laterrrr')
            lm = LATER.match(later.get('body') or '')
            assert lm, (seed, later.get('body'))
            assert lm.group('name') == m.group('name'), (seed, lm.group('name'))
            results['go'] = results.get('go', 0) + 1
        else:
            # The offer answer chains the contract card (REPORT.en.md 371), which
            # carries the terms and the decision; the result card follows it.
            offer = next((e for e in follow if e.get('title') == 'Contract'), None)
            assert offer, (seed, 'offer answer opened no contract card', titles)
            om = CONTRACT.match(offer.get('body') or '')
            assert om, (seed, offer.get('body'))
            assert om.group('name') == m.group('name'), (seed, om.group('name'))
            assert (offer.get('choice') or '').endswith('this offer'), (seed, offer.get('choice'))
            assert (offer.get('choice') or '').startswith('Make '), (seed, offer.get('choice'))
            both = {'deal': DEAL, 'crazy': CRAZY}
            hit = [(k, rx.match(e.get('body') or '')) for e in follow
                   for k, rx in both.items() if e.get('title') == (
                       "You've got a deal" if k == 'deal' else "You're crazy")]
            assert hit, (seed, titles)
            kind, match = hit[0]
            assert match, (seed, kind, [e.get('body') for e in follow])
            assert match.group('name') == m.group('name'), (seed, match.group('name'))
            want = STAYED if kind == 'deal' else REFUSED
            tail = [x for x in offer.get('logLines') or [] if want.match(x)]
            assert tail, (seed, kind, offer.get('logLines'))
            results[kind] = results.get(kind, 0) + 1
assert seeds > 0, 'the renewal card never fired'
assert len(lanes) == 2, lanes
assert results.get('go') and (results.get('deal') or results.get('crazy')), results

# No casino, no card.
assert not [e for e in run(3, casino=False)['events']
            if 'ContractRenewal' in (e.get('table') or '')]

print(f"seeds: {seeds} | lanes: {lanes} | results: {results}")
print('ok')