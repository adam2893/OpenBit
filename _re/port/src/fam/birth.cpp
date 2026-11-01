// src/fam/birth.cpp -- the pregnancy card, the miscarriage and the birth
// (REPORT.en.md 151 and 338).
//
// The game carries a pregnancy on SimPerson+0x48 and plays three beats: conception
// on the hero's own IncrementAge, ScenarioPregnancy to ask about it, and then -- on
// the next age-up -- ShouldSufferMiscarriage decides between ScenarioMiscarriage and
// ScenarioBirth. The port plays the same beats: a conception tick, the pregnancy
// card, and a resolution tick that pushes either the miscarriage or the birth.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>

namespace bl {

namespace {

// 'girlfriend' / 'wife' -- RelationshipStatusString(lover) lowercased, which the
// pregnancy and miscarriage bodies both open with.
std::string loverWord(const Hero& h) {
  if (h.loverStatus == "married") return h.loverFemale ? "wife" : "husband";
  if (h.loverStatus == "engaged") return h.loverFemale ? "fiancee" : "fiance";
  return h.loverFemale ? "girlfriend" : "boyfriend";
}

// GetNaturalConceptionAgeProbability (vaddr 0x2b3a730) as percent.
double conceptionPercent(int age) {
  if (age < 13) return 0.0;
  if (age < 18) return 45.0;
  if (age < 35) return 70.0;
  if (age < 40) return 40.0;
  if (age < 45) return 20.0;
  if (age < 50) return 6.0;
  if (age < 55) return 3.0;
  return 0.0;
}

}  // namespace

// The conception roll of SimPregnancy::GetResultingPregnancyForMotherAndFather...
// (vaddr 0x2b3a438): the mother's age probability first, then her fertility
// (SimPerson+0xa4) against a draw from 0 to 119.
void Sim::conceptionTick(Life& life) {
  Hero& h = life.hero;
  if (h.loverPregnant || h.loverName.empty()) return;
  if (h.age < 18 || h.loverAge < 13 || h.loverAge > 55) return;
  // The native gate reads the mother's own OnBirthControl (SimPerson+0x50). The
  // lover has no person model here, so the hero's flag stands for the couple.
  if (h.onBirthControl) return;
  // The lover has no attribute model either: draw her fertility once, in the shape
  // of SimPerson+0xa4 (a 0-100 float), instead of reading it.
  if (h.loverFertility < 0) h.loverFertility = int(loverRng_.below(101));
  if (!(conceptionPercent(h.loverAge) > double(loverRng_.below(100)))) return;
  if (!(double(h.loverFertility) > double(loverRng_.below(120)))) return;
  h.loverPregnant = true;
  h.loverPregnantByHero = true;
  life.pendingCards.push_back(kPendingPregnancy);
}

// The next age-up's decision: SimPerson::ShouldSufferMiscarriage (vaddr 0x2c47514)
// runs before the birth card is built, and the two are mutually exclusive
// (SimHero::IncrementAge 0x2a6378c and 0x2a637b8).
void Sim::pregnancyTick(Life& life) {
  Hero& h = life.hero;
  // The card owes a resolution; until it drains no second pregnancy may start, so
  // loverPregnant stays set and this flag says the card is already queued.
  if (!h.loverPregnant || h.loverBirthQueued) return;
  const int a = h.loverAge;
  double base = 0.0;
  if (a < 18) base = 15.0;
  else if (a < 30) base = 7.0;
  else if (a < 35) base = 8.0;
  else base = 3.0 * double(a) - 105.0 + 8.0;
  // The mother's stat average scales it. The lover has no stats, so the hero's
  // stand in (shortcut, and the reason the band is drawn from him).
  const double avg = (double(h.health) + double(h.looks) + double(h.smarts)) / 3.0;
  const double mult = avg < 20.0 ? 2.0 : avg < 40.0 ? 1.5 : avg < 80.0 ? 0.75 : 0.5;
  const int chance = int(base * mult);
  h.loverBirthQueued = true;
  if (int(loverRng_.below(101)) < chance) {
    life.pendingCards.push_back(kPendingMiscarriage);
  } else {
    life.pendingCards.push_back(kPendingBirth);
  }
}

// ScenarioPregnancy (RefreshScenario 0x2573b04, ChoiceSelectedAtIndex 0x2574344):
// body #50740 over the wrapper #68715, two stored choices, the decline clearing the
// pregnancy. The Golden Pacifier option is IAP-gated on Type==1 and a monetization
// flag the port has no access to, so it is left out (ceiling).
bool Sim::pregnancyYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.loverPregnant) return false;
  if (h.loverName.empty()) return false;
  out.table = "registry:ScenarioPregnancy";
  out.title = "Pregnancy";                                     // #34481
  out.body = "Your " + loverWord(h) + ", " + h.loverName +
             ", is pregnant with your baby!" +                 // #50740
             "\n\nWhat will you do?";                        // #68715
  out.narrative = h.loverName + " became pregnant with my baby!";  // #69869
  out.options = {"Keep the baby", "Don't keep the baby"};           // #28214 / #18038
  const int pick = forcedChoice < 0 ? int(loverRng_.below(2))
                                    : (forcedChoice > 1 ? 1 : forcedChoice);
  out.choice = out.options[size_t(pick)];
  if (pick == 1) {
    h.loverPregnant = false;
    h.loverPregnantByHero = false;
  h.loverBirthQueued = false;
    // The decline lane fires an unnamed -6 impact through vtable+0x508; the
    // attribute-point scale treats -6 as the 100-point step (REPORT.en.md 338).
    out.happiness = pointsForImpact(-6);
    out.extra.push_back("We chose not to keep the baby!");  // #46097
  }
  return true;
}

