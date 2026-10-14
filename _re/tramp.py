import json, subprocess, re
DEV="127.0.0.1:16384"; ADB="/opt/homebrew/bin/adb"
d=json.load(open("/Users/Adam/Documents/bitlife/_re/hookbytes.json"))
o=subprocess.check_output([ADB,"-s",DEV,"shell","ps -A"]).decode()
pid=[int(l.split()[1]) for l in o.splitlines() if "com.candywriter.bitlife" in l and ":ssd" not in l][0]
maps=subprocess.check_output([ADB,"-s",DEV,"shell","cat /proc/%d/maps"%pid]).decode()
segs=[]
for line in maps.splitlines():
    m=re.match(r"([0-9a-f]+)-([0-9a-f]+) (....) ([0-9a-f]+) .* (\S+)$", line)
    if m: segs.append((int(m.group(1),16), int(m.group(2),16), m.group(3), m.group(5)))
BASE=0x73d8402000
def inmap(a):
    for s,e,p,n in segs:
        if s <= a < e: return (hex(s),hex(e),p,n)
    return None
tgts=[]
for off,f,live in d["diff"]:
    w0=int.from_bytes(bytes.fromhex(live)[:4],"little")
    op=w0>>26
    imm=w0 & 0x3FFFFFF
    if imm & 0x2000000: imm -= 0x4000000
    site=BASE+int(off,16)
    tgt=site+imm*4
    tgts.append((off,hex(op),hex(site),hex(tgt),inmap(tgt)))
print("total", len(tgts))
for t in tgts[:8]: print(t)
import collections
c=collections.Counter([t[4][3] if t[4] else "UNMAPPED" for t in tgts])
print("opcodes", collections.Counter([t[1] for t in tgts]))
print("target modules", c)
json.dump([list(map(str,t)) for t in tgts], open("/Users/Adam/Documents/bitlife/_re/hooktramp.json","w"))