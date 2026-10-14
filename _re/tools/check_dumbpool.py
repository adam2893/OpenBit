import json,subprocess,collections,sys
P='/Users/Adam/Documents/bitlife/_re/port'
def run(seed, ages, date=None, country='United States'):
    s={'seed':seed,'country':country,'gender':'male','ages':ages}
    if date: s['date']=date
    r=subprocess.run([P+'/build/blsim','--script','/dev/stdin'],input=json.dumps(s),capture_output=True,text=True,cwd=P)
    if r.returncode: return None, r.stderr[:200]
    return json.loads(r.stdout), None
# baseline: no date -> dated sets out of the pool
c=collections.Counter(); tot=0
for seed in range(400):
    o,e=run(seed,40); assert o,e
    if e: print(e); break
    for ev in o['events']:
        c[ev.get('table')]+=1; tot+=1
print('no-date tables', dict(c.most_common(6)), 'total', tot)
c2=collections.Counter(); tot2=0
for seed in range(400):
    o,e=run(seed,40,date='10-31')
    if e: print(e); break
    for ev in o['events']:
        c2[ev.get('table')]+=1; tot2+=1
print('date 10-31 tables', dict(c2.most_common(8)), 'total', tot2)
# age-gated holiday rows only become eligible from age 4 up
hal=[ev['age'] for s in range(60) for ev in run(s,20,date='10-31')[0]['events'] if 'holiday' in (ev.get('table') or '')]
print('halloween ages', sorted(set(hal))[:12])
print('min halloween age', min(hal) if hal else None)