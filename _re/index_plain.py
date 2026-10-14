import os, re, json, collections
P="/Users/Adam/Documents/bitlife/_re/gamedata_plain"
rows=[]
for f in sorted(os.listdir(P)):
    p=os.path.join(P,f)
    raw=open(p,'rb').read()
    txt=raw.decode('utf-8','ignore')
    first=txt.split('\n',1)[0][:160].rstrip('\r')
    nl=txt.count('\n')+1
    rows.append({'file':f,'bytes':len(raw),'lines':nl,'header':first})
json.dump(rows, open('/Users/Adam/Documents/bitlife/_re/gamedata_plain_index.json','w'), ensure_ascii=False, indent=1)
print("files",len(rows),"total bytes",sum(r['bytes'] for r in rows))
pref=collections.Counter()
for r in rows:
    n=r['file']
    if re.match(r'^a16_', n): pref['a16_*']+=1
    elif n.startswith('genesis'): pref['genesis*']+=1
    elif ' ' in n: pref['spaced-names']+=1
    else: pref['other']+=1
print(pref)
# top 25 biggest
for r in sorted(rows,key=lambda x:-x['bytes'])[:18]:
    print(f"  {r['bytes']:>9} {r['lines']:>7}L  {r['file'][:70]} :: {r['header'][:90]}")
json.dump(rows, open('/Users/Adam/Documents/bitlife/_re/gamedata_plain_index.json','w'), ensure_ascii=False, indent=1)

