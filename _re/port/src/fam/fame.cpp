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

// ---------------------------------------------------------------------------
// The magazine photo shoot (REPORT.en.md 335): SimPlace::RefreshAvailableFameOpportunities
// 0x2a431d4 builds a SimMagazineOpportunity, and ScenarioFameMagazine (.ctor 0x373c358,
// RefreshScenario 0x373c414, ChoiceSelectedAtIndex 0x373c864) asks for the answer. The place
// object is not ported, so the opportunity is rebuilt from the hero's own attributes on the
// yearly tick and queued as a card, the way the aircraft's wear is.

namespace {

const char* const kMonthNames[12] = {"January", "February", "March", "April", "May", "June",
                                     "July", "August", "September", "October", "November",
                                     "December"};

// The appeal the opportunity is built from: fame, lifted when the hero is beautiful and cut
// to a third for a criminal record, low karma or a plain face.
double magazineAppeal(const Hero& h) {
  double appeal = double(h.fame);
  if (h.looks > 90) appeal = (double(h.fame) + 200.0) / 3.0;
  else if (h.looks > 70) appeal = (double(h.fame) + 100.0) * 0.5;
  if (h.crimesAttempted > 0) appeal /= 3.0;  // SimPast::HasCriminalRecordInPlace
  if (h.karma < 10) appeal /= 3.0;
  if (h.looks < 30) appeal /= 3.0;
  return appeal;
}

// SimMagazineOpportunity::GetPaymentAmount 0x2b29464: the per-pose {A, B} pair, quartered
// at both ends for a male hero, the B end cut by the appeal, then
// DataCountry::GetLocalCostForAmountWithMinimumCostAndRoundTo(amount, 1000, 100).
long long magazinePayment(int pose, bool female, double appeal) {
  double a = 0.0, b = 0.0;
  switch (pose) {
    case 1: a = 15000.0; b = 100000.0; break;
    case 2: a = 50000.0; b = 150000.0; break;
    case 3: a = 50000.0; b = 250000.0; break;
    case 4: a = 250000.0; b = 1000000.0; break;
    case 5: a = 750000.0; b = 2500000.0; break;
    default: return 0;
  }
  if (!female) { a *= 0.25; b *= 0.25; }
  if (appeal < 70.0) b /= 3.0;
  if (appeal < 90.0) b *= 0.5;
  double amount = a + (b - a) * appeal / 100.0;
  amount = double(static_cast<long long>(amount / 100.0 + 0.5)) * 100.0;
  if (amount < 1000.0) amount = 1000.0;
  return static_cast<long long>(amount);
}

const char* magazineAppearance(int pose) {
  switch (pose) {
    case 1: return "clothed";                   // #55827
    case 2: return "in a swimsuit";             // #60870
    case 3: return "topless";                   // #67124
    case 4: return "nude";                      // #63041
    default: return "as the nude centerfold";   // #54422
  }
}

}  // namespace

// The pose ladder of RefreshAvailableFameOpportunities: 0 means no magazine wants the hero
// this year. A famous hero (over 80) is asked for a topless shoot first, and everything below
// that falls back to swimsuit then clothed.
int Sim::magazinePoseFor(const Hero& h) {
  const double appeal = magazineAppeal(h);
  Rng& g = magRng_;
  if (h.fame > 80.0f) {
    if (appeal > double(g.below(100))) return 3;
    return appeal > 80.0 ? 2 : 0;
  }
  if (appeal > double(g.below(100))) return 2;
  if (appeal > double(g.below(75))) return 1;
  return 0;
}

