import json,glob,os
def load(p):
    try:
        d=json.load(open(p))
    except Exception as e:
        return None
    return d
scen={}
for f in ['/tmp/c2_scen.json','/tmp/c3_scen.json','/tmp/c4_scen.json']:
    d=load(f)
    if not d: continue
    rows=d.get('rows') if isinstance(d,dict) else d
    for r in (rows or []):
        c=r.get('_class') or r.get('cls')
        scen.setdefault(c,[]).append({k:r.get(k) for k in ('_class','title','Title','body','Body','ext','ExteriorTitle') if k in r})
logs={}
for f in glob.glob('/tmp/c*_logs.json'):
    d=load(f)
    if not d: continue
    rows=d.get('rows') if isinstance(d,dict) else d
    for r in (rows or []):
        logs.setdefault(r.get('age'),[]).append(r.get('text') or r.get('s') or r)
for c in sorted(scen):
    print('==',c)
    for it in scen[c][:2]:
        b=(it.get('Body') or it.get('body') or '')
        print('   body:', b.replace(chr(10),' | ')[:200])
print()
for a in sorted([k for k in logs if isinstance(k,int)])[:15]:
    print(a, json.dumps(logs[a],ensure_ascii=False)[:300])
