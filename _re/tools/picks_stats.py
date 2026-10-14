import json,collections
d=json.load(open('/tmp/c5_picks.json'))
rows=d if isinstance(d,list) else (d.get('rows') or [])
print('total',len(rows))
c=collections.Counter(r.get('m') for r in rows)
for k,v in c.most_common(14): print('  %-48s %d'%(k,v))
dumb=[r for r in rows if 'DumbScenario' in (r.get('m') or '')]
print('dumb-ish', len(dumb))
for r in dumb[:14]:
    print('  age',r.get('age'), r.get('m'),'args',r.get('args'),'cls',r.get('retCls'),'row',json.dumps(r.get('row'),ensure_ascii=False)[:140])
