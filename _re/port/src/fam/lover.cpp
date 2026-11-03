// src/fam/lover.cpp -- the birth-control demand of the lover family (REPORT.en.md 142).
//
// ScenarioLoverDemandsBirthControl is built on three literals the class reads out
// of its own slots, and on the person's OnBirthControl flag (hero+0x50).
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <cctype>
#include <cstdlib>
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
  // A partner does not demand contraception at 66 (REPORT.en.md 205): the card's own
  // upper bound was missing, and the life that produced it had a 66-year-old hero.
  if (h.age > 50) return false;
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
  if (forcedChoice < 0 && loverRng_.below(7) != 0) return false;
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
  // reason 1 (#70077). reason 0 -- the refused gift (#70089) -- is not wired: the
  // gift card's refusal pool never fires in the port (REPORT.en.md 154).
  out.body = subj + " is arguing with you because you cheated on your " + word +
             ", " + h.loverName + ".";
  out.narrative = subj + " is arguing with me because I cheated on " +
                  (h.loverFemale ? "her" : "him") + ".";
  return true;
}
// ScenarioLoverCallOffEngagement (REPORT.en.md 158): the hero ends an engagement.
//
// A report card -- the class has RefreshScenario and DismissSelected and no
// ChoiceSelectedAtIndex, so nothing is asked. RefreshScenario builds:
//   title      PickRandomFromStringArray over 'Let's wait a little longer',
//              'Just not sure anymore', 'Just kidding'
//   body       'You called off your engagement with your {0}, {1}.'   #47929
//   narrative  'I called off my engagement with {0}.'                #23687
//   extra      '{0} still wants the commitment.'                     #3450
//   impacts    ImpactKarma(-5), ImpactDiscipline(-3)
bool Sim::loverCallOffEngagementYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.loverName.empty() || h.loverStatus != "engaged") return false;
  if (h.age < 18) return false;
  if (forcedChoice < 0 && loverRng_.below(9) != 0) return false;
  hero_ = &h;
  life_ = &life;
  static const char* const kTitles[] = {"Let's wait a little longer",
                                        "Just not sure anymore", "Just kidding"};
  const std::string word = (h.loverFemale ? "girlfriend" : "boyfriend");
  out.table = "relationship";
  out.row_id = "0";
  out.title = kTitles[loverRng_.below(3)];
  out.body = "You called off your engagement with your " + word + ", " +
             h.loverName + ".";
  out.body[0] = static_cast<char>(::toupper(static_cast<unsigned char>(out.body[0])));
  out.narrative = "I called off my engagement with " + h.loverName + ".";
  out.extra.push_back(h.loverName + " still wants the commitment.");
  out.karma = pointsForImpact(-5);
  out.discipline = pointsForImpact(-3);
  h.loverStatus = "dating";
  h.relationship -= 15;
  if (h.relationship < 0) h.relationship = 0;
  return true;
}
// ScenarioLoverElopePush (REPORT.en.md 159): the partner asks to run off and elope.
//
//   body     'Your {0}, {1}, has asked you to run off and get eloped.'   #50691
//   buttons  'Agree to elope' (#11010)   'Plan a wedding instead' (#34055)
//   journals '{0} asked me to get eloped.' (#69853)
//            '{0} I suggested we plan a normal wedding instead.' (#69619)
//
// The handler classifies the label: Contains('agree') pushes
// ScenarioLoverWeddingPlanning and logs; Contains('plan') is the counter-offer.
// ScenarioIsEligibleForRandomPush refuses royals and prisoners and asks the partner
// (SimLover::WillAgreeToGetEloped).
//
// The planning card is not ported, so eloping marries on the spot and the
// counter-offer leaves the couple engaged -- the two states the port does have.
bool Sim::loverElopeYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.loverName.empty()) return false;
  if (h.loverStatus != "dating" && h.loverStatus != "engaged") return false;
  if (h.royal || h.inPrison) return false;
  if (h.age < 18) return false;
  if (forcedChoice < 0 && loverRng_.below(10) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const std::string word = h.loverFemale ? "girlfriend" : "boyfriend";
  const std::string who = "My " + word + ", " + h.loverName + ",";
  out.table = "relationship";
  out.row_id = "0";
  out.body = "Your " + word + ", " + h.loverName +
             ", has asked you to run off and get eloped.";
  out.body[0] = static_cast<char>(::toupper(static_cast<unsigned char>(out.body[0])));
  const int lane = forcedChoice >= 0 ? forcedChoice : int(loverRng_.below(2));
  if (lane == 0) {
    out.choice = "Agree to elope";
    out.narrative = h.loverName + " asked me to get eloped.";
    // The planning card is not ported: eloping is the marriage.
    h.loverStatus = "married";
    h.relationship = 80;
  } else {
    out.choice = "Plan a wedding instead";
    out.narrative = h.loverName + " I suggested we plan a normal wedding instead.";
    h.loverStatus = "engaged";
    h.relationship = 75;
  }
  return true;
}
// ScenarioLoverWedding, the venue half (REPORT.en.md 160).
//
// The real card is a plan: a venue (a16_wedding-venues-enc, 12 rows carrying a Cost
// Score and a Lover Happiness Score), a honeymoon destination and a total cost, with
// three buttons ('do it', 'Edit the plan', 'Actually, never mind'). The port has the
// venue table and the two strings that matter:
//
//   'You have chosen to marry {0} at {1}'          #48760
//   'I called off my wedding with {0}.'            #23688
//
// shortcut: the game's wedding cost is a formula over the venue's Cost Score, the
// destination and the country's class; only the venue table's own score is read
// here, priced at 1000 per point. The honeymoon half is not modelled.
bool Sim::loverWeddingYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.loverName.empty() || h.loverStatus != "engaged") return false;
  if (!ensureTable("a16_wedding-venues-enc")) return false;
  if (h.age < 18) return false;
  hero_ = &h;
  life_ = &life;
  const Table& v = tbl("a16_wedding-venues-enc");
  if (v.size() == 0) return false;
  // Only a venue the hero can pay for is offered: the game's card prices the plan and
  // refuses the ones the hero cannot afford, and offering an unaffordable wedding is
  // what left a life engaged with a wedding it never had (REPORT.en.md 161).
  std::vector<size_t> affordable;
  for (size_t r = 0; r < v.size(); ++r) {
    if (1000LL * int(v.num(r, "Cost Score")) <= h.money) affordable.push_back(r);
  }
  // With nothing affordable the card still appears at the cheapest venue, and its
  // 'Do it' lane reports that the hero cannot pay -- the game does not hide the plan.
  const size_t row = affordable.empty()
                         ? size_t(0)
                         : affordable[loverRng_.below(uint32_t(affordable.size()))];
  const int rowIdx = int(row);
  const std::string venue = trim(v.at(row, "Venue"));
  const bool definite = v.flag(row, "Uses Definite Article");
  const int costScore = int(v.num(row, "Cost Score"));
  const int happyScore = int(v.num(row, "Lover Happiness Score"));
  const long long price = 1000LL * costScore;
  out.table = "a16_wedding-venues-enc";
  out.row_id = std::to_string(rowIdx + 1);
  out.title = "The wedding plan";
  out.body = "You have chosen to marry " + h.loverName + " at " +
             (definite ? "the " : "") + venue + ".";
  const int lane = forcedChoice >= 0 ? forcedChoice : int(loverRng_.below(2));
  if (lane == 0) {
    out.choice = "Do it";
    if (h.money < price) {
      out.narrative = "I could not afford the wedding at " +
                      (definite ? std::string("the ") : std::string()) + venue + ".";
      out.extra.push_back("The wedding cost " + money(price) + ", more than I had.");
      return true;
    }
    h.money -= price;
    h.loverStatus = "married";
    h.relationship = std::min(100, h.relationship + happyScore);
    out.narrative = "You have chosen to marry " + h.loverName + " at " +
                    (definite ? std::string("the ") : std::string()) + venue + ".";
    out.extra.push_back("The wedding cost " + money(price) + ".");
    // The identity card follows the wedding (REPORT.en.md 396).
    h.nameChangePending = true;
  } else {
    out.choice = "Actually, never mind";
    out.narrative = "I called off my wedding with " + h.loverName + ".";
    h.loverStatus = "dating";
    h.relationship = 40;
  }
  return true;
}
// ScenarioLoverLeaves (REPORT.en.md 164): the partner ends it, with buttons.
//
//   body     '{0} wants to break up with you.'  #70377
//            '{0} wants a divorce.'             #70376   (when married)
//   buttons  'Beg {0} to stay' (#13308)  'Wish {0} well' (#47041)
//            'Hypnotize {0} to stay' (#23344) -- the vampire lane
//
// The port already has the break-up announcement and the ex-lover record (REPORT
// 125/134), so both leaving lanes hand off to that machinery instead of writing a
// second exit path.
//
// shortcut: the game's success roll for begging is not read; the port keeps the
// couple together when the relationship is still above 40 and lets them go otherwise.
// ScenarioNameChangeMarriage (REPORT.en.md 396): the identity card that follows a
// wedding. Its strings were resolved out of the class's static ctor
// (0x73f28b842c): title #26311 'Identity', body #32275
// "Now that you're married, you're wondering if you should change your last name.
//
// What last name will you choose?", rows #50142 'Your Last Name', #69671
// '{0} Last Name' and #31091 'Name', and the three buttons #28237 'Keep {0} {1}',
// #15401 'Change to {0} {1}' and #11136 'All the way: {0} {1}'.
//
// shortcut: the {0} {1} pairs are the hero's first name and a surname here, and the
// spouse's surname is drawn from the port's own name table because a partner record
// in this port carries a given name only (a native spouse is a full SimPerson with
// last name and honorific -- the same static ctor also loads #30530/#30532/#18178/
// #30671 Mr./Mrs./Dr./Mx. for the 'Name' row, which this card does not model).
// Which button writes which surname was not recovered past the two fields
// _halfSurname and _fullSurname, so the mapping below is the reading the button
// labels support: keep, take the spouse's, hyphenate.
// ScenarioNameChangeDivorce (REPORT.en.md 397): the identity card that follows a
// divorce. Strings from the class's static ctor 0x73f28b767c: title #26311
// 'Identity', body #32272 "Now that you're divorced, you're thinking about changing
// your name back.\n\nWhat will you do?", row #31091 'Name' (with the same
// Mr./Mrs./Dr./Mx. honourific pool the marriage card carries) and the two buttons
// #21802 'Go back to {0} {1}' and #39986 'Stay as {0} {1}'.
//
// shortcut: the slogans' journals were not recovered, so both lines below are the
// port's own; and the card is offered only when the marriage card actually changed
// the hero's surname (Hero::nameChangeMaiden carries the old one).
bool Sim::nameChangeDivorceYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.nameChangeDivorcePending) return false;
  h.nameChangeDivorcePending = false;
  if (h.nameChangeMaiden.empty() || h.firstName.empty()) return false;
  const std::string maiden = h.nameChangeMaiden;
  const std::string kept = h.lastName.empty() ? maiden : h.lastName;
  const std::string back = h.firstName + " " + maiden;
  const std::string stays = h.firstName + " " + kept;
  const std::vector<std::string> extra = out.extra;
  out = AgeUp();
  out.extra = extra;
  out.table = "registry:ScenarioNameChangeDivorce";
  out.title = "Identity";  // #26311
  out.body = "Now that you're divorced, you're thinking about changing your name back."
             "\n\nWhat will you do?";  // #32272
  out.body += "\n\nName: " + stays;  // #31091
  out.options = {"Go back to " + back, "Stay as " + stays};
  int lane = forcedChoice;
  if (lane < 0 || lane > 1) lane = int(nameRng_.below(2));
  out.choice = out.options[size_t(lane)];
  if (lane == 0) {
    h.lastName = maiden;
    out.narrative = "I went back to the name " + back + ".";
  } else {
    out.narrative = "I stayed " + stays + ".";
  }
  h.nameChangeMaiden.clear();
  return true;
}

bool Sim::nameChangeMarriageYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.loverStatus != "married") return false;
  if (!h.nameChangePending) return false;
  h.nameChangePending = false;
  if (h.firstName.empty()) return false;
  if (!ensureTable("a16_casino-casino-celebrity-name-enc")) return false;
  const Table& names = tbl("a16_casino-casino-celebrity-name-enc");
  if (names.size() == 0) return false;
  if (h.nameChangeSurname.empty()) {
    h.nameChangeSurname = trim(names.at(nameRng_.below(uint32_t(names.size())), "Last Name"));
    if (h.nameChangeSurname.empty()) return false;
  }
  const std::string own = h.lastName.empty() ? h.firstName : h.lastName;
  const std::string spouse = h.nameChangeSurname;
  const std::string kept = h.firstName + " " + own;
  const std::string taken = h.firstName + " " + spouse;
  const std::string joined = h.firstName + " " + own + "-" + spouse;
  const std::vector<std::string> extra = out.extra;
  out = AgeUp();
  out.extra = extra;
  out.table = "registry:ScenarioNameChangeMarriage";
  out.title = "Identity";  // #26311
  out.body = "Now that you're married, you're wondering if you should change your last "
             "name.\n\nWhat last name will you choose?";  // #32275
  out.body += "\n\nYour Last Name: " + own;             // #50142
  out.body += "\n" + h.firstName + " Last Name: " + spouse;  // #69671
  out.body += "\nName: " + kept;                        // #31091
  out.options = {"Keep " + kept, "Change to " + taken, "All the way: " + joined};
  int lane = forcedChoice;
  if (lane < 0 || lane > 2) lane = int(nameRng_.below(3));
  out.choice = out.options[size_t(lane)];
  if (lane == 1) {
    h.nameChangeMaiden = own;
    h.lastName = spouse;
    out.narrative = "I took the last name " + spouse + ".";
  } else if (lane == 2) {
    h.nameChangeMaiden = own;
    h.lastName = own + "-" + spouse;
    out.narrative = "I took both last names and became " + joined + ".";
  } else {
    out.narrative = "I kept the last name " + own + ".";
  }
  return true;
}

