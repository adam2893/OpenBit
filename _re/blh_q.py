import json, os, sys, time
CMD='/tmp/blh_cmd.json'; OUT='/tmp/blh_out.json'
op = sys.argv[1]
req = {'op': op}
if op=='ticks': req['n']=int(sys.argv[2]) if len(sys.argv)>2 else 10
if op=='dec': req['b64']=sys.argv[2]
if op=='cls': req['ns']=sys.argv[2]; req['name']=sys.argv[3]
if op=='sub': req['which']=sys.argv[2]; req['field']=sys.argv[3]
if os.path.exists(OUT): os.remove(OUT)
json.dump(req, open(CMD,'w'))
for _ in range(200):
    if os.path.exists(OUT):
        try:
            d=json.load(open(OUT)); print(json.dumps(d, ensure_ascii=False)[:int(os.environ.get('MAXOUT','3000'))]); sys.exit(0)
        except Exception: pass
    time.sleep(0.2)
print('TIMEOUT')
