"""A new replay request replaces the previous request, including rejection/cancel.
Public API checks compare real outgoing cards with identically seeded controls.
"""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "replay.cpp"
    binary = Path(directory) / "replay"
    source.write_text(r'''#include "bl/sim.h"
#include <cassert>
#include <iostream>
#include <string>
using namespace bl;
int main(int argc, char** argv) {
  const std::string table="a16_adoption-scenarios-enc";
  for (int lane=0; lane<19; ++lane) {
    Sim sim(argv[1],7), control(argv[1],7);
    std::string error;
    assert(sim.load(&error) && control.load(&error));
    Life life=sim.createLife(false), expected=control.createLife(false);
    life.hero.age=expected.hero.age=30;
    switch (lane) {
      case 0:
        assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
        assert(!sim.forceScenarioClass("ScenarioDefinitelyNotImplemented",0)); break;
      case 1:
        assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
        assert(!sim.forceScenarioClass("ScenarioWitchDoctor",1)); break;
      case 2:
        assert(sim.forceScenario(table,1));
        assert(!sim.forceScenario("a16_missing-replay-fixture-enc",1)); break;
      case 3:
        assert(sim.forceScenario(table,1));
        assert(!sim.forceScenario(table,9223372036854775807LL)); break;
      case 4:
        assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
        assert(sim.forceScenario("",0)); break;
      case 5:
        assert(sim.forceScenario(table,1));
        assert(sim.forceScenarioClass("",-1)); break;
      case 6:
        assert(sim.forceScenario(table,1));
        assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
        assert(control.forceScenarioClass("ScenarioWitchDoctor",0)); break;
      case 7:
        assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
        assert(sim.forceScenario(table,1));
        assert(control.forceScenario(table,1)); break;
      case 8:
        assert(sim.forceScenario(table,1));
        assert(!sim.forceScenarioClass("ScenarioDefinitelyNotImplemented",0)); break;
      case 9:
        assert(sim.forceScenario(table,1));
        assert(!sim.forceScenarioClass("ScenarioWitchDoctor",1)); break;
      case 10:
        assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
        assert(!sim.forceScenario("a16_missing-replay-fixture-enc",1)); break;
      case 11:
        assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
        assert(!sim.forceScenario(table,9223372036854775807LL)); break;
      case 12:
        assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
        assert(!sim.forceScenarioClass("ScenarioDefinitelyNotImplemented",0));
        assert(sim.forceScenario(table,1));
        assert(control.forceScenario(table,1)); break;
      case 13:
        assert(sim.forceScenario(table,1));
        assert(!sim.forceScenario("a16_missing-replay-fixture-enc",1));
        assert(sim.forceScenarioClass("ScenarioOfferFood",1));
        assert(control.forceScenarioClass("ScenarioOfferFood",1)); break;
      case 14:
        assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
        assert(sim.forceScenario(table,-1)); break;
      case 15:
        assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
        assert(sim.forceScenarioClass("",-1)); break;
      case 16:
        assert(sim.forceScenario(table,1));
        assert(sim.forceScenario("",0)); break;
      case 17:
        assert(sim.forceScenarioClass("ScenarioWitchDoctor",0));
        assert(sim.forceScenarioClass("ScenarioOfferFood",1));
        assert(control.forceScenarioClass("ScenarioOfferFood",1)); break;
      case 18:
        assert(sim.forceScenario(table,1));
        assert(sim.forceScenario(table,2));
        assert(control.forceScenario(table,2)); break;
    }
    for (int step=0; step<2; ++step) {
      // Independently cancel the control after consumption; symmetric retention must fail.
      if (step==1) assert(control.forceScenario("",0));
      AgeUp actual, wanted;
      assert(sim.ageUp(life,actual)==control.ageUp(expected,wanted));
      // These table/class fixtures produce no follow-up masking the next annual card.
      if (step==0 && (lane==7 || lane==17))
        assert(life.pendingCards.empty() && expected.pendingCards.empty());
      if (actual.table!=wanted.table || actual.row_id!=wanted.row_id ||
          actual.title!=wanted.title || actual.body!=wanted.body ||
          actual.narrative!=wanted.narrative) {
        std::cerr << "replay lifecycle lane=" << lane << " step=" << step
                  << " actual=" << actual.table << " expected=" << wanted.table << "\n";
        return 1;
      }
      assert(life.hero.age==expected.hero.age);
    }
  }
}
''')
    subprocess.run(["c++", "-std=c++17", "-I"+str(ROOT/"port/include"), str(source),
                    str(ROOT/"port/build/libblport.a"), "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(ROOT)], check=True)
print("ok: rejected/cleared/replaced replay requests do not leak into this or subsequent cards")
