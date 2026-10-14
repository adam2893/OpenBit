import frida, subprocess, json, re
DEV="127.0.0.1:16384"; ADB="/opt/homebrew/bin/adb"
offs=json.load(open("/tmp/real_offs.json"))
if isinstance(offs, dict): offs=[int(k,16) if isinstance(k,str) else int(k) for k in offs.keys()]
else: offs=[int(x,16) if isinstance(x,str) else int(x) for x in offs]
o=subprocess.check_output([ADB,"-s",DEV,"shell","ps -A"]).decode()
pid=[int(l.split()[1]) for l in o.splitlines() if "com.candywriter.bitlife" in l and ":ssd" not in l][0]
sess=frida.get_device(DEV).attach(pid)
sc0=sess.create_script('rpc.exports={base:function(){return String(Process.findModuleByName("libil2cpp.so").base);}};')
sc0.load()
BASE=int(sc0.exports_sync.base(),16)
maps=subprocess.check_output([ADB,"-s",DEV,"shell","cat /proc/%d/maps"%pid]).decode()
segs=[]
for line in maps.splitlines():
    m=re.match(r"([0-9a-f]+)-([0-9a-f]+) (....) ([0-9a-f]+) .* (\S+)$", line)
    if not m or not m.group(5).endswith("libil2cpp.so"): continue
    a,b,fo=int(m.group(1),16), int(m.group(2),16), int(m.group(4),16)
    if BASE <= a < BASE+0x8000000: segs.append((a,b,fo,m.group(3)))
print("base", hex(BASE), "segments", len(segs))
print("RWX(patched) pages:", [(hex(a),hex(b),hex(fo)) for a,b,fo,p in segs if "w" in p and "x" in p])
def to_addr(off):
    for a,b,fo,p in segs:
        if fo <= off < fo + (b-a): return a + (off-fo)
    return None
targets=[to_addr(off) for off in offs]
sc=sess.create_script(open("/tmp/hookbytes2.js").read())
sc.on("message", lambda m,d: print("[js]", m.get("payload")))
sc.load()
res=sc.exports_sync.readat([hex(t) if t else "0x0" for t in targets])
f=open("/Users/Adam/Documents/bitlife/_re/lib/arm64-v8a/libil2cpp.so","rb")
same=0; diff=[]; err=0
for off,ad in zip(offs,targets):
    live=res.get(hex(ad),"ERR") if ad else "NOMAP"
    if live in ("ERR","NOMAP"): err+=1; continue
    f.seek(off); fb=f.read(16).hex()
    if fb==live: same+=1
    else: diff.append((hex(off), fb, live))
print("sites",len(offs),"identical",same,"differ",len(diff),"unmapped",err)
for d in diff[:25]: print("  off",d[0],"file",d[1],"live",d[2])
json.dump({"diff":diff,"same":same,"total":len(offs),"unmapped":err}, open("/Users/Adam/Documents/bitlife/_re/hookbytes.json","w"))