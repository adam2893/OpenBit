#!/usr/bin/env python3
"""il2cpp scenario toolkit for BitLife 3.25.1 (metadata v39).

Two subcommands, both driving a frida-server on the emulator through the
exported il2cpp C API (libil2cpp.so exports 241 il2cpp_* symbols, and
il2cpp_method_get_pointer is NOT among them, so a method address is read from
MethodInfo+0x00).

  dump   enumerate every Scenario* class -> method -> arm64 address as JSON
  watch  hook Scenario*.RefreshScenario / ChoiceSelectedAtIndex and print the
         Il2CppString fields stored on the scenario object, i.e. the card title,
         prompt body and outcome line of every scenario that fires while the
         watcher is attached (deduped per class+method+text).

Usage:
  adb -s 127.0.0.1:16384 forward tcp:27042 tcp:27042
  adb -s 127.0.0.1:16384 shell 'nohup /data/local/tmp/fsrv16 -l 0.0.0.0:27042 >/tmp/fsrv.log 2>&1 &'
  python3 il2cpp_scenarios.py dump  --out scenario_methods.json
  python3 il2cpp_scenarios.py watch --prefix ScenarioCasino --prefix ScenarioAuction

Il2CppString layout (arm64): [klass* 8][monitor* 8][length i32 @0x10][utf16 @0x14].
"""
import argparse, json, sys, time

import frida

ADDR = "127.0.0.1:27042"

PRELUDE = r"""
const G = n => Module.getExportByName('libil2cpp.so', n);
const domain_get = new NativeFunction(G('il2cpp_domain_get'), 'pointer', []);
const domain_get_assemblies = new NativeFunction(G('il2cpp_domain_get_assemblies'), 'pointer', ['pointer','pointer']);
const assembly_get_image = new NativeFunction(G('il2cpp_assembly_get_image'), 'pointer', ['pointer']);
const image_get_class_count = new NativeFunction(G('il2cpp_image_get_class_count'), 'uint', ['pointer']);
const image_get_class = new NativeFunction(G('il2cpp_image_get_class'), 'pointer', ['pointer','uint']);
const class_get_name = new NativeFunction(G('il2cpp_class_get_name'), 'pointer', ['pointer']);
const class_get_namespace = new NativeFunction(G('il2cpp_class_get_namespace'), 'pointer', ['pointer']);
const class_get_methods = new NativeFunction(G('il2cpp_class_get_methods'), 'pointer', ['pointer','pointer']);
const method_get_name = new NativeFunction(G('il2cpp_method_get_name'), 'pointer', ['pointer']);

function eachScenarioClass(cb) {
  const dom = domain_get();
  if (dom.isNull()) return 0;
  const cnt = Memory.alloc(4);
  const asms = domain_get_assemblies(dom, cnt);
  const n = cnt.readU32();
  let classes = 0;
  for (let i = 0; i < n; i++) {
    const img = assembly_get_image(asms.add(i * Process.pointerSize).readPointer());
    if (img.isNull()) continue;
    const cc = image_get_class_count(img);
    for (let j = 0; j < cc; j++) {
      const k = image_get_class(img, j);
      if (k.isNull()) continue;
      const kn = class_get_name(k).readUtf8String();
      if (!kn || kn.indexOf('Scenario') !== 0) continue;
      classes++;
      cb(k, kn);
    }
  }
  return classes;
}
"""

DUMP_JS = PRELUDE + r"""
const out = [];
eachScenarioClass(function (k, kn) {
  const ns = class_get_namespace(k).readUtf8String() || '';
  const iter = Memory.alloc(Process.pointerSize);
  iter.writePointer(NULL);
  const methods = [];
  while (true) {
    const mi = class_get_methods(k, iter);
    if (mi.isNull()) break;
    methods.push({ n: method_get_name(mi).readUtf8String(), a: mi.readPointer().toString() });
  }
  out.push({ ns: ns, cls: kn, methods: methods });
});
send({ count: out.length, classes: out });
"""

