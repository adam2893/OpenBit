import blhrpc, subprocess, time, json, sys, shutil
sys.path.insert(0, '/tmp')
import dlg5
from PIL import Image

LAST = [None]
# Blockers dlg5 cannot classify (IAP promos, marketplace panels, Google Play
# alerts) keep the Age circle visible but swallow the tap. Battery of blind
# recoveries, tried one per attempt before giving up on the round.
RECOVER = [('tap', 110, 87), ('tap', 303, 586), ('tap', 303, 731), ('key', 4),
           ('tap', 720, 2190), ('tap', 720, 640), ('tap', 303, 459)]

def recover(i, db=None):
    act = RECOVER[i % len(RECOVER)]
    if act[0] == 'key':
        subprocess.run([ADB, '-s', S, 'shell', 'input', 'keyevent', str(act[1])], capture_output=True)
        a = 'key%d' % act[1]
    else:
        tap(act[1], act[2]); a = 'rec%d_%d' % (act[1], act[2])
    if db is not None: db.append({'act': a, 'kind': 'recover'})
    return a

ADB = '/opt/homebrew/bin/adb'; S = '127.0.0.1:16384'
AGE = (720, 1934)

def tap(x, y):
    subprocess.run([ADB, '-s', S, 'shell', 'input', 'tap', str(x), str(y)], capture_output=True)

def age_visible(path):
    return dlg5.age_circle(path) is not None

DTRIES = [0]
CX = [0]
# card text -> option indices already taken, so a menu card is not re-answered
# with the same button every time it comes up.
SEEN = {}
TAKEN = [0]
# Plausible close-button anchors, cycled through on consecutive attempts:
# top-right promo X, Movie Director modal X, marketplace X, panel X.
CLOSERS = [(1330, 88), (303, 586), (110, 87), (121, 271)]

def step(db=None):
    global AGE
    p = dlg5.capture()
    LAST[0] = p
    ac = dlg5.age_circle(p)
    if ac:
        AGE = ac
        tap(*AGE); a = 'age%d' % AGE[1]
    else:
        st = dlg5.state(p)
        if st['kind'] == 'alert':
            tap(*st['x']); a = 'alert%d_%d' % st['x']
        elif st['kind'] == 'dialog':
            # Always taking the first option can loop forever when that option opens a
            # confirm sub-dialog we then cancel, and it never reveals the other
            # branches. Prefer an option this card text has not taken yet.
            n = len(st['buttons'])
            key = str(st.get('card') or '')[:120]
            tried = SEEN.setdefault(key, set())
            k = next((i for i in range(n) if i not in tried), DTRIES[0] % n)
            tried.add(k)
            TAKEN[0] = k
            cy = st['buttons'][k][1]; tap(720, cy); a = 'btn%d#%d' % (cy, k)
            DTRIES[0] += 1
        elif st['kind'] == 'exppanel':
            cx = CLOSERS[CX[0] % len(CLOSERS)]; CX[0] += 1
            tap(cx[0], cx[1]); a = 'expX%d_%d' % cx
        elif st['kind'] == 'panel':
            tap(*st['x']); a = 'panelX'
        elif st['kind'] == 'modalx':
            # Cream modal cards (Items/IAP promo, marketplace) carry a red ring X
            # that dlg5.red_x() locates; dlg5 does not classify them, so without
            # this branch the driver fell through to a blind top-left tap and the
            # whole life stalled on the card.
            tap(*st['x']); a = 'modalx%d_%d' % st['x']
        else:
            # Never tap a remembered Age position: on a full-screen list page
            # (MAIN MENU, ACCESSORIES, ...) that lands on a random row, and one of
            # those rows is New Life. Measure first; if there is no disc, this is
            # some list screen, so hit its top-left close control instead.
            ac = dlg5.age_circle(p)
            if ac:
                AGE = ac; tap(*AGE); a = 'age%d' % AGE[1]
            else:
                tap(92, 62); a = 'topback'
        if db is not None: db.append({'act': a, 'kind': st['kind'], 'card': st['card'], 'nb': len(st['buttons']), 'k': TAKEN[0]})
    return a

