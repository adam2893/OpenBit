"""412: naturally produced funeral disposition and ashes honor public choices."""
import subprocess
import tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "pending_funeral.cpp"
    binary = Path(directory) / "pending_funeral"
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
int main(int argc, char** argv) {
  // Natural default fixture discovered with seeds1..60, age60,256 calls/life.
  // No queue/due/relation/cash injection: seed15,prefix52,age30,mother.
  for (int lane : {0,1,2,3,-1}) {
    Sim sim(argv[1],15); Life life; std::string error,hero;
    std::vector<std::string> birth;
    assert(sim.load(&error)); ScriptSpec spec; spec.seed=15;spec.country="United States";
    assert(setupLife(sim,spec,life,&hero,&birth,&error));
    for(int i=0;i<52;++i) { AgeUp ev;assert(life.alive);assert(sim.ageUp(life,ev));apply(sim,life,ev); }
    assert(life.alive && life.hero.age==30 && life.hero.money==943702);
    assert(life.pendingCards.size()==1 && life.pendingCards[0].kind==kPendingFuneralPlan);
    assert(life.hero.pendingFuneralPlan && life.hero.funeralRelation=="mother" && life.hero.funeralFemale);
    const int age=life.hero.age,year=life.year;const auto cash=life.hero.money;
    AgeUp out;assert(sim.ageUp(life,out,lane));
    assert(out.table=="registry:ScenarioFuneralPlanning" && out.title=="Funeral Planning");
    assert((out.options==std::vector<std::string>{"Bury her","Cremate her","Have her taxidermied","Donate her body to science"}));
    int selected=lane;
    if(selected<0) { auto it=std::find(out.options.begin(),out.options.end(),out.choice);assert(it!=out.options.end());selected=int(it-out.options.begin()); }
    std::cout<<"planning forced="<<lane<<" actual="<<out.choice<<std::endl;
    assert(out.choice==out.options[selected]);
    const long long costs[]={7710,990,45360,0};
    const char* logs[]={"I planned her funeral and arranged for her to be buried.","I planned her funeral and arranged for her to be cremated.","I planned her funeral and had her taxidermied.","I planned her funeral and donated her body to science."};
    assert(out.narrative==logs[selected] && life.hero.money==cash-costs[selected]);
    assert(life.hero.age==age && life.year==year && !life.hero.pendingFuneralPlan && life.hero.funeralRelation.empty());
    assert(life.pendingCards.size()==(selected==1?1u:0u));
    assert(life.hero.pendingFuneralAshes==(selected==1));
    if(selected==1) {
      assert(life.pendingCards[0].kind==kPendingFuneralAshes && life.hero.ashesRelation=="mother" && life.hero.ashesFemale);
      // Copy BOTH simulator and life at a genuinely producer-created state.
      for(int ash : {0,1,2,-1}) {
        Sim fork=sim;Life branch=life;AgeUp ashes;assert(fork.ageUp(branch,ashes,ash));
        assert(ashes.table=="registry:ScenarioFuneralAshes" && ashes.title=="Ashes");
        assert(ashes.body=="You've received your mother's ashes back from the crematorium.\n\nWhat will you do?");
        assert((ashes.options==std::vector<std::string>{"Sprinkle them somewhere special","Keep them in an urn","Throw them in the trash"}));
        int pick=ash;
        if(pick<0) {auto it=std::find(ashes.options.begin(),ashes.options.end(),ashes.choice);assert(it!=ashes.options.end());pick=int(it-ashes.options.begin());}
        std::cout<<"ashes forced="<<ash<<" actual="<<ashes.choice<<std::endl;
        assert(ashes.choice==ashes.options[pick]);
        if(pick==0)assert(ashes.narrative.find("I sprinkled my mother's ashes ")==0 && ashes.narrative.find('<')==std::string::npos);
        if(pick==1)assert(ashes.narrative=="I put my mother's ashes in an urn and stored it in a safe place.");
        if(pick==2)assert(ashes.narrative=="I threw my mother's ashes in the trash.");
        assert(branch.hero.age==age && branch.year==year && branch.hero.money==cash-990);
        assert(branch.pendingCards.empty() && !branch.hero.pendingFuneralAshes && branch.hero.ashesRelation.empty());
        apply(fork,branch,ashes);assert(branch.alive);AgeUp next;assert(fork.ageUp(branch,next));
        assert(branch.hero.age==age+1 && branch.year==year+1);
        assert(next.table!="registry:ScenarioFuneralAshes");
      }
    }
  }
}
''')
    subprocess.run(["c++", "-std=c++17", "-I"+str(ROOT/"port/include"), str(source), str(ROOT/"port/build/libblport.a"), "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(ROOT)], check=True)
print("ok: natural funeral disposition and ashes explicit choices, costs, same-year cleanup")
