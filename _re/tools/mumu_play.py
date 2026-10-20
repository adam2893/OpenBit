import os, subprocess, time, sys
sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parents[1]))
import blhrpc
DEV='127.0.0.1:16384'
def tap(x,y): subprocess.run(['adb','-s',DEV,'shell','input','tap',str(int(x)),str(int(y))],capture_output=True)
def swipe(x1,y1,x2,y2,ms=300):
    subprocess.run(['adb','-s',DEV,'shell','input','swipe',str(x1),str(y1),str(x2),str(y2),str(ms)],capture_output=True)
def logs_age():
    ls=blhrpc.rpc('logs', n=60) or []
    best=0
    for l in ls:
        if isinstance(l, dict):
            a=l.get('age')
            if isinstance(a,(int,float)) and a>best: best=int(a)
    return best

N=int(sys.argv[1]) if len(sys.argv)>1 else 140
for i in range(N):
    # A stray tap can open a link in the browser; bring the game back every few
    # rounds so the Age button is what the taps actually hit (REPORT.en.md 278).
    if i % 12 == 0:
        os.system('adb -s 127.0.0.1:16384 shell am start -n '
                  'com.candywriter.bitlife/com.unity3d.player.UnityPlayerActivity '
                  '>/dev/null 2>&1')
    tap(723,1764); time.sleep(0.55)
    for y in range(1080,1861,40):
        tap(720,y); time.sleep(0.06)
    # Modal dismissals: every IAP/metric modal puts its X on the same vertical strip
    # (device x ~300, y anywhere from 300 to 1100), so walk the strip and be done
    # with knowing each popup's height.
    for y in range(300, 1901, 40):
        tap(300, y); time.sleep(0.06)
    tap(1332, 140); time.sleep(0.12)  # bundle ad corner
    if i%6==5:
        swipe(720,1700,720,1150,250); time.sleep(0.3)
    if i%10==0:
        print('i',i,'log_age',logs_age(),flush=True)
print('done', logs_age(), flush=True)