def age_of(hero):
    h = hero[-1] if isinstance(hero, list) and hero else hero
    if isinstance(h, dict):
        for k in ('<Age>k__BackingField', 'Age'):
            if k in h: return h[k]
    return None

N = int(sys.argv[1]) if len(sys.argv) > 1 else 16

def counts():
    # diag is a few hundred bytes; logs(2000) is the whole log table. The
    # driver only needs to know whether anything moved, so ask for counts.
    d = blhrpc.rpc('diag', 1) or {}
    n = d.get('n', {}) if isinstance(d, dict) else {}
    return (n.get('logs', 0) or 0, n.get('scen', 0) or 0, n.get('ticks', 0) or 0)

T0 = time.time()
STALL = [0]
trace = []
for r in range(N):
    c0 = counts()
    acts = []; db = []; c1 = c0
    for attempt in range(8):
        acts.append(step(db) if attempt < 4 else recover(attempt - 4, db))
        time.sleep(1.0)
        c1 = counts()
        # Any of the three growing means the tap landed: a log line resolved,
        # a new card is up, or the year advanced. Waiting only for logs made
        # the driver burn its recover battery on every intermediate card.
        if any(b > a for a, b in zip(c0, c1)): break
    if any(b > a for a, b in zip(c0, c1)):
        STALL[0] = 0
    else:
        # A round that moved nothing is usually a screen the classifier cannot
        # name. Three in a row means the driver has lost the life screen, and
        # campaign 7 burned twenty rounds that way before anyone noticed.
        STALL[0] += 1
    DTRIES[0] = 0
    n1 = c1[0]
    if len(acts) == 8 and not any(b > a for a, b in zip(c0, c1)) and LAST[0]:
        shutil.copy(LAST[0], '/tmp/stuck_r%d.png' % r)
    # Prefix slicing is valid only while retained buffers have not shifted/reset.
    # ponytail: bounded tails lack cursors; add sequence/overflow evidence for lossless capture.
    logs = (blhrpc.rpc('logs', n=3000) or [])[c0[0]:]
    scen = (blhrpc.rpc('scenarios', n=1000) or [])[c0[1]:]
    ticks = (blhrpc.rpc('ticks', n=500) or [])[c0[2]:]
    hero = blhrpc.rpc('hero', 1)
    rec = {'round': r, 'acts': acts, 'dbg': db, 'age': age_of(hero), 'logs': logs, 'scenarios': scen,
           'ticks': ticks, 'hero': (hero[-1] if isinstance(hero, list) and hero else hero)}
    trace.append(rec)
    first = db[0].get('kind') if db else None
    print('r%-2d %-30s age=%s logs=%d scen=%d tick=%d t=%.0f first=%s' % (r, ','.join(acts), rec['age'], len(logs), len(scen), len(ticks), time.time() - T0, first), flush=True)
    if STALL[0] >= 3:
        # Shoot the screen again instead of copying the round's last capture: the
        # two can be different screens, and guessing from a stale file cost a full
        # debugging round on campaign 8. Print the classifier's own verdict and the
        # hook counters so the log alone says whether the game moved at all.
        p = dlg5.capture()
        shutil.copy(p, '/tmp/stalled_r%d.png' % r)
        st = dlg5.state(p)
        hits = (blhrpc.rpc('diag', 1) or {}).get('hits', {})
        print('stalled at r%d age=%s -> stopping; state=%s card=%s nb=%s hits=%s' % (
            r, rec['age'], st['kind'], st['card'], len(st['buttons']), hits), flush=True)
        break
json.dump(trace, open('/Users/Adam/Documents/bitlife/_re/trace_game10.json', 'w'), indent=1)
print('saved ->', '/Users/Adam/Documents/bitlife/_re/trace_game9.json')
