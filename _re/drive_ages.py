import json, os, subprocess, sys, time
ADB='/opt/homebrew/bin/adb'; S='127.0.0.1:16384'
def sh(*a): return subprocess.run([ADB,'-s',S]+list(a), capture_output=True, text=True).stdout
def rpc(op, **kw):
    kw['op']=op
    if os.path.exists('/tmp/blh_out.json'): os.remove('/tmp/blh_out.json')
    json.dump(kw, open('/tmp/blh_cmd.json','w'))
    for _ in range(150):
        if os.path.exists('/tmp/blh_out.json'):
            try: return json.load(open('/tmp/blh_out.json'))
            except Exception: pass
        time.sleep(0.2)
    return {'error':'timeout'}
def tap(x,y,wait=3.5): sh('shell','input','tap',str(x),str(y)); time.sleep(wait)
AGE=(738,2122); OK=(737,1937); C1=(737,940); C2=(737,1120); C3=(737,1290)
def shot(name):
    with open('/tmp/'+name,'wb') as f:
        f.write(subprocess.run([ADB,'-s',S,'exec-out','screencap','-p'], capture_output=True).stdout)
print('clear', rpc('clear'))
n_ages=int(sys.argv[1]) if len(sys.argv)>1 else 3
for i in range(n_ages):
    tap(*AGE, 5)
    for _ in range(3):
        tap(*OK, 2.5)
    tap(*C1, 2.5)
    tap(*OK, 2.0)
    print('after age', i+1, json.dumps(rpc('pointers')))
shot('bl_run_end.png')
logs = rpc('logs', n=60)
print('LOGS n=', len(logs))
for l in logs: print(' ', l.get('age'), '|', str(l.get('text'))[:150])
scs = rpc('scenarios', n=20)
print('SCENARIOS n=', len(scs))
for s in scs: print('  *', str(s.get('Title'))[:60], '|', str(s.get('Body'))[:110])
h = rpc('hero')
keep = {k.replace('<','').replace('>k__BackingField',''): v for k,v in (h or {}).items() if k.replace('<','').replace('>k__BackingField','') in ('Age','Alive','Att_happiness','Att_health','Att_intelligence','Att_appearance','Att_karma','Att_discipline','PlaceOfResidenceString')}
print('HERO', json.dumps(keep))