bool Sim::loverLeavesYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.loverName.empty()) return false;
  if (h.age < 18) return false;
  if (forcedChoice < 0 && loverRng_.below(5) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const bool married = h.loverStatus == "married";
  out.table = "relationship";
  out.row_id = "0";
  out.body = h.loverName + (married ? " wants a divorce." : " wants to break up with you.");
  out.body[0] = static_cast<char>(::toupper(static_cast<unsigned char>(out.body[0])));
  const int lane = forcedChoice >= 0 ? forcedChoice : int(loverRng_.below(h.vampire ? 3 : 2));
  if (lane == 0) {
    out.choice = "Beg " + h.loverName + " to stay";
    if (h.relationship >= 40) {
      out.narrative = "I begged " + h.loverName + " to stay, and " +
                      (h.loverFemale ? "she" : "he") + " did.";
      h.relationship = std::min(100, h.relationship + 10);
      return true;
    }
    out.narrative = "I begged " + h.loverName + " to stay, but " +
                    (h.loverFemale ? "she" : "he") + " left anyway.";
  } else if (lane == 2) {
    out.choice = "Hypnotize " + h.loverName + " to stay";
    out.narrative = "I hypnotized " + h.loverName + " to stay.";
    h.relationship = std::min(100, h.relationship + 20);
    return true;
  } else {
    out.choice = "Wish " + h.loverName + " well";
    out.narrative = "I wished " + h.loverName + " well.";
  }
  // Both leaving lanes record the ex and clear the pairing, exactly as the port's own
  // break-up block does, then push the announcement for the journal (REPORT 125/134).
  Hero::ExLover ended;
  ended.name = h.loverName;
  ended.female = h.loverFemale;
  ended.age = h.loverAge;
  ended.status = h.loverStatus;
  ended.relationship = h.relationship;
  ended.year = h.age;
  ended.city = h.city;
  h.exLovers.push_back(ended);
  ++h.exes;
  if (married) h.nameChangeDivorcePending = true;  // REPORT.en.md 397
  h.breakUpPending = true;
  h.breakUpWord = h.loverStatus;
  h.breakUpName = h.loverName;
  h.breakUpFull = h.loverName;
  h.loverName.clear();
  h.loverStatus.clear();
  h.relationship = 0;
  life.pendingCards.push_back(kPendingBreakUpAnnouncement);
  return true;
}
// ScenarioExLoverInboundSext (REPORT.en.md 173): the ex sends a picture.
//
// RefreshScenario @0x73f3de5450:
//   DataHub::GetRandomSexyTextItemForGender(ex gender)   -> {2}
//   String::Replace(<pronoun> in it, the ex's pronoun)    -> the item reads right
//   body     'Your {0}, {1}, just sent you {2}.'   #50742
//   journal  'My {0}, {1}, sent me {2}.'            #30997
//   buttons  'Send {0} a txxxt back' (#38590)  'Forward it to my friends' (#20572)
//            'Do these save to the cloud?' (#17931)  'File a restraining order' (#20129)
//
// The item pool is a16_sexy-text-items-enc (Item, Gender, Clean) -- the port had never
// read it, which is why this card stopped at a hole in REPORT.en.md 172.
bool Sim::exLoverSextYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  const Hero::ExLover* ex = h.latestEx();
  if (ex == nullptr) return false;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.age < 18) return false;
  if (!ensureTable("a16_sexy-text-items-enc")) return false;
  if (forcedChoice < 0 && loverRng_.below(14) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_sexy-text-items-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string g = trim(t.at(r, "Gender"));
    if (!g.empty() && g != "both" && g != (ex->female ? "female" : "male")) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[loverRng_.below(uint32_t(rows.size()))];
  std::string item = trim(t.at(row, "Item"));
  const std::string poss = ex->female ? "her" : "his";
  for (size_t at = item.find("<pronoun>"); at != std::string::npos;
       at = item.find("<pronoun>", at)) {
    item.replace(at, 9, poss);
    at += poss.size();
  }
  const std::string exWord = lower(trim(ex->status == "married"
                                        ? (ex->female ? "Ex-Wife" : "Ex-Husband")
                                        : (ex->female ? "Ex-Girlfriend" : "Ex-Boyfriend")));
  out.table = "a16_sexy-text-items-enc";
  out.row_id = std::to_string(row + 1);
  out.body = "Your " + exWord + ", " + ex->name + ", just sent you " + item + ".";
  out.body[0] = static_cast<char>(::toupper(static_cast<unsigned char>(out.body[0])));
  const int lane = forcedChoice >= 0 ? forcedChoice : int(loverRng_.below(3));
  if (lane == 0) {
    out.choice = "Send " + ex->name + " a txxxt back";
    out.narrative = "My " + exWord + ", " + ex->name + ", sent me " + item +
                    ". I sent " + (ex->female ? "her" : "him") + " one back.";
  } else if (lane == 1) {
    out.choice = "Forward it to my friends";
    out.narrative = "My " + exWord + ", " + ex->name + ", sent me " + item +
                    ". I forwarded it to all of my friends.";
  } else {
    out.choice = "Do these save to the cloud?";
    out.narrative = "My " + exWord + ", " + ex->name + ", sent me " + item + ".";
  }
  return true;
}
// ScenarioExLoverMarriedNow (REPORT.en.md 174): the ex has married someone else.
//
//   body     'Your {0}, {1}, said {2} can't {3} you because {4} is married{5}.'
//   {3}      'start dating' (#66066) or 'get back together with' (#59507)
//   journal  the same sentence in the first person (#30996)
//   buttons  'Respect that' (#36054)  'Ask {0} to come over anyway' (#12217)
//            'Convince {0} to divorce {1}' (#16351)
//
// {5} is a tail the port leaves empty: the literal table has no candidate that reads as
// a completion of 'is married', and a wrong one would change the sentence's meaning.
bool Sim::exLoverMarriedNowYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  const Hero::ExLover* ex = h.latestEx();
  if (ex == nullptr) return false;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.age < 18) return false;
  if (forcedChoice < 0 && loverRng_.below(12) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const std::string exWord = lower(trim(ex->status == "married"
                                        ? (ex->female ? "Ex-Wife" : "Ex-Husband")
                                        : (ex->female ? "Ex-Girlfriend" : "Ex-Boyfriend")));
  const std::string subj = ex->female ? "she" : "he";
  // The verb the card offers the hero: getting back together if there was a romance,
  // starting to date if the pair never were (the two literals the class carries).
  const std::string verb = (ex->status == "dating" || ex->status == "engaged" ||
                            ex->status == "married")
                               ? "get back together with"
                               : "start dating";
  out.table = "relationship";
  out.row_id = "0";
  out.body = "Your " + exWord + ", " + ex->name + ", said " + subj + " can't " +
             verb + " you because " + subj + " is married.";
  out.body[0] = static_cast<char>(::toupper(static_cast<unsigned char>(out.body[0])));
  const std::string journalHead = "My " + exWord + ", " + ex->name + ", said " + subj +
                                 " can't " + verb + " me because " + subj +
                                 " is married.";
  const int lane = forcedChoice >= 0 ? forcedChoice : int(loverRng_.below(3));
  if (lane == 0) {
    out.choice = "Respect that";
    out.narrative = journalHead;
  } else if (lane == 1) {
    out.choice = "Ask " + ex->name + " to come over anyway";
    out.narrative = journalHead + " I asked " + (ex->female ? "her" : "him") +
                    " to come over anyway.";
    out.karma = pointsForImpact(-2);
  } else {
    // The divorce lane opens the court chain, which is not ported (REPORT 172).
    out.choice = "Convince " + ex->name + " to divorce " + (ex->female ? "him" : "her");
    out.narrative = journalHead + " I tried to talk " + (ex->female ? "her" : "him") +
                    " into a divorce.";
    out.karma = pointsForImpact(-3);
  }
  return true;
}
// ScenarioChildDiscipline (REPORT.en.md 179): a child misbehaves.
//
// The card is table-driven for its text and code-built for its buttons:
//   body     a16_child-discipline-enc, filtered by Min/Max Age, Child Gender, Hero Gender,
//            Student and Clean, with <child> / <child-possessive> substituted
//   journal  the same row's 'Log Version'
//   buttons  'Yell at {0}' (#47498)  'Ground {0}' (#22277)
//            'Have a talk with {0}' (#22687)  'Do nothing' (#17927)
//
// shortcut: the handler's stat moves were not read, so the lanes differ only in their
// journal -- the card is about the exchange, not about a number.
bool Sim::childDisciplineYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (life.children.empty()) return false;
  if (!ensureTable("a16_child-discipline-enc")) return false;
  Child* kid = nullptr;
  for (Child& c : life.children) {
    if (!c.alive || c.movedOut) continue;
    if (c.age < 2 || c.age > 17) continue;
    if (c.city != h.city) continue;
    kid = &c;
    break;
  }
  if (kid == nullptr) return false;
  if (forcedChoice < 0 && loverRng_.below(8) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const std::string name = kid->name.empty() ? std::string("the child") : kid->name;
  const std::string poss = kid->female ? "her" : "his";
  const Table& t = tbl("a16_child-discipline-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const double mn = t.num(r, "Min Age");
    const double mx = t.num(r, "Max Age");
    if (mn > 0 && kid->age < mn) continue;
    if (mx > 0 && kid->age > mx) continue;
    const std::string g = trim(t.at(r, "Child Gender"));
    if (!g.empty() && g != "both" && g != (kid->female ? "female" : "male")) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[loverRng_.below(uint32_t(rows.size()))];
  auto fill = [&](std::string s) {
    for (size_t at = s.find("<child-possessive>"); at != std::string::npos;
         at = s.find("<child-possessive>", at)) {
      s.replace(at, 17, poss);
      at += poss.size();
    }
    for (size_t at = s.find("<child>"); at != std::string::npos;
         at = s.find("<child>", at)) {
      s.replace(at, 7, name);
      at += name.size();
    }
    return s;
  };
  out.table = "a16_child-discipline-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(fill(trim(t.at(row, "Scenario"))));
  const std::string log = fill(trim(t.at(row, "Log Version")));
  const int lane = forcedChoice >= 0 ? forcedChoice : int(loverRng_.below(4));
  if (lane == 0) {
    out.choice = "Yell at " + name;
    out.narrative = log;
  } else if (lane == 1) {
    // The grounded lane is the one the handler details: a roll of 1..30 days
    // (GetRandomNumber(0, 0x1e) at 0x73f3a86718), ImpactDiscipline(+1) and the class's
    // own sentence #69569.
    const int days = 1 + int(loverRng_.below(30));
    out.choice = "Ground " + name;
    out.narrative = log + " I grounded " + (kid->female ? "her" : "him") + " for " +
                    std::to_string(days) + " days.";
    out.discipline = pointsForImpact(1);
  } else if (lane == 2) {
    out.choice = "Have a talk with " + name;
    out.narrative = log;
  } else {
    out.choice = "Do nothing";
    out.narrative = log;
  }
  return true;
}
// ScenarioChildDemands (REPORT.en.md 181): a child asks for money.
//
// The row is the card: a16_child-demands-enc carries Age, Amount, and two
// comma-separated option lists (Boy, Girl). The class supplies the sentences:
//
//   body     'Your {0}, {1}, is asking you for {2} to {3}.'        #50717
//   buttons  'Yes, give {0} {1}' (#47505)  'No, {0} has enough' (#31869)
//   journals 'I gave my {0}, {1}, {2} to {3}.'                    #24224
//            'I refused to give my {0}, {1}, {2} to {3}.'         #24941
bool Sim::childDemandsYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  // A single parent is asked too, so this card does not need a partner -- only a child.
  if (life.children.empty()) return false;
  if (!ensureTable("a16_child-demands-enc")) return false;
  Child* kid = nullptr;
  for (Child& c : life.children) {
    if (!c.alive || c.movedOut || c.city != h.city) continue;
    if (c.age < 4 || c.age > 17) continue;
    kid = &c;
    break;
  }
  if (kid == nullptr) return false;
  if (forcedChoice < 0 && childDemandRng_.below(3) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_child-demands-enc");
  // Every row the child is old enough for is eligible; picking the last one made the
  // card ask the same thing every time (REPORT.en.md 235).
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (t.num(r, "Age") <= kid->age) rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[childDemandRng_.below(uint32_t(rows.size()))];
  const long long amount = static_cast<long long>(t.num(row, "Amount"));
  std::vector<std::string> options;
  for (const char* col : {"Boy", "Girl"}) {
    const std::string list = trim(t.at(row, col));
    if (list.empty()) continue;
    size_t at = 0;
    while (at <= list.size()) {
      const size_t comma = list.find(',', at);
      const std::string opt = trim(list.substr(at, comma == std::string::npos
                                                        ? std::string::npos
                                                        : comma - at));
      if (!opt.empty()) options.push_back(opt);
      if (comma == std::string::npos) break;
      at = comma + 1;
    }
  }
  if (options.empty()) return false;
  const std::string demand = options[loverRng_.below(uint32_t(options.size()))];
  const std::string word = kid->female ? "daughter" : "son";
  const std::string name = kid->name.empty() ? std::string("the child") : kid->name;
  out.table = "a16_child-demands-enc";
  out.row_id = std::to_string(row + 1);
  out.body = "Your " + word + ", " + name + ", is asking you for " + money(amount) +
             " to " + demand + ".";
  out.body[0] = static_cast<char>(::toupper(static_cast<unsigned char>(out.body[0])));
  const int lane = forcedChoice >= 0 ? forcedChoice : int(loverRng_.below(2));
  if (lane == 0) {
    out.choice = "Yes, give " + name + " " + money(amount);
    h.money -= amount;
    out.narrative = "I gave my " + word + ", " + name + ", " + money(amount) +
                    " to " + demand + ".";
  } else {
    out.choice = "No, " + name + " has enough";
    out.narrative = "I refused to give my " + word + ", " + name + ", " +
                    money(amount) + " to " + demand + ".";
  }
  return true;
}
// ScenarioParanormalEncounter (REPORT.en.md 186): something is there.
//
// a16_paranormal-encounters-enc, 19 rows: Scenario, Log Version, Gender, Entity,
// Danger Score. A report card -- the row is the whole exchange -- drawn rarely and
// filtered by the hero's gender.
//
// shortcut: Danger Score is not turned into damage. The game's own use of it was not
// read, and a health hit on a random encounter would be a rule the port invented.
bool Sim::paranormalEncounterYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_paranormal-encounters-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(22) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_paranormal-encounters-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string g = trim(t.at(r, "Gender"));
    if (!g.empty() && g != "both" && g != (h.female ? "female" : "male")) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_paranormal-encounters-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  out.narrative = spyUpperFirst(resolve(trim(t.at(row, "Log Version"))));
  return true;
}
// ScenarioStalker (REPORT.en.md 187): somebody is following the hero.
//
// a16_stalker-scenarios-enc, 37 rows: Scenario, Log Version, Can Assault, Stalker
// Gender, Hero Gender. A report card, filtered by both genders, drawn rarely.
//
// shortcut: Can Assault is not acted on -- the assault card is a different class and
// is not ported, so this card reports being watched and stops there.
bool Sim::stalkerYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_stalker-scenarios-enc")) return false;
  if (h.age < 16) return false;
  if (forcedChoice < 0 && ambientRng_.below(20) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_stalker-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string hero = trim(t.at(r, "Hero Gender"));
    if (!hero.empty() && hero != "both" && hero != (h.female ? "female" : "male")) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_stalker-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  out.narrative = spyUpperFirst(resolve(trim(t.at(row, "Log Version"))));
  out.happiness -= 2;
  return true;
}
// ScenarioParentCheat (REPORT.en.md 188): the hero walks in on a parent.
//
// a16_parent-cheat-scenarios-enc, 36 rows: Scenario, Log Version, Parent Gender,
// Parent Might Notice, Hero Min/Max Age. The body carries an alternative
// '<a/b/c>' and the journal refers back to whichever the body chose ('<rb>'), so the
// card picks once and uses it twice.
bool Sim::parentCheatYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_parent-cheat-scenarios-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(30) != 0) return false;
  const Person* parent = nullptr;
  for (const Person& p : life.family) {
    if (p.relation != "mother" && p.relation != "father") continue;
    if (!p.alive) continue;
    parent = &p;
    break;
  }
  if (parent == nullptr) {
    return false;
  }
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_parent-cheat-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string g = trim(t.at(r, "Parent Gender"));
    if (!g.empty() && g != "both" && g != (parent->female ? "female" : "male")) continue;
    const double mn = t.num(r, "Hero Min Age");
    const double mx = t.num(r, "Hero Max Age");
    if (mn > 0 && h.age < mn) continue;
    if (mx > 0 && h.age > mx) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  std::string body = trim(t.at(row, "Scenario"));
  std::string chosen;
  const size_t open = body.find('<');
  const size_t close = body.find('>', open == std::string::npos ? 0 : open);
  if (open != std::string::npos && close != std::string::npos) {
    const std::string group = body.substr(open + 1, close - open - 1);
    if (group.find('/') != std::string::npos) {
      std::vector<std::string> alts;
      splitOn(group, '/', alts);
      chosen = alts[ambientRng_.below(uint32_t(alts.size()))];
      body.replace(open, close - open + 1, chosen);
    }
  }
  std::string log = trim(t.at(row, "Log Version"));
  for (size_t at = log.find("<rb>"); at != std::string::npos && !chosen.empty();
       at = log.find("<rb>", at)) {
    log.replace(at, 4, chosen);
    at += chosen.size();
  }
  out.table = "a16_parent-cheat-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(body);
  out.narrative = spyUpperFirst(log);
  out.happiness -= 3;
  return true;
}
// The ambient year (REPORT.en.md 190): one coin, one card, three tables.
//
// The three ambient cards used to be dispatched separately, which made them compete
// for the year: dispatchYear returns at the first card that fires, so each one's
// rate was its share of the years the others left (REPORT 189.1). One entry point
// fixes the total rate and divides it by weight instead.
bool Sim::ambientYear(Life& life, AgeUp& out, int forcedChoice) {
  // 1 in 12 years attempts an ambient card. The sub-cards can still decline (no parent,
  // no rows for the hero's gender, no child), which is why the measured rate is well
  // under that (REPORT.en.md 190).
  if (forcedChoice < 0 && ambientRng_.below(12) != 0) return false;
  // The weights are the proportions §189.1 measured, kept so the merge changes the
  // split rather than inventing one.
  const int roll = int(ambientRng_.below(100));
  if (roll < 55) return paranormalEncounterYear(life, out, 0);
  if (roll < 90) return stalkerYear(life, out, 0);
  return parentCheatYear(life, out, 0);
}
// ScenarioGenderIdentity (REPORT.en.md 194): the hero's sense of self.
//
// a16_gender-scenarios-enc, 28 rows: Scenario, Log Version, Gender, Min Age, Max Age,
// Transgender Identity, Other Identity, Student, Both Parents Alive. A report card,
// filtered by the hero's gender and age, with the row's 'Other Identity' naming the
// identity the hero takes up.
//
// shortcut: the identity is reported, not stored -- the port's Hero has no identity
// field, and adding one is a structural change this card does not need.
bool Sim::genderIdentityYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_gender-scenarios-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(6) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_gender-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string g = trim(t.at(r, "Gender"));
    if (!g.empty() && g != "both" && g != (h.female ? "female" : "male")) continue;
    const double mn = t.num(r, "Min Age");
    const double mx = t.num(r, "Max Age");
    if (mn > 0 && h.age < mn) continue;
    if (mx > 0 && h.age > mx) continue;
    if (t.flag(r, "Student") && !h.schooling) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_gender-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  out.narrative = spyUpperFirst(resolve(trim(t.at(row, "Log Version"))));
  // The row flags one of the two identity columns; the port keeps the fact
  // (REPORT.en.md 195). No string is stored because the columns hold 'x', not a name.
  if (t.flag(row, "Transgender Identity") || t.flag(row, "Other Identity")) {
    h.identityQuestioned = true;
  }
  return true;
}
// Two report cards off one helper (REPORT.en.md 196).
//
// a16_paranormal-phenomena-enc and a16_surrogate-run-off-scenarios-enc are the same
// shape: Scenario, Log Version, nothing else. Rather than two near-identical
// functions, one helper renders any such table and each card is a wrapper that sets
// its own coin and gate.
bool Sim::tableReport(const char* table, int oneIn, AgeUp& out) {
  if (!ensureTable(table)) return false;
  const Table& t = tbl(table);
  if (t.size() == 0 || !t.has("Scenario")) return false;
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (!trim(t.at(r, "Scenario")).empty()) rows.push_back(r);
  }
  if (rows.empty()) return false;
  (void)oneIn;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = table;
  out.row_id = std::to_string(row + 1);
  // The rows carry tokens ('<musician>' and friends); the shared resolver answers what it
  // can and leaves the rest, which the residue probe then reports (REPORT.en.md 205).
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  if (t.has("Log Version")) {
    const std::string log = trim(t.at(row, "Log Version"));
    if (!log.empty()) out.narrative = spyUpperFirst(resolve(log));
  }
  return true;
}

