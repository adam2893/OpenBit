import UnityPy, collections, os, json, sys
base='/Users/Adam/Documents/bitlife/_re'
out=os.path.join(base,'gamedata'); os.makedirs(out, exist_ok=True)
for fn in ['assets/bin/Data/data.unity3d','assets/bin/Data/datapack.unity3d','assets/bin/Data/sharedassets0.resource']:
    p=os.path.join(base,fn)
    if not os.path.exists(p): print('MISSING',fn); continue
    print('=== ',fn, os.path.getsize(p), flush=True)
    try:
        env=UnityPy.load(p)
        c=collections.Counter()
        names=[]
        for o in env.objects:
            c[o.type.name]+=1
            if o.type.name in ('TextAsset',):
                try:
                    d=o.read()
                    names.append((getattr(d,'m_Name','?'), len(getattr(d,'m_Script','') or b'')))
                except Exception as e: names.append(('?', -1))
        print(dict(c.most_common(20)), flush=True)
        print('TextAssets:', len(names), flush=True)
        for n,l in sorted(names, key=lambda x:-x[1])[:25]: print('   ', n, l, flush=True)
    except Exception as e:
        print('ERR', e, flush=True)

