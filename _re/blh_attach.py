import frida, json, os, sys, time, glob, traceback, uuid
LOG = '/Users/Adam/Documents/bitlife/_re/blh_hold.log'
REQ = '/tmp/blh_req.'
def log(m):
    with open(LOG,'a') as f: f.write(time.strftime('%H:%M:%S ')+str(m)+'\n')
    print(m, flush=True)
for f in glob.glob(REQ+'*.json'): os.remove(f)
dev = frida.get_device('127.0.0.1:16384', timeout=10)
import subprocess
subprocess.run(['adb','-s','127.0.0.1:16384','shell','monkey','-p','com.candywriter.bitlife','-c','android.intent.category.LAUNCHER','1'], capture_output=True)
time.sleep(12)
out = subprocess.run(['adb','-s','127.0.0.1:16384','shell','pidof','com.candywriter.bitlife'], capture_output=True, text=True).stdout.strip()
pid = int(out.split()[0]) if out else 0
log('spawned pid='+str(pid))
s = dev.attach(pid)
script = s.create_script('var CAPTURE_SESSION = '+json.dumps(uuid.uuid4().hex)+';\n'+open('/Users/Adam/Documents/bitlife/_re/blh.js').read())
script.on('message', lambda m,d: log('MSG '+str(m)[:250]))
script.on('destroyed', lambda: log('!! SCRIPT DESTROYED (process died?)'))
script.load()
pass
# attached, not spawned: no resume needed
log('resumed')
open('/tmp/blh_pid.txt','w').write(str(pid))
ready = False
for i in range(600):
    try:
        h = script.exports_sync.hooks()
        if isinstance(h, list) and h:
            log('hooks '+str(h)); ready = True; break
    except Exception: pass
    time.sleep(0.5)
if not ready:
    log('!! never became ready, exiting'); sys.exit(1)
log('HARNESS READY pid='+str(pid))
while True:
    for rf in sorted(glob.glob(REQ+'*.json')):
        rid = rf[len(REQ):-5]
        try:
            req = json.load(open(rf)); os.remove(rf)
            op = req.pop('op')
            log('op '+op+' '+json.dumps(req)[:80])
            if op == 'hero': r = script.exports_sync.hero()
            elif op == 'life': r = script.exports_sync.life()
            elif op == 'pointers': r = script.exports_sync.pointers()
            elif op == 'ticks': r = script.exports_sync.ticks(req.get('n',10))

            elif op == 'births': r = script.exports_sync.births(req.get('n',5))

            elif op == 'attsets': r = script.exports_sync.attsets(req.get('n',200))

            elif op == 'wraps': r = script.exports_sync.wraps(req.get('n',500))

            elif op == 'family': r = script.exports_sync.family()

            elif op == 'persons': r = script.exports_sync.persons(req.get('n',100))
            elif op == 'chain': r = script.exports_sync.chain()
            elif op == 'branch': r = script.exports_sync.branch(req.get('n',10))
            elif op == 'logs': r = script.exports_sync.logs(req.get('n',50))
            elif op == 'scenarios': r = script.exports_sync.scenarios(req.get('n',20))
            elif op == 'ageticks': r = script.exports_sync.ageticks(req.get('n',20))
            elif op == 'clear': r = script.exports_sync.clear()
            elif op == 'key': r = script.exports_sync.key()
            elif op == 'cls': r = script.exports_sync.cls(req.get('ns',''), req.get('name',''))
            elif op == 'sub': r = script.exports_sync.sub(req.get('which','life'), req.get('field',''))
            elif op == 'missed': r = script.exports_sync.missed()
            elif op == 'hits': r = script.exports_sync.hits()
            elif op == 'diag': r = script.exports_sync.diag()
            elif op == 'probe': r = script.exports_sync.probe()
            elif op == 'buttons': r = script.exports_sync.buttons()
            elif op == 'age': r = script.exports_sync.age()
            elif op == 'sceninfo': r = script.exports_sync.sceninfo()
            elif op == 'resolve': r = script.exports_sync.resolve(req.get('ptr',''), req.get('idx',0))
            elif op == 'dec': r = script.exports_sync.dec(req.get('b64',''))
            elif op == 'seed': r = script.exports_sync.seed()
            elif op == 'picks': r = script.exports_sync.picks(req.get('n', 40), req.get('tag'))
            elif op == 'prod2': r = script.exports_sync.prod2(req.get('n', 40))
            elif op == 'pushdumb': r = script.exports_sync.pushdumb(req.get('n', 20))
            elif op == 'queues': r = script.exports_sync.queues(req.get('n', 20))
            elif op == 'bags': r = script.exports_sync.bags(req.get('n', 10))
            elif op == 'picksall': r = script.exports_sync.picksall(req.get('n', 50))
            elif op == 'qchecks': r = script.exports_sync.qchecks(req.get('n', 500))
            elif op == 'qtexts': r = script.exports_sync.qtexts(req.get('n', 50))
            elif op == 'prod': r = script.exports_sync.prod(req.get('n', 60))
            elif op == 'rand': r = script.exports_sync.rand(req.get('n', 600))
            elif op == 'decks': r = script.exports_sync.decks(req.get('n',40))
            elif op == 'deckstats': r = script.exports_sync.deckstats()
            elif op == 'deckstats': r = script.exports_sync.deckstats()
            elif op == 'uer': r = script.exports_sync.uer(req.get('n',300))
            elif op == 'alget': r = script.exports_sync.alget(req.get('n',200))
            elif op == 'deckmath': r = script.exports_sync.deckmath(req.get('ps',[0.0,0.05,0.1,0.25,0.5,0.75,1.0]))
            elif op == 'deckmathres': r = script.exports_sync.deckmathres()
            elif op == 'deckobj': r = script.exports_sync.deckobj(req.get('size',100), req.get('succ',50))
            elif op == 'statw': r = script.exports_sync.statw(req.get('n',500))
            elif op == 'reload':
                try: script.unload()
                except Exception as e: log('unload: '+str(e))
                script = s.create_script('var CAPTURE_SESSION = '+json.dumps(uuid.uuid4().hex)+';\n'+open('/Users/Adam/Documents/bitlife/_re/blh.js').read())
                script.on('message', lambda m,d: log('MSG '+str(m)[:250]))
                script.on('destroyed', lambda: log('!! SCRIPT DESTROYED (process died?)'))
                script.load()
                r = {'ok': True, 'hooks': script.exports_sync.hooks()}
            elif op == 'hk': r = script.exports_sync.hk(req.get('n',''), req.get('t',''))
            elif op.startswith('x_'): r = getattr(script.exports_sync, op[2:])(*req.get('args',[]))
            elif op == 'quit': json.dump({'ok':True}, open('/tmp/blh_res.'+rid+'.json','w')); log('quit'); sys.exit(0)
            else: r = {'error':'unknown op'}
            json.dump(r if r is not None else {'ok':False}, open('/tmp/blh_res.'+rid+'.json','w'))
        except Exception as e:
            json.dump({'error':str(e)}, open('/tmp/blh_res.'+rid+'.json','w'))
    time.sleep(0.3)