bool Sim::paranormalPhenomenaYear(Life& life, AgeUp& out, int forcedChoice) {
  if (forcedChoice < 0 && ambientRng_.below(50) != 0) return false;
  hero_ = &life.hero;
  life_ = &life;
  return tableReport("a16_paranormal-phenomena-enc", 50, out);
}

// a16_surrogate-run-off-scenarios-enc is NOT ported: its rows say
// '<surrogate> just texted you ... she is keeping the baby.', and the token needs a
// surrogate actor the port does not have (REPORT.en.md 196). The table stays untouched
// until one exists -- the same shape as the custody card (REPORT 183).
// a16_adoption-scenarios-enc (REPORT.en.md 197): the hero's own adoption beat.
//
// Scenario, Log Version, Adoptee Gender, Adoptee Min/Max Age -- and nothing but
// alternative tokens in the text, which the shared resolver already draws and caches
// for '<rb>'. So the card is a tableReport with a gender filter.
bool Sim::adoptionScenarioYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (forcedChoice < 0 && ambientRng_.below(45) != 0) return false;
  if (!ensureTable("a16_adoption-scenarios-enc")) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_adoption-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string g = trim(t.at(r, "Adoptee Gender"));
    if (!g.empty() && g != "both" && g != (h.female ? "female" : "male")) continue;
    const double mn = t.num(r, "Adoptee Min Age");
    const double mx = t.num(r, "Adoptee Max Age");
    if (mn > 0 && h.age < mn) continue;
    if (mx > 0 && h.age > mx) continue;
    if (trim(t.at(r, "Scenario")).empty()) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  // '<age>' and '<adoptee>' need the adoptee this card does not name; the card declines
  // rather than printing the token (REPORT.en.md 206).
  if (resolve(trim(t.at(row, "Scenario"))).find('<') != std::string::npos) return false;
  out.table = "a16_adoption-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  const std::string log = trim(t.at(row, "Log Version"));
  if (!log.empty()) out.narrative = spyUpperFirst(log);
  return true;
}
// ScenarioBoost (REPORT.en.md 198): a chance to improve one attribute.
//
// a16_boosts-enc, 125 rows: Attribute, Scenario, Conclusion, Accept Choice, Reject
// Choice, Accept Feedback, Log Version, Gender, Min/Max Age. The row carries the whole
// card -- body, both buttons and both journals -- and the Attribute names the stat the
// accept lane moves.
//
// shortcut: the size of the boost is not in the table. The port moves the attribute by
// pointsForImpact(+1) = 4, the smallest step in its own scale, and says so here.
bool Sim::boostYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_boosts-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(14) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_boosts-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string g = trim(t.at(r, "Gender"));
    if (!g.empty() && g != "both" && g != (h.female ? "female" : "male")) continue;
    const double mn = t.num(r, "Min Age");
    const double mx = t.num(r, "Max Age");
    if (mn > 0 && h.age < mn) continue;
    if (mx > 0 && h.age > mx) continue;
    if (trim(t.at(r, "Scenario")).empty()) continue;
    if (trim(t.at(r, "Accept Choice")).empty()) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  const std::string attribute = trim(t.at(row, "Attribute"));
  // A row whose text still holds a token after resolution is skipped rather than
  // rendered with a hole (REPORT.en.md 206): '<random-person-indefinite>' needs an
  // actor this card does not have.
  if (resolve(trim(t.at(row, "Scenario"))).find('<') != std::string::npos) return false;
  out.table = "a16_boosts-enc";
  out.row_id = std::to_string(row + 1);
  // The body is the scenario and its conclusion, which is how the table reads: the
  // situation, then the urge it creates.
  std::string body = trim(t.at(row, "Scenario"));
  const std::string conclusion = trim(t.at(row, "Conclusion"));
  if (!conclusion.empty()) body += body.empty() ? conclusion : " " + conclusion;
  out.body = spyUpperFirst(body);
  const int lane = forcedChoice >= 0 ? forcedChoice : int(ambientRng_.below(2));
  if (lane == 0) {
    out.choice = spyUpperFirst(resolve(trim(t.at(row, "Accept Choice"))));
    out.narrative = spyUpperFirst(resolve(trim(t.at(row, "Log Version"))));
    const int gain = pointsForImpact(1);
    if (attribute == "Happiness") out.happiness += gain;
    else if (attribute == "Health") out.health += gain;
    else if (attribute == "Smarts") out.smarts += gain;
    else if (attribute == "Looks") out.looks += gain;
    const std::string feedback = trim(t.at(row, "Accept Feedback"));
    if (!feedback.empty()) out.extra.push_back(spyUpperFirst(feedback));
  } else {
    out.choice = spyUpperFirst(resolve(trim(t.at(row, "Reject Choice"))));
    out.narrative = "I turned it down.";
  }
  // A field that still holds a token after resolution means the row needs an actor this
  // card does not have; the card declines rather than printing the token (REPORT 206).
  for (const std::string* field : {&out.body, &out.choice, &out.narrative}) {
    if (field->find('<') != std::string::npos) {
      // A card that writes and then declines must not leave its strings behind: the next
      // card in the chain would inherit the button and the table would not match it
      // (REPORT.en.md 208).
      out = AgeUp();
      return false;
    }
  }
  return true;
}
// ScenarioJobTermination (REPORT.en.md 201): the hero is let go.
bool Sim::jobTerminationYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.job.empty()) return false;
  // A student is not employed and cannot be let go (REPORT.en.md 232).
  if (h.job == "Student") return false;
  if (!ensureTable("a16_termination-scenarios-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(28) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_termination-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string cond = trim(t.at(r, "Condition"));
    if (!cond.empty() && !(cond == "Age" && h.age >= 55)) continue;
    const std::string g = trim(t.at(r, "Gender"));
    if (!g.empty() && g != "both" && g != (h.female ? "female" : "male")) continue;
    if (trim(t.at(r, "Scenario")).empty()) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_termination-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  out.narrative = "I was let go from my job as " + h.job + ".";
  h.employed = false;
  h.job.clear();
  out.happiness -= 6;
  return true;
}
// ScenarioMoneyRequestRejection (REPORT.en.md 202): the hero asks a relative for money
// and is turned down.
//
// a16_money-request-rejections-enc, 34 rows: Scenario, Log Version, Multiple Request,
// Hero Min/Max Age, Person Gender, Person Min/Max Money, Hero Needs Money, Hero Doesn't
// Need Money, Parent, Sibling, Lover, Child. A row flags which relatives it can be
// about, which is how '<person>' is answered: the flag picks the relation, the
// relation picks the actor the port already has.
bool Sim::moneyRequestRejectionYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_money-request-rejections-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(24) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_money-request-rejections-enc");
  // The relatives the hero can ask, in the order the row's flags are checked.
  struct Who { const char* flag; std::string phrase; bool female; };
  std::vector<Who> roster;
  for (const Person& p : life.family) {
    if (!p.alive) continue;
    const char* flag = p.relation == "mother" || p.relation == "father" ? "Parent"
                       : p.relation == "sister" || p.relation == "brother" ? "Sibling"
                       : nullptr;
    if (flag == nullptr) continue;
    roster.push_back(Who{flag, p.name.empty() ? "my " + p.relation
                                                        : "my " + p.relation + ", " + p.name + ",",
                         p.female});
  }
  if (!h.loverName.empty()) {
    roster.push_back(Who{"Lover", "my " + (h.loverFemale ? std::string("girlfriend")
                                             : std::string("boyfriend")) +
                                   ", " + h.loverName + ",", h.loverFemale});
  }
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const double mn = t.num(r, "Hero Min Age");
    const double mx = t.num(r, "Hero Max Age");
    if (mn > 0 && h.age < mn) continue;
    if (mx > 0 && h.age > mx) continue;
    if (trim(t.at(r, "Scenario")).empty()) continue;
    bool any = false;
    for (const Who& w : roster) if (t.flag(r, w.flag)) any = true;
    if (!any) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  std::vector<std::string> asked;
  std::vector<bool> askedFemale;
  for (const Who& w : roster) {
    if (!t.flag(row, w.flag)) continue;
    asked.push_back(w.phrase);
    askedFemale.push_back(w.female);
  }
  if (asked.empty()) return false;
  const size_t pickAsked = ambientRng_.below(uint32_t(asked.size()));
  const std::string who = asked[pickAsked];
  auto fill = [&](std::string s) {
    for (size_t at = s.find("<person>"); at != std::string::npos;
         at = s.find("<person>", at)) {
      s.replace(at, 8, who);
      at += who.size();
    }
    return s;
  };
  out.table = "a16_money-request-rejections-enc";
  out.row_id = std::to_string(row + 1);
  // The row's other tokens ('<him/her>' and friends) are answered by the shared
  // resolver with the partner's gender set, the same way the gift card does it
  // (REPORT.en.md 202).
  const bool savedSet = pronounOtherSet_;
  const bool savedFemale = pronounOtherFemale_;
  pronounOtherSet_ = true;
  pronounOtherFemale_ = askedFemale[pickAsked];
  out.body = spyUpperFirst(resolve(fill(trim(t.at(row, "Scenario")))));
  out.narrative = spyUpperFirst(resolve(fill(trim(t.at(row, "Log Version")))));
  pronounOtherSet_ = savedSet;
  pronounOtherFemale_ = savedFemale;
  return true;
}
// a16_social-media-troll-outbounds-enc (REPORT.en.md 203): the hero trolls back.
//
// Scenario, Log Version, Is BitLifeApp, Twitch Exempt, Twitch Only. The text carries
// '<insult>', which the shared resolver already answers out of the insult pool
// (REPORT.en.md 138), so the card is a filtered report.
bool Sim::trollOutboundYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_social-media-troll-outbounds-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(26) != 0) return false;
  if (h.age < 13) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_social-media-troll-outbounds-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Scenario")).empty()) continue;
    // 'Twitch Only' rows need a streaming career the port does not model, and a row whose
    // text names a '<channel>' needs the platform the hero posts to, which this card does
    // not carry (REPORT.en.md 203).
    if (t.flag(r, "Twitch Only")) continue;
    if (t.at(r, "Scenario").find("<channel>") != std::string::npos) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_social-media-troll-outbounds-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  out.narrative = spyUpperFirst(resolve(trim(t.at(row, "Log Version"))));
  out.karma = pointsForImpact(-1);
  return true;
}
// ScenarioAlcohol (REPORT.en.md 204): a drink is offered.
//
// a16_alcohol-scenarios-enc: Condition, Scenario, CTA, Yes Choice, No Choice. The
// Condition picks who is offering -- a job, a club, a lover, or nobody in particular --
// and '<singular>' is answered out of a16_alcohol-enc's own Singular column, which is
// the table that exists for exactly that token.
bool Sim::alcoholScenarioYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_alcohol-scenarios-enc") || !ensureTable("a16_alcohol-enc")) return false;
  if (h.age < 18) return false;
  if (forcedChoice < 0 && ambientRng_.below(8) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_alcohol-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string cond = trim(t.at(r, "Condition"));
    if (cond == "Job" && h.job.empty()) continue;
    if (cond == "Lover" && h.loverName.empty()) continue;
    if (cond == "Club" && h.loverName.empty() && h.job.empty()) continue;
    if (trim(t.at(r, "Scenario")).empty()) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  const Table& a = tbl("a16_alcohol-enc");
  if (a.size() == 0) return false;
  const size_t drink = ambientRng_.below(uint32_t(a.size()));
  const std::string singular = trim(a.at(drink, "Singular"));
  const std::string plural = trim(a.at(drink, "Plural"));
  const std::string indefinite = trim(a.at(drink, "Indefinite"));
  // The three forms the drink table carries, each answered from its own column
  // (REPORT.en.md 204): '<singular>' the glass, '<plural>' the bottles, '<indefinite>'
  // the bare name for a sentence that already supplies its own article.
  auto fill = [&](std::string s) {
    const std::pair<const char*, std::string> forms[] = {
        {"<singular>", singular.empty() ? std::string("a drink") : singular},
        {"<plural>", plural.empty() ? std::string("drinks") : plural},
        {"<indefinite>", indefinite.empty() ? std::string("a drink") : indefinite},
    };
    for (const auto& form : forms) {
      const size_t len = std::strlen(form.first);
      for (size_t at = s.find(form.first); at != std::string::npos;
           at = s.find(form.first, at)) {
        s.replace(at, len, form.second);
        at += form.second.size();
      }
    }
    return s;
  };
  out.table = "a16_alcohol-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(fill(trim(t.at(row, "Scenario")))));
  const int lane = forcedChoice >= 0 ? forcedChoice : int(ambientRng_.below(2));
  if (lane == 0) {
    out.choice = spyUpperFirst(resolve(fill(trim(t.at(row, "Yes Choice")))));
    out.narrative = "I went for " + (singular.empty() ? std::string("a drink") : singular) + ".";
    out.health -= 2;
    out.happiness += 2;
  } else {
    out.choice = spyUpperFirst(resolve(fill(trim(t.at(row, "No Choice")))));
    out.narrative = "I turned " + singular + " down.";
    out.discipline = pointsForImpact(1);
  }
  return true;
}
// Two report cards from tables with no entity tokens (REPORT.en.md 208).
//
// a16_surrogate-rejections-enc: Scenario, Gender, Gay, Min/Max Age, Single. A row
// flagged Gay or Single needs a state the port does not model, so those rows are
// skipped rather than guessed at.
bool Sim::surrogateRejectionYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  // A report card must not take the newborn years: the play driver needs the first event
  // of a life to carry choices (REPORT.en.md 231).
  if (h.age < 10) return false;
  if (!ensureTable("a16_surrogate-rejections-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(40) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_surrogate-rejections-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Scenario")).empty()) continue;
    if (t.flag(r, "Gay") || t.flag(r, "Single")) continue;
    const std::string g = trim(t.at(r, "Gender"));
    if (!g.empty() && g != "both" && g != (h.female ? "female" : "male")) continue;
    const double mn = t.num(r, "Min Age");
    const double mx = t.num(r, "Max Age");
    if (mn > 0 && h.age < mn) continue;
    if (mx > 0 && h.age > mx) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_surrogate-rejections-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  return true;
}

