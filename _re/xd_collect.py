import json, glob
sces = []
for f in sorted(glob.glob('/Users/Adam/Documents/bitlife/_re/trace_game*.json')):
    try: tr = json.load(open(f))
    except Exception: continue
    for r in (tr if isinstance(tr, list) else []):
        if not isinstance(r, dict):
            continue
        for s in (r.get('scenarios') or []):
            if isinstance(s, dict) and s.get('Title'):
                sces.append({'title': s.get('Title'), 'body': s.get('Body'), 'cls': s.get('_class'),
                             'ext': s.get('ExteriorTitle'),
                             'nchoice': (s.get('_ChoiceToOriginalIndex') or {}).get('_count'),
                             'src': f.split('/')[-1], 'age': r.get('age')})
seen = set(); uniq = []
for s in sces:
    k = (s['title'], (s['body'] or '')[:80])
    if k in seen: continue
    seen.add(k); uniq.append(s)
print('unique scenarios', len(uniq))
json.dump(uniq, open('/tmp/xd_scen.json', 'w'), indent=1)
pats = []
for s in uniq:
    pats.append(s['title'])
    if s['body']: pats.append(s['body'])
open('/tmp/xd_pats.txt', 'w').write('\n'.join(dict.fromkeys(pats)))
print('patterns', len(pats))
