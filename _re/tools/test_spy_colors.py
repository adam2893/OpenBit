"""Secret-agent colour tables wired into the spy card (REPORT.en.md 311).

Each colour must appear inside its native wrapper and obey the native filter
(/tmp/qdir/spy_wrap_findings.txt):
  hack-mission-file      (file type match) and (blank client or match), shown as 'name: colour.'
  hack-mission-briefing  'Reportedly, the file contains X.', exact client type, file type match
  honeypot briefing      '<He/She> says <he/she> wants X.' (unfiltered)
  honeypot approach      'the target prefers lovers who X.' (unfiltered)
  honeypot meeting       "caught the target's eye X" -- Location == mission location or blank
  client-mission-terminated 'Your client, NAME, ' + row, exact mission type, failures only
  stealth alarm          door + three wires (correct + two incorrect) from one row;
                         success text iff the chosen wire contains the correct colour
"""
import csv
import json
import re
import subprocess
import sys
import tempfile
from multiprocessing import Pool
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DATA = Path("/Users/Adam/Documents/bitlife/_re/gamedata_plain")
if (ROOT / "gamedata_plain" / "a16_secret-agent-stealth-alarm-color-enc").exists():
    DATA = ROOT / "gamedata_plain"
SEEDS = 1200


def table(name):
    with open(DATA / f"a16_secret-agent-{name}-color-enc", newline="") as fh:
        return list(csv.DictReader(fh))


def pattern(snippet, anchored=True):
    """A resolved-text regex for one table snippet: '<a/b>' -> (a|b), tokens -> .+?"""
    out, i = "", 0
    while i < len(snippet):
        if snippet[i] != "<":
            out += re.escape(snippet[i])
            i += 1
            continue
        depth, j = 0, i
        while j < len(snippet):
            depth += snippet[j] == "<"
            depth -= snippet[j] == ">"
            if depth == 0:
                break
            j += 1
        tok = snippet[i + 1:j]
        if "/" in tok and "<" not in tok:
            out += "(?:" + "|".join(re.escape(o) for o in tok.split("/")) + ")"
        else:
            out += ".+?"
        i = j + 1
    return re.compile("^" + out + "$", re.S) if anchored else out


def compiled(rows):
    return [(pattern(r["Snippet"].strip()), r) for r in rows]


HFILE = compiled(table("hack-mission-file"))
HBRIEF = compiled(table("hack-mission-briefing"))
HPB = compiled(table("honeypot-misssion-briefing"))
HAPP = compiled(table("honeypot-approach"))
HMEET = compiled(table("honeypot-meeting-scenario"))
TERM = table("client-mission-terminated")
ALARM = table("stealth-alarm")


def split(cell):
    return [c.strip().lower() for c in cell.split(",") if c.strip()]


def matches(rows, text):
    return [r for rx, r in rows if rx.match(text)]


def mission_type(body):
    if body.startswith("Pull the "):
        return "hacking"
    if body.startswith("Meet "):
        return "honeypot"
    if body.startswith("Get into "):
        return "stealth"
    if body.startswith("The client wants "):
        return "assassination"
    if body.startswith("Tail "):
        return "intel"
    return None


def run(seed):
    spec = {"seed": seed, "ages": 80, "spy": True}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr[-300:])
    cards = []
    for ev in json.loads(proc.stdout)["events"]:
        body = ev.get("body") or ""
        kind = mission_type(body)
        if kind:
            logs = ev.get("logLines") or []
            if isinstance(logs, str):
                logs = [logs]
            cards.append({"seed": seed, "type": kind, "body": body, "choice": ev.get("choice") or "",
                          "log": " ".join(logs), "residue": ev.get("residue") or []})
    return cards


