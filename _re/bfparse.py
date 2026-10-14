import os, sys, json, glob
sys.setrecursionlimit(200000)
import nrbf
BF="/Users/Adam/Documents/bitlife/_re/gamedata_bf"
OUT="/Users/Adam/Documents/bitlife/_re/gamedata_json"
os.makedirs(OUT, exist_ok=True)
def to_json(o, depth=0):
    if depth>60: return str(o)
    if isinstance(o, dict):
        return {str(k): to_json(v, depth+1) for k,v in o.items()}
    if isinstance(o, (list, tuple)):
        return [to_json(v, depth+1) for v in o]
    if isinstance(o, (str,int,float,bool)) or o is None:
        return o
    if isinstance(o, bytes):
        return {'__bytes__': o[:64].hex(), '__len__': len(o)}
    return str(o)
for f in sorted(os.listdir(BF)):
    p=os.path.join(BF,f)
    try:
        with open(p,'rb') as fh:
            obj=nrbf.load(fh)
        j=to_json(obj)
        open(os.path.join(OUT,f+'.json'),'w').write(json.dumps(j, ensure_ascii=False, indent=1))
        n=len(j) if isinstance(j,(list,dict)) else 1
        keys=''
        sample=j[0] if isinstance(j,list) and j else (j if isinstance(j,dict) else None)
        if isinstance(sample,dict): keys=', '.join(list(sample.keys())[:12])
        print(f"{f}: type={type(obj).__name__} count={n} bytes={os.path.getsize(p)} keys=[{keys[:150]}]",flush=True)
    except Exception as e:
        print(f"{f}: FAIL {type(e).__name__}: {str(e)[:120]}",flush=True)
print("--- done ---")

