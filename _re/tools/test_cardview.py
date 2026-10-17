"""tools/cardview.py must reduce a card without losing the card (REPORT.en.md 140).

The tool filters a disassembly down to the lines that are the card: game-method
calls, their immediate arguments, and writes to the scenario's own fields. The
test pins two cards whose expected calls are already known from their report
sections, so a filter change that hides the logic fails here.
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


# ScenarioLoverBreakUp.RefreshScenario (REPORT.en.md 125): FullName, then
# String.Format, then the status word lowered, then the three field writes.
text = view("0x73f3daf658", 90)
assert "SimPersonName::FullName" in text, text
assert "SimLover::RelationshipStatusString" in text, text
assert "[x19, #0x38]" in text, text          # Title
assert "[x19, #0x48]" in text, text          # Body
assert "[x19, #0x178]" in text, text         # _narrative
kept = [l for l in text.splitlines() if l.strip() and not l.strip().startswith("...")]
assert len(kept) < 20, kept

# ScenarioExLoverPaternityTestCourtOrder.ChoiceSelectedAtIndex (REPORT.en.md 140):
# the refuse lane moves karma and discipline, the other pushes a crime scenario.
text = view("0x73f3deeb2c", 190)
assert "SimPerson::ImpactKarma" in text, text
assert "SimPerson::ImpactDiscipline" in text, text
assert "GetCrimeScenarioForCrimeType" in text, text
assert "LifeLog::Log" in text, text

# And the point of the tool: 190 instructions become a couple of dozen lines.
kept = [l for l in text.splitlines() if l.strip() and not l.strip().startswith("...")]
assert len(kept) < 20, kept

print("cardview keeps the card and drops the codegen: ok")