"""411: a naturally queued safe-sex card honors either explicit public choice."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "pending_safe_sex.cpp"
    binary = Path(directory) / "pending_safe_sex"
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
  // Bounded discovery (seeds1..60, age60, 256 calls/life) found this real
  // producer prefix: seed12, 65 cards, age57, partner Layla Smith. No replay
  // overrides or injected queue/due/name state. Pin it rather than scan forever.
  for (int forced : {0, 1, -1}) {
    Sim sim(argv[1], 12); Life life;
    std::string error, hero; std::vector<std::string> birth;
    assert(sim.load(&error));
    ScriptSpec spec; spec.seed = 12; spec.country = "United States";
    assert(setupLife(sim, spec, life, &hero, &birth, &error));
    AgeUp previous;
    for (int card = 0; card < 65; ++card) {
      assert(life.alive);
      assert(sim.ageUp(life, previous));
      apply(sim, life, previous);
    }
    assert(life.alive && life.hero.age == 57);
    assert(life.pendingCards.size() == 1);
    assert(life.pendingCards.front().kind == kPendingSafeSex);
    assert(life.hero.safeSexDue && life.hero.safeSexWith == "Layla Smith");
    const int age = life.hero.age, year = life.year;
    const std::string partner = life.hero.safeSexWith;
    AgeUp out;
    assert(sim.ageUp(life, out, forced));
    assert(out.table == "registry:ScenarioSafeSex" && out.title == "Safe Sex");
    assert(out.body == "Things are getting hot with " + partner +
                       " and you're thinking about safe sex.");
    assert((out.options == std::vector<std::string>{"Use a condom", "Don't use a condom"}));
    const int selected = forced < 0 ? (out.choice == out.options[0] ? 0 : 1) : forced;
    std::cout << "forced=" << forced << " actual=" << out.choice << std::endl;
    assert(out.choice == out.options[selected]);
    assert(out.narrative == (selected == 0 ? "I practiced safe sex." : "I practiced unsafe sex."));
    assert(life.hero.age == age && life.year == year && life.alive);
    assert(life.pendingCards.empty());
    assert(!life.hero.safeSexDue && life.hero.safeSexWith.empty());
    // The consumed card cannot recur on the next ordinary annual call.
    apply(sim, life, out);
    assert(life.alive);
    AgeUp next; assert(sim.ageUp(life, next));
    assert(life.hero.age == age + 1 && life.year == year + 1);
    assert(next.table != "registry:ScenarioSafeSex");
  }
}
''')
    subprocess.run(["c++", "-std=c++17", "-I" + str(ROOT / "port/include"),
                    str(source), str(ROOT / "port/build/libblport.a"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(ROOT)], check=True)
print("ok: natural pending safe-sex explicit choices, default, same-year drain and cleanup")
