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

// ScenarioLoverStopsBirthControl (REPORT.en.md 144): the mirror of the demand card.
//
// ChoiceSelectedAtIndex @0x73f3921280 reads the chosen button's own Title, lowers it
// and dispatches on Contains('disapprove'): that lane moves karma and discipline by
// -2 and rolls the argue follow-up; the other lane clears the person's
// OnBirthControl flag (strb wzr, [hero+0x50]).
bool Sim::loverStopsBirthControlYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.loverName.empty()) return false;
  // The mirror of the demand: only a partner who IS on birth control can want off.
  if (!h.onBirthControl) return false;
  if (h.age < 18) return false;
  if (forcedChoice < 0 && loverRng_.below(20) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const std::string word = h.loverStatus == "married"
                               ? (h.loverFemale ? "wife" : "husband")
                               : (h.loverFemale ? "girlfriend" : "boyfriend");
  const std::string who = "My " + word + ", " + h.loverName + ",";
  const std::string poss = h.loverFemale ? "her" : "his";
  out.table = "relationship";
  out.row_id = "0";
  // Body: Format(cell 0x5ca6b38) -- 'Your {0}, {1}, wants to stop taking {2} birth
  // control.', the {2} being the partner's own possessive pronoun.
  out.body = "Your " + word + ", " + h.loverName + ", wants to stop taking " + poss +
             " birth control.";
  out.body[0] = static_cast<char>(::toupper(static_cast<unsigned char>(out.body[0])));
  const int lane = forcedChoice >= 0 ? forcedChoice : int(loverRng_.below(2));
  if (lane == 0) {
    // 'Yes, stop taking it' (#47510): the flag is cleared and the journal is
    // #70379 -- '{0} wants to stop taking {1} birth control. I agreed that was a
    // good idea.'
    out.choice = "Yes, stop taking it";
    out.narrative = who + " wants to stop taking " + poss +
                    " birth control. I agreed that was a good idea.";
    h.onBirthControl = false;
    h.ageWentOffBirthControl = h.age;
  } else {
    // 'Disapprove of that idea' (#17777): karma -2, discipline -2, journal
    // #70380 -- '{0} wants to stop taking {1} birth control. I told {2} I don't
    // approve of that.' The argue roll that follows is not ported.
    out.choice = "Disapprove of that idea";
    out.narrative = who + " wants to stop taking " + poss +
                    " birth control. I told " + poss + " I don't approve of that.";
    out.karma = pointsForImpact(-2);
    out.discipline = pointsForImpact(-2);
  }
  return true;
}
// ScenarioLoverCounselingPush (REPORT.en.md 152): the partner demands counseling.
//
// ChoiceSelectedAtIndex @0x73f3db38c4 classifies the chosen label by text:
// Contains('agree') -> the session happens (ImpactDiscipline(2) and journal #23416),
// Contains('refuse') -> journal #70366. The other two comparisons the handler makes
// ('<disease>', '<addiction>') pick the subject the session is about.
bool Sim::loverCounselingPushYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.loverName.empty()) return false;
  if (h.age < 18) return false;
  if (forcedChoice < 0 && loverRng_.below(16) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const std::string word = h.loverStatus == "married"
                               ? (h.loverFemale ? "wife" : "husband")
                               : (h.loverFemale ? "girlfriend" : "boyfriend");
  const std::string who = "My " + word + ", " + h.loverName + ",";
  const std::string whoLower = "my " + word + ", " + h.loverName + ",";
  const std::string them = h.loverFemale ? "her" : "him";
  out.table = "relationship";
  out.row_id = "0";
  // RefreshScenario @0x73f3db367c: Format(cell 0x5cc4418, status lowered, name,
  // pronoun) -- 'Your {0}, {1}, is demanding that you visit a relationship
  // counselor with {2}.\n\nWhat will you do?'
  out.body = "Your " + word + ", " + h.loverName +
             ", is demanding that you visit a relationship counselor with " + them +
             ".\n\nWhat will you do?";
  out.body[0] = static_cast<char>(::toupper(static_cast<unsigned char>(out.body[0])));
  const int lane = forcedChoice >= 0 ? forcedChoice : int(loverRng_.below(2));
  if (lane == 0) {
    // 'Agree to go' (cell 0x5ca2ae0): ImpactDiscipline(+2), journal #23416.
    out.choice = "Agree to go";
    out.narrative = "I agreed to attend relationship counseling with " + whoLower;
    out.discipline = pointsForImpact(2);
  } else {
    // 'Refuse to go' (cell 0x5c39c00): journal #70366, no stat move read.
    out.choice = "Refuse to go";
    out.narrative = who + " tried to make me attend relationship counseling with " +
                    them + " but I refused.";
  }
  return true;
}
// ScenarioLoverArgues, reason 1 (REPORT.en.md 153): the partner is arguing because
// the hero cheated. The class has ten constructors and a Refresh that switches on
// which object it was built with; the int reason is the simplest of them and the
// only one the port can feed yet:
//
//   reason 1  '{0} is arguing with you because you cheated on {1}.'   #70077
//   reason 0  '{0} is arguing with you because you gave {1} {2}.'    #70089
//
// The card's buttons are not ported, so it arrives as a report -- the same shape as
// the break-up announcement (REPORT.en.md 125).
bool Sim::loverArguesYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (h.loverName.empty()) return false;
  if (!h.loverArguesCheating) return false;
  h.loverArguesCheating = false;
  const std::string subj = h.loverFemale ? "She" : "He";
  const std::string word = h.loverStatus == "married"
                               ? (h.loverFemale ? "wife" : "husband")
                               : (h.loverFemale ? "girlfriend" : "boyfriend");
  out.table = "relationship";
  out.row_id = "0";
  out.body = subj + " is arguing with you because you cheated on your " + word +
             ", " + h.loverName + ".";
  out.narrative = subj + " is arguing with me because I cheated on " +
                  (h.loverFemale ? "her" : "him") + ".";
  return true;
}
}  // namespace bl