// a16_time-machine-scenarios-enc is read only by useTimeMachine (REPORT.en.md 310):
// native prints one of those lines after a real restore and never from a wheel seat,
// so the old ambient card was removed in 315.
// Two more tables, both blocked only on a token the port can answer (REPORT 209).
//
// a16_arrest-scenarios-enc: Scenario, Min/Max Attitude, Hero Gender, Serious Offense
// Only. The text carries '<insult>' (the §138 pool) and '<numN-M>', which the resolver
// already draws. The Attitude gate belongs to a stat the port does not have, so rows
// are filtered by gender and by the serious-offence flag alone.
bool Sim::arrestScenarioYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_arrest-scenarios-enc")) return false;
  if (h.age < 12) return false;
  if (forcedChoice < 0 && ambientRng_.below(12) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_arrest-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Scenario")).empty()) continue;
    const std::string g = trim(t.at(r, "Hero Gender"));
    if (!g.empty() && g != "both" && g != (h.female ? "female" : "male")) continue;
    if (t.flag(r, "Serious Offense Only") && !h.inPrison) continue;
    if (resolve(trim(t.at(r, "Scenario"))).find('<') != std::string::npos) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_arrest-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  return true;
}

// a16_child-pressure-rejections-enc: Feedback Scenario, Log Version, Min/Max Age,
// Commonness. '<child>' is the child who refused and '<him/her>' that child, so the
// pronoun pair is set from the child's own gender.
bool Sim::childPressureRejectionYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (life.children.empty()) return false;
  if (!ensureTable("a16_child-pressure-rejections-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(30) != 0) return false;
  Child* kid = nullptr;
  for (Child& c : life.children) {
    if (!c.alive || c.movedOut || c.city != h.city) continue;
    kid = &c;
    break;
  }
  if (kid == nullptr) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_child-pressure-rejections-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const double mn = t.num(r, "Min Age");
    const double mx = t.num(r, "Max Age");
    if (mn > 0 && kid->age < mn) continue;
    if (mx > 0 && kid->age > mx) continue;
    if (trim(t.at(r, "Feedback Scenario")).empty()) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  const std::string name = kid->name.empty() ? std::string("the child") : kid->name;
  const bool savedSet = pronounOtherSet_;
  const bool savedFemale = pronounOtherFemale_;
  pronounOtherSet_ = true;
  pronounOtherFemale_ = kid->female;
  auto fill = [&](std::string s) {
    for (size_t at = s.find("<child>"); at != std::string::npos;
         at = s.find("<child>", at)) {
      s.replace(at, 7, name);
      at += name.size();
    }
    return resolve(s);
  };
  out.table = "a16_child-pressure-rejections-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(fill(trim(t.at(row, "Feedback Scenario"))));
  out.narrative = spyUpperFirst(fill(trim(t.at(row, "Log Version"))));
  pronounOtherSet_ = savedSet;
  pronounOtherFemale_ = savedFemale;
  return true;
}
// ScenarioNotificationBait (REPORT.en.md 210): a phone notification that remembers
// something.
//
// a16_notification-bait-enc, 36 rows: Index, Author, Notification, Log Version, Hero
// Min/Max Age, Student, Job, Hero Has/Doesn't Have Lover, Both Parents Alive, Hero
// Gender, Lover Gender. A report card; the flags are gates the port can evaluate, and a
// row whose text still needs an actor after resolution is skipped.
bool Sim::notificationBaitYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_notification-bait-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(20) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_notification-bait-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string note = trim(t.at(r, "Notification"));
    if (note.empty()) continue;
    const double mn = t.num(r, "Hero Min Age");
    const double mx = t.num(r, "Hero Max Age");
    if (mn > 0 && h.age < mn) continue;
    if (mx > 0 && h.age > mx) continue;
    if (t.flag(r, "Student") && (h.age >= 18 || !h.schooling)) continue;
    if (t.flag(r, "Job") && h.job.empty()) continue;
    if (t.flag(r, "Hero Has Lover") && h.loverName.empty()) continue;
    if (t.flag(r, "Hero Doesn't Have Lover") && !h.loverName.empty()) continue;
    if (t.flag(r, "Both Parents Alive") && life.family.empty()) continue;
    const std::string g = trim(t.at(r, "Hero Gender"));
    if (!g.empty() && g != "both" && g != (h.female ? "female" : "male")) continue;
    if (resolve(note).find('<') != std::string::npos) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_notification-bait-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Notification"))));
  const std::string log = trim(t.at(row, "Log Version"));
  if (!log.empty() && resolve(log).find('<') == std::string::npos) {
    out.narrative = spyUpperFirst(resolve(log));
  }
  return true;
}
// ScenarioChildBreakItem (REPORT.en.md 211): the hero breaks a relative's thing.
//
// a16_child-break-items-enc: Item, Scenario. '<what>' is the Item column and '<who>' is
// the relative whose thing it was, which the port takes from the family it already has.
bool Sim::childBreakItemYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.age < 3 || h.age > 17) return false;
  if (!ensureTable("a16_child-break-items-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(26) != 0) return false;
  std::vector<std::string> kin;
  for (const Person& p : life.family) {
    if (!p.alive) continue;
    kin.push_back(p.relation);
  }
  if (kin.empty()) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_child-break-items-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Item")).empty()) continue;
    if (trim(t.at(r, "Scenario")).empty()) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  std::string scenario = trim(t.at(row, "Scenario"));
  const std::string what = trim(t.at(row, "Item"));
  const std::string who = kin[ambientRng_.below(uint32_t(kin.size()))];
  auto fill = [&](std::string s, const char* token, const std::string& value) {
    const size_t len = std::strlen(token);
    for (size_t at = s.find(token); at != std::string::npos; at = s.find(token, at)) {
      s.replace(at, len, value);
      at += value.size();
    }
    return s;
  };
  // The sentence is 'your <who> <what>', so the relative goes in possessive.
  scenario = fill(scenario, "<who>", who + "'s");
  scenario = fill(scenario, "<what>", what);
  out.table = "a16_child-break-items-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(scenario));
  out.narrative = "I broke my " + who + "'s " + what + " while playing.";
  out.happiness -= 3;
  return true;
}

// ScenarioNewsEvent (REPORT.en.md 212): the world's news, read by the hero.
//
// a16_news-events-enc, 157 rows: Type, Scenario. Every token in the corpus is
// '<city>' / '<city2>' / '<country>' / '<country2>' / '<country-possessive>' or a
// '<numN-M>' / '<decnumN-M>' range, and the shared resolver answers all of them, so
// this needs a gate and nothing else.
bool Sim::newsEventYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_news-events-enc")) return false;
  if (h.age < 12) return false;
  if (forcedChoice < 0 && ambientRng_.below(24) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_news-events-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string text = trim(t.at(r, "Scenario"));
    if (text.empty()) continue;
    if (resolve(text).find('<') != std::string::npos) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_news-events-enc";
  out.row_id = std::to_string(row + 1);
  out.title = spyUpperFirst(trim(t.at(row, "Type")));
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  return true;
}

// ScenarioDrugOffer (REPORT.en.md 214): someone offers the hero a drug.
//
// a16_drug-scenarios-enc: Condition, Scenario, CTA, Yes Choice, No Choice, Specific Drug.
// '<drug>' is answered from a16_drugs-enc (REPORT 213) and '<musician-indefinite>' from the
// resolver's own musician pool, so rows whose text still holds a token are the only ones
// skipped.
bool Sim::drugOfferYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_drug-scenarios-enc")) return false;
  if (h.age < 12) return false;
  if (forcedChoice < 0 && ambientRng_.below(30) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_drug-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string text = trim(t.at(r, "Scenario"));
    if (text.empty()) continue;
    const std::string cond = trim(t.at(r, "Condition"));
    if (cond == "School" && (h.age >= 18 || !h.schooling)) continue;
    if (resolve(text).find('<') != std::string::npos) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_drug-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  out.cta = spyUpperFirst(trim(t.at(row, "CTA")));
  const int lane = forcedChoice >= 0 ? forcedChoice : int(ambientRng_.below(2));
  const std::string drug = trim(t.at(row, "Specific Drug"));
  if (lane == 0) {
    out.choice = spyUpperFirst(resolve(trim(t.at(row, "Yes Choice"))));
    out.narrative = drug.empty() ? "I took it." : "I tried " + drug + ".";
    out.health -= 3;
    out.happiness += 2;
  } else {
    out.choice = spyUpperFirst(resolve(trim(t.at(row, "No Choice"))));
    out.narrative = drug.empty() ? "I turned it down." : "I turned down " + drug + ".";
    out.discipline = pointsForImpact(1);
  }
  return true;
}

// ScenarioCrashout (REPORT.en.md 215): a famous hero loses it in public.
//
// a16_crashout-scenario-enc: Severity, Location, Scenario, Magazine Snippet, BitLife
// Snippet, Log String, Hero Gender, Relatable, Fame Impact. The body's alternatives are
// the resolver's job; the gate is fame, and the row's Fame Impact is what the card costs.
bool Sim::crashoutYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_crashout-scenario-enc")) return false;
  if (h.fame < 40.0f) return false;
  if (h.age < 16) return false;
  if (forcedChoice < 0 && ambientRng_.below(22) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_crashout-scenario-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string text = trim(t.at(r, "Scenario"));
    if (text.empty()) continue;
    const std::string g = trim(t.at(r, "Hero Gender"));
    if (!g.empty() && g != "both" && g != (h.female ? "female" : "male")) continue;
    if (resolve(text).find('<') != std::string::npos) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_crashout-scenario-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  const std::string log = trim(t.at(row, "Log String"));
  if (!log.empty() && resolve(log).find('<') == std::string::npos) {
    out.narrative = spyUpperFirst(resolve(log));
  }
  // The row's own Fame Impact, in the game's integer scale, through ImpactFame.
  // The row's own Fame Impact, reported as a delta so the runner applies and clamps
  // it like every other stat (REPORT.en.md 228).
  out.fame = pointsForImpact(int(t.num(row, "Fame Impact")));
  out.happiness -= 4;
  // The public's answer to the crashout (a16_crashout-public-response-string-enc,
  // REPORT.en.md 305): 32 complete sentences, and the table's own Positive Reception
  // column marks the half that cheers. The card's Fame Impact decides which side of
  // the crowd answers; a row still carrying an alternative is skipped so the line
  // costs no draw from the main stream (the slash pool would).
  if (ensureTable("a16_crashout-public-response-string-enc")) {
    const Table& f = tbl("a16_crashout-public-response-string-enc");
    const bool positive = out.fame >= 0;
    std::vector<size_t> pool;
    for (size_t r = 0; r < f.size(); ++r) {
      const std::string text = trim(f.at(r, "String"));
      if (text.empty() || text.find('<') != std::string::npos) continue;
      if (f.flag(r, "Positive Reception") != positive) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t fr = weightedPickOf(f, "Commonness Score", pool, poolRng_);
      out.extra.push_back(spyUpperFirst(trim(f.at(fr, "String"))));
    }
  }
  return true;
}

// ScenarioPrayWealth (REPORT.en.md 216): the BitLife devs answer a prayer with money.
//
// a16_pray-wealth-scenarios-enc: Scenario, Log Version, Positive, Negative, Commonness.
// Both strings carry '<value>' and '<circumstance-public>'; the first is the amount this
// card rolls, the second is the captured public-scene pool (report 38), so the value is
// substituted here rather than in the resolver -- one roll must drive the sentence and the
// ledger at once (REPORT.en.md 216).
bool Sim::prayWealthYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_pray-wealth-scenarios-enc")) return false;
  if (h.age < 6) return false;
  if (forcedChoice < 0 && ambientRng_.below(40) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_pray-wealth-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Scenario")).empty()) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  const bool positive = t.flag(row, "Positive");
  const long long amount = 50 + static_cast<long long>(ambientRng_.below(20000));
  auto fill = [&](std::string s) {
    const std::string v = money(amount);
    for (size_t at = s.find("<value>"); at != std::string::npos; at = s.find("<value>", at)) {
      s.replace(at, 7, v);
      at += v.size();
    }
    return resolve(s);
  };
  out.table = "a16_pray-wealth-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(fill(trim(t.at(row, "Scenario"))));
  const std::string log = trim(t.at(row, "Log Version"));
  if (!log.empty()) out.narrative = spyUpperFirst(fill(log));
  if (positive) {
    h.money += amount;
  } else if (h.money > amount) {
    h.money -= amount;
  }
  // The prayer itself, in the hero's words (a16_pray-feedback-enc, REPORT.en.md 305):
  // 21 journal lines, the Pray column marking the blessing rows and the Curse column
  // the rows for a curse. Plain rows only, for the same reason as the crashout pool.
  if (ensureTable("a16_pray-feedback-enc")) {
    const Table& f = tbl("a16_pray-feedback-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < f.size(); ++r) {
      const std::string text = trim(f.at(r, "Log"));
      if (text.empty() || text.find('<') != std::string::npos) continue;
      if (!f.flag(r, positive ? "Pray" : "Curse")) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t fr = weightedPickOf(f, "Commonness Score", pool, poolRng_);
      out.extra.push_back(spyUpperFirst(trim(f.at(fr, "Log"))));
    }
  }
  return true;
}

// ScenarioSexuality (REPORT.en.md 217): the hero's feelings, in their own words.
//
// a16_sexuality-scenarios-enc: Scenario, Log Version, Gender, Sexuality, Min/Max Age,
// Student, Classmate Gender, Teacher Gender, Friend Gender, ... Most rows are pure
// alternatives and need nothing but a gate; a row gated on a Sexuality the port does not
// track is skipped rather than shown to everybody.
bool Sim::sexualityYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_sexuality-scenarios-enc")) return false;
  if (h.age < 8) return false;
  if (forcedChoice < 0 && ambientRng_.below(34) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_sexuality-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string text = trim(t.at(r, "Scenario"));
    if (text.empty()) continue;
    const std::string want = lower(trim(t.at(r, "Sexuality")));
    if (!want.empty() && want != h.sexuality) continue;
    const std::string g = trim(t.at(r, "Gender"));
    if (!g.empty() && g != "both" && g != (h.female ? "female" : "male")) continue;
    const double mn = t.num(r, "Min Age");
    const double mx = t.num(r, "Max Age");
    if (mn > 0 && h.age < mn) continue;
    if (mx > 0 && h.age > mx) continue;
    if (t.flag(r, "Student") && (h.age >= 18 || !h.schooling)) continue;
    if (resolve(text).find('<') != std::string::npos) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_sexuality-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  const std::string log = trim(t.at(row, "Log Version"));
  if (!log.empty() && resolve(log).find('<') == std::string::npos) {
    out.narrative = spyUpperFirst(resolve(log));
  }
  return true;
}

// ScenarioHomePurchase (REPORT.en.md 218): the hero buys a home.
//
// The port had no general ownership model -- only a rental's propType and the royal
// castle -- so the spouse-reaction cards (which fire on a purchase) could never trigger.
// This card is that model's first half: a house-type row, a lane that pays for it, and
// Hero::home set so the reaction cards have something to react to.
//
// shortcut: the price is Price Score * 10000, a scale the house table does not state; the
// game's own valuation lives in SimPlace and was not read.
bool Sim::homePurchaseYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.home.empty()) return false;
  if (h.age < 20) return false;
  if (!ensureTable("a16_house-types-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(2) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_house-types-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Type")).empty()) continue;
    if (t.flag(r, "Royal")) continue;
    const double mn = t.num(r, "Minimum Age");
    if (mn > 0 && h.age < mn) continue;
    const long long price = static_cast<long long>(t.num(r, "Price Score")) * 10000;
    if (price <= 0 || price > h.money) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  const std::string type = trim(t.at(row, "Type"));
  const long long price = static_cast<long long>(t.num(row, "Price Score")) * 10000;
  out.table = "a16_house-types-enc";
  out.row_id = std::to_string(row + 1);
  out.title = "House hunting";
  out.body = "A " + type + " is on the market for " + money(price) + ".";
  const int lane = forcedChoice >= 0 ? forcedChoice : int(ambientRng_.below(2));
  if (lane == 0) {
    out.choice = "Buy it";
    h.money -= price;
    h.home = type;
    out.narrative = "I bought a " + type + " for " + money(price) + ".";
  } else {
    out.choice = "Keep looking";
    out.narrative = "I passed on the " + type + ".";
  }
  return true;
}

