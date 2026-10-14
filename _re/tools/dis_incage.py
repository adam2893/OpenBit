import re,json
from capstone import *
SO='/Users/Adam/Documents/bitlife/_re/lib/arm64-v8a/libil2cpp.so'
DELTA=0x73F049E000
addr=0x73f373b9d8
off=addr-DELTA
data=open(SO,'rb').read()
md=Cs(CS_ARCH_ARM64,CS_MODE_ARM); md.detail=True
code=data[off:off+0x1200]
out=[]
for i in md.disasm(code,0x2a9d9d8):
    s='%06x  %-8s %s'%(i.address,i.mnemonic,i.op_str)
    out.append(s)
open('/tmp/incage.asm','w').write('\n'.join(out))
print('insns',len(out))
print('\n'.join(out[:60]))