"""Dump the il2cpp string-literal slot table once, and keep it offline.

Every card's text comes out of a slot: code does adrp/ldr to a cell that holds an
Il2CppString*, filled at il2cpp start-up from the metadata literal table. Reading
one cell at a time needs a Frida attach per card; the slots are contiguous, so one
read of the whole range resolves every literal the binary can ever use.

  python3 tools/literals_dump.py            # read the slots from the running game
  python3 tools/literals_dump.py --show 0x5c46bd8
  python3 tools/literals_dump.py --list | head

Writes tools/lit_cells.json: {cell offset: literal text}. The harness (blh_hold.py)
must be running and the game at any screen -- the slots are filled regardless of
which life is loaded, so no life has to be driven for this one.
"""
import json
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, "tools", "lit_cells.json")
REQ = "/tmp/blh_req."
RES = "/tmp/blh_res."
# The slot table's own footprint, measured from the cells the ported cards use
# (0x5c2xxxx .. 0x5c6xxxx) plus a page of slack either side.
LO = 0x5c00000
HI = 0x5c90000

JS = """
var out = {};
var mods = Process.enumerateModules(), m = null;
for (var i = 0; i < mods.length; i++) if (mods[i].name.indexOf('il2cpp') >= 0) { m = mods[i]; break; }
function isStr(p) {
  try {
    var n = p.add(0x10).readS32();
    if (n < 0 || n > 400) return null;
    var t = p.add(0x14).readUtf16String(n * 2);
    if (t === null) return null;
    for (var i = 0; i < t.length; i++) { var c = t.charCodeAt(i); if (c < 9 || (c > 13 && c < 32)) return null; }
    return t;
  } catch (e) { return null; }
}
rpc.exports = { dump: function (lo, hi) {
  var res = [];
  for (var off = lo; off < hi; off += 8) {
    var p;
    try { p = m.base.add(off).readPointer(); } catch (e) { continue; }
    if (p.isNull()) continue;
    var s = isStr(p);
    if (s === null) { continue; }
    res.push([off, s]);
  }
  return res;
} };
"""


def call(op, args, timeout=120.0):
    rid = "%d%s" % (int(time.time() * 1000), os.getpid())
    with open(REQ + rid + ".json", "w") as fh:
        json.dump({"op": op, "args": args}, fh)
    out = RES + rid + ".json"
    deadline = time.time() + timeout
    while time.time() < deadline:
        if os.path.exists(out):
            try:
                with open(out) as fh:
                    data = json.load(fh)
            except Exception:
                time.sleep(0.05)
                continue
            os.remove(out)
            return data
        time.sleep(0.05)
    return {"error": "timeout"}


def main(argv):
    if argv[:1] == ["--show"]:
        with open(CACHE) as fh:
            cells = json.load(fh)
        for key in argv[1:]:
            k = hex(int(key, 16))
            print("%s -> %r" % (k, cells.get(k, cells.get(key))))
        return 0
    if argv[:1] == ["--list"]:
        with open(CACHE) as fh:
            cells = json.load(fh)
        for k in sorted(cells, key=lambda s: int(s, 16)):
            print("%s\t%r" % (k, cells[k]))
        return 0

    # The slot read has to happen inside the game's own process; blh_hold.py's
    # generic x_ passthrough cannot express "run this script", so the read is done
    # here with a direct Frida attach instead.
    import frida
    pid = int(open("/tmp/blh_pid.txt").read().strip())
    dev = frida.get_device_manager().get_device("127.0.0.1:16384", timeout=10)
    session = dev.attach(pid)
    script = session.create_script(JS)
    script.load()
    rows = script.exports_sync.dump(LO, HI)
    session.detach()
    cells = {hex(int(off)): text for off, text in rows}
    tmp = CACHE + ".tmp"
    with open(tmp, "w") as fh:
        json.dump(cells, fh, indent=0, sort_keys=True, ensure_ascii=False)
    os.replace(tmp, CACHE)
    print("resolved %d slots in [%#x, %#x) -> %s" % (len(cells), LO, HI, CACHE))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