// ScenarioSpouseHouseReaction (REPORT.en.md 219): the partner reacts to the new home.
//
// a16_spouse-house-reactions-enc. '<house>' is now the home the hero owns (REPORT 218/219)
// and '<lover>' the partner, so the rows that matter are the post-purchase ones; the
// prepurchase rows need a purchase preview the port does not build, and are skipped.
bool Sim::spouseHouseReactionYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.home.empty() || h.loverName.empty()) return false;
  if (!ensureTable("a16_spouse-house-reactions-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(6) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_spouse-house-reactions-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string text = trim(t.at(r, "Scenario"));
    if (text.empty()) continue;
    if (t.flag(r, "Prepurchase Rejection") || t.flag(r, "Prepurchase Acceptance")) continue;
    if (t.flag(r, "Too Many Houses") || t.flag(r, "Multiple Children")) continue;
    if (resolve(text).find('<') != std::string::npos) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_spouse-house-reactions-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  const std::string log = trim(t.at(row, "Log Version"));
  if (!log.empty() && resolve(log).find('<') == std::string::npos) {
    out.narrative = spyUpperFirst(resolve(log));
  }
  return true;
}

// The car's condition and its repair card (REPORT.en.md 323).
//
// SimHero::IncrementAge runs a car check every year (0x2b12b1c): A = GetRandomNumber(0, max)
// where max is the model's Reliability Score ladder (100/70/50/25/5 for scores 1..5, so a
// score of 1 is the most breakable), B = GetRandomNumber(0, 20), and the car needs work when
// Condition + B < A. A broken car queues ScenarioCarRepair; when the condition is gone the
// *ScrapPush lane replaces it. The card's own text is recovered (see the report): the body
// is #50813 'Your {0}car has {1}.' wrapped by #68715 "{0}\n\nWhat will you do?", the rows are
// #35747 'Repair' and #45558 'Vehicle Condition', and the buttons #35752 'Repair it',
// #45180 'Use the Golden Wrench' and #24668 'I might fix it later'.
//
// The port keeps the car's own state on the Hero (no SimCar array), uses the parts table's
// Break Commonness Score for the part pick, and calls the model's own Cost the price scale.
void Sim::makeCar(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.ownsCar = true;
  h.carCondition = 100;
  h.carModel.clear();  // ensureCarFields draws one
  ensureCarFields(life);
}

void Sim::ensureCarFields(Life& life) {
  Hero& h = life.hero;
  if (!h.ownsCar || !ensureTable("a16_car-models-enc")) return;
  if (!h.carModel.empty()) return;
  const Table& t = tbl("a16_car-models-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Model")).empty()) continue;
    if (t.flag(r, "Unavailable") || t.flag(r, "Race Car Dealership Only")) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return;
  const size_t row = rows[carRng_.below(uint32_t(rows.size()))];
  h.carMake = trim(t.at(row, "Make"));
  h.carModel = trim(t.at(row, "Model"));
  h.carCost = int(t.num(row, "Cost"));
  const int rel = int(t.num(row, "Reliability Score"));
  h.carReliability = rel < 1 ? 1 : rel > 5 ? 5 : rel;
  if (h.carCondition <= 0) h.carCondition = 100;
  if (h.outdoorVehicle.empty()) h.outdoorVehicle = h.carMake + " " + h.carModel;
}

