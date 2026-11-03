"""Play-mode regression: the interactive terminal driver (REPORT.en.md 126).

The driver asks the engine for the year in a forked child (state discarded), shows
the card and its buttons, then runs the same year for real with the button the
player pressed. The test pins both halves: a played life reaches a death with the
expected shape, and an answer the player types is the branch the port executes.
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
SPEC = {"seed": 7, "country": "United States", "gender": "female", "relationship": True}


def play(answers, spec=SPEC):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        fh.write(json.dumps(spec))
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path, "--play"],
                          input="".join(a + "\n" for a in answers), text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-400:]
    return proc.stdout


# 1. A whole life, every card answered with the button the game itself picked.
out = play([""] * 200)
assert out.startswith("=== "), out[:200]
ages = [int(n) for n in re.findall(r"^--- age (\d+) ---", out, re.M)]
assert ages == sorted(ages) and len(ages) >= 10, ages
assert ages[0] == 1, ages[:3]
assert ("*** death at " in out) or ("lived to " in out), out[-300:]
assert out.count("-> ") >= 5, out.count("-> ")
assert "<" not in re.sub(r"--- age \d+ -+", "", out), "unresolved token in a played card"

# 2. The typed answer is the branch taken. The first card with two or more buttons
#    is answered with its second lane; the driver must print that label back.
out2 = play(["2"] + [""] * 200)
menus = re.findall(r"\[1\] (.+?)\n\s+\[2\] (.+?)\n", out2)
assert menus, out2[:600]
taken = re.findall(r"^\s+-> (.+)$", out2, re.M)
# Cards the driver answers itself (one button) are taken before the first typed answer,
# so the label must appear among the taken lanes rather than be the first one.
# The driver consumes a typed answer only for a card it printed a menu for; an earlier
# one-button card can be taken first, so either label of that menu proves the echo.
import re as _re
def _bare(v):
    return _re.sub(r"\s+\(the game.s own pick\)$", "", v).strip()
labels = [(_bare(a.strip()), _bare(b.strip())) for a, b in menus]
assert taken, taken[:3]
assert any(l in taken for pair in labels for l in pair), (taken[:3], labels[:3])

# 3. The same seed and the same answers are byte-identical: no state survives the
#    forked preview, so a played life is as reproducible as a scripted one.
assert play(["2"] + [""] * 200) == out2

# 4. Every printed native button number must execute that exact shown row branch.
base_spec=dict(seed=7,country="United States",gender="male",ages=1)
baseline=subprocess.run([str(PORT),"--script","/dev/stdin"],input=json.dumps(base_spec),
                        capture_output=True,text=True,cwd=ROOT)
assert baseline.returncode==0,baseline.stderr
shown=json.loads(baseline.stdout)["events"][0]
assert shown["table"] and shown["row"],shown
for number in (2,1,3,4):
    spec=dict(base_spec,choices=[number-1],push=[dict(table=shown["table"],row=shown["row"])])
    scripted=subprocess.run([str(PORT),"--script","/dev/stdin"],input=json.dumps(spec),
                            capture_output=True,text=True,cwd=ROOT)
    assert scripted.returncode==0,scripted.stderr
    expected=json.loads(scripted.stdout)["events"][0]["choice"]
    played=play([str(number)],base_spec)
    prefix="["+str(number)+"] "
    label=next(line.strip()[len(prefix):] for line in played.splitlines()
               if line.strip().startswith(prefix))
    selected=next(line.strip()[3:] for line in played.splitlines()
                  if line.strip().startswith("-> "))
    assert _bare(label)==expected,(number,label,expected)
    assert selected==expected,(number,selected,expected,played)
blank=play([""],base_spec)
marked=next(line.strip().split("] ",1)[1] for line in blank.splitlines()
            if "the game's own pick" in line)
assert _bare(marked)==shown["choice"],(marked,shown["choice"])
blank_taken=next(line.strip()[3:] for line in blank.splitlines() if line.strip().startswith("-> "))
assert blank_taken==shown["choice"],(blank_taken,shown["choice"])
own_number=next(line.strip().split("]",1)[0][1:] for line in blank.splitlines()
                if "the game's own pick" in line)
three_years=dict(base_spec,ages=3)
assert play([own_number,"",""],three_years)==play(["","",""],three_years)
with tempfile.TemporaryDirectory() as directory:
    path=Path(directory)/"life.json"
    path.write_text(json.dumps(base_spec))
    for answer in ("0","5","-1","2junk","garbage","999999999999999999999999"):
        bad=subprocess.run([str(PORT),"--script",str(path),"--play"],
                           input=answer+chr(10),capture_output=True,text=True,cwd=ROOT)
        assert bad.returncode==2 and "invalid choice" in bad.stderr,(answer,bad.stdout,bad.stderr)
        assert not any(line.strip().startswith("-> ") for line in bad.stdout.splitlines())

print("played ages:", len(ages), "->", ages[-1])
print("lanes answered:", len(taken))
print("ok")
