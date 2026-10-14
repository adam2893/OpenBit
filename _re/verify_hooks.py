import frida, subprocess
DEV="127.0.0.1:16384"; ADB="/opt/homebrew/bin/adb"
o=subprocess.check_output([ADB,"-s",DEV,"shell","ps -A"]).decode()
pid=[int(l.split()[1]) for l in o.splitlines() if "com.candywriter.bitlife" in l and ":ssd" not in l][0]
sc=frida.get_device(DEV).attach(pid).create_script(open("/tmp/verify_hooks.js").read())
sc.on("message", lambda m,d: print("[js]", m.get("payload") if m.get("type")=="send" else m))
sc.load(); print(sc.exports_sync.run())