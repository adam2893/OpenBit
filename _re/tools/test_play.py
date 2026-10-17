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
first_menu = re.search(r"\[1\] (.+?)\n\s+\[2\] (.+?)\n", out2)
assert first_menu, out2[:600]
want = first_menu.group(2).strip()
taken = re.findall(r"^\s+-> (.+)$", out2, re.M)
assert taken and taken[0] == want, (taken[:2], want)

# 3. The same seed and the same answers are byte-identical: no state survives the
#    forked preview, so a played life is as reproducible as a scripted one.
assert play(["2"] + [""] * 200) == out2

print("played ages:", len(ages), "->", ages[-1])
print("lanes answered:", len(taken))
print("ok")
