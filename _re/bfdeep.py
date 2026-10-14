import sys, threading, json, os
try:
    import nrbf
except Exception as e:
    print("no nrbf", e); sys.exit(0)
sys.setrecursionlimit(1000000)
threading.stack_size(512*1024*1024)
D="/Users/Adam/Documents/bitlife/_re/gamedata_bf"
OUT="/Users/Adam/Documents/bitlife/_re/gamedata_json"
res={}
def work(fn):
    try:
        data=open(os.path.join(D,fn),'rb').read()
        obj=nrbf.loads(data)
        with open(os.path.join(OUT,fn+'.json'),'w') as f:
            json.dump(obj,f,default=str)
        res[fn]='ok'
    except Exception as e:
        res[fn]='FAIL %s: %s' % (type(e).__name__, str(e)[:120])
for fn in ["female0-age13","male1-age13"]:
    t=threading.Thread(target=work,args=(fn,))
    t.start(); t.join()
    print(fn, res.get(fn))
print("json files:", len(os.listdir(OUT)))