bool Sim::carRepairYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCar) return false;
  if (h.age < 16) return false;
  hero_ = &h;
  life_ = &life;
  ensureCarFields(life);
  ++h.carYearsOwned;

  const std::string car = h.carMake.empty() ? "car" : h.carMake + " " + h.carModel;
  if (!h.carNeedsRepair) {
    // The damage roll. A score of 1 breaks the car often, a score of 5 almost never.
    static const int kMax[] = {100, 70, 50, 25, 5};
    const int rel = h.carReliability < 1 ? 1 : h.carReliability > 5 ? 5 : h.carReliability;
    const int a = int(carRng_.below(uint32_t(kMax[rel - 1] + 1)));
    const int b = int(carRng_.below(21));
    if (h.carCondition + b >= a) {
      // Nothing broke this year; the condition still drifts down with age.
      if (carRng_.chance(0.25) && h.carCondition > 0) --h.carCondition;
      return false;
    }
    // Something broke: pick the part by its Break Commonness Score, price it from the
    // parts table and remember the broken terms the card quotes.
    if (!ensureTable("a16_car-parts-enc")) return false;
    const Table& p = tbl("a16_car-parts-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < p.size(); ++r) {
      if (trim(p.at(r, "Part")).empty()) continue;
      const size_t weight = size_t(std::max(1.0, p.num(r, "Break Commonness Score")));
      for (size_t k = 0; k < weight; ++k) pool.push_back(r);
    }
    if (pool.empty()) return false;
    const size_t part = pool[carRng_.below(uint32_t(pool.size()))];
    h.carPart = trim(p.at(part, "Part"));
    const std::string terms = trim(p.at(part, "Broken Terms"));
    h.carBrokenTerms = terms.empty() ? "broken" : terms;
    // 0x2804f88: price = part Base Cost * (model Cost / 30000 when the model costs 30000
    // or more). The native multiplier M is unrecovered, so the port uses 1 (ceiling).
    double price = p.num(part, "Base Cost");
    if (h.carCost >= 30000) price *= double(h.carCost) / 30000.0;
    h.carRepairCost = static_cast<long long>(price);
    if (h.carRepairCost < 1) h.carRepairCost = 1;
    h.carNeedsRepair = true;
    h.carYearAtLastRepair = h.age;
  }

  // The car is past saving: the scrap card replaces the repair card.
  if (h.carCondition <= 0) {
    out.table = "registry:ScenarioCarScrapPush";
    out.title.clear();  // the scrap push's own title literal is not recovered
    out.body = "Your car will no longer start and is beyond repair.\n\nAge: " +
               std::to_string(h.carYearsOwned) + " Years";
    out.options = {"Scrap it"};
    out.choice = "Scrap it";
    out.narrative = "I had to scrap my " + car + ".";
    h.ownsCar = false;
    h.carNeedsRepair = false;
    return true;
  }

  out.table = "registry:ScenarioCarRepair";
  out.title = car;  // Title = Format(#70426 '{0} {1}', make, model)
  out.body = "Your " + car + " car has " + h.carBrokenTerms + ".\n\nRepair: " +
             money(h.carRepairCost) + "\nVehicle Condition: " +
             std::to_string(h.carCondition) + "\n\nWhat will you do?";
  const bool afford = h.money >= h.carRepairCost;
  // The native third button is the Golden Wrench, an in-app purchase this port does not
  // carry, so the port offers the two free lanes. The declined card is not cleared in the
  // native game, which is why a declined car comes back next year.
  out.options = afford ? std::vector<std::string>{"Repair it", "I might fix it later"}
                       : std::vector<std::string>{"I might fix it later"};
  int pick = 0;
  if (forcedChoice >= 0) pick = forcedChoice == 0 && afford ? 0 : int(out.options.size()) - 1;
  else pick = (afford && carRng_.chance(0.6)) ? 0 : int(out.options.size()) - 1;
  out.choice = out.options[size_t(pick)];
  if (pick == 0) {
    h.money -= h.carRepairCost;
    h.carNeedsRepair = false;
    h.carRepairCost = 0;
    out.narrative = "I repaired the " + h.carPart + " on my " + car + ".";
  } else {
    out.narrative = "I decided not to repair it.";
  }
  return true;
}

// ScenarioVehiclePurchase (REPORT.en.md 220): the hero buys a car.
//
// a16_outdoor-vehicle-enc: Make, Model, Type, Cost, ... The sibling of the home purchase
// (218): the vehicle is the state the spouse-vehicle reactions need, and the row's own
// Cost is the price, so nothing is scaled here.
bool Sim::vehiclePurchaseYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.outdoorVehicle.empty()) return false;
  if (h.age < 17) return false;
  if (!ensureTable("a16_outdoor-vehicle-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(1) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_outdoor-vehicle-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Model")).empty()) continue;
    const long long price = static_cast<long long>(t.num(r, "Cost"));
    if (price <= 0 || price > h.money) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  const std::string name = (trim(t.at(row, "Make")) + " " + trim(t.at(row, "Model"))).substr(0, 64);
  const long long price = static_cast<long long>(t.num(row, "Cost"));
  out.table = "a16_outdoor-vehicle-enc";
  out.row_id = std::to_string(row + 1);
  out.title = "Car shopping";
  out.body = "A " + name + " is for sale at " + money(price) + ".";
  const int lane = forcedChoice >= 0 ? forcedChoice : int(ambientRng_.below(2));
  if (lane == 0) {
    out.choice = "Buy it";
    h.money -= price;
    h.outdoorVehicle = name;
    out.narrative = "I bought a " + name + " for " + money(price) + ".";
  } else {
    out.choice = "Walk away";
    out.narrative = "I walked away from the " + name + ".";
  }
  return true;
}

// ScenarioSpouseVehicleReaction (REPORT.en.md 220): the partner reacts to the new car.
//
// a16_spouse-vehicle-reactions-enc: Scenario, Log Version, Cheap Car, Expensive Car, Old
// Car, Misc Car, Watercraft, Aircraft, Prepurchase Rejection. '<vehicle>' is the hero's own
// (219) and '<lover>' the partner; the prepurchase rows need a preview the port does not
// build and are skipped, as are the watercraft and aircraft rows (the port's purchase is
// a road vehicle).
bool Sim::spouseVehicleReactionYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.outdoorVehicle.empty() || h.loverName.empty()) return false;
  if (!ensureTable("a16_spouse-vehicle-reactions-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(1) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_spouse-vehicle-reactions-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string text = trim(t.at(r, "Scenario"));
    if (text.empty()) continue;
    if (t.flag(r, "Prepurchase Rejection")) continue;
    if (t.flag(r, "Watercraft") || t.flag(r, "Aircraft")) continue;
    if (resolve(text).find('<') != std::string::npos) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_spouse-vehicle-reactions-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  const std::string log = trim(t.at(row, "Log Version"));
  if (!log.empty() && resolve(log).find('<') == std::string::npos) {
    out.narrative = spyUpperFirst(resolve(log));
  }
  return true;
}

// ScenarioAircraftPurchase (REPORT.en.md 222): the hero buys an aircraft.
//
// a16_aircraft-enc: Make, Model, Cost, ... , Luxury Only. Cost is a '$304,000' string and
// Table::num strips the punctuation, so the price needs no parsing of its own. Hero::aircraft
// is the state a16_aircraft-danger-scenarios-enc needs, and '<aircraft>' now returns it.
bool Sim::aircraftPurchaseYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.aircraft.empty()) return false;
  if (h.age < 18) return false;
  if (!ensureTable("a16_aircraft-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(1) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_aircraft-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Model")).empty()) continue;
    if (t.flag(r, "Luxury Only") && !h.luxury) continue;
    // 'Min Age' is the aircraft's own age floor (SimAircraft::.ctor reads it as the base
    // of a used aircraft's age, REPORT.en.md 334), not the buyer's age.
    const long long price = static_cast<long long>(t.num(r, "Cost"));
    if (price <= 0 || price > h.money) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  const std::string name = (trim(t.at(row, "Make")) + " " + trim(t.at(row, "Model"))).substr(0, 64);
  const long long price = static_cast<long long>(t.num(row, "Cost"));
  out.table = "a16_aircraft-enc";
  out.row_id = std::to_string(row + 1);
  out.title = "Aircraft shopping";
  out.body = "A " + name + " is for sale at " + money(price) + ".";
  const int lane = forcedChoice >= 0 ? forcedChoice : int(ambientRng_.below(2));
  if (lane == 0) {
    out.choice = "Buy it";
    h.money -= price;
    h.aircraft = name;
    aircraftSetup(h, row);
    // The first flight is the danger card, next year: a family behind the chooser is
    // reached rarely, and a crash that waits for its own turn may never happen.
    life.pendingCards.push_back(kPendingAircraftDanger);
    out.narrative = "I bought a " + name + " for " + money(price) + ".";
  } else {
    out.choice = "Walk away";
    out.narrative = "I walked away from the " + name + ".";
  }
  return true;
}

// ScenarioAircraftDanger (REPORT.en.md 222): something goes wrong in the air.
//
// a16_aircraft-danger-scenarios-enc: Title, Scenario, Log Version, Danger Score, Fatal and
// one flag per aircraft kind. The port does not record the kind it bought, so the kind
// flags are not evaluated; the Fatal rows are left in, because a fatal flight is the point
// of the table and the death path is the one every other fatal card uses.
bool Sim::aircraftDangerYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.aircraft.empty()) return false;
  if (!ensureTable("a16_aircraft-danger-scenarios-enc")) return false;
  // The danger card only runs when the hero owns an aircraft, and the chooser reaches a
  // given family rarely; its own coin is therefore the gate, not a rate (REPORT.en.md 242).
  if (forcedChoice < 0 && ambientRng_.below(1) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_aircraft-danger-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string text = trim(t.at(r, "Scenario"));
    if (text.empty()) continue;
    // The lightning family owns the strike: its variant of this row carries a title the
    // lightning probe matches, and that probe reads a card with lanes (REPORT.en.md 242).
    if (trim(t.at(r, "Title")) == "Lightning Strike") continue;
    if (resolve(text).find('<') != std::string::npos) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_aircraft-danger-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.title = spyUpperFirst(trim(t.at(row, "Title")));
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  const std::string log = trim(t.at(row, "Log Version"));
  if (!log.empty() && resolve(log).find('<') == std::string::npos) {
    out.narrative = spyUpperFirst(resolve(log));
  }
  return true;
}

// ScenarioCustody (REPORT.en.md 223): the ex takes the hero to court for a child.
//
// Three tables, one card each of three kinds:
//   a16_custody-case-enc          139 rows, Reason + the defendant's own profile
//   a16_custody-ex-argument-enc    35 rows, what the ex says in court
//   a16_custody-child-argument-enc 54 rows, what the child says
// The game pushes the arguments and the verdict as follow-up scenarios; the port's
// payload queue (REPORT.en.md 184) is the same machinery, so the trial is a pending card
// and the arguments land in the following year rather than being crammed into this one.
bool Sim::custodyCaseYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (life.children.empty()) return false;
  if (h.exLovers.empty()) return false;
  if (!ensureTable("a16_custody-case-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(26) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_custody-case-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string reason = trim(t.at(r, "Reason"));
    if (reason.empty()) continue;
    const double mn = t.num(r, "Min Defendant Money");
    const double mx = t.num(r, "Max Defendant Money");
    if (mn > 0 && h.money < static_cast<long long>(mn)) continue;
    if (mx > 0 && h.money > static_cast<long long>(mx)) continue;
    if (t.flag(r, "Defendant Is Unemployed") && h.employed) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  const std::string ex = h.exLovers.front().name.empty() ? std::string("your ex") : h.exLovers.front().name;
  out.table = "a16_custody-case-enc";
  out.row_id = std::to_string(row + 1);
  out.title = "Custody hearing";
  // The reason rows already name the ex and read as a sentence about them, so the
  // card does not repeat the name (REPORT.en.md 223).
  out.body = ex + " is taking you to court for custody, claiming that " +
             spyUpperFirst(resolve(trim(t.at(row, "Reason")))) + ".";
  const int lane = forcedChoice >= 0 ? forcedChoice : int(ambientRng_.below(2));
  life.pendingCards.push_back(kPendingCustodyTrial);
  if (lane == 0) {
    out.choice = "Fight it";
    out.narrative = "I told the court I would fight for my child.";
  } else {
    out.choice = "Let them have custody";
    out.narrative = "I did not fight it.";
    out.happiness -= 6;
  }
  return true;
}

// The trial itself, one year on: the ex's argument, the child's, and the verdict.
bool Sim::custodyTrialYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (life.children.empty() || h.exLovers.empty()) return false;
  if (!ensureTable("a16_custody-ex-argument-enc")) return false;
  if (!ensureTable("a16_custody-child-argument-enc")) return false;
  hero_ = &h;
  life_ = &life;
  std::string lines;
  const Hero::ExLover& ex = h.exLovers.front();
  const Table& et = tbl("a16_custody-ex-argument-enc");
  std::vector<size_t> erows;
  for (size_t r = 0; r < et.size(); ++r) {
    const std::string s = trim(et.at(r, "String"));
    if (s.empty()) continue;
    const std::string g = trim(et.at(r, "Ex Gender"));
    if (!g.empty() && g != "both" && g != (ex.female ? "female" : "male")) continue;
    if (resolve(s).find('<') != std::string::npos) continue;
    erows.push_back(r);
  }
  if (!erows.empty()) {
    const size_t r = erows[ambientRng_.below(uint32_t(erows.size()))];
    lines += spyUpperFirst(resolve(trim(et.at(r, "String"))));
  }
  const Table& ct = tbl("a16_custody-child-argument-enc");
  std::vector<size_t> crows;
  for (size_t r = 0; r < ct.size(); ++r) {
    const std::string s = trim(ct.at(r, "String"));
    if (s.empty()) continue;
    const std::string g = trim(ct.at(r, "Child Gender"));
    const Child& kid = life.children.front();
    if (!g.empty() && g != "both" && g != (kid.female ? "female" : "male")) continue;
    const double mn = ct.num(r, "Min Child Age");
    const double mx = ct.num(r, "Max Child Age");
    if (mn > 0 && kid.age < mn) continue;
    if (mx > 0 && kid.age > mx) continue;
    if (resolve(s).find('<') != std::string::npos) continue;
    crows.push_back(r);
  }
  if (!crows.empty()) {
    const size_t r = crows[ambientRng_.below(uint32_t(crows.size()))];
    if (!lines.empty()) lines += " ";
    lines += spyUpperFirst(resolve(trim(ct.at(r, "String"))));
  }
  out.table = "a16_custody-ex-argument-enc";
  out.title = "Custody hearing";
  out.body = lines.empty() ? "The court heard both sides." : lines;
  // Verdict: the side with more money wins more often, as the case table's own
  // defendant-money columns imply. shortcut: a 60/40 split, not the game's formula.
  const bool heroWins = ambientRng_.chance(h.money > 50000 ? 0.6 : 0.4);
  if (heroWins) {
    out.narrative = "The judge granted me custody.";
    out.happiness += 4;
  } else {
    out.narrative = "The judge granted custody to " + (ex.name.empty() ? std::string("my ex") : ex.name) + ".";
    out.happiness -= 8;
  }
  return true;
}

