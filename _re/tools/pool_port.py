import json,subprocess,collections
p='/Users/Adam/Documents/bitlife/_re/port'
sc={"seed":12345,"country":"United States","gender":"male","ages":40}
r=subprocess.run([p+'/build/blsim','--script','/dev/stdin'],input=json.dumps(sc),capture_output=True,text=True)
d=json.loads(r.stdout)
c=collections.Counter()
for e in d['events']: c[e.get('table') or '?']+=1
for k,v in c.most_common(): print('  %-46s %d' % (k,v))
