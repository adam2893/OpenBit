"""Capture real card text from the running game into tools/cards.json.

Why this exists: decoding a card from the binary only tells you which cell holds a
literal; the formatted sentence -- the thing the port has to reproduce -- is what
the game builds at run time. Reading it out of the device one card at a time costs
a harness restart each time, so this tool batches: one request per class against
the already-running harness, results cached in tools/cards.json and reusable
offline by every later decode.

  python3 tools/cards_capture.py ScenarioLoverPrude ScenarioLoverCounselingPush
  python3 tools/cards_capture.py --show ScenarioLoverPrude
  python3 tools/cards_capture.py --list

The harness is blh_hold.py (Frida, MuMu at 127.0.0.1:16384); it must be running:
  cd /Users/Adam/Documents/bitlife/_re && python3 blh_hold.py &
Classes whose .ctor takes an argument are skipped with a note -- probeCard only
knows how to fill _friend, and those cards need their own pointer.
"""
import json
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, "tools", "cards.json")
REQ = "/tmp/blh_req."
RES = "/tmp/blh_res."


def call(op, args, timeout=45.0):
    rid = "%d%s" % (int(time.time() * 1000), os.getpid())
    path = REQ + rid + ".json"
    with open(path, "w") as fh:
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
    return {"error": "timeout waiting for the harness (%s)" % op}


def load_cache():
    try:
        with open(CACHE) as fh:
            return json.load(fh)
    except Exception:
        return {}


def save_cache(cache):
    tmp = CACHE + ".tmp"
    with open(tmp, "w") as fh:
        json.dump(cache, fh, indent=1, sort_keys=True, ensure_ascii=False)
    os.replace(tmp, CACHE)


def summarise(cls, rec):
    if rec.get("error"):
        return "%s: %s" % (cls, rec["error"])
    if rec.get("err"):
        return "%s: %s" % (cls, rec["err"])
    lines = [cls + ("" if rec.get("refreshExc") in (None, "null") else
                    "  (RefreshScenario threw: %s)" % rec.get("refreshExc"))]
    for name, text in sorted((rec.get("strings") or {}).items()):
        if text and text != "null":
            lines.append("  %-14s %r" % (name, text))
    labels = [c.get("label") for c in (rec.get("choices") or []) if c.get("label")]
    if labels:
        lines.append("  %-14s %s" % ("buttons", json.dumps(labels, ensure_ascii=False)))
    return "\n".join(lines)


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    cache = load_cache()
    if argv[0] == "--list":
        for cls in sorted(cache):
            print(cls)
        return 0
    if argv[0] == "--show":
        for cls in argv[1:]:
            print(summarise(cls, cache.get(cls, {"error": "not cached"})))
        return 0
    if argv[0] == "--all":
        argv = [c.replace("Scenario", "", 1) for c in sorted(cache)]
        if not argv:
            print("cache is empty")
            return 0
    got = 0
    for cls in argv:
        if not cls.startswith("Scenario"):
            cls = "Scenario" + cls
        # fc = friendCard(cls, gender, minAge, maxAge, null): queue the build on the
        # game's main thread, then fcres collects it. A ctor() card needs no person.
        queued = call("x_fc", [cls, 1, 18, 40])
        if queued.get("error"):
            cache[cls] = {"err": queued["error"]}
            print(summarise(cls, cache[cls]))
            got += 1
            continue
        rec = {}
        for _ in range(60):
            time.sleep(0.5)
            res = call("x_fcres", [])
            card = (res or {}).get("card")
            if card:
                rec = card
                break
        if "strings" not in rec and "choices" not in rec:
            # A failed capture is not evidence: show it, do not cache it, so a
            # later run with a live life overwrites nothing real.
            print(summarise(cls, rec or {"err": "no result from the game"}))
            got += 1
            continue
        cache[cls] = rec
        print(summarise(cls, rec))
        got += 1
    save_cache(cache)
    print("\ncached %d card(s) -> %s" % (got, CACHE))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
