import json, glob, os, csv, io

ROOT = '/Users/Adam/Documents/bitlife/_re/gamedata_plain'
scen = json.load(open('/tmp/xd_scen.json'))

def load(path):
    raw = open(path, 'rb').read().decode('utf-8', 'replace')
    return raw

def find_rows(raw, needle):
    out = []
    for i, line in enumerate(raw.splitlines(), 1):
        if needle in line:
            out.append((i, line[:160]))
    return out

report = []
for s in scen:
    t = s['title']
    hits = []
    for f in glob.glob(ROOT + '/*'):
        raw = load(f)
        if t and t in raw:
            rows = find_rows(raw, t)
            body_ok = bool(s['body']) and s['body'][:40] in raw
            hits.append({'table': os.path.basename(f), 'rows': [r[0] for r in rows], 'body_ok': body_ok})
    report.append({'title': t, 'cls': s['cls'], 'ext': s['ext'], 'nchoice': s['nchoice'],
                   'found': len(hits), 'where': hits[:4], 'body_ok': any(h['body_ok'] for h in hits)})

ok = [r for r in report if r['found']]
print('scenarios: %d, title found in tables: %d, body also matched: %d' % (len(report), len(ok), sum(1 for r in ok if r['body_ok'])))
for r in report:
    print('%-28s %-26s choices=%-4s found=%-2d body_ok=%s %s' % (r['title'][:26], (r['cls'] or '')[:24], r['nchoice'], r['found'], r['body_ok'], (r['where'][0]['table'][:44] if r['where'] else '-')))
json.dump(report, open('/Users/Adam/Documents/bitlife/_re/xdiff_report.json', 'w'), indent=1)
