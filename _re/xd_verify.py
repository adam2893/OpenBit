import json, re, glob

idx = json.load(open('/tmp/xd_index.json'))
cells = {}
for tab, arr in idx.items():
    for c in arr:
        cells.setdefault(c, tab)

def norm(s):
    s = s.lower().replace('\\n', ' ').replace('\u2019', "'")
    s = re.sub(r'<[^>]*>|\{[^}]*\}|\[\[[^\]]*\]\]', ' ', s)
    s = re.sub(r'[0-9]+', '#', s)
    s = re.sub(r"[^a-z' ]+", ' ', s)
    return re.sub(r'\s+', ' ', s).strip()

def chunks(text):
    text = text.replace('\\n', ' ')
    parts = re.split(r'(?:"[^"]*"|[0-9]+|[A-Z][a-z]+(?: [A-Z][a-z]+)*|\u20ac[0-9.,]+)', text)
    return [n for n in (norm(p) for p in parts) if len(n) >= 16]

def lookup(ch):
    if ch in cells:
        return cells[ch], ch
    if len(ch) >= 16:
        for c, tab in cells.items():
            if len(ch) >= 20 and (ch in c or c in ch):
                return tab, c
    return None, None

sces = json.load(open('/tmp/xd_scen.json'))
logs = []
for f in glob.glob('/Users/Adam/Documents/bitlife/_re/trace_game*.json'):
    try: tr = json.load(open(f))
    except Exception: continue
    if not isinstance(tr, list): continue
    for r in tr:
        if not isinstance(r, dict): continue
        for l in (r.get('logs') or []):
            if isinstance(l, dict) and len(l.get('text') or '') > 20:
                logs.append({'text': l['text'], 'age': l.get('age'), 'src': f.split('/')[-1]})
uniq = {}
for l in logs:
    uniq.setdefault(l['text'], l)
logs = list(uniq.values())

def cover(items, get):
    rows = []
    for it in items:
        t = get(it)
        chs = chunks(t)
        hits = []
        for ch in chs:
            tab, c = lookup(ch)
            if tab: hits.append((tab, ch))
        rows.append({'text': t[:150], 'nchunks': len(chs), 'hits': hits[:3],
                     'table': hits[0][0] if hits else None})
    return rows

sr = cover(sces, lambda s: (s.get('body') or ''))
lr = cover(logs, lambda l: l['text'])
print('SCENARIO BODIES: %d items, %d matched to a port table (%.0f%%)' % (
    len(sr), sum(1 for r in sr if r['table']), 100.0 * sum(1 for r in sr if r['table']) / max(1, len(sr))))
print('LOG LINES: %d items, %d matched (%.0f%%)' % (
    len(lr), sum(1 for r in lr if r['table']), 100.0 * sum(1 for r in lr if r['table']) / max(1, len(lr))))
print()
print('--- unmatched scenario bodies (script events / no table row) ---')
for r in sr:
    if not r['table']:
        print('  %s' % r['text'][:110])
print()
print('--- unmatched log lines ---')
for r in lr:
    if not r['table']:
        print('  %s' % r['text'][:110])
json.dump({'scenarios': sr, 'logs': lr}, open('/Users/Adam/Documents/bitlife/_re/xdiff_verify.json', 'w'), indent=1)
