import json,subprocess,collections,glob,sys
tot=collections.Counter(); byage=collections.defaultdict(collections.Counter)
for f in sorted(glob.glob('/Users/Adam/Documents/bitlife/_re/trace_*.json')):
    try:
        d=json.load(open(f))
    except Exception: continue
    if 'ages' not in d: continue
    p=subprocess.run(['python3','tools/row_recover.py',f],capture_output=True,text=True,cwd='/Users/Adam/Documents/bitlife/_re')
    # row_recover prints per-scenario lines: "  age N  cls  table  ..."
    lines=[l for l in p.stdout.splitlines() if l.startswith('  age')]
    ages=[a.get('age') for a in d.get('ages',[])]
    print(f.split('/')[-1], 'ages', len(ages), 'scen problems', len(lines))
    for l in lines[:6]: print('   ', l[:110])
print()
print('--- aggregate tables')
