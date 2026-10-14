import os, re, json, collections
P="/Users/Adam/Documents/bitlife/_re/gamedata_plain"
files=[]
for fn in os.listdir(P):
    p=os.path.join(P,fn)
    if not os.path.isfile(p): continue
    b=open(p,'rb').read()
    t=b.decode('utf-8','ignore')
    lines=t.count('\n')+1
    # header: first non-comment line
    hdr=''
    for ln in t.split('\n')[:6]:
        if ln.strip() and not ln.startswith('#'):
            hdr=ln.strip(); break
    cols=hdr.count(',')+hdr.count('\t')+1 if hdr else 0
    files.append((fn,len(b),lines,cols,hdr[:110]))
def fam(fn):
    n=fn
    for suf in ('-enc','.csv','.tsv','.txt'):
        if n.endswith(suf): n=n[:-len(suf)]
    m=re.match(r'^(a16|genesis)[-_]([A-Za-z0-9]+)', n)
    if m: return m.group(1)+'/'+m.group(2)
    return re.sub(r'[^A-Za-z].*$','',n)[:14] or 'other'
g=collections.defaultdict(list)
for f in files: g[fam(f[0])].append(f)
out=[]
out.append('# BitLife content catalog (decrypted TextAssets)\n')
out.append('source: assets/bin/Data/datapack.unity3d -> TextAsset -> EncryptionManager.DecodeAndDecryptString(text, com.wtfapps.apollo16)\n')
out.append('files: %d, bytes: %d\n' % (len(files), sum(f[1] for f in files)))
out.append('| family | files | bytes | largest / header |')
out.append('|---|---|---|---|')
for k in sorted(g, key=lambda k:-sum(f[1] for f in g[k])):
    v=sorted(g[k], key=lambda f:-f[1])
    top=v[0]
    out.append('| %s | %d | %d | %s (%d B, %d lines, %d cols) |' % (k, len(v), sum(f[1] for f in v), top[0], top[1], top[2], top[3]))
out.append('\n## per-file detail (files >= 40 KB)\n')
out.append('| file | bytes | lines | cols | header |')
out.append('|---|---|---|---|---|')
for f in sorted(files, key=lambda f:-f[1]):
    if f[1] < 40000: continue
    out.append('| %s | %d | %d | %d | %s |' % (f[0], f[1], f[2], f[3], f[4].replace('|','/')))
open('/Users/Adam/Documents/bitlife/_re/gamedata_CATALOG.md','w').write('\n'.join(out))
print('families', len(g), 'files', len(files))
print('\n'.join(out[:40]))