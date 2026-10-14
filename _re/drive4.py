import blhrpc, subprocess, time, json, sys
sys.path.insert(0, '/tmp')
import dlg2

ADB = '/opt/homebrew/bin/adb'; S = '127.0.0.1:16384'
AGE = (720, 1934); CLOSE = (121, 271)

def tap(x, y):
    subprocess.run([ADB, '-s', S, 'shell', 'input', 'tap', str(x), str(y)], capture_output=True)

def age_of(hero):
    h = hero[-1] if isinstance(hero, list) and hero else hero
    if isinstance(h, dict):
        for k in ('<Age>k__BackingField', 'Age'):
            if k in h: return h[k]
    return None

N = int(sys.argv[1]) if len(sys.argv) > 1 else 12
trace = []
last_age = None
for r in range(N):
    blhrpc.rpc('clear')
    acts = []
    for attempt in range(6):
        st = dlg2.state(dlg2.capture())
        if st['kind'] == 'dialog':
            cy = st['buttons'][0][1]; tap(720, cy); acts.append('btn%d' % cy); time.sleep(1.4)
        elif st['kind'] == 'panel':
            tap(*CLOSE); acts.append('close'); time.sleep(1.4)
        else:
            tap(*AGE); acts.append('age'); time.sleep(2.2)
        tk = blhrpc.rpc('ticks', 3)
        if tk:
            break
    tk = blhrpc.rpc('ticks', 3); sc = blhrpc.rpc('scenarios', 4); lg = blhrpc.rpc('logs', 12)
    hero = blhrpc.rpc('hero', 1)
    rec = {'round': r, 'acts': acts, 'age': age_of(hero), 'logs': lg, 'scenarios': sc, 'ticks': tk,
           'hero': (hero[-1] if isinstance(hero, list) and hero else hero)}
    trace.append(rec)
    print('r%-2d %-22s age=%s logs=%d scen=%d tick=%d' % (r, ','.join(acts), rec['age'], len(lg), len(sc), len(tk)), flush=True)
json.dump(trace, open('/Users/Adam/Documents/bitlife/_re/trace_game4.json', 'w'), indent=1)
print('saved ->', '/Users/Adam/Documents/bitlife/_re/trace_game4.json')
