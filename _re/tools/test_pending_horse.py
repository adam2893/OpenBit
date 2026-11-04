"""413: a naturally queued horse picker honors every explicit public option."""
import subprocess
import tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "pending_horse.cpp"
    binary = Path(directory) / "pending_horse"
    source.write_text(r'''#include "bl/sim.h"
#include "bl/script.h"
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace bl;
void apply(Sim& sim, Life& life, const AgeUp& ev) {
  Hero& h = life.hero;
  h.karma += ev.karma; h.discipline += ev.discipline;
  h.happiness += ev.happiness; h.health += ev.health - ev.ailmentCost;
  h.smarts += ev.smarts; h.looks += ev.looks;
  h.fame = std::clamp(h.fame + float(ev.fame), 0.0f, 100.0f);
  for (int* stat : {&h.karma, &h.discipline, &h.happiness, &h.health, &h.smarts, &h.looks})
    *stat = std::clamp(*stat, 0, 100);
  std::string type, sentence;
  if (sim.checkDeath(life, type, sentence)) life.alive = false;
}
int main(int argc,char** argv) {
  std::vector<std::string> dealt;
  for(int lane : {0,1,2,3,4,-1}) {
    Sim sim(argv[1],5);Life life;std::string error,hero;std::vector<std::string> birth;
    assert(sim.load(&error));ScriptSpec spec;spec.seed=5;spec.country="United States";
    assert(setupLife(sim,spec,life,&hero,&birth,&error));
    // Bounded natural discovery: seed5,80 ordinary calls,age46. No injection.
    for(int i=0;i<80;++i){AgeUp ev;assert(life.alive);assert(sim.ageUp(life,ev));apply(sim,life,ev);}
    assert(life.alive && life.hero.age==46 && life.hero.money==1052515);
    assert(life.pendingCards.size()==1 && life.pendingCards[0].kind==kPendingHorseBet);
    const int age=life.hero.age,year=life.year;const auto cash=life.hero.money;
    AgeUp pick;assert(sim.ageUp(life,pick,lane));
    assert(pick.table=="a16_horses-enc" && pick.title=="Horse Races" && pick.body=="Pick your horse");
    assert(pick.options.size()==5 && pick.options==life.hero.horseField);
    if(dealt.empty())dealt=pick.options;else assert(dealt==pick.options);
    int index=lane;
    if(index<0){auto it=std::find(pick.options.begin(),pick.options.end(),pick.choice);assert(it!=pick.options.end());index=int(it-pick.options.begin());}
    std::cout<<"forced="<<lane<<" actual="<<pick.choice<<std::endl;
    assert(pick.choice==pick.options[index] && life.hero.horsePicked==pick.choice);
    assert(life.hero.age==age && life.year==year && life.hero.money==cash);
    assert(life.pendingCards.size()==1 && life.pendingCards[0].kind==kPendingHorseOutcome);
    const std::string selected=pick.choice;apply(sim,life,pick);assert(life.alive);
    AgeUp result;assert(sim.ageUp(life,result));
    assert(result.table=="registry:ScenarioHorseRacesOutcome" && result.options.empty());
    assert(result.narrative.find(selected)!=std::string::npos);
    assert(life.hero.age==age && life.year==year && life.pendingCards.empty());
    assert(life.hero.horseField.empty() && life.hero.horseScores.empty() && life.hero.horsePicked.empty() && life.hero.horseWager==0);
    apply(sim,life,result);assert(life.alive);AgeUp next;assert(sim.ageUp(life,next));
    assert(life.hero.age==age+1 && life.year==year+1 && next.table!="registry:ScenarioHorseRacesOutcome");
  }
}
''')
    subprocess.run(["c++", "-std=c++17", "-I"+str(ROOT/"port/include"), str(source), str(ROOT/"port/build/libblport.a"), "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(ROOT)], check=True)
print("ok: natural horse picker all options/default, selected horse, same-year outcome and cleanup")
