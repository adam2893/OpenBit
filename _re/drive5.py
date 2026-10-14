import blhrpc, subprocess, time, json, sys
sys.path.insert(0, '/tmp')
import dlg3

ADB = '/opt/homebrew/bin/adb'; S = '127.0.0.1:16384'
AGE = (720, 1934)

def tap(x, y):
    subprocess.run([ADB, '-s', S, 'shell', 'input', 'tap', str(x), str(y)], capture_output=True)

def back():
    subprocess.run([ADB, '-s', S, 'shell', 'input', 'keyevent', '4'], capture_output=True)

def age_of(hero):
    h = hero[-1] if isinstance(hero, list) and hero else hero
    if isinstance(h, dict):
        for k in ('<Age>k__BackingField', 'Age'):
            if k in h: return h[k]
    return None

N = int(sys.argv[1]) if len(sys.argv) > 1 else 12
trace = []
for r in range(N):
    n0l = len(blhrpc.rpc('logs', 1000)); n0s = len(blhrpc.rpc('scenarios', 1000)); n0t = len(blhrpc.rpc('ticks', 1000))
    acts = []
    for attempt in range(7):
        st = dlg3.state(dlg3.capture())
        if st['kind'] == 'button':
            cy = st['buttons'][0][1]; tap(720, cy); acts.append('btn%d' % cy); time.sleep(1.5)
        elif st['kind'] == 'panel':
            back(); acts.append('back'); time.sleep(1.5)
        else:
            tap(*AGE); acts.append('age'); time.sleep(2.2)
        n1t = len(blhrpc.rpc('ticks', 1000))
        if n1t > n0t: break
        if attempt >= 2 and st['kind'] == 'main':
            back(); acts.append('back'); time.sleep(1.0)
    logs = blhrpc.rpc('logs', 1000)[n0l:]
    scen = blhrpc.rpc('scenarios', 1000)[n0s:]
    ticks = blhrpc.rpc('ticks', 1000)[n0t:]
    hero = blhrpc.rpc('hero', 1)
    rec = {'round': r, 'acts': acts, 'age': age_of(hero), 'logs': logs, 'scenarios': scen, 'ticks': ticks,
           'hero': (hero[-1] if isinstance(hero, list) and hero else hero)}
    trace.append(rec)
    print('r%-2d %-26s age=%s logs=%d scen=%d tick=%d' % (r, ','.join(acts), rec['age'], len(logs), len(scen), len(ticks)), flush=True)
json.dump(trace, open('/Users/Adam/Documents/bitlife/_re/trace_game5.json', 'w'), indent=1)
print('saved ->', '/Users/Adam/Documents/bitlife/_re/trace_game5.json')
