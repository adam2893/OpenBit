import blhrpc, subprocess, time, json, os, sys

ADB = '/opt/homebrew/bin/adb'
S = '127.0.0.1:16384'

def tap(x, y):
    subprocess.run([ADB, '-s', S, 'shell', 'input', 'tap', str(x), str(y)], capture_output=True)

AGE = (720, 1934)
LADDER = [(719,1428),(719,1600),(719,1771),(719,1910),(718,1829)]

def shot(name):
    subprocess.run(['screencapture','-x','-o','-R394,308,608,184', f'/tmp/{name}.png'], capture_output=True)

def choice_count(sc):
    if not sc: return 0
    last = sc[-1]
    n = 0
    for k in ('_ChoiceToAction','_Choice'):
        v = last.get(k)
        if isinstance(v, dict):
            c = v.get('_count') or v.get('Count') or 0
            if isinstance(c, int) and c > n: n = c
    return n

trace = []
N = int(sys.argv[1]) if len(sys.argv) > 1 else 12
# warm up: one Age tap to instantiate Life
blhrpc.rpc('clear')
tap(*AGE); time.sleep(2.5)
for r in range(N):
    blhrpc.rpc('clear')
    tap(*AGE)
    time.sleep(2.6)
    tk = blhrpc.rpc('ticks', 3)
    sc = blhrpc.rpc('scenarios', 3)
    lg = blhrpc.rpc('logs', 6)
    nch = choice_count(sc)
    # dismiss dialogs until an age tick shows up
    tries = 0
    order = LADDER[:nch] + [LADDER[3]] + LADDER[nch:3] + [LADDER[4]] if nch else LADDER
    for (x, y) in order:
        if tries >= 5: break
        tries += 1
        tap(x, y); time.sleep(1.0)
        tk2 = blhrpc.rpc('ticks', 3)
        if len(tk2) > len(tk):
            tk = tk2
            sc = sc + blhrpc.rpc('scenarios', 3)
            lg = lg + blhrpc.rpc('logs', 6)
            break
    hero = blhrpc.rpc('hero', 1)
    h = hero[-1] if isinstance(hero, list) and hero else hero
    trace.append({'round': r, 'tap_ladder_index': tries, 'choice_count': nch,
                  'logs': lg, 'scenarios': sc, 'ticks': tk, 'hero': h,
                  'life': bool(blhrpc.rpc('pointers'))})
    print('round', r, 'choice', nch, 'ladder', tries, 'ticks', len(tk), 'logs', len(lg), 'scen', len(sc), flush=True)
    if r in (0, 4, N-1): shot('bl_d2_%d' % r)
json.dump(trace, open('/Users/Adam/Documents/bitlife/_re/trace_game2.json','w'), indent=1)
print('saved trace_game2.json entries', len(trace))
