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

def capture_cursor(snapshot):
    return dict(v=1, session=snapshot['session'], generation=snapshot['generation'],
                streams={name:dict(next=row['next']) for name,row in snapshot['streams'].items()})


def capture_fetch(cursor=None):
    result = blhrpc.rpc('x_capture', timeout=1, args=[cursor])
    if not isinstance(result, dict) or result.get('error'):
        raise ValueError('capture unavailable: ' + str(result))
    safe = lambda x: type(x) is int and 0 <= x <= 9007199254740991
    if type(result.get('v')) is not int or result['v'] != 1 or not isinstance(result.get('session'), str) or not result['session'] or not safe(result.get('generation')):
        raise ValueError('invalid capture identity')
    if cursor and (result['session'],result['generation']) != (cursor['session'],cursor['generation']):
        raise ValueError('capture epoch changed')
    streams = result.get('streams')
    if not isinstance(streams, dict) or set(streams) != {'logs','scenarios','ticks'}:
        raise ValueError('invalid capture streams')
    for name,cap in (('logs',3000),('scenarios',1000),('ticks',400)):
        row = streams[name]
        if not isinstance(row,dict) or not safe(row.get('first')) or not safe(row.get('next')) or not 0 <= row['next']-row['first'] <= cap:
            raise ValueError('invalid capture bounds: ' + name)
        if cursor:
            previous = cursor['streams'][name]['next']
            start = max(row['first'],previous)
            if row['next'] < previous or not safe(row.get('start')) or row['start'] != start or not safe(row.get('dropped')) or row['dropped'] != max(0,row['first']-previous):
                raise ValueError('invalid capture delta: ' + name)
            records = row.get('records')
            if not isinstance(records,list) or len(records) != row['next']-start or not all(isinstance(x,dict) for x in records):
                raise ValueError('invalid capture records: ' + name)
    return result

def capture_record(snapshot, baseline):
    streams = snapshot['streams']
    return dict(logs=streams['logs']['records'],scenarios=streams['scenarios']['records'],ticks=streams['ticks']['records'],
                capture=dict(v=1,session=snapshot['session'],generation=snapshot['generation'],from_cursor=baseline,
                             streams={name:{k:v for k,v in row.items() if k!='records'} for name,row in streams.items()}),
                capture_complete=not any(row['dropped'] for row in streams.values()))


T0 = time.time()
STALL = [0]
trace = []
current = None
accepted = None
capture_error = None
try:
    accepted = capture_cursor(capture_fetch())
    for r in range(N):
        baseline = accepted
        current = dict(round=r, acts=[], dbg=[], age=None, logs=[], scenarios=[], ticks=[])
        snapshot = None
        for attempt in range(8):
            current['acts'].append(step(current['dbg']) if attempt < 4 else recover(attempt-4,current['dbg']))
            time.sleep(1.0)
            snapshot = capture_fetch(baseline)
            current.update(capture_record(snapshot,baseline))
            if not current['capture_complete']:
                raise ValueError('capture overflow')
            if any(snapshot['streams'][name]['next'] > baseline['streams'][name]['next'] for name in baseline['streams']):
                break
        # Polls never consume baseline. Final fetch includes in-flight appends.
        snapshot = capture_fetch(baseline)
        current.update(capture_record(snapshot,baseline))
        if not current['capture_complete']:
            raise ValueError('capture overflow')
        moved = any(snapshot['streams'][name]['next'] > baseline['streams'][name]['next'] for name in baseline['streams'])
        STALL[0] = 0 if moved else STALL[0]+1
        DTRIES[0] = 0
        if len(current['acts'])==8 and not moved and LAST[0]:
            shutil.copy(LAST[0],'/tmp/stuck_r%d.png' % r)
        hero = blhrpc.rpc('hero', timeout=1)
        if not isinstance(hero,(dict,list)) or (isinstance(hero,dict) and hero.get('error')):
            raise ValueError('hero capture unavailable: '+str(hero))
        current['hero'] = hero[-1] if isinstance(hero,list) and hero else hero
        current['age'] = age_of(hero)
        trace.append(current)
        accepted = capture_cursor(snapshot)
        print('r%d age=%s logs=%d scen=%d tick=%d' % (r,current['age'],len(current['logs']),len(current['scenarios']),len(current['ticks'])),flush=True)
        current = None
        if STALL[0]>=3:
            p = dlg5.capture()
            shutil.copy(p,'/tmp/stalled_r%d.png' % r)
            st = dlg5.state(p)
            hits = blhrpc.rpc('diag',timeout=1)
            if not isinstance(hits,dict) or hits.get('error'):
                raise ValueError('stalled diagnostics unavailable: '+str(hits))
            print('stalled at r%d state=%s hits=%s' % (r,st,hits.get('hits',{})),flush=True)
            break
except (Exception, KeyboardInterrupt) as error:
    capture_error = str(error) or type(error).__name__
    if current is None:
        current = dict(round=len(trace),acts=[],dbg=[],age=None,logs=[],scenarios=[],ticks=[])
    current['capture_error'] = capture_error
    current['capture_complete'] = False
    if not trace or trace[-1] is not current:
        trace.append(current)
finally:
    result = dict(rounds=trace,capture_complete=capture_error is None,capture_error=capture_error,
                  capture_cursor=accepted,capture_scope='helper-successful-appends')
    json.dump(result,open('/Users/Adam/Documents/bitlife/_re/trace_game10.json','w'),indent=1)
print('saved ->','/Users/Adam/Documents/bitlife/_re/trace_game10.json')
if capture_error is not None:
    sys.exit(1)
