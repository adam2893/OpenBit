import json, subprocess, re, frida
DEV="127.0.0.1:16384"; ADB="/opt/homebrew/bin/adb"
tg=json.load(open("/Users/Adam/Documents/bitlife/_re/hooktramp.json"))
o=subprocess.check_output([ADB,"-s",DEV,"shell","ps -A"]).decode()
pid=[int(l.split()[1]) for l in o.splitlines() if "com.candywriter.bitlife" in l and ":ssd" not in l][0]
maps=subprocess.check_output([ADB,"-s",DEV,"shell","cat /proc/%d/maps"%pid]).decode()
lines=[l for l in maps.splitlines()]
def overlaps(a):
    out=[]
    for l in lines:
        m=re.match(r"([0-9a-f]+)-([0-9a-f]+) (....)", l)
        if not m: continue
        s,e=int(m.group(1),16),int(m.group(2),16)
        if s<=a<e: out.append(l)
    return out
addrs=[t[3] for t in tg[:3]]
sess=frida.get_device(DEV).attach(pid)
sc=sess.create_script(open("/tmp/rd48.js").read()); sc.load()
res=sc.exports_sync.readat(addrs)
for a in addrs:
    print("target", a, res.get(a))
    for l in overlaps(int(a,16)): print("   map:", l[:140])
