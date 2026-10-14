import json, glob, csv, os
base='/Users/Adam/Documents/bitlife/_re'
sc={}; lg={}
for f in sorted(glob.glob(base+'/trace_game*.json')):
    try: d=json.load(open(f))
    except Exception as e: print('skip', f, e); continue
    if not isinstance(d, list): continue
    for e in d:
        if not isinstance(e, dict): continue
        age=e.get('age')
        for s in e.get('scenarios') or []:
            if not isinstance(s, dict): continue
            cls=s.get('_class') or s.get('class') or ''
            title=s.get('Title') or ''
            body=s.get('Body') or ''
            if not body and not title: continue
            k=(cls,title,body[:60])
            r=sc.setdefault(k, {'cls':cls,'title':title,'body':body,'ages':set(),'srcs':set()})
            if age is not None: r['ages'].add(age)
            r['srcs'].add(os.path.basename(f))
        for l in e.get('logs') or []:
            if not isinstance(l, dict): continue
            t=l.get('text') or ''
            if not t.strip(): continue
            k=t
            r=lg.setdefault(k, {'text':t,'ages':set(),'srcs':set()})
            if age is not None: r['ages'].add(age)
            r['srcs'].add(os.path.basename(f))
os.makedirs(base+'/obs_corpus', exist_ok=True)
with open(base+'/obs_corpus/scenarios.csv','w',newline='') as fh:
    w=csv.writer(fh)
    w.writerow(['Class','Title','Body','Ages','Sources'])
    for (cls,title,b60),r in sorted(sc.items()):
        w.writerow([r['cls'], r['title'], r['body'], ','.join(str(a) for a in sorted(r['ages'])), ','.join(sorted(r['srcs']))])
with open(base+'/obs_corpus/logs.csv','w',newline='') as fh:
    w=csv.writer(fh)
    w.writerow(['Text','Ages','Sources'])
    for t,r in sorted(lg.items()):
        w.writerow([r['text'], ','.join(str(a) for a in sorted(r['ages'])), ','.join(sorted(r['srcs']))])
print('scenarios', len(sc), 'logs', len(lg))
import collections
print('top classes:', collections.Counter(r['cls'] for r in sc.values()).most_common(12))
