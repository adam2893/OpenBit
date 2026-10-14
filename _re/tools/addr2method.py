import re,sys,bisect
P='/Users/Adam/Documents/bitlife/_re/il2cpp_dump.txt'
pat=re.compile(r'^(\s*)method\s+@0x([0-9a-f]+)\s+(.+)$')
cls=re.compile(r'^(?:public |private |internal |protected )*(?:sealed |abstract |static )*class\s+(\S+)')
rows=[];clsrows=[]
for i,l in enumerate(open(P,encoding='utf-8',errors='ignore'),1):
    m=pat.match(l.rstrip())
    if m: rows.append((int(m.group(2),16), i, m.group(3).strip()))
    c=cls.match(l.strip())
    if c: clsrows.append((i,c.group(1)))
rows.sort(); addrs=[r[0] for r in rows]; clsrows.sort()
def owner(ln):
    return [c for c in clsrows if c[0]<=ln][-1][1]
def look(t):
    i=bisect.bisect_right(addrs,t)-1
    a,ln,sig=rows[i]
    return '%s delta=%#x %s :: %s' % (hex(t), t-a, owner(ln), sig[:120])
if __name__=='__main__':
    for s in sys.argv[1:]:
        print(look(int(s,16)))