// ScenarioMiscarriage (RefreshScenario 0x25730a8): a dismiss-only card. The rare
// typed loss replaces both the title and the sentence; the ordinary card names a
// trimester and sometimes a blighted ovum.
bool Sim::miscarriageYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  if (h.loverName.empty()) return false;
  const std::string who = "Your " + loverWord(h) + ", " + h.loverName + ", ";
  out.table = "registry:ScenarioMiscarriage";
  if (int(loverRng_.below(10)) == 9) {
    static const char* kTypes[] = {"ectopic pregnancy",   // #58054
                                   "molar pregnancy",     // #62496
                                   "stillbirth",          // #66149
                                   "chemical pregnancy"}; // #55704
    const std::string type = kTypes[size_t(loverRng_.below(4))];
    const std::string art = articleFor(type);
    out.title = "Pregnancy Loss";                              // #34482
    out.body = who + "had " + art + type + " and lost the baby!";   // #50683
    out.narrative = h.loverName + " had " + art + type + " and lost the baby!";  // #69984
  } else {
    std::string descriptor = int(loverRng_.below(4)) == 0 ? "second-trimester"   // #65123
                                                          : "first-trimester";   // #58959
    if (int(loverRng_.below(20)) == 0) descriptor = "blighted ovum";             // #55036
    out.title = "Miscarriage";                                 // #30203
    out.body = who + "suffered a " + descriptor +
               " miscarriage and lost the baby!";              // #50784
    out.narrative = h.loverName + " suffered a " + descriptor + " miscarriage!";  // #70306
  }
  // The father who is the current lover takes a -2 impact (vtable+0x508, w1=-2).
  out.happiness = pointsForImpact(-2);
  h.loverPregnant = false;
  h.loverPregnantByHero = false;
  h.loverBirthQueued = false;
  return true;
}

// ScenarioBirth (RefreshScenario 0x256c9f8): the arrival, and the port names the
// child from the game's own pool because there is no text entry (ceiling).
bool Sim::birthYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  if (h.loverName.empty()) return false;
  hero_ = &h;
  life_ = &life;
  const bool girl = loverRng_.chance(0.5);
  const std::string word = loverWord(h);
  Child born;
  born.female = girl;
  born.name = drawFirstName(girl);
  born.lastName = h.lastName;
  born.age = 0;
  born.bornAtHeroAge = h.age;
  born.otherParent = h.loverName;
  born.isHeroes = h.loverPregnantByHero;
  born.withCurrentLover = true;
  born.city = h.city;
  out.table = "relationship";
  out.row_id = "0";
  // Body: the class's own sentence (#70148), cut before the naming question the
  // port cannot ask -- there is no text entry, and the child is named from the
  // game's pool instead.
  out.body = "Your " + word + ", " + h.loverName + ", just gave birth to a baby " +
             (girl ? "girl" : "boy") + "!";
  out.body[0] = static_cast<char>(::toupper(static_cast<unsigned char>(out.body[0])));
  out.narrative = "My " + word + ", " + h.loverName + " gave birth to a baby " +
                  (girl ? "girl" : "boy") + " named " + born.name + ".";
  out.extra.push_back("I named " + (girl ? std::string("her ") : std::string("him ")) +
                       born.name + ".");
  life.children.push_back(born);
  h.loverPregnant = false;
  h.loverPregnantByHero = false;
  h.loverBirthQueued = false;
  return true;
}

}  // namespace bl
