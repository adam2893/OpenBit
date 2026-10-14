import csv, glob, os, re, json, sys

ROOT = '/Users/Adam/Documents/bitlife/_re/gamedata_plain'
OUT = '/tmp/xd_index.json'

def norm(s):
    s = s.lower().replace('\\n', ' ').replace('\u2019', "'")
    s = re.sub(r'<[^>]*>|\{[^}]*\}|\[\[[^\]]*\]\]', ' ', s)
    s = re.sub(r'[0-9]+', '#', s)
    s = re.sub(r"[^a-z' ]+", ' ', s)
    return re.sub(r'\s+', ' ', s).strip()

def rows_of(path):
    raw = open(path, 'rb').read().decode('utf-8', 'replace')
    try:
        rd = list(csv.reader(io.StringIO(raw)))
    except Exception:
        rd = [raw.split(',')]
    return rd

import io
idx = {}
files = sorted(glob.glob(ROOT + '/*'))
for f in files:
    name = os.path.basename(f)
    try:
        rd = rows_of(f)
    except Exception:
        continue
    cells = []
    for r in rd:
        for c in r:
            n = norm(c)
            if len(n) >= 12 and n not in cells:
                cells.append(n)
    idx[name] = cells
json.dump(idx, open(OUT, 'w'))
tot = sum(len(v) for v in idx.values())
print('files', len(idx), 'cells', tot)
