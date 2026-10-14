#!/usr/bin/env python3
"""Map scenario type names to their likely content table.

Token match between a Scenario* class name and the kebab-case names of the
decrypted data tables.  This is a worklist for the port, not proof: a class whose
name shares tokens with a table name still needs its content confirmed.
"""
import json, os, re, sys, collections

RE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REG = os.path.join(RE, 'obs_corpus', 'scenario_registry.json')
OUT = os.path.join(RE, 'obs_corpus', 'scen_source.json')

WORDS = {
    'a16', 'enc', 'scenarios', 'scenario', 'scen', 'push', 'dumb', 'universal',
    'and', 'the', 'of', 'for', 'to', 'in', 'on', 'with', 'your', 'you',
}
STOP = WORDS


def tokens(name):
    s = re.sub(r'^Scenario', '', name)
    s = re.sub(r'(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])', '-', s)
    parts = [p.lower() for p in re.split(r'[-_]+', s) if p]
    return [p for p in parts if p and p not in STOP]


def main():
    data = json.load(open(REG))
    names = sorted(set([c['name'] for c in data.get('candidates', [])] +
                       [c['name'] for c in data.get('rejected', [])]))
    tables = []
    for d in ('gamedata_plain', 'gamedata'):
        p = os.path.join(RE, d)
        if os.path.isdir(p):
            for fn in os.listdir(p):
                q = os.path.join(p, fn)
                if not os.path.isfile(q) or fn.startswith('.'):
                    continue
                # plain tables carry no extension, json tables use .txt
                tables.append(fn[:-4] if fn.endswith('.txt') else fn)
    tables = sorted(set(tables))
    ttok = {t: set(tokens(t)) for t in tables}

    out = {}
    for n in names:
        nt = set(tokens(n))
        scored = []
        for t, tk in ttok.items():
            if not nt or not tk:
                continue
            inter = nt & tk
            if not inter:
                continue
            # coverage both ways: how much of the class name we explain, and how
            # much of the table name we explain
            cov_c = len(inter) / len(nt)
            cov_t = len(inter) / len(tk)
            scored.append((round(cov_c + cov_t + 0.5 * cov_c * cov_t, 3), t, sorted(inter)))
        scored.sort(reverse=True)
        out[n] = {'tokens': sorted(nt),
                  'candidates': [{'table': t, 'score': s, 'shared': sh} for s, t, sh in scored[:3]]}
    hit = sum(1 for v in out.values() if v['candidates'])
    strong = sum(1 for v in out.values()
                 if v['candidates'] and v['candidates'][0]['score'] >= 1.5)
    json.dump({'tables': len(tables), 'classes': len(out), 'with_candidate': hit,
               'with_strong_candidate': strong, 'map': out},
              open(OUT, 'w'), indent=1)
    print('classes', len(out), 'tables', len(tables))
    print('with candidate', hit, 'strong', strong)
    for n, v in list(out.items())[:12]:
        c = v['candidates'][0] if v['candidates'] else None
        print('  ', n, '->', (c['table'], c['score']) if c else None)
    print('written', OUT)


if __name__ == '__main__':
    main()
