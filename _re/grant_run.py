import frida, sys, subprocess, time, json
DEV = "127.0.0.1:16384"
ADB = "/opt/homebrew/bin/adb"
def pid_of():
    o = subprocess.check_output([ADB,"-s",DEV,"shell","ps -A"]).decode()
    for line in o.splitlines():
        if "com.candywriter.bitlife" in line and ":ssd" not in line:
            return int(line.split()[1])
    return None
pid = pid_of()
print("pid", pid)
if not pid: sys.exit(1)
sess = frida.get_device(DEV).attach(pid)
sc = sess.create_script(open("/tmp/grant.js").read())
sc.on("message", lambda m,d: print("[js]", m.get("payload") if m.get("type")=="send" else m))
sc.load()
print(sc.exports_sync.run())