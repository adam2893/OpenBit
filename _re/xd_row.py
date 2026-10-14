import json, re
ROOT = '/Users/Adam/Documents/bitlife/_re/gamedata_plain/'
rep = json.load(open('/Users/Adam/Documents/bitlife/_re/xdiff_match.json'))
seen = set()
for r in rep:
    m = r['match']
    if not m: continue
    key = (m['table'], m['at'] // 4000)
    if key in seen: continue
    seen.add(key)
    raw = open(ROOT + m['table'], 'rb').read().decode('utf-8', 'replace')
    i = m['at']
    print('=' * 100)
    print('GAME  %s  [%s]  choices=%s' % (r['title'], r['cls'], r['nchoice']))
    print('TABLE %s' % m['table'])
    seg = raw[max(0, i - 400):i + 700]
    print('RAW   ...%s...' % seg.replace('\n', ' \\n ')[:900])
