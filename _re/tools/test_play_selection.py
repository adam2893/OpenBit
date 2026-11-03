"""Late played-table selection rejects mismatched provenance and invalid requests."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "selection.cpp"
    binary = Path(directory) / "selection"
    source.write_text(r'''#include "bl/sim.h"
#include "bl/script.h"
#include <cassert>
using namespace bl;
void prepare(Sim& sim, Life& life) {
  std::string error, hero; std::vector<std::string> birth;
  assert(sim.load(&error));
  ScriptSpec spec; spec.seed=7; spec.country="United States";
  assert(setupLife(sim,spec,life,&hero,&birth,&error));
}
int main(int argc,char** argv) {
  Sim preview(argv[1],7); Life original; prepare(preview,original);
  AgeUp shown; assert(preview.ageUp(original,shown));
  assert(shown.tableChoiceSupported && shown.options.size()==4);
  for (int lane=0;lane<8;++lane) {
    Sim sim(argv[1],7); Life life; prepare(sim,life);
    TableChoice request{shown.table,shown.row_id,0};
    if (lane==0) request.table="not the shown table";
    if (lane==1) request.row_id="not the shown row";
    if (lane==2) request.index=int(shown.options.size());
    if (lane==3) request.index=-1;
    if (lane==4) request.table.clear();
    if (lane>=6) assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
    if (lane==7) {
      AgeUp visit; assert(sim.ageUp(life,visit));
      assert(!life.pendingCards.empty());
    }
    AgeUp out; out.tableChoiceSupported=true;
    const int age=life.hero.age;
    assert(!sim.ageUp(life,out,lane==5 ? 0 : -1,&request));
    assert(!out.tableChoiceSupported);
    if (lane>=3 && lane<=5) {
      assert(life.hero.age==age);
      request={shown.table,shown.row_id,0};
      assert(sim.ageUp(life,out,-1,&request));
      assert(out.tableChoiceSupported && out.table==shown.table && out.row_id==shown.row_id);
      assert(out.choice==shown.options[0]);
    }
  }
}
''')
    subprocess.run(["c++", "-std=c++17", "-I"+str(ROOT/"port/include"), str(source),
                    str(ROOT/"port/build/libblport.a"), "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(ROOT)], check=True)
    probe=Path(directory)/"dispatch.cpp"
    for call,allowed in (("s.dispatchYear(l,o,-1)",True),
                         ("s.dispatchYear(l,o,-1,&r)",False),
                         ("s.dispatchYearWithTableChoice(l,o,-1,&r)",False)):
        probe.write_text('#include "bl/sim.h"\nvoid check(bl::Sim& s,bl::Life& l,bl::AgeUp& o,bl::TableChoice& r){'+call+';}\n')
        result=subprocess.run(["c++","-std=c++17","-I"+str(ROOT/"port/include"),
                               "-fsyntax-only",str(probe)],capture_output=True,text=True)
        assert (result.returncode==0)==allowed,(call,result.returncode,result.stderr)
print("ok: late table-choice provenance, bounds, incompatible mode and stale-capability rejection; early-error re-arm")
