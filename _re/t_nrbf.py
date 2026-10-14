import nrbf, json, sys, io
p='/Users/Adam/Documents/bitlife/_re/gamedata/a16_countries-enc'
data=open(p,'rb').read()
print('bytes', len(data))
try:
    obj=nrbf.loads(data)
    print('type', type(obj))
    def shape(o, d=0, maxd=3):
        pre='  '*d
        if d>maxd: return
        if isinstance(o, dict):
            ks=list(o.keys())[:14]
            print(pre+'{'+str(len(o))+' keys} '+', '.join(map(str,ks)))
            for k in ks[:6]: shape(o[k], d+1, maxd)
        elif isinstance(o,(list,tuple)):
            print(pre+'['+str(len(o))+']')
            if o: shape(o[0], d+1, maxd)
        else:
            print(pre+repr(o)[:140])
    shape(obj)
except Exception as e:
    import traceback; traceback.print_exc()
    print('FAILED', e)

