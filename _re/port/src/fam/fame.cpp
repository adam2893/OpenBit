// src/fam/fame.cpp -- the paparazzi (REPORT.en.md 139).
//
// ScenarioFamePaparazzi is the Fame family's first card in the port. Its gate is
// fame >= 75.0f (REPORT.en.md 136), its text comes out of
// a16_paparazzi-scenarios-enc through BracketParser, and its three buttons are the
// class's own literals.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>

namespace bl {

bool Sim::paparazziYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  // A pushed card: the year keeps its own draw when the bench forces one.
  if (forcedChoice < 0 && !isFamous(h)) return false;
  // shortcut: the game's gate also requires the hero's own Fame record to exist
  // (_Fame at hero+0x170); the port has no such object, and a hero under 18 cannot
  // have earned one, so adulthood stands in for it.
  if (h.age < 18) return false;
  if (forcedChoice < 0 && fameRng_.below(6) != 0) return false;
  if (!ensureTable("a16_paparazzi-scenarios-enc")) return false;
  const Table& t = tbl("a16_paparazzi-scenarios-enc");
  if (!t.size()) return false;
  hero_ = &h;
  life_ = &life;
  const size_t row = fameRng_.below(uint32_t(t.size()));
  const std::string scene = resolve(t.at(row, "Scenario"));
  // The row's own log sentence is the {0} every lane journal starts with.
  const std::string log = resolve(t.at(row, "Log Version"));
  out.table = "a16_paparazzi-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = scene;
  // The three buttons, in the order RefreshScenario builds them: 'Pose for his
  // photo' (#34352), "Pretend he's not there" (#34511), 'Cover my face' (#16565).
  // ChoiceSelectedAtIndex @0x73f3bd7e28 dispatches on the *label text*, not on the
  // index, which is why each lane below is keyed the same way.
  const int lane = forcedChoice >= 0 ? forcedChoice : int(fameRng_.below(3));
  if (lane == 0) {
    // Contains(choice, 'pose'): Format('{0} I smiled for his picture.' #69613),
    // ImpactDiscipline(+3), ImpactKarma(+3), then SimHero::ImpactFame(+2).
    out.choice = "Pose for his photo";
    out.narrative = log + " I smiled for his picture.";
    out.discipline = pointsForImpact(3);
    out.karma = pointsForImpact(3);
    impactFame(h, 2);
  } else if (lane == 1) {
    // The fall-through lane: Format('{0} I pretended not to notice him.' #69597),
    // ImpactDiscipline(+2), ImpactKarma(+2), and no fame call at all.
    out.choice = "Pretend he's not there";
    out.narrative = log + " I pretended not to notice him.";
    out.discipline = pointsForImpact(2);
    out.karma = pointsForImpact(2);
  } else {
    // Contains(choice, 'cover'): Format('{0} I covered my face to make it hard
    // for him to take my picture.' #69547) and SimHero::ImpactFame(-1). The lane
    // moves nothing else -- it is the only one with no discipline or karma.
    out.choice = "Cover my face";
    out.narrative = log + " I covered my face to make it hard for him to take my picture.";
    impactFame(h, -1);
  }
  return true;
}

}  // namespace bl
