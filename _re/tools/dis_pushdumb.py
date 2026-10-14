from capstone import *
SO='/Users/Adam/Documents/bitlife/_re/lib/arm64-v8a/libil2cpp.so'
data=open(SO,'rb').read(); md=Cs(CS_ARCH_ARM64,CS_MODE_ARM)
off=0x2e25238
out=[]
for i in md.disasm(data[off:off+0x140],off):
    out.append('%06x  %-8s %s'%(i.address,i.mnemonic,i.op_str))
print('\n'.join(out))