// ScenarioPrayAsset (REPORT.en.md 224): the hero is granted a prize asset.
//
// a16_pray-asset-scenarios-enc, six rows, every one of them 'a brand-new <asset>'. The
// '<asset>' pool is the vehicle catalogue, so the prize is a car and the card hands it
// over the way the vehicle purchase does. Two rows carry '<friend>', which still has no
// pool, and are skipped.
bool Sim::prayAssetYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.age < 8) return false;
  if (!ensureTable("a16_pray-asset-scenarios-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(45) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_pray-asset-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string text = trim(t.at(r, "Scenario"));
    if (text.empty()) continue;
    if (resolve(text).find('<') != std::string::npos) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_pray-asset-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  const std::string log = trim(t.at(row, "Log Version"));
  if (!log.empty()) {
    const std::string resolved = resolve(log);
    if (resolved.find('<') == std::string::npos) out.narrative = spyUpperFirst(resolved);
  }
  if (h.outdoorVehicle.empty()) {
    const std::string prize = poolToken("vehicle");
    if (!prize.empty()) h.outdoorVehicle = prize;
  }
  out.happiness += 3;
  return true;
}

// ScenarioOnlyFans (REPORT.en.md 225): a channel, a tip, and a thank-you.
//
// The port had no channel at all, so this is the smallest one the data asks for:
// a16_social-media-channels-onlyfans-tip-responses-enc is the only card the channel owns
// (seven rows, each 'Send <him/her> a ... video'), and every one of them needs a fan to
// send it to. The fan is a random person, so '<him/her>' is set from that person's own
// gender, and the tip is what the response earns.
bool Sim::onlyfansStartYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.onlyfans || h.age < 18) return false;
  if (forcedChoice < 0 && ambientRng_.below(15) != 0) return false;
  hero_ = &h;
  life_ = &life;
  out.table = "a16_social-media-channels-enc";
  out.title = "OnlyFans";
  out.body = "A friend tells you that people are making real money on OnlyFans.";
  const int lane = forcedChoice >= 0 ? forcedChoice : int(ambientRng_.below(2));
  if (lane == 0) {
    out.choice = "Start a channel";
    h.onlyfans = true;
    h.onlyfansFans = 1 + int(ambientRng_.below(20));
    out.narrative = "I started an OnlyFans channel.";
    out.happiness += 2;
  } else {
    out.choice = "Not for me";
    out.narrative = "I passed on the idea.";
  }
  return true;
}

bool Sim::onlyfansTipYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.onlyfans || h.onlyfansFans <= 0) return false;
  if (!ensureTable("a16_social-media-channels-onlyfans-tip-responses-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(6) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_social-media-channels-onlyfans-tip-responses-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string option = trim(t.at(r, "Option"));
    if (option.empty()) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  const bool fanFemale = ambientRng_.chance(0.5);
  const std::string fan = drawFirstName(fanFemale) + " " + drawSurname();
  const bool savedSet = pronounOtherSet_;
  const bool savedFemale = pronounOtherFemale_;
  pronounOtherSet_ = true;
  pronounOtherFemale_ = fanFemale;
  out.table = "a16_social-media-channels-onlyfans-tip-responses-enc";
  out.row_id = std::to_string(row + 1);
  out.title = "OnlyFans";
  out.body = fan + " sent you a tip. How do you say thanks?";
  out.cta = "Send a video";
  out.choice = spyUpperFirst(resolve(trim(t.at(row, "Option"))));
  const std::string log = trim(t.at(row, "Log Version"));
  if (!log.empty()) {
    const std::string resolved = resolve(log);
    if (resolved.find('<') == std::string::npos) out.narrative = spyUpperFirst(resolved);
  }
  pronounOtherSet_ = savedSet;
  pronounOtherFemale_ = savedFemale;
  // The tip itself: the fan's money, scaled by how many subscribers the channel has.
  const long long tip = 5 + static_cast<long long>(ambientRng_.below(60)) * (1 + h.onlyfansFans / 10);
  h.money += tip;
  out.happiness += 1;
  return true;
}

// The three jewellery tables describe one piece of jewellery (REPORT.en.md 303):
// a16_jewelry-types-enc carries the type and its Cost Score, a16_jewelry-gemstones-enc
// the carat range, the per-carat price and the cost bounds, a16_jewelry-metals-enc
// the metal. The price itself is in no table: the live shop showed a 2.55-carat
// sapphire bracelet at $19,400 and First Carat Price x carats x the type's Cost
// Score gives $20,400 for it, so that product is the price, clamped to the
// gemstone's own Min/Max Cost. (shortcut: the real formula carries a quality roll
// the port does not reproduce.)
std::string Sim::Jewel::caratLabel() const {
  const long long hundredths = static_cast<long long>(carats * 100.0 + 0.5);
  std::string frac = std::to_string(hundredths % 100);
  if (frac.size() < 2) frac = "0" + frac;
  return std::to_string(hundredths / 100) + "." + frac;
}

bool Sim::drawJewel(Life& life, const std::string& typeFilter, bool allowLuxury,
                    Rng& rng, Jewel& jewel) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!ensureTable("a16_jewelry-types-enc")) return false;
  if (!ensureTable("a16_jewelry-gemstones-enc")) return false;
  if (!ensureTable("a16_jewelry-metals-enc")) return false;
  const Table& types = tbl("a16_jewelry-types-enc");
  const Table& gems = tbl("a16_jewelry-gemstones-enc");
  const Table& metals = tbl("a16_jewelry-metals-enc");
  auto number = [](std::string cell) -> double {
    std::string clean;
    for (char c : cell)
      if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') clean.push_back(c);
    return clean.empty() ? 0.0 : std::atof(clean.c_str());
  };
  std::vector<size_t> pool;
  for (size_t r = 0; r < types.size(); ++r) {
    if (!typeFilter.empty() && lower(trim(types.at(r, "Category"))) != typeFilter) continue;
    if (!allowLuxury && types.flag(r, "Luxury Only")) continue;
    pool.push_back(r);
  }
  const size_t trow = pickFrom(pool, rng);
  if (trow == size_t(-1)) return false;
  pool.clear();
  for (size_t r = 0; r < gems.size(); ++r) {
    if (!allowLuxury && gems.flag(r, "Luxury Only")) continue;
    pool.push_back(r);
  }
  const size_t grow = weightedPickOf(gems, "Commonness Score", pool, rng);
  if (grow == size_t(-1)) return false;
  jewel.type = trim(types.at(trow, "Category"));
  jewel.gem = trim(gems.at(grow, "Gemstone"));
  const double lo = number(gems.at(grow, "Min Carats"));
  const double hi = number(gems.at(grow, "Max Carats"));
  jewel.carats = hi > lo ? lo + (hi - lo) * rng.unit() : lo;
  double price = number(gems.at(grow, "First Carat Price")) * jewel.carats *
                 number(types.at(trow, "Cost Score"));
  const double minCost = number(gems.at(grow, "Min Cost"));
  const double maxCost = number(gems.at(grow, "Max Cost"));
  if (price < minCost) price = minCost;
  if (maxCost > 0 && price > maxCost) price = maxCost;
  jewel.price = static_cast<long long>(price + 0.5);
  const size_t mrow = pickRow("a16_jewelry-metals-enc", "Cost Score", "", "", "", rng);
  jewel.metal = mrow == size_t(-1) ? std::string("gold") : trim(metals.at(mrow, "Metal"));
  return true;
}

// ScenarioLoverProposeInbound (REPORT.en.md 303): the partner proposes. The live
// card read 'Your boyfriend, Arnaud, proposed to you with a 0.44-carat diamond ring
// at a fish market!' over the detail rows 'Ring: Diamond Ring (0.44ct)',
// 'Setting: Fish Market', 'Years Together: 8' and the two buttons alone.
//
// body     'Your {0}, {1}, proposed to you with {2} {3}!'                  #50749
// rows     'Ring: {0} {1} ({2}ct)'  'Setting: {0}'  'Years Together: {0}'
// options  'Accept {0} proposal' (#10553)  "I'm not ready for marriage" (#26026)
// journals 'I accepted {0} proposal!' (#68934)  'I refused to accept {0}.' (#69160)
//
// The card fires while the pair are dating; accepting makes them engaged, where the
// wedding family already picks them up. The hero-driven side
// (ScenarioLoverProposeOutbound, with ShouldForceLoverAcceptsProposal and the #49138
// body) is launched from the Relationships menu and is not ported.
bool Sim::proposalYear(Life& life, AgeUp& out, int forcedChoice) {
  hero_ = &life.hero;
  life_ = &life;
  Hero& h = life.hero;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.loverName.empty()) return false;
  if (h.loverStatus != "dating") return false;
  if (h.age < 18) return false;
  if (h.relationship < 55) return false;
  if (forcedChoice < 0 && propRng_.below(9) != 0) return false;
  if (h.loverSinceAge < 0 || h.loverSinceName != h.loverName) {
    h.loverSinceName = h.loverName;
    h.loverSinceAge = h.age;
  }
  Jewel ring;
  if (!drawJewel(life, "ring", h.money >= 100000, propRng_, ring)) return false;
  if (!ensureTable("a16_proposal-settings-enc")) return false;
  const Table& settings = tbl("a16_proposal-settings-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < settings.size(); ++r) {
    const std::string minAge = trim(settings.at(r, "Min Age"));
    if (!minAge.empty() && std::atoi(minAge.c_str()) > h.age) continue;
    pool.push_back(r);
  }
  const size_t srow = weightedPickOf(settings, "Romance Score", pool, propRng_);
  if (srow == size_t(-1)) return false;
  const std::string place = trim(settings.at(srow, "Place"));
  const std::string snippet = trim(settings.at(srow, "Snippet"));
  if (place.empty() || snippet.empty()) return false;
  auto titleWords = [](std::string text) {
    bool start = true;
    for (char& c : text) {
      if (start && std::isalpha(static_cast<unsigned char>(c))) {
        c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
        start = false;
      } else if (c == ' ') {
        start = true;
      }
    }
    return text;
  };
  const std::string word = h.loverFemale ? "girlfriend" : "boyfriend";
  const std::string whom = h.loverFemale ? "her" : "his";
  const int years = h.age - h.loverSinceAge;
  out.table = "registry:ScenarioLoverProposeInbound";
  out.row_id = "0";
  out.title = "Proposal";
  out.body = "Your " + word + ", " + h.loverName + ", proposed to you with a " +
             ring.caratLabel() + "-carat " + ring.gem + " " + ring.type + " " + snippet + "!";
  out.body += "\n\nRing: " + spyUpperFirst(ring.gem) + " " + spyUpperFirst(ring.type) +
              " (" + ring.caratLabel() + "ct)";
  out.body += "\nSetting: " + titleWords(place);
  out.body += "\nYears Together: " + std::to_string(years);
  out.body += "\n\nWhat will you do?";
  out.options.clear();
  out.options.push_back("Accept " + whom + " proposal");
  out.options.push_back("I'm not ready for marriage");
  const int lane = forcedChoice >= 0 ? forcedChoice : int(propRng_.below(2));
  if (lane == 0) {
    out.choice = "Accept " + whom + " proposal";
    h.loverStatus = "engaged";
    h.relationship = std::min(100, h.relationship + 15);
    out.narrative = "I accepted " + whom + " proposal!";
  } else {
    out.choice = "I'm not ready for marriage";
    h.relationship = std::max(0, h.relationship - 10);
    out.narrative = "I refused to accept " + whom + " proposal.";
  }
  return true;
}

// The jewelry shop (REPORT.en.md 304). The live build opens a JEWELERS list whose
// rows lead to a purchase card: title bar '#27900 Jewel', title '<Gem> <Type>',
// body '#14339 Buy this {0} {1} {2} today!', rows Price / Gemstone / Carats /
// Metal, one button '#14316 Buy it'. Port rows are AgeUp lines, so the card is
// fired as a year event from the same three tables the proposal ring already
// reads. The shop screen itself is a menu the port does not have.
bool Sim::jewelShopYear(Life& life, AgeUp& out, int forcedChoice) {
  hero_ = &life.hero;
  life_ = &life;
  Hero& h = life.hero;
  if (h.inPrison) return false;
  if (h.age < 18) return false;
  if (forcedChoice < 0 && !jewRng_.chance(1.0 / 12.0)) return false;
  if (!ensureTable("a16_jewelry-types-enc")) return false;
  const Table& types = tbl("a16_jewelry-types-enc");
  const bool luxury = h.money >= 100000;
  std::vector<size_t> pool;
  for (size_t r = 0; r < types.size(); ++r) {
    if (trim(types.at(r, "Category")).empty()) continue;
    if (!luxury && types.flag(r, "Luxury Only")) continue;
    // 'earrings' and 'chandelier earrings' are the only plural categories, and
    // #14339 is written for one item ('Buy this ... today!'); no literal for the
    // plural shape was found, so the port does not sell them rather than invent
    // 'Buy these ...'.
    if (types.flag(r, "Is Plural")) continue;
    pool.push_back(r);
  }
  if (pool.empty()) return false;
  const size_t tr = weightedPickOf(types, "Commonness Score", pool, jewRng_);
  Jewel jewel;
  if (!drawJewel(life, lower(trim(types.at(tr, "Category"))), luxury, jewRng_, jewel))
    return false;
  // A shop row is only offered at a price the hero can actually pay: the port
  // has no 'Penniless' screen to fall back to (the live modal is #14317's lane).
  if (h.money < jewel.price) return false;
  // The card is Title Case on screen ('Sapphire Bracelet', 'Metal: 24K Gold'), so
  // every word is raised there; the body line stays lowercase as the live card's
  // is ('Buy this 2.55-carat sapphire bracelet today!').
  auto titleCase = [](const std::string& text) {
    std::string out2 = text;
    bool start = true;
    for (char& c : out2) {
      if (c == ' ') {
        start = true;
      } else if (start) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        start = false;
      }
    }
    return out2;
  };
  const std::string gem = titleCase(jewel.gem);
  const std::string type = titleCase(jewel.type);
  const std::string item = gem + " " + type;
  const bool carats = types.flag(tr, "Show Carats");
  out.table = "registry:ScenarioJewelShop";
  out.row_id = "0";
  out.title = item;
  if (carats) {
      out.body = "Buy this " + jewel.caratLabel() + "-carat " + lower(gem) + " " +
               lower(jewel.type) + " today!";
  } else {
    out.body = "Buy this " + lower(gem) + " " + lower(jewel.type) + " today!";
  }
  out.body += "\n\nPrice: " + money(jewel.price);
  out.body += "\nGemstone: " + gem;
  if (carats) out.body += "\nCarats: " + jewel.caratLabel();
  out.body += "\nMetal: " + titleCase(jewel.metal);
  out.options.clear();
  // The live card carries only 'Buy it' (#14316); dismissing it is the screen's
  // X, which a port card has no equivalent for.
  out.options.push_back("Buy it");
  out.choice = "Buy it";
  // 'I bought a ' is the game's own log fragment (#23640); the rest of the line
  // is written here because the item and the price are the port's own cells.
  const bool vowel = !item.empty() && std::strchr("AEIOU", item[0]) != nullptr;
  out.narrative = std::string("I bought ") + (vowel ? "an " : "a ") + item + " for " +
                  money(jewel.price) + ".";
  h.money -= jewel.price;
  return true;
}

