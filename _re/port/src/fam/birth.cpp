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

#include <algorithm>
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
  // The hero as the mother (REPORT.en.md 342). SimHero::IncrementAge runs the same
  // SimPregnancy::GetResultingPregnancyForMotherAndFatherAndTypeAndAssignToMother with the
  // hero as the mother and the current lover as the father, so the two rolls are the same
  // ones, over her age and her own Att_fertility. The port draws that fertility once
  // because the hero has no field for it, and the hero's birth-control flag is the
  // native OnBirthControl (SimPerson+0x50).
  if (h.female) {
    if (h.heroPregnant || h.heroBirthQueued) return;
    if (h.loverName.empty() || h.loverFemale) return;
    if (h.onBirthControl || h.age < 13 || h.age > 55) return;
    if (h.heroFertility < 0) h.heroFertility = int(loverRng_.below(101));
    if (!(conceptionPercent(h.age) > double(loverRng_.below(100)))) return;
    if (!(double(h.heroFertility) > double(loverRng_.below(120)))) return;
    h.heroPregnant = true;
    h.heroPregFather = h.loverName;
    life.pendingCards.push_back(kPendingHeroPregnancy);
    return;
  }
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
  // The hero as the mother (REPORT 342): the same ShouldSufferMiscarriage arithmetic, with
  // the carrying person's own age and her own three stats (the mother is the hero here, not
  // the lover, so nothing has to stand in).
  if (h.heroPregnant && !h.heroBirthQueued) {
    const int a = h.age;
    double base = 0.0;
    if (a < 18) base = 15.0;
    else if (a < 30) base = 7.0;
    else if (a < 35) base = 8.0;
    else base = 3.0 * double(a) - 105.0 + 8.0;
    const double avg = (double(h.health) + double(h.looks) + double(h.smarts)) / 3.0;
    const double mult = avg < 20.0 ? 2.0 : avg < 40.0 ? 1.5 : avg < 80.0 ? 0.75 : 0.5;
    h.heroBirthQueued = true;
    if (int(loverRng_.below(101)) < int(base * mult)) {
      life.pendingCards.push_back(kPendingHeroMiscarriage);
    } else {
      life.pendingCards.push_back(kPendingHeroBirth);
    }
    return;
  }
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
  // Multiples: the game rolls a lifetime denominator of 50 (8 for a surrogate or
  // donor pregnancy, which the port has no flags for) and, when that lands, a
  // 1-in-10 roll that makes three children instead of two
  // (PersonGenerator::GenerateHeroChildArrayInPlaceWithParentsAndGeneration
  // @0x337c1c8 and @0x337c1f4). Every child of a multiple birth shares the roll's
  // gender, which the same generator copies from the first child at 0x337c3a8.
  int kids = 1;
  if (birthRng_.below(50) == 0) kids = birthRng_.below(10) == 0 ? 3 : 2;
  const bool girl = birthRng_.chance(0.5);
  const std::string word = loverWord(h);
  out.table = "relationship";
  out.row_id = "0";
  if (kids == 1) {
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
  } else {
    // The multiple-birth body (#70149): '{0} just gave birth to {1} {2}{3}!', with
    // {1} the twin/triplet word and {2} 'boys' or 'girls'. The port names each child
    // itself and never repeats a name, the way the game's own name prompt does.
    const char* kind = kids == 3 ? "triplet" : "twin";     // #67250 / #67315
    const char* plural = girl ? "girls" : "boys";          // #59794 / #55180
    out.body = "Your " + word + ", " + h.loverName + ", just gave birth to " +
               kind + " " + plural + "!";
    out.body[0] = static_cast<char>(::toupper(static_cast<unsigned char>(out.body[0])));
    out.narrative = "My " + word + ", " + h.loverName + " gave birth to " +
                    kind + " " + plural + ".";
    std::vector<std::string> names;
    for (int i = 0; i < kids; ++i) {
      Child born;
      born.female = girl;
      born.name = drawFirstName(girl);
      for (int guard = 0; guard < 8 && std::find(names.begin(), names.end(), born.name) != names.end(); ++guard) {
        born.name = drawFirstName(girl);
      }
      names.push_back(born.name);
      born.lastName = h.lastName;
      born.age = 0;
      born.bornAtHeroAge = h.age;
      born.otherParent = h.loverName;
      born.isHeroes = h.loverPregnantByHero;
      born.withCurrentLover = true;
      born.city = h.city;
      // SimChild.Twins (+0x1fc) holds the number of siblings the birth produced.
      born.twins = kids - 1;
      life.children.push_back(born);
      out.extra.push_back("I named " + (girl ? std::string("her ") : std::string("him ")) +
                          born.name + ".");
    }
  }
  h.loverPregnant = false;
  h.loverPregnantByHero = false;
  h.loverBirthQueued = false;
  return true;
}


