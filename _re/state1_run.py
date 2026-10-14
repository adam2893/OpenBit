import frida, sys, json
pid = int(sys.argv[1])
dev = frida.get_device("127.0.0.1:16384", timeout=10)
s = dev.attach(pid)
src = open('/tmp/state1.js').read()
script = s.create_script(src)
msgs = []
script.on('message', lambda m, d: msgs.append(m))
script.load()
classes = [["Game","GameVars"],["","Life"],["","LifeInterface"],["","SimPerson"],["","DataHub"],["Game","MonetizationVars"],["","SimPlace"]]
res = script.exports_sync.probe(json.dumps(classes))
print(json.dumps(res, indent=1)[:6000])
for m in msgs[:5]:
    print('MSG', str(m)[:300])
