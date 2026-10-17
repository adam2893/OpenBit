"""The cardview label-dispatch rule (REPORT.en.md 143).

ChoiceSelectedAtIndex dispatches on the choice's text in several cards, and those
comparisons are String methods -- framework, and therefore dropped by the noise
filter. That was a real blind spot: the paparazzi decode in 139 had to find them
by hand. The rule now keeps them and marks them.
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VIEW = ROOT / "tools" / "cardview.py"


def view(addr, count):
    proc = subprocess.run([sys.executable, str(VIEW), addr, str(count)],
                          text=True, capture_output=True, cwd=str(ROOT))
    assert proc.returncode == 0, proc.stderr[-300:]
    return proc.stdout


# ScenarioFamePaparazzi.ChoiceSelectedAtIndex: four text comparisons, which are
# the card's whole dispatch ("attack"/"pose"/"cover") plus its row test.
text = view("0x73f3bd7e28", 244)
marked = [l for l in text.splitlines() if "label dispatch" in l]
assert len(marked) >= 3, text
assert any("System.String::Contains" in l for l in marked), marked

# And the card's own calls are still there beside them.
assert "SimPerson::ImpactDiscipline" in text, text
assert "ScenarioCustomAttack::.ctor" in text, text

print("label dispatch is visible: %d comparisons" % len(marked))