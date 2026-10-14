import re
p='/Users/Adam/Documents/bitlife/_re/assets/bin/Data/Managed/Metadata/global-metadata.dat'
d=open(p,'rb').read()
print('size',len(d))
for needle in [b'-enc', b'DecodeAndDecryptString', b'DecodeBase64', b'genesis_master_tags_reference_2']:
    idxs=[m.start() for m in re.finditer(re.escape(needle), d)]
    print('==',needle,len(idxs))
    for i in idxs[:6]:
        s=max(0,i-90); e=min(len(d), i+90)
        chunk=d[s:e]
        print('   @',i, re.sub(rb'[^ -~]', b'.', chunk).decode('latin1'))

