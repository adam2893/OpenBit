#!/usr/bin/env python3
"""God Mode edits (REPORT.en.md 312): SaveChangesAfterEditingPerson 0x73f338e424.

Checks raw slider writes, the native sexuality slider map and orientation bands,
names, the two log pools, and the refusals for attributes the port does not model.
"""
import json, os, subprocess, sys, tempfile

RE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLSIM = os.environ.get("BLSIM", os.path.join(RE, "port", "build", "blsim"))
SELF = {"I don't know who I am anymore.", "Something strange came over me.",
        "I may have been abducted by aliens."}
OTHER = ["{0} seems different.", "There is something off about {0} lately.",
         "{0} seems like a whole different person.", "Something happened to {0}.",
         "I'm not sure what's gotten into {0} lately."]


def run(spec):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(spec, f)
    p = subprocess.run([BLSIM, "--script", f.name], cwd=RE, capture_output=True, text=True)
    os.unlink(f.name)
    return p.returncode, (json.loads(p.stdout) if p.returncode == 0 else None), p.stderr


def f32(x):
    import struct
    return struct.unpack("<f", struct.pack("<f", x))[0]


def orientation(s):
    a, b, c = f32(0.45454547), f32(0.75757575), f32(1.8181819)
    g = f32(s * a) if s < 34 else f32(f32((s - 33) * b) + 15) if s < 68 else f32(f32((s - 67) * c) + 40)
    h = int(min(g, 100.0))
    return "straight" if h <= 15 else "bi" if h <= 40 else "gay"


# 1. raw stat writes and the orientation map across the whole slider.
bands = {}
for s in range(0, 101):
    rc, d, err = run({"seed": 3, "ages": 0, "push": [{"godMode": "hero", "godHappiness": s,
                     "godHealth": 100 - s, "godSmarts": s, "godLooks": 7, "godKarma": s,
                     "godDiscipline": 1, "godSexuality": s}]})
    assert rc == 0, err
    st = d["final"]["stats"]
    assert st == {"happiness": s, "health": 100 - s, "smarts": s, "looks": 7, "karma": s,
                  "discipline": 1}, (s, st)
    assert d["final"]["sexuality"] == orientation(s), (s, d["final"]["sexuality"])
    bands.setdefault(orientation(s), []).append(s)
    ev = d["events"][0]
    assert ev["title"] == "God Mode" and len(ev["logLines"]) == 1 and ev["logLines"][0] in SELF, ev
assert set(bands) == {"straight", "bi", "gay"}, bands
assert bands["straight"][-1] == 34 and bands["bi"][0] == 35 and bands["bi"][-1] == 67, bands

# 2. names and the other-person pool across seeds.
seen_self, seen_other = set(), set()
for seed in range(1, 41):
    rc, d, err = run({"seed": seed, "ages": 0, "push": [
        {"godMode": "family:0", "godFirst": "Bob", "godLast": "Ross"},
        {"godMode": "hero", "godLooks": 50}]})
    assert rc == 0, err
    fam = d["final"]["family"][0]
    assert fam["name"] == "Bob", fam
    line = d["events"][0]["logLines"][0]
    who = "my " + fam["relation"]
    hit = [o for o in OTHER if o.replace("{0}", who)[0].upper() + o.replace("{0}", who)[1:] == line]
    assert hit, line
    seen_other.add(hit[0])
    seen_self.add(d["events"][1]["logLines"][0])
assert seen_other == set(OTHER) and seen_self == SELF, (seen_other, seen_self)

# 3. refusals: unmodelled attributes, relatives' attributes, bad slider, bad person, empty edit.
for push in ({"godMode": "hero", "godWillpower": 5}, {"godMode": "family:0", "godHealth": 5},
             {"godMode": "hero", "godLooks": 101}, {"godMode": "family:99", "godFirst": "X"},
             {"godMode": "nobody", "godFirst": "X"}, {"godMode": "hero"}):
    rc, d, err = run({"seed": 1, "ages": 0, "push": [push]})
    assert rc != 0 and "godMode" in err, (push, rc, err)

# 4. an edit mid-life leaves later years running.
rc, d, err = run({"seed": 5, "ages": 10, "push": [{"godMode": "hero", "godSmarts": 100}]})
assert rc == 0 and d["final"]["age"] == 10, err
print("god mode: slider bands", {k: (v[0], v[-1]) for k, v in bands.items()},
      "other lines", len(seen_other), "self lines", len(seen_self))
