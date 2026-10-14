import re,bisect,subprocess,sys
pat=re.compile(r'^(\s*)method\s+@0x([0-9a-f]+)\s+(.+)$')
rows=[]
for i,l in enumerate(open('il2cpp_dump.txt',encoding='utf-8',errors='ignore'),1):
    m=pat.match(l.rstrip())
    if m: rows.append((int(m.group(2),16), i, m.group(3).strip()))
rows.sort(); addrs=[r[0] for r in rows]
def next_name(a):
    i=bisect.bisect_right(addrs,a)
    return rows[i] if i<len(rows) else None
s=sys.argv[1]; a=int(s,16)
n=next_name(a)
print('start',hex(a),'next',hex(n[0]),'len',n[0]-a,'nxt method',n[2][:70])
cnt=(n[0]-a)//4
out=subprocess.run(['python3','tools/dis_range.py',s,str(cnt)],capture_output=True,text=True).stdout
open('/tmp/pd.asm','w').write(out)
print('insns',len(out.splitlines()))