// ScenarioPregnancy with the hero as the mother (REPORT.en.md 342). The body is the
// father-aware #47765 when the father is the current lover and the generic #47766
// otherwise, both over the same #68715 wrapper and the same two stored choices; the
// decline clears the hero's pregnancy, fires the same unnamed -6 impact and logs #48177
// instead of the couple's #46097.
bool Sim::heroPregnancyYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.heroPregnant) return false;
  out.table = "registry:ScenarioPregnancy";
  out.title = "Pregnancy";  // #34481
  if (!h.heroPregFather.empty() && h.heroPregFather == h.loverName) {
    const std::string word = loverWord(h);
    out.body = "You are pregnant with your " + word + ", " + h.heroPregFather + ", baby!";  // #47765
    out.narrative = "I became pregnant with my " + word + ", " + h.heroPregFather + ", baby!";  // #23614
  } else {
    out.body = "You are pregnant!";  // #47766
    out.narrative = "I'm pregnant!";  // #26043
  }
  out.body += "\n\nWhat will you do?";  // #68715
  out.options = {"Keep the baby", "Don't keep the baby"};  // #28214 / #18038
  const int pick = forcedChoice < 0 ? int(loverRng_.below(2))
                                    : (forcedChoice > 1 ? 1 : forcedChoice);
  out.choice = out.options[size_t(pick)];
  if (pick == 1) {
    h.heroPregnant = false;
    h.heroBirthQueued = false;
    h.heroPregFather.clear();
    out.happiness = pointsForImpact(-6);
    out.extra.push_back("You chose not to keep the baby!");  // #48177
  }
  return true;
}

// ScenarioMiscarriage with the hero carrying it (REPORT 342): her own body and journal
// (#49447 / #25197, and the rare typed loss' #48616 / #24334) and the -5 impact the
// mother-hero path takes, against the lover's -2.
bool Sim::heroMiscarriageYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  if (!h.heroPregnant) return false;
  out.table = "registry:ScenarioMiscarriage";
  if (int(loverRng_.below(10)) == 9) {
    static const char* kTypes[] = {"ectopic pregnancy",   // #58054
                                   "molar pregnancy",     // #62496
                                   "stillbirth",          // #66149
                                   "chemical pregnancy"}; // #55704
    const std::string type = kTypes[size_t(loverRng_.below(4))];
    const std::string art = articleFor(type);
    out.title = "Pregnancy Loss";                                       // #34482
    out.body = "You had " + art + type + " and lost the baby!";         // #48616
    out.narrative = "I had " + art + type + " and lost the baby!";      // #24334
  } else {
    std::string descriptor = int(loverRng_.below(4)) == 0 ? "second-trimester"   // #65123
                                                          : "first-trimester";   // #58959
    if (int(loverRng_.below(20)) == 0) descriptor = "blighted ovum";             // #55036
    out.title = "Miscarriage";                                                  // #30203
    out.body = "You suffered a " + descriptor + " miscarriage and lost the baby!";  // #49447
    out.narrative = "I suffered a " + descriptor + " miscarriage!";                 // #25197
  }
  out.happiness = pointsForImpact(-5);
  h.heroPregnant = false;
  h.heroBirthQueued = false;
  h.heroPregFather.clear();
  return true;
}

// ScenarioBirth with the hero as the mother (REPORT 342): body #70148's own {0} is 'You'
// (#47521), so the card reads 'You just gave birth to a baby girl!'; the journal is the
// hero-mother line #24209 'I gave birth to {0} named {1}{2}.'. The port names the child
// from the game's own pool (there is no text entry) and re-draws on a collision, as the
// birth card already does (REPORT 340).
bool Sim::heroBirthYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  if (!h.heroPregnant) return false;
  hero_ = &h;
  life_ = &life;
  int kids = 1;
  if (birthRng_.below(50) == 0) kids = birthRng_.below(10) == 0 ? 3 : 2;
  const bool girl = birthRng_.chance(0.5);
  const std::string father = h.heroPregFather;
  std::string how;
  if (kids == 1) how = std::string("a baby ") + (girl ? "girl" : "boy");
  else how = std::string(kids == 3 ? "triplet " : "twin ") + (girl ? "girls" : "boys");
  out.table = "relationship";
  out.row_id = "0";
  out.body = "You just gave birth to " + how + "!";
  std::vector<std::string> names;
  for (int i = 0; i < kids; ++i) {
    Child born;
    born.female = girl;
    born.name = drawFirstName(girl);
    for (int guard = 0; guard < 8 && std::find(names.begin(), names.end(), born.name) != names.end(); ++guard) {
      born.name = drawFirstName(girl);
    }
    names.push_back(born.name);
    born.lastName = h.lastName;
    born.age = 0;
    born.bornAtHeroAge = h.age;
    born.otherParent = father;
    born.isHeroes = true;
    born.withCurrentLover = true;
    born.city = h.city;
    born.twins = kids - 1;
    life.children.push_back(born);
    out.extra.push_back("I named " + (girl ? std::string("her ") : std::string("him ")) +
                        born.name + ".");
  }
  std::string named = names.front();
  for (size_t i = 1; i < names.size(); ++i) named += " and " + names[i];
  out.narrative = "I gave birth to " + how + " named " + named + ".";  // #24209
  h.heroPregnant = false;
  h.heroBirthQueued = false;
  h.heroPregFather.clear();
  return true;
}

}  // namespace bl
