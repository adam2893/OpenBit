import os, json, nrbf, io, sys, collections
base='/Users/Adam/Documents/bitlife/_re'
src=os.path.join(base,'gamedata'); dst=os.path.join(base,'gamedata_json'); os.makedirs(dst, exist_ok=True)
BF=b'\x00\x01\x00\x00\x00'
idx=[]; ok=0; fail=0; text=0
files=sorted(os.listdir(src))
for i,f in enumerate(files):
    p=os.path.join(src,f); data=open(p,'rb').read()
    if data[:5]==BF:
        try:
            obj=nrbf.loads(data)
            with open(os.path.join(dst,f+'.json'),'w') as o: json.dump(obj,o,ensure_ascii=False,default=str)
            top = obj if not isinstance(obj,list) else obj[:1]
            kind = 'list[%d]'%len(obj) if isinstance(obj,list) else type(obj).__name__
            keys=[]
            probe = obj[0] if isinstance(obj,list) and obj else obj
            if isinstance(probe,dict): keys=sorted(probe.keys())[:40]
            idx.append((f,len(data),kind,','.join(keys))); ok+=1
        except Exception as e:
            fail+=1; idx.append((f,len(data),'NRBF-FAIL '+str(e)[:80],''))
    else:
        try: txt=data.decode('utf-8')
        except: txt=data.decode('latin-1')
        with open(os.path.join(dst,f+'.txt'),'w') as o: o.write(txt)
        first=txt.split(chr(10))[0][:120]
        idx.append((f,len(data),'TEXT',first)); text+=1
    if i%200==0: print('..',i,flush=True)
with open(os.path.join(base,'gamedata_INDEX.md'),'w') as o:
    o.write('# BitLife 3.25.1 content assets (from assets/bin/Data/datapack.unity3d)\n\n')
    o.write('total %d files | binaryformatter %d | text %d | failed %d\n\n'%(len(files),ok,text,fail))
    o.write('| file | bytes | kind | fields / first line |\n|---|---|---|---|\n')
    for f,n,k,extra in idx:
        o.write('| %s | %d | %s | %s |\n'%(f,n,k,extra.replace('|','/')[:160]))
print('DONE ok',ok,'text',text,'fail',fail,flush=True)

