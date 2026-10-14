import blhrpc, subprocess, time, json, sys
sys.path.insert(0, '/tmp')
import dlg4

ADB = '/opt/homebrew/bin/adb'; S = '127.0.0.1:16384'
AGE = (720, 1934); PROMO_X = (303, 586); PANEL_X = (121, 271)

def tap(x, y):
    subprocess.run([ADB, '-s', S, 'shell', 'input', 'tap', str(x), str(y)], capture_output=True)

def act(st):
    if st['kind'] == 'dialog' and st['buttons']:
        cy = st['buttons'][0][1]; tap(720, cy); return 'btn%d' % cy
    if st['kind'] == 'promo':
        tap(*PROMO_X); return 'promoX'
    if st['kind'] == 'panel':
        tap(*PANEL_X); return 'panelX'
    tap(*AGE); return 'age'

def age_of(hero):
    h = hero[-1] if isinstance(hero, list) and hero else hero
    if isinstance(h, dict):
        for k in ('<Age>k__BackingField', 'Age'):
            if k in h: return h[k]
    return None

N = int(sys.argv[1]) if len(sys.argv) > 1 else 12
trace = []
for r in range(N):
    n0l = len(blhrpc.rpc('logs', 2000)); n0s = len(blhrpc.rpc('scenarios', 2000)); n0t = len(blhrpc.rpc('ticks', 2000))
    acts = []
    for attempt in range(8):
        st = dlg4.state(dlg4.capture())
        acts.append(act(st))
        time.sleep(1.6)
        if len(blhrpc.rpc('ticks', 2000)) > n0t: break
    logs = blhrpc.rpc('logs', 2000)[n0l:]
    scen = blhrpc.rpc('scenarios', 2000)[n0s:]
    ticks = blhrpc.rpc('ticks', 2000)[n0t:]
    hero = blhrpc.rpc('hero', 1)
    rec = {'round': r, 'acts': acts, 'age': age_of(hero), 'logs': logs, 'scenarios': scen, 'ticks': ticks,
           'hero': (hero[-1] if isinstance(hero, list) and hero else hero)}
    trace.append(rec)
    print('r%-2d %-28s age=%s logs=%d scen=%d tick=%d' % (r, ','.join(acts), rec['age'], len(logs), len(scen), len(ticks)), flush=True)
json.dump(trace, open('/Users/Adam/Documents/bitlife/_re/trace_game6.json', 'w'), indent=1)
print('saved ->', '/Users/Adam/Documents/bitlife/_re/trace_game6.json')
