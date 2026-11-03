"""401: native dash ordering, guaranteed mutual naming and queued-state recovery."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "hyphen.cpp"
    binary = Path(directory) / "hyphen"
    source.write_text(r'''#include "bl/sim.h"
#include <cassert>
#include <string>
using namespace bl;
int main(int argc, char** argv) {
  Sim sim(argv[1], 7);
  for (int craziness : {0, 19, 20, 69, 70, 100}) {
    Life life; auto& h=life.hero;
    h.firstName="Alex"; h.lastName="Carter-Reed"; h.age=30;
    h.relationshipOn=true; h.loverStatus="married"; h.loverName="Sam";
    h.loverFemale=true; h.loverCraziness=craziness;
    h.nameChangeSurname="Stone-Jones"; h.nameChangePending=true;
    AgeUp marriage;
    assert(sim.nameChangeMarriageYear(life,marriage,1));
    assert(h.lastName=="Stone-Carter" && h.nameChangeMaiden=="Carter-Reed");
    assert(marriage.choice=="Change to Alex Stone-Carter");
    assert(h.nameChangeSurname=="Stone-Jones");
    if (craziness>=70) {
      // Outcome needs missing willingness inputs; do not fabricate one.
      assert(life.pendingCards.empty());
      continue;
    }
    assert(life.pendingCards.size()==1);
    Life restored=life;
    restored.hero.lastName="Changed";  // another queued rename must not rewrite the captured spouse name
    AgeUp follow;
    assert(sim.ageUp(life,follow));
    assert(follow.table=="registry:ScenarioNameChangeSpouse");
    assert(follow.title=="Legally bound");
    assert(follow.body=="Your wife has also decided to change her name to Sam Stone-Carter.");
    assert(follow.narrative=="Sam also decided to hyphenate her last name.");
    assert(h.nameChangeSurname=="Stone-Carter" && h.lastName=="Stone-Carter");
    assert(h.nameChangeMaiden=="Carter-Reed");
    assert(life.pendingCards.empty() && h.age==30 && life.year==0);
    AgeUp replay;
    assert(sim.ageUp(restored,replay));
    assert(replay.body==follow.body && replay.narrative==follow.narrative);
    assert(restored.hero.nameChangeSurname=="Stone-Carter" && restored.pendingCards.empty());
    assert(restored.hero.lastName=="Changed");
    assert(!sim.nameChangeMarriageYear(life,marriage,1));
  }
  Life male; auto& h=male.hero;
  h.firstName="Alex"; h.lastName="Carter-Reed";
  h.loverStatus="married"; h.loverName="Sam"; h.loverFemale=false;
  h.nameChangeSurname="Stone-Jones"; h.nameChangePending=true; AgeUp out;
  assert(sim.nameChangeMarriageYear(male,out,1));
  assert(h.lastName=="Carter-Stone" && out.choice=="Change to Alex Carter-Stone");
  assert(male.pendingCards.empty());
  male.pendingCards.emplace_back(kPendingSpouseName, 1);  // malformed capture must not erase a name
  assert(!sim.ageUp(male,out));
  assert(h.nameChangeSurname=="Stone-Jones" && h.lastName=="Carter-Stone");
  assert(male.pendingCards.empty() && h.age==0);
}
''')
    subprocess.run(["c++", "-std=c++17", "-I"+str(ROOT/"port/include"), str(source),
                    str(ROOT/"port/build/libblport.a"), "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(ROOT)], check=True)
print("ok: native surname order/truncation, craziness boundaries, mutual name mutation, restored queue")
