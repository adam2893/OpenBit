// src/fam/birth.cpp -- a child is born (REPORT.en.md 151).
//
// The game runs this through SimPregnancy and two cards: ScenarioPregnancy asks the
// hero about the pregnancy, and ScenarioBirth reports the arrival and asks for a
// name ('{0} just gave birth to a baby {1}{2}!\n\nWhat will you name {3}?',
// #70148). The port carries the pregnancy as a flag on the hero and reports the
// birth as a report card, naming the child from the game's own pool.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>

namespace bl {

bool Sim::birthYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (forcedChoice < 0 && !h.relationshipOn) return false;
  if (h.loverName.empty()) return false;
  hero_ = &h;
  life_ = &life;
  if (!h.loverPregnant) {
    // shortcut: the game's conception roll lives behind SimPregnancy and the
    // hookup cards; the port uses a yearly coin on a partner of age.
    if (h.age < 18 || h.loverAge < 18 || h.loverAge > 45) return false;
    if (loverRng_.below(12) != 0) return false;
    h.loverPregnant = true;
    h.loverPregnantByHero = true;
    return false;   // the arrival is next year's card
  }
  h.loverPregnant = false;
  const bool girl = loverRng_.chance(0.5);
  const std::string word = h.loverStatus == "married"
                               ? (h.loverFemale ? "wife" : "husband")
                               : (h.loverFemale ? "girlfriend" : "boyfriend");
  Child born;
  born.female = girl;
  born.name = drawFirstName(girl);
  born.lastName = h.lastName;
  born.age = 0;
  born.bornAtHeroAge = h.age;
  born.otherParent = h.loverName;
  born.isHeroes = h.loverPregnantByHero;
  born.withCurrentLover = true;
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
  h.loverPregnantByHero = false;
  return true;
}

}  // namespace bl