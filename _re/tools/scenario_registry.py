#!/usr/bin/env python3
"""Scenario registry extractor for the Phase 1 differential bench.

Reads prod2 dumps captured from the frida harness (/tmp/blh.js 'prod2' RPC) and
emits obs_corpus/scenario_registry.json:

  candidates: every Scenario* class name Life.ValidateAndSetScenario accepted,
              with the second argument (per-life occurrence limit) and the ages
              at which it was offered.
  picked:     classes returned by Life.GetRandomPushScenario (the scenario that
              was actually pushed to the player, per age).
  offered:    every name passed to ValidateAndSetScenario, accepted or not.

Usage:
  python3 tools/scenario_registry.py /tmp/prod2_run1.json /tmp/prod2_run2.json
  python3 tools/scenario_registry.py            # defaults to /tmp/prod2_*.json
"""
import glob
import json
import os
import sys

RE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(RE, 'obs_corpus', 'scenario_registry.json')


def load(paths):
    rows = []
    for p in paths:
        try:
            d = json.load(open(p))
        except Exception as e:
            print('skip %s: %s' % (p, e))
            continue
        if isinstance(d, list):
            rows += d
    return rows


def main(argv):
    paths = argv[1:] or sorted(glob.glob('/tmp/prod2_*.json'))
    rows = load(paths)
    if not rows:
        print('no prod2 rows found')
        return 1

    cand, offered, picked = {}, set(), []
    for r in rows:
        m = r.get('m')
        if m == 'ValidateAndSetScenario':
            a = r.get('args') or []
            if not a:
                continue
            name = a[0]
            offered.add(name)
            limit = a[1] if len(a) > 1 else None
            # Boolean return: 1 accepted, 0 rejected, anything else unknown
            # (an unmasked register read is not evidence).
            n = r.get('n')
            ok = True if n == 1 else (False if n == 0 else None)
            ent = cand.setdefault(name, {'name': name, 'limit': limit,
                                         'ages': [], 'accepted': None})
            if r.get('age') is not None and r['age'] not in ent['ages']:
                ent['ages'].append(r['age'])
            if ok is not None:
                ent['accepted'] = ok
        elif m == 'GetRandomPushScenario' and r.get('cls'):
            picked.append({'age': r.get('age'), 'cls': r['cls']})

    vals = list(cand.values())
    accepted = sorted([v for v in vals if v['accepted'] is True], key=lambda x: x['name'])
    rejected = sorted([v for v in vals if v['accepted'] is False], key=lambda x: x['name'])
    unknown = sorted([v for v in vals if v['accepted'] is None], key=lambda x: x['name'])
    out = {
        'source_files': paths,
        'rows': len(rows),
        'counts': {'offered': len(cand), 'accepted': len(accepted),
                   'rejected': len(rejected), 'unknown': len(unknown),
                   'picked': len(picked)},
        'candidates': accepted,
        'rejected': rejected,
        'unknown_acceptance': unknown,
        'offered_only': sorted(offered - set(cand)),
        'picked': picked,
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(out, open(OUT, 'w'), indent=1)
    print('rows %d  offered %d  accepted %d  rejected %d  unknown %d  picked %d' % (
        len(rows), len(offered), len(accepted), len(rejected), len(unknown), len(picked)))
    print('written %s' % OUT)
    for p in picked:
        print('  picked age=%s %s' % (p['age'], p['cls']))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
