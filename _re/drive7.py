import blhrpc, subprocess, time, json, sys
from PIL import Image
sys.path.insert(0, '/tmp')
import dlg4

ADB = '/opt/homebrew/bin/adb'; S = '127.0.0.1:16384'
AGE = (720, 1934)

def tap(x, y):
    subprocess.run([ADB, '-s', S, 'shell', 'input', 'tap', str(x), str(y)], capture_output=True)

def back():
    subprocess.run([ADB, '-s', S, 'shell', 'input', 'keyevent', '4'], capture_output=True)

def is_green(c):
    r, g, b = c
    return g > 100 and g - r > 35 and g - b > 30

def age_visible(path):
    px = Image.open(path).convert('RGB').load()
    return sum(1 for dx in (-40, 0, 40) for dy in (-40, 0, 40) if is_green(px[720 + dx, 1934 + dy])) >= 7

def step():
    p = dlg4.capture()
    if age_visible(p):
        tap(*AGE); return 'age'
    st = dlg4.state(p)
    if st['kind'] == 'dialog' and st['buttons']:
        cy = st['buttons'][0][1]; tap(720, cy); return 'btn%d' % cy
    back(); return 'back'

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
        acts.append(step())
        time.sleep(1.5)
        if len(blhrpc.rpc('ticks', 2000)) > n0t: break
    logs = blhrpc.rpc('logs', 2000)[n0l:]
    scen = blhrpc.rpc('scenarios', 2000)[n0s:]
    ticks = blhrpc.rpc('ticks', 2000)[n0t:]
    hero = blhrpc.rpc('hero', 1)
    rec = {'round': r, 'acts': acts, 'age': age_of(hero), 'logs': logs, 'scenarios': scen, 'ticks': ticks,
           'hero': (hero[-1] if isinstance(hero, list) and hero else hero)}
    trace.append(rec)
    print('r%-2d %-28s age=%s logs=%d scen=%d tick=%d' % (r, ','.join(acts), rec['age'], len(logs), len(scen), len(ticks)), flush=True)
json.dump(trace, open('/Users/Adam/Documents/bitlife/_re/trace_game7.json', 'w'), indent=1)
print('saved ->', '/Users/Adam/Documents/bitlife/_re/trace_game7.json')
