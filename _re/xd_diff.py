"""Phase-1 differential report: game-observed text/tables vs what the C++ port implements.

Inputs:  trace_game*.json (game), /tmp/xd_index.json (table cells), xdiff_sources.json
Outputs: xdiff2.json + a stdout summary
"""
import json, re, glob, os, hashlib, subprocess

RE = '/Users/Adam/Documents/bitlife/_re'

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

idx = json.load(open('/tmp/xd_index.json'))
cells = {}
for tab, arr in idx.items():
    for c in arr:
        cells.setdefault(c, tab)

def lookup(ch):
    if ch in cells:
        return cells[ch], ch
    if len(ch) >= 20:
        for c, tab in cells.items():
            if ch in c or c in ch:
                return tab, c
    return None, None

# --- implemented tables in the C++ port ---
port_tabs = set()
for p in glob.glob(RE + '/port/src/*.cpp') + glob.glob(RE + '/port/include/bl/*.h'):
    port_tabs.update(re.findall(r'"(a16_[a-z0-9_.\-]+)"', open(p, encoding='utf-8', errors='replace').read()))

# --- game-side evidence ---
src_items = json.load(open(RE + '/xdiff_sources.json'))
logs = {}
for f in sorted(glob.glob(RE + '/trace_game*.json')):
    try: tr = json.load(open(f))
    except Exception: continue
    if not isinstance(tr, list): continue
    for r in tr:
        if not isinstance(r, dict): continue
        for l in (r.get('logs') or []):
            t = (l.get('text') or '').strip()
            if len(t) > 20:
                logs.setdefault(t, {'n': 0, 'age': l.get('age'), 'src': os.path.basename(f)})
                logs[t]['n'] += 1

# --- port-side capability ---
have, missing_tab, missing_none = [], {}, 0
for it in src_items:
    tab = it.get('table')
    if it.get('src') == 'table':
        if tab in port_tabs: have.append(it['title'])
        else: missing_tab[tab] = missing_tab.get(tab, 0) + 1
    elif it.get('src') == 'none':
        missing_none += 1
for t in logs:
    tab, _ = None, None
    for ch in chunks(t):
        tab, hit = lookup(ch)
        if tab: break
    if tab in port_tabs: have.append(t)

tot = len(src_items) + len(logs)
report = {
    'port_tables_implemented': sorted(port_tabs),
    'game_scenarios_total': len(src_items),
    'game_scenarios_already_covered_by_port': sum(1 for it in src_items if it.get('table') in port_tabs),
    'game_logs_total': len(logs),
    'game_logs_already_covered_by_port': sum(1 for t in logs if any(lookup(c)[0] in port_tabs for c in chunks(t))),
    'top_missing_tables': sorted(missing_tab.items(), key=lambda kv: -kv[1])[:15],
    'scenarios_with_no_known_source': missing_none,
}
json.dump(report, open(RE + '/xdiff2.json', 'w'), indent=1)
print(json.dumps(report, indent=1))

# --- determinism check on the port ---
def run(seed, ages, out):
    sc = {'seed': seed, 'country': 'Finland', 'gender': 'male', 'ages': ages, 'choices': []}
    open('/tmp/_xd_sc.json', 'w').write(json.dumps(sc))
    r = subprocess.run([RE + '/port/build/blsim', '--script', '/tmp/_xd_sc.json'], capture_output=True)
    open(out, 'wb').write(r.stdout)
    return hashlib.sha256(r.stdout).hexdigest()[:16], r.returncode

a = run(12345, 8, '/tmp/_xd_a.json')
b = run(12345, 8, '/tmp/_xd_b.json')
c = run(999, 8, '/tmp/_xd_c.json')
print('determinism: same-seed', a, b, 'equal=', a == b, '| different-seed', c['0' if False else 0])
