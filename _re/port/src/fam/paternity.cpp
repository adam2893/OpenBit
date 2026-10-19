// src/fam/paternity.cpp -- the court-ordered paternity test (REPORT.en.md 149).
//
// ScenarioExLoverPaternityTestCourtOrder is pushed by an ex-lover who has taken the
// hero to court over a child. Its body is built from six arguments, all of them
// read out of the Refresh:
//
//   {0} ExRelationshipStatusString(6) lowered   the ex's own word
//   {1} [ex+0x10]+0x18                           the ex's first name
//   {2} PronounStringForType(5) on the ex         he / she
//   {3} PronounStringForType(1) on the ex         his / her
//   {4} Format('{0}-year old {1}', child age, son/daughter)
//   {5} [child+0x10]+0x18                         the child's first name
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>

namespace bl {

bool Sim::paternityCourtOrderYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  const Hero::ExLover* ex = h.latestEx();
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (ex == nullptr) return false;
  // The card is about one of the ex's children, and about a hero who has not been
  // asked for the test yet (SimChild.NumTimesHeroAskedToTakePaternityTest).
  Child* kid = nullptr;
  for (Child& c : life.children) {
    // shortcut: a fixture child has no other parent recorded (REPORT.en.md 146), so
    // an empty parent matches the most recent ex rather than excluding every child.
    if (!c.alive) continue;
    if (!c.otherParent.empty() && c.otherParent != ex->name) continue;
    if (c.paternityAsked > 0) continue;
    // The real gate is ScenarioExLoverPaternityTest::ScenarioIsEligibleForRandomPush:
    // the ex must live in the same place as the hero
    // (SimPerson::LivesInSamePlaceAsHero @0x73f3ded080) and must have an eligible child
    // (GetEligibleChildFromExLover, which also consults SimLover::StepchildArray). The
    // port has no place for an ex, so it stands in with the one thing it can know: a
    // child whose father is not recorded as the hero is the child a test is asked over.
    if (c.isHeroes) continue;
    // The place half of the gate, now that an ex has one (REPORT.en.md 163):
    // SimPerson::LivesInSamePlaceAsHero. An empty city means the record predates the
    // field or the fixture did not set it, and is not evidence of distance.
    if (!ex->city.empty() && !h.city.empty() && ex->city != h.city) continue;
    kid = &c;
    break;
  }
  if (kid == nullptr) return false;
  if (h.age < 18) return false;
  if (forcedChoice < 0 && loverRng_.below(4) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const std::string exWord = lower(trim(ex->status == "married"
                                        ? (ex->female ? "Ex-Wife" : "Ex-Husband")
                                        : (ex->female ? "Ex-Girlfriend" : "Ex-Boyfriend")));
  const std::string who = "Your " + exWord + ", " + ex->name + ",";
  const std::string subj = ex->female ? "She" : "He";
  const std::string poss = ex->female ? "her" : "his";
  const std::string kidPhrase = std::to_string(kid->age) + "-year old " +
                                (kid->female ? "daughter" : "son");
  out.table = "relationship";
  out.row_id = "0";
  // Body: Format(cell 0x5cc5bb0).
  out.body = who + " has obtained a court order requiring you to take a paternity "
             "test. " + subj + " believes you are the father of " + poss + " " +
             kidPhrase + ", " + kid->name + ".";
  const int lane = forcedChoice >= 0 ? forcedChoice : int(loverRng_.below(2));
  if (lane == 0) {
    // 'Take the test' (#40919): the game pushes the results card. The port has no
    // results card yet, so only the counter moves (shortcut).
    out.choice = "Take the test";
    out.narrative = "My " + exWord + ", " + ex->name +
                    ", obtained a court order requiring me to take a paternity "
                    "test over " + poss + " " + kidPhrase + ", " + kid->name + ".";
    ++kid->paternityAsked;
    // The game pushes the results card in the same age-up (REPORT.en.md 133).
    life.pendingCards.push_back(kPendingPaternityResults);
  } else {
    // 'Refuse to take the test' (#35534): karma -2, discipline -2, and the game
    // pushes a crime scenario for crime type 38 -- refusing a court order.
    // shortcut: that scenario is not ported, so the stat move is all of it.
    out.choice = "Refuse to take the test";
    // The journal is the class's own _narrative (#30991), which the Refresh builds;
    // the branch only moves the stats and pushes the crime scenario.
    out.narrative = "My " + exWord + ", " + ex->name +
                    ", obtained a court order requiring me to take a paternity "
                    "test over " + poss + " " + kidPhrase + ", " + kid->name + ".";
    out.karma = pointsForImpact(-2);
    out.discipline = pointsForImpact(-2);
    ++kid->paternityAsked;
  }
  return true;
}

// ScenarioExLoverPaternityTestResults (REPORT.en.md 150): the answer to the court
// order. Two outcomes, each with its own title pool, body and journal:
//
//   positive  title  'New responsibilities' / 'Proof is in the pudding' /
//                    'The DNA matches'
//             body   'The paternity test for your {0}, {1}, {2} came back positive!'
//             journal mirrors it in first person
//   negative  title  'One less stress' / 'Positively negative' / 'Get outta here!'
//             body   '... came back negative!'
//
// The game decides which by rolling GetRandomNumber(0, 1000) against a field on the
// hero; the port keeps the answer on the child (Child::isHeroes) instead, so a
// fixture child that belongs to the ex stays hers (REPORT.en.md 150's shortcut).
// ScenarioExLoverPaternityTest (REPORT.en.md 251), the polite twin of the court
// order: the ex asks, and the played-life trace has every line of it --
//
//   My ex-fling, Ariana, asked me to take a paternity test for her 2-year old son,
//   Cleveland. / I agreed to take a paternity test.
//   The paternity test for my ex-fling, Ariana's, 2-year old son came back positive!
//   I became the legal father of Cleveland Jenner.
bool Sim::exPaternityYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.female) return false;
  const Hero::ExLover* ex = h.latestEx();
  if (ex == nullptr) return false;
  if (forcedChoice < 0 && !h.relationship && h.relationshipOn) return false;
  Child* kid = nullptr;
  for (Child& c : life.children) {
    if (!c.alive || c.isHeroes) continue;
    if (!c.otherParent.empty() && c.otherParent != ex->name) continue;
    if (!ex->city.empty() && !h.city.empty() && ex->city != h.city) continue;
    kid = &c;
    break;
  }
  if (kid == nullptr) return false;
  if (h.age < 18) return false;
  // No push rate is readable off the class; the coin is the port's own on the
  // family's stream, at about the one-in-twenty the trace suggests.
  if (forcedChoice < 0 && exPatRng_.below(12) != 0) return false;
  hero_ = &h;
  life_ = &life;
  const std::string exWord = lower(trim(ex->status == "married"
                                        ? (ex->female ? "Ex-Wife" : "Ex-Husband")
                                        : (ex->female ? "Ex-Girlfriend" : "Ex-Boyfriend")));
  const std::string kidPhrase = std::to_string(kid->age) + "-year old " +
                                (kid->female ? "daughter" : "son");
  const int lane = forcedChoice >= 0 ? (forcedChoice == 0 ? 0 : 1) : int(exPatRng_.below(2));
  const std::vector<std::string> extra = out.extra;
  out = AgeUp();
  out.extra = extra;
  out.table = "registry:ScenarioExLoverPaternityTest";
  out.title = "Paternity Test";
  // shortcut: the prompt's own sentence was not captured; the log sentence is the
  // one the game writes and it is rebuilt from the same six arguments as the log.
  out.body = "My " + exWord + ", " + ex->name + ", asked me to take a paternity "
             "test for her " + kidPhrase + ", " + kid->name + ".";
  out.options = {"I agree to take a paternity test", "Refuse the test"};
  if (lane == 0) {
    out.choice = "I agree to take a paternity test";
    out.narrative = "My " + exWord + ", " + ex->name + ", asked me to take a " +
                    "paternity test for her " + kidPhrase + ", " + kid->name + "." +
                    "\nI agreed to take a paternity test.";
    h.paternityProactive = true;
    life.pendingCards.push_back(kPendingProactivePaternity);
  } else {
    out.choice = "Refuse the test";
    // shortcut: no refusal line is in the trace, so the port writes none.
  }
  return true;
}

