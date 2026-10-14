import json, os, sys, time, uuid
def rpc(op, timeout=40, **kw):
    rid = uuid.uuid4().hex[:10]
    kw['op'] = op
    json.dump(kw, open('/tmp/blh_req.'+rid+'.json','w'))
    res = '/tmp/blh_res.'+rid+'.json'
    t0 = time.time()
    while time.time()-t0 < timeout:
        if os.path.exists(res):
            try:
                d = json.load(open(res))
                os.remove(res)
                return d
            except Exception: pass
        time.sleep(0.2)
    return {'error':'timeout'}
if __name__ == '__main__':
    op = sys.argv[1]
    kw = {}
    if op in ('logs','scenarios','ticks','ageticks','births','rand','picks','prod','prod2','attsets','wraps','family','persons','bags','picksall','qchecks','qtexts','pushdumb','queues','statw','rbdump','rbdump2') and len(sys.argv)>2: kw['n']=int(sys.argv[2])
    if op.startswith('x_') and len(sys.argv)>2:
        args=[]
        for a in sys.argv[2:]:
            try: args.append(int(a,0))
            except Exception: args.append(a)
        kw['args']=args
    if op=='hk': kw['n']=sys.argv[2]; kw['t']=sys.argv[3] if len(sys.argv)>3 else ''
    if op=='dec': kw['b64']=sys.argv[2]
    if op=='cls': kw['ns']=sys.argv[2]; kw['name']=sys.argv[3]
    if op=='sub': kw['which']=sys.argv[2]; kw['field']=sys.argv[3]
    print(json.dumps(rpc(op, **kw), ensure_ascii=False)[:int(os.environ.get('MAXOUT','4000'))])
