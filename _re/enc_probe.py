import base64, os, collections, math, re
base='/Users/Adam/Documents/bitlife/_re/gamedata'
def load(f):
    t=open(os.path.join(base,f),'rb').read()
    try: return base64.b64decode(t, validate=False)
    except Exception as e: return None
for f in ['a16_accessories-headwear-enc','a16_actor-agent-drops-hero-color-enc','a16_actor-agent-fire-color-enc','a16_countries-enc']:
    b=load(f)
    if b is None: print(f,'b64 fail'); continue
    printable=sum(1 for c in b if 32<=c<127 or c in (9,10,13))
    print('==',f,'bytes',len(b),'printable%',round(100*printable/max(1,len(b)),1))
    print('   head', b[:64])
    print('   hex ', b[:48].hex())
a=load('a16_actor-agent-drops-hero-color-enc'); c=load('a16_actor-agent-hire-color-enc')
if a and c:
    n=min(len(a),len(c)); x=bytes(a[i]^c[i] for i in range(n))
    z=x.count(0)
    print('XOR pair: len',n,'zeros',z, 'first80', x[:80])
    # runs
    runs=[]; cur=0
    for v in x:
        if v==0: cur+=1
        else:
            if cur>3: runs.append(cur)
            cur=0
    print('zero-run>3 count', len(runs), 'max', max(runs) if runs else 0, runs[:12])

