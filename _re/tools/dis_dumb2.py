from capstone import *
SO='/Users/Adam/Documents/bitlife/_re/lib/arm64-v8a/libil2cpp.so'
data=open(SO,'rb').read(); md=Cs(CS_ARCH_ARM64,CS_MODE_ARM)
off=0x28202b8
lines=[]
for i in md.disasm(data[off:off+0x700],off):
    lines.append('%06x  %-8s %s'%(i.address,i.mnemonic,i.op_str))
# print only interesting: branches, RNG calls, field loads near end
for l in lines:
    if 'bl ' in l or 'b.' in l or 'cbz' in l or 'cbnz' in l:
        print(l)
