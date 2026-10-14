import frida, sys, subprocess
DEV = "127.0.0.1:16384"; ADB = "/opt/homebrew/bin/adb"
o = subprocess.check_output([ADB,"-s",DEV,"shell","ps -A"]).decode()
pid = [int(l.split()[1]) for l in o.splitlines() if "com.candywriter.bitlife" in l and ":ssd" not in l][0]
print("pid", pid)
sess = frida.get_device(DEV).attach(pid)
sc = sess.create_script(open("/tmp/grant2.js").read())
sc.on("message", lambda m,d: print("[js]", m.get("payload") if m.get("type")=="send" else m))
sc.load()
try: print(sc.exports_sync.run())
except Exception as e: print("RPC", e)