"""Assert the newborn draw sequence: ctor values at age 0, setup on the first age-up."""
import json, subprocess, collections
P='/Users/Adam/Documents/bitlife/_re/port'
def run(seed):
    # ages=1: SimHero.SetupEverythingElseForNewHero runs on the first age-up, so
    # the post-setup state is the 'newborn' block; the age-0 record ('hero')
    # keeps the ctor values, which is what the game records at age 0.
    sc={'seed':seed,'country':'United States','gender':'male','ages':1}
    p=subprocess.run([P+'/build/blsim','--script','/dev/stdin'], input=json.dumps(sc), capture_output=True, text=True, cwd=P)
    if p.returncode: raise SystemExit(p.stderr[-300:])
    return json.loads(p.stdout)
c=collections.Counter(); bad=[]; age0=[]
for s in range(1,61):
    out=run(s)
    h=out['newborn']['stats']; b0=out['hero']['stats']
    c['smart' if h['smarts']<20 else 'looks']+=1
    age0.append((b0['happiness'],b0['health']))
    if not (81<=h['happiness']<=100 and 81<=h['health']<=100): bad.append((s,'primary',h))
    if not ((10<=h['smarts']<=19 and 61<=h['looks']<=70) or (61<=h['smarts']<=70 and 10<=h['looks']<=19)): bad.append((s,'secondary',h))
    if not (0<=h['karma']<=100 and 0<=h['discipline']<=100): bad.append((s,'ctor',h))
print('branch split smart/looks:', c['smart'], c['looks'])
print('age0 ctor happiness range:', min(x[0] for x in age0), max(x[0] for x in age0),
      '| health range:', min(x[1] for x in age0), max(x[1] for x in age0))
print('violations:', len(bad), bad[:3])