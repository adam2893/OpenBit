import json,sys,collections
d=json.load(open(sys.argv[1]))
reg=set()
tot=collections.Counter(); per=collections.defaultdict(collections.Counter)
for a in d.get('ages',[]):
    sc=a.get('scenarios') or []
    if not sc:
        tot['no_scenario']+=1; continue
    for s in sc:
        cls=(s.get('cls') or s.get('_class') or '?')
        tot[cls]+=1; per[a.get('age')][cls]+=1
print('ages', len(d.get('ages',[])))
for k,v in tot.most_common(): print('  %-46s %d' % (k,v))
print('per age:')
for a in sorted(per, key=lambda x: (x is None, x)): print('  ',a, dict(per[a]))
