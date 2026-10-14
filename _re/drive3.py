import blhrpc, subprocess, time, json, sys, os
sys.path.insert(0, '/tmp')
import dlg

ADB = '/opt/homebrew/bin/adb'; S = '127.0.0.1:16384'
AGE = (720, 1934)

def tap(x, y):
    subprocess.run([ADB, '-s', S, 'shell', 'input', 'tap', str(x), str(y)], capture_output=True)

def age_of(hero):
    h = hero[-1] if isinstance(hero, list) and hero else hero
    if isinstance(h, dict):
        for k in ('<Age>k__BackingField', 'Age'):
            if k in h: return h[k]
    return None

N = int(sys.argv[1]) if len(sys.argv) > 1 else 15
trace = []
for r in range(N):
    blhrpc.rpc('clear')
    info = dlg.analyze(dlg.capture())
    used = 'age'
    if info['buttons']:
        kind, cy, band = info['buttons'][0]
        tap(720, cy); used = 'btn:' + kind
    else:
        tap(*AGE)
    time.sleep(2.4)
    tk = blhrpc.rpc('ticks', 3); sc = blhrpc.rpc('scenarios', 4); lg = blhrpc.rpc('logs', 12)
    hero = blhrpc.rpc('hero', 1)
    rec = {'round': r, 'used': used, 'card': info['card'], 'nbtn': len(info['buttons']),
           'age': age_of(hero), 'logs': lg, 'scenarios': sc, 'ticks': tk, 'hero': (hero[-1] if isinstance(hero, list) and hero else hero)}
    trace.append(rec)
    print('r%-2d %-10s btns=%d age=%s logs=%d scen=%d tick=%d' % (r, used, len(info['buttons']), rec['age'], len(lg), len(sc), len(tk)), flush=True)
json.dump(trace, open('/Users/Adam/Documents/bitlife/_re/trace_game3.json', 'w'), indent=1)
print('saved', len(trace), '->', '/Users/Adam/Documents/bitlife/_re/trace_game3.json')
