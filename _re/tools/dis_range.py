import re,sys,bisect
from capstone import *
from capstone.arm64 import *
DELTA=0x73F049E000
SO='lib/arm64-v8a/libil2cpp.so'
pat=re.compile(r'^(\s*)method\s+@0x([0-9a-f]+)\s+(.+)$')
rows=[]
for i,l in enumerate(open('il2cpp_dump.txt',encoding='utf-8',errors='ignore'),1):
    m=pat.match(l.rstrip())
    if m: rows.append((int(m.group(2),16), i, m.group(3).strip()))
rows.sort(); addrs=[r[0] for r in rows]
def name_of(a):
    i=bisect.bisect_right(addrs,a)-1
    if i<0: return None
    return (rows[i][0],rows[i][2])
def dis(dumpaddr, n):
    fo=dumpaddr-DELTA
    data=open(SO,'rb').read()
    md=Cs(CS_ARCH_ARM64, CS_MODE_ARM); md.detail=True
    code=data[fo:fo+n*4]
    out=[]
    for ins in md.disasm(code, dumpaddr):
        tgt=None
        if ins.mnemonic.startswith('b') or ins.mnemonic in ('bl','blr'):
            op=ins.operands
            if op and op[0].type==ARM64_OP_IMM:
                t=tgt=op[0].imm; nm=name_of(t)
                if nm: out.append('%x  %-8s %s   ; -> %s (+0x%x)'%(ins.address,ins.mnemonic,ins.op_str,nm[1][:80],t-nm[0]))
                else: out.append('%x  %-8s %s'%(ins.address,ins.mnemonic,ins.op_str))
                continue
        out.append('%x  %-8s %s'%(ins.address,ins.mnemonic,ins.op_str))
    return out
if __name__=='__main__':
    a=int(sys.argv[1],16); n=int(sys.argv[2])
    print('\n'.join(dis(a,n)))