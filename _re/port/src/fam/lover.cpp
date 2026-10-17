// src/fam/lover.cpp -- the birth-control demand of the lover family (REPORT.en.md 142).
//
// ScenarioLoverDemandsBirthControl is built on three literals the class reads out
// of its own slots, and on the person's OnBirthControl flag (hero+0x50).
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>

namespace bl {

bool Sim::loverBirthControlYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.loverName.empty()) return false;
  // The card only asks a partner who is not already satisfied: the gate reads
  // the person's own OnBirthControl flag before it draws.
  if (h.onBirthControl) return false;
  if (h.age < 18) return false;
  if (forcedChoice < 0 && loverRng_.below(14) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const std::string word = h.loverStatus == "married"
                               ? (h.loverFemale ? "wife" : "husband")
                               : (h.loverFemale ? "girlfriend" : "boyfriend");
  out.table = "relationship";
  out.row_id = "0";
  // RefreshScenario @0x73f3920670: Format(cell 0x5ca6ad0, status lowered, name)
  // -- 'Your {0}, {1}, wants you to go on birth control.\n\nWhat will you do?'
  out.body = "Your " + word + ", " + h.loverName +
             ", wants you to go on birth control.\n\nWhat will you do?";
  out.body[0] = static_cast<char>(::toupper(static_cast<unsigned char>(out.body[0])));
  const int lane = forcedChoice >= 0 ? forcedChoice : int(loverRng_.below(2));
  if (lane == 0) {
    // 'Agree to go on birth control' (#0x5ca6ae0). The flag lands on the person
    // (strb w10, [hero+0x50]) and the journal is #23417, whose {0} is the
    // partner's own possessive pronoun.
    out.choice = "Agree to go on birth control";
    out.narrative = "I agreed to go on birth control following " +
                    std::string(h.loverFemale ? "her" : "his") + " suggestion.";
    h.onBirthControl = true;
    h.ageWentOnBirthControl = h.age;
  } else {
    // 'Refuse to go on birth control' (cell 0x5ca6ad8): journal #70365,
    // '{0} tried to get me to go on birth control but I refused.', where {0}
    // is the partner named the way the family names him.
    out.choice = "Refuse to go on birth control";
    out.narrative = "My " + word + ", " + h.loverName +
                    ", tried to get me to go on birth control but I refused.";
  }
  return true;
}

}  // namespace bl