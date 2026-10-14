import re,sys,bisect
pat=re.compile(r'^(\s*)method\s+@0x([0-9a-f]+)\s+(.+)$')
rows=[]
for i,l in enumerate(open('il2cpp_dump.txt',encoding='utf-8',errors='ignore'),1):
    m=pat.match(l.rstrip())
    if m: rows.append((int(m.group(2),16), i, m.group(3).strip()))
rows.sort()
addrs=[r[0] for r in rows]
for t in [0x73f373f1dc,0x73f3745b48,0x73f2eb3238]:
    i=bisect.bisect_right(addrs,t)-1
    a,ln,sig=rows[i]
    print(hex(t),'-> line',ln,'delta',hex(t-a),sig[:110])