bool Sim::paternityResultsYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  const Hero::ExLover* ex = h.latestEx();
  if (ex == nullptr) return false;
  Child* kid = nullptr;
  for (Child& c : life.children) {
    if (!c.alive) continue;
    // The court-order result is about a baby (the class asks within three years);
    // the ex who asks in person can do it at any age, as the trace's 2-year old
    // and the fixture's grown child both show (REPORT.en.md 251).
    if (!h.paternityProactive && c.age > 3) continue;
    if (!c.otherParent.empty() && c.otherParent != ex->name) continue;
    kid = &c;
    break;
  }
  if (kid == nullptr) return false;
  const std::string who = "your " + (kid->female ? std::string("daughter") : "son") +
                          ", " + kid->name;
  const std::string mine = "my " + (kid->female ? std::string("daughter") : "son") +
                           ", " + kid->name;
  out.table = "relationship";
  out.row_id = "0";
  if (h.paternityProactive) {
    out.table = "registry:ScenarioExLoverPaternityTestResult";
    // The proactive test names the ex and the child the way the trace does
    // (REPORT.en.md 251), and a positive result makes the hero the legal father.
    const std::string exWord = lower(trim(ex->status == "married"
                                          ? (ex->female ? "Ex-Wife" : "Ex-Husband")
                                          : (ex->female ? "Ex-Girlfriend" : "Ex-Boyfriend")));
    const std::string exPhrase = "my " + exWord + ", " + ex->name + "'s, " +
                                 std::to_string(kid->age) + "-year old " +
                                 (kid->female ? "daughter" : "son");
    const bool positive = kid->isHeroes || loverRng_.chance(0.6);
    h.paternityProactive = false;
    if (positive) {
      kid->isHeroes = true;
      out.title = "The DNA matches";
      out.body = "The paternity test for " + exPhrase + " came back positive!";
      out.narrative = "The paternity test for " + exPhrase + " came back positive!";
      out.extra.push_back("I became the legal father of " + kid->name + " " +
                          h.lastName + ".");
    } else {
      out.title = "One less stress";
      out.body = "The paternity test for " + exPhrase + " came back negative!";
      out.narrative = "The paternity test for " + exPhrase + " came back negative!";
    }
    return true;
  }
  if (kid->isHeroes) {
    static const char* const kTitles[] = {"New responsibilities",
                                           "Proof is in the pudding", "The DNA matches"};
    out.title = kTitles[loverRng_.below(3)];
    out.body = "The paternity test for " + who + " came back positive!";
    out.narrative = "The paternity test for " + mine + " came back positive!";
    out.choice = "I had no doubt";
  } else {
    static const char* const kTitles[] = {"One less stress", "Positively negative",
                                           "Get outta here!"};
    out.title = kTitles[loverRng_.below(3)];
    out.body = "The paternity test for " + who + " came back negative!";
    out.narrative = "The paternity test for " + mine + " came back negative!";
    out.choice = "Get outta here!";
  }
  return true;
}
}  // namespace bl