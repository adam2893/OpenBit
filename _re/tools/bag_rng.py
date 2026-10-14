#!/usr/bin/env python3
"""Turn a device capture into bag vectors for blsim --bagcheck.

For every push-scenario pick the tool rebuilds the bag exactly as the game did
(accepted ValidateAndSetScenario calls sharing one ArrayList, in call order) and
then looks for the draw that selected the winner: a UnityEngine.Random or
System.Random integer taken between the last bag append and the pick, smaller
than the bag slot count, whose slot holds the type the game constructed.

Writes tools/bag_vectors.json.  Run blsim --bagcheck afterwards for the C++ side
verdict.
"""
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import blhrpc  # noqa: E402

RE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(RE, 'tools', 'bag_vectors.json')


def bags(rows):
    """bag pointer -> accepted (name, weight, t) appends, in call order."""
    g = collections.OrderedDict()
    for x in rows:
        if x.get('m') != 'ValidateAndSetScenario' or x.get('n') != 1:
            continue
        bp = x.get('bp')
        if not bp:
            continue
        name = x.get('args', [None])[0]
        weight = x.get('args', [None, 0])[1]
        if not name or not isinstance(weight, int) or weight <= 0:
            continue
        g.setdefault(bp, []).append((name, weight, x['t']))
    return g


def collect(prod_n=800, rng_n=6000, uer_n=4000):
    rows = blhrpc.rpc('prod2', n=prod_n)
    rng = blhrpc.rpc('rand', n=rng_n).get('rows', [])
    uer = blhrpc.rpc('uer', n=uer_n).get('rows', [])
    g = bags(rows)
    picks = [x for x in rows if x.get('m') == 'GetRandomPushScenario']
    cases, report = [], []
    for p in picks:
        t = p['t']
        cand = [v for v in g.values() if v[-1][2] < t]
        if not cand:
            continue
        bag = max(cand, key=lambda v: v[-1][2])
        slots = sum(w for _, w, _ in bag)
        name_index = {}
        for i, (n, w, _) in enumerate(bag):
            name_index[n] = (name_index.get(n, 0), w)
        t0 = bag[-1][2]
        draws = []
        for d in uer:
            if d.get('t') and t0 <= d['t'] <= t and isinstance(d.get('ri'), int) and 0 <= d['ri'] < slots:
                draws.append(('unity:' + str(d.get('tag')), d['ri'], d['t']))
        for d in rng:
            if d.get('t') and t0 <= d['t'] <= t and isinstance(d.get('v'), int) and 0 <= d['v'] < slots:
                draws.append(('dotnet:' + str(d.get('m')), d['v'], d['t']))
        expect = p.get('cls')
        hit = [d for d in draws if slot_name(bag, d[1]) == expect]
        case = {'age': p.get('age'), 'bag': [[n, w] for n, w, _ in bag], 'expect': expect,
                'source': 'capture'}
        if hit:
            case['index'] = hit[-1][1]
            case['rngValue'] = hit[-1][1]
        cases.append(case)
        report.append({'age': p.get('age'), 'expect': expect, 'entries': len(bag), 'slots': slots,
                       'draws': len(draws), 'winner': hit[-1][:2] if hit else None,
                       'sample': draws[:3]})
    return cases, report


def slot_name(bag, i):
    k = 0
    for n, w, _ in bag:
        if k + w > i:
            return n
        k += w
    return None


def main():
    cases, report = collect()
    for r in report:
        print('age', r['age'], 'expect', r['expect'], 'entries', r['entries'], 'slots', r['slots'],
              'draws', r['draws'], 'winner', r['winner'], 'sample', r['sample'])
    json.dump({'cases': cases}, open(OUT, 'w'), indent=1)
    print('cases', len(cases), 'written', OUT)


if __name__ == '__main__':
    main()