// ---------------------------------------------------------------------------
// The aircraft's life after purchase (REPORT.en.md 334): SimAircraft ages and wears
// every year, costs its monthly expense twelve times over, breaks (ScenarioAircraftRepair)
// and is finally scrapped (ScenarioAircraftScrapPush). Native: SimAircraft::.ctor 0x2b0f490,
// IncrementAge 0x2b0f7c4, MonthlyExpense 0x2b0fbc4, ScrapValue 0x2b0fcf4,
// IsRepairNeededAfterCheckForAnnualRepair 0x2b10284, GetRandomAircraftPartToBreakForAircraft
// 0x28342b8. GetRandomNumber(a,b) is Random.Next, so every 'below(n)' here is rand(0,n).

namespace {

double aircraftMonthlyExpense(double cost) {
  const double div = cost > 10000000.0 ? 863.0 : cost > 1000000.0 ? 663.0
                   : cost > 500000.0 ? 313.0 : 253.0;
  const double m = cost / div;
  return m < 500.0 ? 500.0 : m;  // ponytail: the economy scale M (GameRef..+0x1d4) is 1
}

std::vector<std::string> commaList(const std::string& cell) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : cell + ",") {
    if (c == ',') { const std::string s = trim(cur); if (!s.empty()) out.push_back(s); cur.clear(); }
    else cur += c;
  }
  return out;
}

}  // namespace

// SimAircraft::GenericDescriptionString 0x2b0f6a4: the override or 'Make Model', then the
// suffix after a space ('Robinson R22 helicopter').
std::string Sim::aircraftDescription(const Hero& h) {
  if (h.aircraftRow < 0 || !ensureTable("a16_aircraft-enc")) return h.aircraft;
  const Table& t = tbl("a16_aircraft-enc");
  const size_t r = size_t(h.aircraftRow);
  std::string s = trim(t.at(r, "Descriptive Name Override"));
  if (s.empty()) s = trim(t.at(r, "Make")) + " " + trim(t.at(r, "Model"));
  const std::string suffix = trim(t.at(r, "Descriptive Name Suffix"));
  if (!suffix.empty()) s += " " + suffix;
  return s;
}

// SimAircraft::.ctor(data, true), the dealership's call: a row with Min Age 0 is brand new,
// any other row draws one of three used-age modes (the fourth arm is rand(0,3)'s dead 3),
// and the condition follows the age tier.
void Sim::aircraftSetup(Hero& h, size_t row) {
  const Table& t = tbl("a16_aircraft-enc");
  Rng& g = aircraftRng_;
  h.aircraftRow = int(row);
  h.aircraftAgeAtLastRepair = 0;
  h.aircraftRepairPending = false;
  h.aircraftScrapPending = false;
  const int minAge = int(t.num(row, "Min Age"));
  const int life = int(t.num(row, "Lifespan Years"));
  if (minAge == 0) { h.aircraftAge = 0; h.aircraftCondition = 100.0; return; }
  const int mode = int(g.below(3));
  if (mode == 0) h.aircraftAge = minAge + int(g.below(5)) + 1;
  else if (mode == 1) h.aircraftAge = minAge + int(g.below(10)) + 1;
  else h.aircraftAge = minAge + int(g.below(uint32_t(std::max(1, life - minAge)))) + 1;
  if (h.aircraftAge <= 4) h.aircraftCondition = 61 + int(g.below(40));
  else if (h.aircraftAge <= 9) h.aircraftCondition = 20 + int(g.below(81));
  else h.aircraftCondition = 1 + int(g.below(100));
}

// The yearly tick: SimAircraft::IncrementAge, the living-expense charge and the repair
// check, in that order (the two native callers' relative order is unrecovered: shortcut).
void Sim::aircraftRunYear(Life& life) {
  Hero& h = life.hero;
  if (h.aircraft.empty() || h.aircraftRow < 0 || h.aircraftTickAge == h.age) return;
  if (!ensureTable("a16_aircraft-enc")) return;
  h.aircraftTickAge = h.age;
  const Table& t = tbl("a16_aircraft-enc");
  const size_t row = size_t(h.aircraftRow);
  Rng& g = aircraftRng_;
  const double lifespan = std::max(1.0, t.num(row, "Lifespan Years"));
  const double cost = t.num(row, "Cost");
  // IncrementAge: Age++, then the wear. Maintenance is not ported, so NumMaint stays 0.
  ++h.aircraftAge;
  if (double(h.aircraftAge) >= lifespan) {
    h.aircraftCondition *= 0.9;
  } else {
    const double cand = 100.0 * (1.0 - double(h.aircraftAge) / lifespan + 0.05 -
                                 double(g.below(10)) / 100.0);
    if (cand < h.aircraftCondition) h.aircraftCondition = cand;  // unclamped, as native
  }
  // IncurAnnualLivingExpenses 0x2a75944: twelve months of MonthlyExpense.
  h.money -= static_cast<long long>(12.0 * aircraftMonthlyExpense(cost));
  if (h.aircraftScrapPending) return;
  // IsRepairNeeded G2: an aircraft a quarter past its lifespan and in ruins is scrapped.
  // The maintenance gap is always >= 2 here (the port has no maintenance card) and the
  // port's aircraft never carries a loan.
  if (double(h.aircraftAge) >= 1.25 * lifespan && h.aircraftCondition < 2.0) {
    h.aircraftScrapPending = true;
    life.pendingCards.push_back(kPendingAircraftScrap);
    return;
  }
  // G3: a repair still open blocks the check, and so does a break in the last four years.
  if (h.aircraftRepairPending) return;
  if (h.aircraftAge - h.aircraftAgeAtLastRepair < 4) return;
  const double age = double(h.aircraftAge);
  const int max = age > lifespan ? 100 : age > 0.8 * lifespan ? 70
                : age > 0.6 * lifespan ? 50 : age > 0.4 * lifespan ? 25 : 5;
  const int a = int(g.below(uint32_t(max)));
  const int b = int(g.below(20));
  if (h.aircraftCondition + b >= a) return;
  h.aircraftAgeAtLastRepair = h.aircraftAge;
  // The part: uniform over the parts flagged for this aircraft's kind (Only One Per
  // Aircraft is loaded and never read).
  if (!ensureTable("a16_aircraft-parts-enc")) return;
  const Table& p = tbl("a16_aircraft-parts-enc");
  const std::string type = trim(t.at(row, "Type"));
  std::string col;
  if (type == "Helicopter") col = "Helicopter";
  else if (type == "Airplane") col = trim(t.at(row, "Airplane Propulsion Type")) == "Jet" ? "Jet" : "Propeller";
  else if (type == "Glider") col = "Glider";
  else if (type == "Blimp") col = "Blimp";
  // Airship, Seaplane and Hot Air Balloon rows: their Type enum value is unrecovered and
  // the picker returns no candidate for any other kind, so the port draws no break.
  if (col.empty()) return;
  std::vector<size_t> parts;
  for (size_t r = 0; r < p.size(); ++r)
    if (!trim(p.at(r, "Part")).empty() && p.flag(r, col)) parts.push_back(r);
  if (parts.empty()) return;
  const size_t part = parts[g.below(uint32_t(parts.size()))];
  h.aircraftPart = trim(p.at(part, "Part"));
  const std::vector<std::string> terms = commaList(p.at(part, "Broken Terms"));
  h.aircraftBrokenTerm = terms.empty() ? "broken" : terms[g.below(uint32_t(terms.size()))];
  static const double kFactor[] = {0.8, 2.0, 4.0, 7.0, 12.0};
  int score = int(p.num(part, "Part Cost Score"));
  score = score < 1 ? 1 : score > 5 ? 5 : score;
  h.aircraftRepairCost = static_cast<long long>(aircraftMonthlyExpense(cost) * kFactor[score - 1]);
  h.aircraftRepairPending = true;
  h.aircraftPartRow = int(part);
  life.pendingCards.push_back(kPendingAircraftRepair);
}

bool Sim::aircraftRepairYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.aircraft.empty() || !h.aircraftRepairPending) return false;
  hero_ = &h;
  life_ = &life;
  Rng& g = aircraftRng_;
  const std::string desc = aircraftDescription(h);
  const std::string broken = h.aircraftBrokenTerm + " " + h.aircraftPart;  // #70426
  out.table = "registry:ScenarioAircraftRepair";
  out.title = h.aircraft;
  // The pushed card's body: half the time (rand(0,3) <= 1) one of the part's own
  // Optional Descriptions under #50593, otherwise #50577; #68715 wraps both.
  std::vector<std::string> optional;
  if (h.aircraftPartRow >= 0 && ensureTable("a16_aircraft-parts-enc"))
    optional = commaList(tbl("a16_aircraft-parts-enc").at(size_t(h.aircraftPartRow), "Optional Descriptions"));
  std::string body;
  if (!optional.empty() && g.below(3) <= 1)
    body = "Your " + desc + " needs a repair.\n\n" + optional[g.below(uint32_t(optional.size()))];
  else
    body = "Your " + desc + " has " + articleFor(broken) + broken + ".";
  out.body = body + "\n\nWhat will you do?\n\nRepair: " + money(h.aircraftRepairCost);
  // The Golden Wrench button (#45180) is an in-app purchase this port does not carry; an
  // unaffordable repair would open ScenarioInsufficientFinances, so the port only offers
  // the free lane then.
  const bool afford = h.money >= h.aircraftRepairCost;
  out.options = afford ? std::vector<std::string>{"Repair it", "I might fix it later"}
                       : std::vector<std::string>{"I might fix it later"};
  // With one option the list index cannot stand in for the lane (an unaffordable repair
  // would read as index 0 and be repaired for free), so the lane is decided first.
  const bool doRepair = afford && (forcedChoice < 0 ? g.chance(0.6) : forcedChoice == 0);
  out.choice = out.options[size_t(doRepair ? 0 : out.options.size() - 1)];
  const std::string pushed = "My " + desc + " has " + articleFor(broken) + broken + ".";  // #30911
  if (doRepair) {
    h.money -= h.aircraftRepairCost;
    h.aircraftRepairPending = false;
    out.narrative = pushed + "\nI repaired the " + broken + " on my " + desc + ".";  // #70735
    static const char* kFixed[] = {"Fixed up", "Ready for liftoff", "No problem too big",
                                   "Repairs: Complete"};
    out.body += "\n\n" + std::string(kFixed[g.below(4)]) + "\nYour " + desc + " is back in service.";
    h.aircraftRepairCost = 0;
  } else {
    // #69029; the pushed card's decline costs ImpactDiscipline(-2). The repair stays open,
    // so this aircraft draws no new break until it is fixed.
    out.narrative = pushed + "\nI decided not to repair it.";
    out.discipline = pointsForImpact(-2);
    static const char* kLater[] = {"Forcing my hand", "I'll fix it later", "No flight zone",
                                   "Flight's cancelled"};
    out.body += "\n\n" + std::string(kLater[g.below(4)]) + "\nYour " + desc +
                " is out of commission until you fix it.";
  }
  return true;
}

bool Sim::aircraftScrapYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  if (h.aircraft.empty() || !h.aircraftScrapPending) return false;
  hero_ = &h;
  life_ = &life;
  const std::string desc = aircraftDescription(h);
  const double cost = h.aircraftRow >= 0 ? tbl("a16_aircraft-enc").num(size_t(h.aircraftRow), "Cost") : 0.0;
  out.table = "registry:ScenarioAircraftScrapPush";
  out.title = h.aircraft;
  const std::string line = "Your " + desc + " will no longer fly and is beyond repair.";  // #50617
  out.body = line + "\n\nAge: " + std::to_string(h.aircraftAge) + " Years";  // #10954 / #69718
  out.options = {"Scrap it"};  // #38269, the card's only button
  out.choice = "Scrap it";
  // ScrapValue 0x2b0fcf4: nothing at or under $1,000,000; otherwise Cost/250M*150000,
  // truncated to a multiple of 100 above 1000 and of 10 below.
  long long scrap = 0;
  if (cost > 1000000.0) {
    const long long x = static_cast<long long>(cost / 250000000.0 * 150000.0);
    const long long step = x > 1000 ? 100 : 10;
    scrap = x / step * step;
  }
  if (scrap > 0) {
    h.money += scrap;
    out.narrative = line + "\nThe scrapyard gave me " + money(scrap) + " for it.";  // #69322
    static const char* kLoot[] = {"I'll take the loot", "Plane for parts", "Worth more this way",
                                  "One man's trash..."};
    out.body += "\n\n" + std::string(kLoot[aircraftRng_.below(4)]) + "\nThe scrapyard gave you " +
                money(scrap) + " for it.";  // #42451
  } else {
    out.narrative = line;
  }
  h.aircraft.clear();
  h.aircraftRow = -1;
  h.aircraftScrapPending = false;
  h.aircraftRepairPending = false;
  return true;
}

}  // namespace bl
