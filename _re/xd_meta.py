import re, json, glob, subprocess, os

META = '/Users/Adam/Documents/bitlife/_re/assets/bin/Data/Managed/Metadata/global-metadata.dat'
CACHE = '/tmp/meta_norm.json'

def norm(s):
    s = s.lower().replace('\\n', ' ').replace('\u2019', "'")
    s = re.sub(r'<[^>]*>|\{[^}]*\}|\[\[[^\]]*\]\]', ' ', s)
    s = re.sub(r'[0-9]+', '#', s)
    s = re.sub(r"[^a-z' ]+", ' ', s)
    return re.sub(r'\s+', ' ', s).strip()

if os.path.exists(CACHE):
    meta = json.load(open(CACHE))
else:
    raw = subprocess.run(['strings', '-n', '8', META], capture_output=True).stdout.decode('utf-8', 'replace')
    seen = set(); out = []
    for line in raw.splitlines():
        n = norm(line)
        if len(n) >= 16 and n not in seen:
            seen.add(n); out.append(n)
    json.dump(out, open(CACHE, 'w'))
    meta = out
print('metadata literals (normalized, >=16 chars):', len(meta))

def chunks(text):
    text = text.replace('\\n', ' ')
    parts = re.split(r'(?:"[^"]*"|[0-9]+|[A-Z][a-z]+(?: [A-Z][a-z]+)*|\u20ac[0-9.,]+)', text)
    return [n for n in (norm(p) for p in parts) if len(n) >= 16]

tab = json.load(open('/tmp/xd_index.json'))
cells = {}
for t, arr in tab.items():
    for c in arr:
        cells.setdefault(c, t)

def in_tables(ch):
    if ch in cells: return cells[ch]
    for c, t in cells.items():
        if len(ch) >= 20 and (ch in c or c in ch): return t
    return None

def in_meta(ch):
    for m in meta:
        if ch in m: return m[:80]
    return None

sces = json.load(open('/tmp/xd_scen.json'))
logs = {}
for f in glob.glob('/Users/Adam/Documents/bitlife/_re/trace_game*.json'):
    try: tr = json.load(open(f))
    except Exception: continue
    if not isinstance(tr, list): continue
    for r in tr:
        if not isinstance(r, dict): continue
        for l in (r.get('logs') or []):
            if isinstance(l, dict) and len(l.get('text') or '') > 20:
                logs.setdefault(l['text'], l)

def classify(text):
    chs = chunks(text)
    tb, mt = None, None
    for ch in chs:
        if not tb: tb = in_tables(ch)
        if not mt: mt = in_meta(ch)
        if tb and mt: break
    src = 'both' if tb and mt else ('table' if tb else ('metadata' if mt else 'none'))
    return src, tb, mt

rows = []
for s in sces:
    src, tb, mt = classify(s.get('body') or '')
    rows.append({'kind': 'scenario', 'title': s['title'], 'cls': s['cls'], 'src': src, 'table': tb, 'meta': mt})
for t in logs:
    src, tb, mt = classify(t)
    rows.append({'kind': 'log', 'title': t[:80], 'src': src, 'table': tb, 'meta': mt})

from collections import Counter
c1 = Counter(r['src'] for r in rows if r['kind'] == 'scenario')
c2 = Counter(r['src'] for r in rows if r['kind'] == 'log')
print('scenario bodies: total %d %s' % (sum(c1.values()), dict(c1)))
print('log lines:       total %d %s' % (sum(c2.values()), dict(c2)))
print()
print('--- resolved ONLY by metadata literals (script-coded events) ---')
for r in rows:
    if r['src'] == 'metadata':
        print('  [%s] %s' % (r['kind'], (r['title'] or '')[:90]))
print()
print('--- resolved by neither store ---')
for r in rows:
    if r['src'] == 'none':
        print('  [%s] %s' % (r['kind'], (r['title'] or '')[:90]))
json.dump(rows, open('/Users/Adam/Documents/bitlife/_re/xdiff_sources.json', 'w'), indent=1)