def check(c, bad, seen):
    body, log, kind = c["body"], c["log"], c["type"]
    if c["residue"]:
        bad.append(("residue", c))
    if kind == "hacking":
        m = re.match(r"Pull the (\S+) (audio|video|image|text) file off the .+? drive and leave no trace\. "
                     r"\1: (.+?)\. Reportedly, the file contains (.+?)\.(?: Security on site| The password|$| Cover:)", body)
        if not m:
            bad.append(("hack shape", c))
            return
        seen["hack"] += 1
        ftype, fcol, brief = m.group(2), m.group(3), m.group(4)
        frows = [r for r in matches(HFILE, fcol) + matches(HFILE, fcol + ".")
                 if r["File Type"].strip() == ftype]
        brows = [r for r in matches(HBRIEF, brief) + matches(HBRIEF, brief + ".")
                 if r["File Type"].strip() == ftype]
        okc = any(b["Client Type"].strip() and
                  any(f["Client Type"].strip() in ("", b["Client Type"].strip()) for f in frows)
                  for b in brows)
        if not okc:
            bad.append(("hack filter", c))
    elif kind == "honeypot":
        m = re.search(r"Your client, (.+?), wants you to seduce .+?\. (He|She) says (he|she) wants (.+?)\. "
                      r"According to your client, \1, the target prefers lovers who (.+?)\. ", body)
        if not m or m.group(2).lower() != m.group(3):
            bad.append(("honeypot shape", c))
            return
        seen["honeypot"] += 1
        if not matches(HPB, m.group(4)):
            bad.append(("honeypot briefing row", c))
        if not matches(HAPP, m.group(5)):
            bad.append(("honeypot approach row", c))
        mm = re.search(r"According to (?:his|her) report, (?:he|she) caught the target's eye (.+?)\.(?: |$)", log)
        if mm:
            seen["meeting"] += 1
            loc = re.match(r"Meet .+? at the (.+?) and work|Meet .+? at the (.+?)\.", body)
            where = (loc.group(1) or loc.group(2)) if loc else ""
            rows = matches(HMEET, mm.group(1))
            if not any(r["Location"].strip().lower() in ("", where.lower()) for r in rows):
                bad.append(("meeting filter", c))
    elif kind == "stealth":
        m = re.search(r"Door Color: (.+?)\. (He|She) informed you that (he|she) removed the access point's "
                      r"protective covering and sees three wires: (\w+), (\w+), and (\w+)\. Which should (he|she) cut\?",
                      body)
        if not m:
            bad.append(("stealth shape", c))
            return
        seen["stealth"] += 1
        door, wires = m.group(1).lower(), [w.lower() for w in m.group(4, 5, 6)]
        rows = [r for r in ALARM if door in split(r["Door Color"])]
        fits = [r for r in rows
                if any(w in split(r["Correct Wire Colors"]) and
                       all(o in split(r["Incorrect Wire Colors"]) for k, o in enumerate(wires) if k != i)
                       for i, w in enumerate(wires))]
        if not fits:
            bad.append(("stealth row", c))
            return
        if c["choice"].lower() not in ["the %s wire" % w for w in wires]:
            bad.append(("stealth choice", c))
        s = re.search(r"reported that the security system died the moment (?:he|she) snipped the (\w+) wire\. "
                      r"(?:He|She) gave the all-clear before resuming (?:his|her) mission\.", log)
        correct_cut = any(any(cw in c["choice"].lower() for cw in split(r["Correct Wire Colors"])) for r in fits)
        if s:
            seen["snip"] += 1
            if s.group(1) not in c["choice"].lower():
                bad.append(("snip wire", c))
        if bool(s) != correct_cut:
            bad.append(("snip iff correct", c))
    # client-mission-terminated: failures only, exact mission type.
    # '<client-spy>' -> 'Your client, NAME, ' natively, so the row's own ' said'
    # follows the dict value's ', ' (two spaces, as the native concat gives).
    log = log.replace(body, "")  # the journal echoes the briefing ('Operation X: <body>')
    if "Your client, " in log:
        seen["terminated"] += 1
        hits = [r for r in TERM if re.search(
            r"Your client, [^,]+, " + pattern(r["Snippet"].strip().replace("<client-spy>", "", 1), False), log)]
        if not hits or not any(r["Mission Type"].strip() == kind for r in hits):
            bad.append(("terminated filter", c))
        if kind == "intel":
            bad.append(("terminated intel", c))


if __name__ == "__main__":
    bad = []
    seen = {k: 0 for k in ("hack", "honeypot", "meeting", "stealth", "snip", "terminated")}
    cards = 0
    with Pool(8) as pool:
        for got in pool.imap_unordered(run, range(1, SEEDS + 1), chunksize=16):
            for c in got:
                cards += 1
                check(c, bad, seen)
    print("cards", cards, seen)
    for kind, c in bad[:10]:
        print("BAD", kind, c["seed"], c["body"][:300], "|", c["log"][:300], "|", c["choice"])
    floor = {"hack": 20, "honeypot": 20, "meeting": 5, "stealth": 20, "snip": 3, "terminated": 10}
    low = [k for k, v in floor.items() if seen[k] < v]
    if bad or low:
        print("FAIL", len(bad), "bad;", "too few:", low)
        sys.exit(1)
    print("PASS")