void Sim::magazineOpportunityYear(Life& life) {
  Hero& h = life.hero;
  if (h.age < 18 || h.inPrison || h.fame <= 0.0f) return;
  if (h.magPose > 0) return;  // one offer open at a time
  Rng& g = magRng_;
  // _monthNI += rand(0,4) on every refresh; GetStringForMonth indexes the twelve names.
  h.magMonthNI += int(g.below(4));
  const int pose = magazinePoseFor(h);
  if (pose <= 0) return;
  if (!ensureTable("a16_magazines-enc")) return;
  const Table& t = tbl("a16_magazines-enc");
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Title")).empty()) continue;
    const std::string gender = trim(t.at(r, "Gender"));
    if (gender == "male" || gender == "female") {
      if ((gender == "female") != h.female) continue;
    }
    rows.push_back(r);
  }
  if (rows.empty()) return;
  const size_t row = rows[g.below(uint32_t(rows.size()))];
  h.magPose = pose;
  h.magTitle = trim(t.at(row, "Title"));
  h.magRow = int(row);
  h.magPayment = magazinePayment(pose, h.female, magazineAppeal(h));
  life.pendingCards.push_back(kPendingMagazineOffer);
}

bool Sim::fameMagazineYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.magPose <= 0 || h.magTitle.empty()) return false;
  hero_ = &h;
  life_ = &life;
  Rng& g = magRng_;
  const std::string appear = magazineAppearance(h.magPose);
  const int monthIdx = h.magMonthNI - 1;
  const std::string month = (monthIdx >= 0 && monthIdx < 12) ? kMonthNames[monthIdx] : "";
  out.table = "registry:ScenarioFameMagazine";
  out.title = "Magazine";
  out.body = h.magTitle + " magazine has offered you a deal to appear " + appear + " in its " +
             month + " issue.\n\nWhat will you do?";  // #70159
  // Stored order: 'I can't do that' then 'Sign me up'; the accept lane is the one whose
  // label Contains 'sign'.
  out.options = {"I can't do that", "Sign me up"};
  const bool accept = forcedChoice >= 0 ? forcedChoice == 1 : g.chance(0.6);
  out.choice = out.options[accept ? 1 : 0];
  const std::string where =
      appear + " in the " + month + " issue of " + h.magTitle + " magazine";
  const int pose = h.magPose;
  const long long payment = h.magPayment;
  h.magPose = 0;
  if (!accept) {
    out.narrative = "I turned down an opportunity to appear " + where + ".";  // #25508
    out.discipline = pointsForImpact(4);
    return true;
  }
  // SuccessScore: rand(0,4)==0 -> rand(0,101), else rand(0,61)+40; the single virtual stat
  // call (ImpactFame) gets 4, 3 or 2 by the score; then discipline and karma -2 each.
  const int score = g.below(4) == 0 ? int(g.below(101)) : 40 + int(g.below(61));
  const int statPoints = score >= 91 ? 4 : score >= 51 ? 3 : 2;
  h.money += payment;
  out.fame = pointsForImpact(statPoints);
  out.discipline = pointsForImpact(-2);
  out.karma = pointsForImpact(-2);
  out.narrative = "I earned " + money(payment) + " by appearing " + where + ".";  // #24080
  // The follow-up card: a headline from the pose's own pool over the earnings line (#48418).
  static const char* const kPose1[] = {"Worked every angle", "Killed the couture",
                                       "Can I keep the clothes?"};
  static const char* const kPose2[] = {"This tan is gonna be wild", "That went swimmingly",
                                       "I didn't pack a towel"};
  static const char* const kPose3[] = {"The girls came out to play",
                                       "Lemme get something off my chest", "#FreeTheNipple"};
  static const char* const kPose4[] = {"Bodies are beautiful", "I should join a colony",
                                       "Nothing left to the imagination"};
  static const char* const kPose5[] = {"One page? That's cute", "Too hot for one page",
                                       "Right where I belong"};
  static const char* const kDefault[] = {"Spread", "Sell out", "Easy money"};
  const char* const* pool = kDefault;
  if (pose == 1) pool = kPose1;
  else if (pose == 2) pool = kPose2;
  else if (pose == 3) pool = kPose3;
  else if (pose == 4) pool = kPose4;
  else if (pose == 5) pool = kPose5;
  out.body += "\n\n" + std::string(pool[g.below(3)]) + "\nYou earned " + money(payment) +
              " by appearing " + where + ".";
  return true;
}

}  // namespace bl
