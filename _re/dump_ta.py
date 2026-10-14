import UnityPy, os, collections
base='/Users/Adam/Documents/bitlife/_re'
out=os.path.join(base,'gamedata'); os.makedirs(out, exist_ok=True)
env=UnityPy.load(os.path.join(base,'assets/bin/Data/datapack.unity3d'))
n=0; total=0; enc=0
for o in env.objects:
    if o.type.name!='TextAsset': continue
    d=o.read()
    name=getattr(d,'m_Name','unknown')
    script=getattr(d,'m_Script',None)
    if script is None: continue
    if isinstance(script,str): b=script.encode('utf-8','surrogateescape')
    else: b=bytes(script)
    safe=name.replace('/','_')
    with open(os.path.join(out,safe),'wb') as f: f.write(b)
    n+=1; total+=len(b)
    if name.endswith('-enc') or name.endswith('_enc'): enc+=1
print('saved',n,'assets',total,'bytes; enc-named',enc)
for probe in ['genesis_object_subtypes','genesis_master_tags_reference_2','genesis_lookup_emoji_visuals','a16_countries-enc']:
    p=os.path.join(out,probe)
    if os.path.exists(p):
        d=open(p,'rb').read(220)
        print('==',probe,len(open(p,'rb').read()),repr(d[:200]))

