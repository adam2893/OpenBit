"""Dump every managed method's native address to tools/symbols.json.

il2cpp keeps the whole map in memory: assemblies -> images -> classes -> methods,
each MethodInfo carrying its own native entry point. Walking it once gives an
addr -> "Namespace.Class::Method" table for the *whole* game, not just the
scenario classes il2cpp_dump.txt happens to list, which is what tools/disasm.py
needs to name every bl target in a card.

  python3 tools/symbols_dump.py            # walk the running game
  python3 tools/symbols_dump.py --count
  python3 tools/symbols_dump.py --lookup 0x73f32ab2a8

The harness (blh_hold.py) must be running; the game can sit on any screen.
"""
import json
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "tools", "symbols.json")
# The module's own load base in the disassembly's address space (the same constant
# the cell arithmetic in the decode tools uses): a static address from the .so and
# a module-relative offset differ by exactly this.
STATIC_BASE = 0x73F049A000

JS = r"""
var M = null, mods = Process.enumerateModules();
for (var i = 0; i < mods.length; i++) if (mods[i].name.indexOf('il2cpp') >= 0) { M = mods[i]; break; }
function exp(name) {
  var p = Module.findExportByName(M.name, name);
  if (p === null) throw new Error('missing export ' + name);
  return p;
}
var api = {
  domain_get:            new NativeFunction(exp('il2cpp_domain_get'), 'pointer', []),
  domain_get_assemblies: new NativeFunction(exp('il2cpp_domain_get_assemblies'), 'pointer', ['pointer', 'pointer']),
  assembly_get_image:    new NativeFunction(exp('il2cpp_assembly_get_image'), 'pointer', ['pointer']),
  image_get_class_count: new NativeFunction(exp('il2cpp_image_get_class_count'), 'uint32', ['pointer']),
  image_get_class:       new NativeFunction(exp('il2cpp_image_get_class'), 'pointer', ['pointer', 'uint32']),
  class_get_name:        new NativeFunction(exp('il2cpp_class_get_name'), 'pointer', ['pointer']),
  class_get_namespace:   new NativeFunction(exp('il2cpp_class_get_namespace'), 'pointer', ['pointer']),
  class_get_methods:     new NativeFunction(exp('il2cpp_class_get_methods'), 'pointer', ['pointer', 'pointer']),
  method_get_name:       new NativeFunction(exp('il2cpp_method_get_name'), 'pointer', ['pointer'])
};
rpc.exports = { walk: function () {
  var out = [];
  var n = Memory.alloc(8);
  n.writeU64(0);
  var asms = api.domain_get_assemblies(api.domain_get(), n);
  var count = n.readU64().toNumber();
  for (var a = 0; a < count; a++) {
    var img = api.assembly_get_image(asms.add(a * Process.pointerSize).readPointer());
    if (img.isNull()) continue;
    var cn = api.image_get_class_count(img);
    for (var c = 0; c < cn; c++) {
      var k = api.image_get_class(img, c);
      if (k.isNull()) continue;
      var ns = api.class_get_namespace(k).readCString() || '';
      var nm = api.class_get_name(k).readCString() || '';
      var cls = ns ? (ns + '.' + nm) : nm;
      var it = Memory.alloc(8);
      it.writeU64(0);
      var m;
      while (!(m = api.class_get_methods(k, it)).isNull()) {
        var fn = m.readPointer();
        if (fn.isNull()) continue;
        // Module-relative, so the table is independent of where the loader put
        // the library this run: static address = offset + STATIC_BASE.
        out.push([fn.sub(M.base).toString(), cls + '::' + (api.method_get_name(m).readCString() || '')]);
      }
    }
  }
  return out;
} };
"""


def main(argv):
    if argv[:1] == ["--count"]:
        with open(OUT) as fh:
            print(len(json.load(fh)))
        return 0
    if argv[:1] == ["--lookup"]:
        with open(OUT) as fh:
            table = json.load(fh)
        for key in argv[1:]:
            want = int(key, 16)
            off = want - STATIC_BASE if want > STATIC_BASE else want
            print("%s -> %s" % (key, table.get(hex(off))))
        return 0

    import frida
    pid = int(open("/tmp/blh_pid.txt").read().strip())
    dev = frida.get_device_manager().get_device("127.0.0.1:16384", timeout=10)
    session = dev.attach(pid)
    script = session.create_script(JS)
    script.load()
    started = time.time()
    rows = script.exports_sync.walk()
    session.detach()
    table = {addr: name for addr, name in rows}
    tmp = OUT + ".tmp"
    with open(tmp, "w") as fh:
        json.dump(table, fh, indent=0, sort_keys=True, ensure_ascii=False)
    os.replace(tmp, OUT)
    print("methods %d in %.1fs -> %s" % (len(table), time.time() - started, OUT))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
