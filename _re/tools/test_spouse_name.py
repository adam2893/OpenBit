"""400: native surname lanes and same-age spouse follow-up."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "spouse.cpp"
    binary = Path(directory) / "spouse"
    source.write_text(r'''#include "bl/sim.h"
#include <cassert>
#include <string>
using namespace bl;
int main(int argc, char** argv) {
  Sim sim(argv[1], 7);
  for (bool female : {false, true}) {
    Life life; auto& h = life.hero;
    h.firstName="Alex"; h.lastName="Carter"; h.age=30;
    h.relationshipOn=true; h.loverStatus="married"; h.loverName="Sam";
    h.loverFemale=female; h.nameChangeSurname="Stone"; h.nameChangePending=true;
    AgeUp marriage;
    assert(sim.nameChangeMarriageYear(life, marriage, 2));
    assert(h.lastName=="Stone" && h.nameChangeMaiden=="Carter");
    assert(marriage.choice=="All the way: Alex Stone");
    assert(life.pendingCards.size()==1);
    const auto age=h.age; const auto year=life.year;
    AgeUp follow;
    assert(sim.ageUp(life, follow));
    const std::string who=female ? "wife" : "husband";
    const std::string pronoun=female ? "her" : "his";
    assert(follow.table=="registry:ScenarioNameChangeSpouse");
    assert(follow.title=="What's my name again?");
    assert(follow.body=="Your name is now Alex Stone."+std::string(2, char(10))+"Your "+who+
           " has decided to keep "+pronoun+" name, Sam Stone.");
    assert(follow.narrative=="Sam decided to keep "+pronoun+" name.");
    assert(life.pendingCards.empty() && h.age==age && life.year==year);
    assert(h.lastName=="Stone" && h.nameChangeSurname=="Stone");
    assert(!sim.nameChangeMarriageYear(life, marriage, 2));
  }
  Life life; auto& h=life.hero; h.firstName="Alex"; h.lastName="Carter";
  h.loverStatus="married"; h.loverName="Sam"; h.nameChangeSurname="Stone";
  h.nameChangePending=true; AgeUp out;
  assert(sim.nameChangeMarriageYear(life,out,1));
  assert(h.lastName=="Carter-Stone" && out.choice=="Change to Alex Carter-Stone");
}
''')
    subprocess.run(["c++", "-std=c++17", "-I"+str(ROOT/"port/include"), str(source),
                    str(ROOT/"port/build/libblport.a"), "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(ROOT)], check=True)
print("ok: 2 spouse genders, same-age one-shot delivery, native marriage lanes")
