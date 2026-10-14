import json, glob, os, re

ROOT = '/Users/Adam/Documents/bitlife/_re/gamedata_plain'
scen = json.load(open('/tmp/xd_scen.json'))

def norm(s):
    s = s.lower()
    s = s.replace('\\n', ' ').replace('\u2019', "'").replace('\u2018', "'")
    s = re.sub(r'[0-9]+', '#', s)
    s = re.sub(r'[^a-z#\' ]+', ' ', s)
    return re.sub(r'\s+', ' ', s).strip()

TABLES = []
for f in glob.glob(ROOT + '/*'):
    try:
        raw = open(f, 'rb').read().decode('utf-8', 'replace')
    except Exception:
        continue
    TABLES.append((os.path.basename(f), norm(raw)))

def static_chunks(body):
    """Longest word-runs that do not contain a substituted value."""
    body = body.replace('\\n', ' ')
    parts = re.split(r'(?:"[^"]*"|[0-9]+|[A-Z][a-z]+(?: [A-Z][a-z]+)*)', body)
    out = []
    for p in parts:
        n = norm(p)
        if len(n) >= 24:
            out.append(n)
    return out

report = []
for s in scen:
    body = s['body'] or ''
    chunks = static_chunks(body)
    best = None
    for ch in chunks:
        for name, raw in TABLES:
            if ch in raw:
                i = raw.index(ch)
                best = {'chunk': ch, 'table': name, 'at': i}
                break
        if best:
            break
    report.append({'title': s['title'], 'cls': s['cls'], 'nchoice': s['nchoice'], 'ntchoice': None,
                   'match': best, 'chunks': len(chunks)})

hit = [r for r in report if r['match']]
print('scenarios %d, template-matched %d (%.0f%%)' % (len(report), len(hit), 100.0 * len(hit) / max(1, len(report))))
for r in report:
    m = r['match']
    print('%-26s %-24s ch=%-3s %s' % (r['title'][:24], (r['cls'] or '')[:22], r['nchoice'],
          (m['table'] + ' @' + str(m['at'])) if m else 'NO MATCH'))
json.dump(report, open('/Users/Adam/Documents/bitlife/_re/xdiff_match.json', 'w'), indent=1)
