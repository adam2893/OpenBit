import frida, json
dev = frida.get_device("127.0.0.1:16384", timeout=10)
pid = int(open('/tmp/blh_pid.txt').read().strip()) if __import__('os').path.exists('/tmp/blh_pid.txt') else None
import subprocess
out = subprocess.check_output(["/opt/homebrew/bin/adb","-s","127.0.0.1:16384","shell","ps -A"]).decode()
pid = [int(l.split()[1]) for l in out.splitlines() if 'com.candywriter.bitlife' in l and 'ssd' not in l][0]
s = dev.attach(pid)
sc = s.create_script(open('/tmp/ex.js').read())
sc.load()
print(json.dumps(sc.exports_sync.ex(), indent=1))
