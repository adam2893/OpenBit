import json, glob, csv, re, os
base='/Users/Adam/Documents/bitlife/_re'
scen={}; logs={}
for f in sorted(glob.glob(base+'/trace_game*.json')):
    try: d=json.load(open(f))
    except Exception: continue
    if not isinstance(d, list): continue
    for e in d:
        if not isinstance(e, dict): continue
        age=e.get('age')
        for s in e.get('scenarios') or []:
            if isinstance(s, dict):
                b=s.get('Body') or ''; t=s.get('Title') or ''; cls=s.get('_class') or ''
                if b or t: scen.setdefault((cls,t,b[:60]), {'cls':cls,'title':t,'body':b,'ages':set()})['ages'].add(age)
        for l in e.get('logs') or []:
            if isinstance(l, dict) and (l.get('text') or '').strip():
                logs.setdefault(l['text'], set()).add(age)

REL = r'(half sister|half brother|stepsister|stepbrother|step sister|step brother|sister|brother|mother|father|stepfather|stepmother|step father|step mother|friend|best friend|girlfriend|boyfriend|wife|husband|son|daughter|vizsla|dog|cat|boss|teacher|classmate|coworker|neighbour|neighbor)'
NAME = r'([A-Z][a-z]+(?: [A-Z][a-z]+)?)'
# relation word -> slot token used in the templates
TOK = {'half sister':'halfsister','half brother':'halfbrother','step sister':'stepsister','step brother':'stepbrother',
       'step sister':'stepsister','sister':'sister','brother':'brother','mother':'mother','father':'father',
       'stepfather':'stepfather','stepmother':'stepmother','step father':'stepfather','step mother':'stepmother',
       'friend':'friend','best friend':'friend','girlfriend':'lover','boyfriend':'lover','wife':'spouse',
       'husband':'spouse','son':'son','daughter':'daughter','vizsla':'dog','dog':'dog','cat':'cat',
       'boss':'boss','teacher':'teacher','classmate':'classmate','coworker':'coworker','neighbour':'neighbour','neighbor':'neighbour'}
pairs={}
def mine(text):
    for m in re.finditer(r'\b[Mm]y '+REL+r',?\s+'+NAME, text):
        pairs.setdefault(m.group(2), m.group(1).lower())
    for m in re.finditer(r'[Yy]our new (stepfather|stepmother|stepbrother|stepsister|half sister|half brother),?\s+'+NAME, text):
        pairs.setdefault(m.group(2), m.group(1).lower())
    for m in re.finditer(r'([Ff]riend|[Cc]lassmate|[Cc]oworker|[Bb]oss|[Tt]eacher|[Nn]eighbour|[Nn]eighbor),?\s+'+NAME, text):
        pairs.setdefault(m.group(2), m.group(1).lower())
    for m in re.finditer(r'([A-Z][a-z]+(?: [A-Z][a-z]+)?), my (half sister|half brother|sister|brother|mother|father|friend)', text):
        pairs.setdefault(m.group(1), m.group(2))
    for m in re.finditer(r'(?i)my '+REL+r',?\s+'+NAME, text):
        pass
for t in logs: mine(t)
for r in scen.values(): mine(r['title']); mine(r['body'])
# marriage lines: "My half sister, Inka, married Juhani Salo, a 24-year old grocer."
for t in list(logs)+[r['body'] for r in scen.values()]:
    for m in re.finditer(r'married '+NAME, t): pairs.setdefault(m.group(1), 'spouse')
print('names mined:', len(pairs))
print(sorted(pairs.items()))
def tokenize(text):
    slots={}
    for name, rel in sorted(pairs.items(), key=lambda kv: -len(kv[0])):
        if re.search(r'\b'+re.escape(name)+r'\b', text):
            key = TOK.get(rel, rel.replace(' ', ''))
            slots[key]=name
            text = re.sub(r'\b'+re.escape(name)+r'\b', '{'+key+'}', text)
    return text, slots
rows=[]
for (cls,t,b60), r in sorted(scen.items()):
    tb, ts = tokenize(r['title']); bb, bs = tokenize(r['body'])
    slots = dict(ts); slots.update(bs)
    rows.append(['scenario', cls, tb, bb, json.dumps(slots, ensure_ascii=False), ','.join(str(a) for a in sorted(r['ages'], key=lambda x:(x is None,x)))])
for t, ages in sorted(logs.items()):
    tb, ts = tokenize(t)
    rows.append(['log','','', tb, json.dumps(ts, ensure_ascii=False), ','.join(str(a) for a in sorted(ages, key=lambda x:(x is None,x)))])
os.makedirs(base+'/obs_corpus', exist_ok=True)
with open(base+'/obs_corpus/templates.csv','w',newline='') as fh:
    w=csv.writer(fh); w.writerow(['Kind','Class','Title','Text','Slots','Ages']); w.writerows(rows)
print('templates rows', len(rows), 'scenarios', sum(1 for r in rows if r[0]=='scenario'), 'with slots', sum(1 for r in rows if r[4]!='{}'))
for r in rows:
    if r[4]!='{}': print(r[0], '|', r[3][:100], '|', r[4][:60])
