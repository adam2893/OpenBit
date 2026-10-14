from capstone import *
SO='/Users/Adam/Documents/bitlife/_re/lib/arm64-v8a/libil2cpp.so'
data=open(SO,'rb').read()
md=Cs(CS_ARCH_ARM64,CS_MODE_ARM)
def dis(name,addr,n=0x180):
    off=addr-0x73F049E000
    print('==',name,hex(addr),'fileoff',hex(off))
    for i in md.disasm(data[off:off+n],off):
        print('%06x  %-8s %s'%(i.address,i.mnemonic,i.op_str))
dis('GetRandomUniversalDumbScenario',0x73f2cbe2b8,0x1c0)
