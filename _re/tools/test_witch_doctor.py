"""308: native ordinary witch-doctor cache, costs, targets, risks and follow-ups."""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "witch.cpp"
    binary = Path(directory) / "witch"
    source.write_text(r'''#include "bl/sim.h"
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace bl;
int main(int argc, char** argv) {
  const std::string root = argv[1];
  const auto remedies = Table::load(root + "/gamedata_plain/a16_witch-remedies-enc");
  const auto diseases = Table::load(root + "/gamedata_plain/a16_disease-types-enc");
  auto row = [&](const std::string& name) {
    for (size_t r=0;r<remedies.size();++r) if (remedies.at(r,"Item")==name) return int(r);
    assert(false); return -1;
  };
  auto illness = [&](const std::string& key, bool diagnosed=true) {
    Hero::Ailment a; a.name=a.key=key; a.diagnosed=diagnosed; a.noCure=true; return a;
  };
  auto lifeFor = [&](int r, int health=100) {
    Life l; auto& h=l.hero; h.age=30; h.money=10000;
    h.health=health; h.happiness=h.looks=h.smarts=100;
    h.witchRemedyAge=30; h.witchRemedyRow=r; h.witchCostFactor=17; return l;
  };
  auto apply = [](Life& l, const AgeUp& o) {
    l.hero.health+=o.health; l.hero.happiness+=o.happiness;
    l.hero.looks+=o.looks; l.hero.smarts+=o.smarts;
  };
  // Constructor/dismissal fixes both item and cost; taking does not consume cache.
  Sim cache(root,7); Life cached; cached.hero.age=30; AgeUp out;
  assert(cache.witchDoctorVisit(cached,out,false));
  const int r=cached.hero.witchRemedyRow, u=cached.hero.witchCostFactor;
  Rng cacheExpected(7 ^ 0xd1310ba698dfb5acULL);
  assert(r==int(cacheExpected.below(remedies.size())) && u==1+int(cacheExpected.below(100)));
  assert(r>=0 && r<int(remedies.size()) && u>=1 && u<=100);
  const auto offer=out.body;
  assert(out.title=="Witch Doctor" && out.options==std::vector<std::string>{"Try it, why not?"});
  assert(out.choice.empty() && cached.hero.money==0 && cached.pendingCards.empty());
  assert(cache.witchDoctorVisit(cached,out,false) && out.body==offer);
  assert(cached.hero.witchRemedyRow==r && cached.hero.witchCostFactor==u);
  // Unaffordable attempts log only the offer, never debit or consume outcome RNG.
  assert(cache.witchDoctorVisit(cached,out,true));
  assert(cached.hero.money==0 && out.narrative.find("I took")==std::string::npos);
  assert(cache.ageUp(cached,out,0) && cached.hero.age==30);
  assert(out.body.find("You are broke.")!=std::string::npos);
  cached.hero.money=1;
  assert(cache.witchDoctorVisit(cached,out,true));
  assert(cache.ageUp(cached,out,0) && cached.hero.money==1);
  assert(out.body.find(" short.")!=std::string::npos);
  cacheExpected.below(3); cacheExpected.below(3); // insufficient popup titles
  ++cached.hero.age;
  assert(cache.witchDoctorVisit(cached,out,false));
  assert(cached.hero.witchRemedyRow==int(cacheExpected.below(remedies.size())));
  assert(cached.hero.witchCostFactor==1+int(cacheExpected.below(100)));
  // The cheapest-risk cached remedy can be paid for repeatedly, including exact balance.
  Sim repeat(root,44); Life repeated=lifeFor(row("monkey ejaculate"));
  repeated.hero.money=68;
  for(int i=0;i<2;++i) {
    assert(repeat.witchDoctorVisit(repeated,out,true));
    apply(repeated,out); assert(repeat.ageUp(repeated,out,0));
    assert(repeated.hero.witchRemedyRow==row("monkey ejaculate") && repeated.hero.witchCostFactor==17);
  }
  assert(repeated.hero.money==0 && repeated.hero.obituaryColors.size()==2);
  // Every artifact row, with a shadow native-bound stream, not sampled percentages.
  Sim sim(root,123); Rng expected(123 ^ 0xd1310ba698dfb5acULL);
  const int points[]={0,-4,-8,-16,-30,-50};
  int fatal=0, neutral=0, harm=0, cures=0, delayed=0;
  for (size_t ri=0;ri<remedies.size();++ri) for(int n=0;n<40;++n) {
    Life l=lifeFor(int(ri)); const int risk=int(remedies.num(ri,"Risk Score"));
    const int roll=int(expected.below(2*risk));
    int health=0,happy=0,looks=0,smarts=0;
    if(roll>=8) {health=looks=smarts=-100; ++fatal;}
    else if(roll) {
      health=happy=points[std::min(roll,5)]; ++harm;
      if(expected.below(3)==0) looks=smarts=health;
    } else ++neutral;
    const int title=roll<8 ? 2+int(expected.below(2)) : -1;
    const int color=int(expected.below(2));
    assert(sim.witchDoctorVisit(l,out,true));
    assert(l.hero.money==10000-34*int(remedies.num(ri,"Cost Score")));
    const std::string item=remedies.at(ri,"Item");
    assert(l.hero.ateSnake==(item.find("snake")!=std::string::npos || item.find("cobra")!=std::string::npos));
    assert(out.health==health && out.happiness==happy && out.looks==looks && out.smarts==smarts);
    assert(l.hero.obituaryColors.back()==std::string("Friends describe <obit-him/her> as ")+
      (color ? "a risk taker." : "a questionable decision maker."));
    if(roll>=8) {
      assert(l.pendingCards.empty() && l.hero.witchDeathRemedy==remedies.at(ri,"Item"));
      std::string type,sentence; assert(sim.checkDeath(l,type,sentence));
      assert(sentence=="I died after taking "+remedies.at(ri,"Item")+".");
    } else {
      assert(l.pendingCards.size()==1 && l.pendingCards[0].a==title);
      apply(l,out); assert(sim.ageUp(l,out,0) && l.hero.age==30 && l.year==0);
      assert(out.title==(title==2 ? "Oh my" : "Practice safe hex"));
      assert(out.body=="You took "+remedies.at(ri,"Item")+".");
      assert(l.pendingCards.empty());
    }
  }
  // Prison happiness caps draw before the shared appearance/intelligence roll.
  Sim prison(root,2024); Rng prisonExpected(2024 ^ 0xd1310ba698dfb5acULL);
  for(int n=0;n<30;++n) {
    Life l=lifeFor(row("monkey ejaculate")); l.hero.inPrison=true;
    const int roll=int(prisonExpected.below(2));
    int happy=0,looks=0;
    if(roll) {
      happy=51+int(prisonExpected.below(3))-100;
      if(prisonExpected.below(3)==0) looks=-4;
    }
    const int title=2+int(prisonExpected.below(2)); prisonExpected.below(2);
    assert(prison.witchDoctorVisit(l,out,true));
    assert(out.happiness==happy && out.looks==looks && out.smarts==looks);
    assert(l.pendingCards[0].a==title);
  }
  // Specialty wins over a more serious disease; no fallback after specialty failure.
  // A non-specialty visit chooses first strict maximum (ties retain earlier entry).
  Sim treatment(root,999); Rng treatmentExpected(999 ^ 0xd1310ba698dfb5acULL);
  for(int mode=0;mode<4;++mode) for(int n=0;n<100;++n) {
    const bool diagnosed=mode!=1;
    Life l=lifeFor(row(mode<2 ? "lion's blood" : "monkey ejaculate"),40);
    l.hero.happiness=l.hero.looks=l.hero.smarts=40;
    const std::string first=mode<2 ? "anxiety" : "depression";
    l.hero.ailments={illness(first,diagnosed),illness("rabies")};
    if(mode==3) {
      l.hero.ailments={illness("rabies"),illness("rabies")};
      l.hero.ailments[0].years=1; l.hero.ailments[1].years=2;
    }
    const bool expectedCure=int(treatmentExpected.below(mode<2 ? 15 : 50)) < (mode<2 ? 5 : 4);
    if(expectedCure) { if(diagnosed) treatmentExpected.below(2); }
    else {
      const int riskRoll=int(treatmentExpected.below(mode<2 ? 8 : 2));
      if(riskRoll) treatmentExpected.below(3);
      if(riskRoll<5) treatmentExpected.below(2); // loss50 kills starting health40
    }
    treatmentExpected.below(2); // take obituary follows every affordable outcome
    assert(treatment.witchDoctorVisit(l,out,true));
    assert(l.hero.pastAilments.empty()!=expectedCure);
    if(!expectedCure) continue;
    ++cures;
    const auto& cured=l.hero.pastAilments.back();
    assert(cured.key==(mode<2 ? "anxiety" : "rabies"));
    if(mode==3) assert(cured.years==1);
    assert(l.hero.ailments.size()==1);
    if(!diagnosed) {
      assert(!l.hero.curedByWitchDoctor && l.pendingCards.empty());
      assert(out.happiness>=16); // minimum positive happiness impact score is 3
    } else {
      assert(l.hero.curedByWitchDoctor && out.health==0 && out.happiness==0);
      apply(l,out);
      assert(treatment.ageUp(l,out,0) && l.hero.age==30);
      assert(out.title=="Wait, did it..." || out.title=="No effing way...");
      assert(out.health==0 && out.happiness==0);
      assert(treatment.ageUp(l,out,0) && l.hero.age==30);
      assert(out.title=="Cured" && out.happiness>=16);
      assert(out.narrative=="I was cured of "+cured.name+".");
      ++delayed;
    }
  }
  assert(cures>=20 && delayed>=10 && fatal && neutral && harm);
  // Low-risk remedies still kill when impact depletes health.
  bool depleted=false;
  for(int n=0;n<50 && !depleted;++n) {
    Life l=lifeFor(row("monkey ejaculate"),1);
    assert(treatment.witchDoctorVisit(l,out,true));
    depleted=!l.hero.witchDeathRemedy.empty();
    if(depleted) assert(out.health==-1 && l.pendingCards.empty());
  }
  assert(depleted);
  // A queued same-year medical chain must not roll annual mortality before restoration.
  bool checked=false;
  for(int n=0;n<100 && !checked;++n) {
    Life l=lifeFor(row("lion's blood"),40);
    l.hero.age=l.hero.witchRemedyAge=400;
    l.hero.ailments={illness("anxiety")};
    assert(treatment.witchDoctorVisit(l,out,true));
    if(l.hero.pastAilments.empty()) continue;
    std::string type,sentence;
    assert(!treatment.checkDeath(l,type,sentence));
    assert(treatment.ageUp(l,out,0));
    assert(!treatment.checkDeath(l,type,sentence));
    checked=true;
  }
  assert(checked);
  assert(treatment.forceScenarioClass("ScenarioWitchDoctor",0));
  assert(!treatment.forceScenarioClass("ScenarioWitchDoctor",1));
  std::cout<<"native witch rows="<<remedies.size()<<" fatal="<<fatal<<" neutral="<<neutral
           <<" harm="<<harm<<" cures="<<cures<<" delayed="<<delayed<<"\n";
}
''')
    subprocess.run(["c++", "-std=c++17", "-I"+str(ROOT/"port/include"), str(source),
                    str(ROOT/"port/build/libblport.a"), "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(ROOT)], check=True)
# Explicit replay reaches the feature; no guessed annual visit probability.
spec = dict(seed=123, country="United States", gender="male", ages=1, cash=10000,
            push=[dict(scenario="ScenarioWitchDoctor", choice=0)])
proc = subprocess.run([str(ROOT/"port/build/blsim"), "--script", "/dev/stdin"],
                      input=json.dumps(spec), capture_output=True, text=True, check=True)
events=json.loads(proc.stdout)["events"]
assert events[0]["title"]=="Witch Doctor", events
assert events[0]["choice"]=="Try it, why not?", events
spec["cash"]=0
proc = subprocess.run([str(ROOT/"port/build/blsim"), "--script", "/dev/stdin"],
                      input=json.dumps(spec), capture_output=True, text=True, check=True)
events=json.loads(proc.stdout)["events"]
assert len(events)==2 and events[0]["age"]==events[1]["age"]==1, events
assert "You are broke." in events[1]["body"], events
print("ok")
