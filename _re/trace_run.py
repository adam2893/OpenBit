import json, subprocess, sys, time, os
sys.path.insert(0, '/Users/Adam/Documents/bitlife/_re')
from blhrpc import rpc
ADB='/opt/homebrew/bin/adb'; DEV='127.0.0.1:16384'
AGE=(720,1934); OK=(718,1829); CHOICE1=(719,1428); SURPRISE=(719,1910)
def tap(p):
    subprocess.run([ADB,'-s',DEV,'shell','input','tap',str(p[0]),str(p[1])],capture_output=True); time.sleep(0.5)
def shot(p):
    with open(p,'wb') as f: subprocess.run([ADB,'-s',DEV,'exec-out','screencap','-p'],stdout=f)
N=int(sys.argv[1]) if len(sys.argv)>1 else 10
out=sys.argv[2] if len(sys.argv)>2 else '/Users/Adam/Documents/bitlife/_re/trace_game.json'
t={'rounds':[]}
for i in range(N):
    rpc('clear')
    tap(AGE)
    time.sleep(2.6)
    logs=rpc('logs',n=400); scen=rpc('scenarios',n=80); ticks=rpc('ticks',n=6); life=rpc('life'); hero=rpc('hero')
    shot('/tmp/bl_t%d.png'%i)
    t['rounds'].append({'i':i,'logs':logs,'scenarios':scen,'ticks':ticks,'life':life,'hero':hero})
    last = scen[-1] if isinstance(scen,list) and scen else None
    nch = 0
    if last:
        c = last.get('_ChoiceToAction') or {}
        nch = c.get('_count') or 0
    age = None
    if isinstance(ticks,list) and ticks:
        age = ((ticks[-1] or {}).get('s') or {}).get('<Age>k__BackingField')
    print(i,'logs',len(logs) if isinstance(logs,list) else logs,
          'scen',len(scen) if isinstance(scen,list) else scen,
          'age',age,'title',(last or {}).get('Title'),'choices',nch,flush=True)
    if nch>0: tap(CHOICE1)
    else: tap(OK)
json.dump(t,open(out,'w'),indent=1)
print('written',out,os.path.getsize(out))
