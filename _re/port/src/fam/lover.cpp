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
bool Sim::loverLeavesYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.loverName.empty()) return false;
  if (h.age < 18) return false;
  if (forcedChoice < 0 && loverRng_.below(12) != 0) return false;
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
  if (forcedChoice < 0 && loverRng_.below(9) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_child-demands-enc");
  const size_t row = [&] {
    size_t best = 0;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.num(r, "Age") <= kid->age) best = r;
    }
    return best;
  }();
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
  if (parent == nullptr) return false;
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
  if (forcedChoice < 0 && ambientRng_.below(40) != 0) return false;
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
  if (forcedChoice < 0 && ambientRng_.below(18) != 0) return false;
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

// a16_time-machine-scenarios-enc: Scenario, Min Age, Prison, Was Dead. A report for
// a hero the machine has already visited: the Prison flag only offers itself while
// the hero is locked up, and 'Was Dead' while the port's death flag is clear.
bool Sim::timeMachineYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!ensureTable("a16_time-machine-scenarios-enc")) return false;
  if (forcedChoice < 0 && ambientRng_.below(55) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_time-machine-scenarios-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Scenario")).empty()) continue;
    if (t.flag(r, "Prison") && !h.inPrison) continue;
    if (!t.flag(r, "Prison") && h.inPrison) continue;
    if (t.flag(r, "Was Dead")) continue;   // the port never revives a hero
    const double mn = t.num(r, "Min Age");
    if (mn > 0 && h.age < mn) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  const size_t row = rows[ambientRng_.below(uint32_t(rows.size()))];
  out.table = "a16_time-machine-scenarios-enc";
  out.row_id = std::to_string(row + 1);
  out.body = spyUpperFirst(resolve(trim(t.at(row, "Scenario"))));
  return true;
}
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
  if (forcedChoice < 0 && ambientRng_.below(34) != 0) return false;
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
}  // namespace bl