WATCH_JS = PRELUDE + r"""
const PREFIXES = __PREFIXES__;
const seen = {};

function readStr(p) {
  try {
    if (p.isNull()) return null;
    const len = p.add(0x10).readS32();
    if (len <= 0 || len > 2000) return null;
    return p.add(0x14).readUtf16String(len);
  } catch (e) { return null; }
}

function dumpThis(tag, cls, thisPtr) {
  if (thisPtr.isNull()) return;
  const found = [];
  for (let off = 0; off < 64 * Process.pointerSize; off += Process.pointerSize) {
    let p;
    try { p = thisPtr.add(off).readPointer(); } catch (e) { continue; }
    const s = readStr(p);
    if (s && /[A-Za-z]/.test(s)) found.push(s);
  }
  if (!found.length) return;
  const key = cls + '|' + tag + '|' + found.join('\u0001');
  if (seen[key]) return;
  seen[key] = 1;
  send({ cls: cls, tag: tag, strings: found });
}

let attached = 0;
eachScenarioClass(function (k, kn) {
  let want = PREFIXES.length === 0;
  for (const p of PREFIXES) { if (kn.indexOf(p) === 0) { want = true; break; } }
  if (!want) return;
  const iter = Memory.alloc(Process.pointerSize);
  iter.writePointer(NULL);
  while (true) {
    const mi = class_get_methods(k, iter);
    if (mi.isNull()) break;
    const mn = method_get_name(mi).readUtf8String();
    if (mn !== 'RefreshScenario' && mn !== 'ChoiceSelectedAtIndex') continue;
    const addr = mi.readPointer();
    if (addr.isNull()) continue;
    try {
      Interceptor.attach(addr, {
        onEnter(args) { this.t = args[0]; },
        onLeave(ret) { try { dumpThis(mn, kn, this.t); } catch (e) {} }
      });
      attached++;
    } catch (e) {}
  }
});
send({ attached: attached });
"""


def attach_device():
    dev = frida.get_device_manager().add_remote_device(ADDR)
    for p in dev.enumerate_processes():
        if 'bitlife' in p.name.lower():
            return dev, p
    raise SystemExit('BitLife process not found (is the game running?)')


def run_js(js, on_payload, timeout=None):
    dev, proc = attach_device()
    sess = dev.attach(proc.pid)
    script = sess.create_script(js)

    def on_message(msg, data):
        if msg['type'] == 'send':
            on_payload(msg['payload'])
        else:
            print('frida error:', msg.get('description'), file=sys.stderr)

    script.on('message', on_message)
    script.load()
    if timeout is None:
        while True:
            time.sleep(1)
    end = time.time() + timeout
    while time.time() < end:
        time.sleep(0.2)


def cmd_dump(args):
    result = {}

    def on_payload(pl):
        result.update(pl)

    run_js(DUMP_JS, on_payload, timeout=120)
    classes = result.get('classes') or []
    total = sum(len(c['methods']) for c in classes)
    with open(args.out, 'w') as f:
        json.dump({'count': len(classes), 'classes': classes}, f, indent=1)
    print(f"{len(classes)} Scenario classes, {total} methods -> {args.out}")


def cmd_watch(args):
    js = WATCH_JS.replace('__PREFIXES__', json.dumps(args.prefix))

    def on_payload(pl):
        if 'attached' in pl:
            print(f"attached {pl['attached']} methods", file=sys.stderr, flush=True)
            return
        print(json.dumps(pl, ensure_ascii=False), flush=True)

    run_js(js, on_payload)


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)
    d = sub.add_parser('dump')
    d.add_argument('--out', default='scenario_methods.json')
    w = sub.add_parser('watch')
    w.add_argument('--prefix', action='append', default=[])
    args = ap.parse_args()
    if args.cmd == 'dump':
        cmd_dump(args)
    else:
        cmd_watch(args)
    return 0


sys.exit(main())
