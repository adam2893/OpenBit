// src/fam/cult.cpp -- moved out of src/sim.cpp (REPORT.en.md 131).
//
// Cut verbatim by tools/split_family.py; the only edits are the includes.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>
#include <vector>

namespace bl {

// The commune: the hero founds a cult, and the year is spent keeping the
// followers believing. Every table of the a16_cult family is drawn from here.
namespace {
// The four philosophies are the flag columns of the commune-name table.
const char* const kCultPhilosophyFlags[] = {"Armageddon", "New Age", "Antiestablishment", "Alien"};
const char* const kCultPhilosophyNames[] = {"armageddon", "new age", "antiestablishment", "alien"};

// A gender cell of this family is blank, "both", "male" or "female".
bool cultGenderFits(const std::string& cell, bool wantFemale) {
  const std::string g = lower(trim(cell));
  if (g.empty() || g == "both" || g == "all" || g == "any") return true;
  if (g == "male") return !wantFemale;
  if (g == "female") return wantFemale;
  return true;
}
}  // namespace

bool Sim::cultYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.cultLeader || h.age < 18) return false;
  if (!cultRng_.chance(0.55)) return false;
  ensureCultFields(life);
  rndCache_.clear(); rbLast_.clear();
  cultCommunard_.clear();
  cultExCommunard_.clear();
  pronounOtherSet_ = false;
  std::string title, body, line, tableRef, rowRef;
  std::vector<std::string> extra;
  int happiness = 0;
  auto say = [&body](const std::string& s) {
    if (!s.empty()) body += body.empty() ? s : " " + s;
  };
  auto tell = [&line](const std::string& s) {
    if (!s.empty()) line += line.empty() ? s : " " + s;
  };
  auto mark = [&tableRef, &rowRef](const Table& t, size_t r) {
    tableRef = t.name();
    rowRef = std::to_string(r + 1);
  };
  auto cell = [this](const Table& t, size_t r, const char* col) -> std::string {
    if (!t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  auto bump = [&h](int d) {
    h.cultLoyalty += d;
    if (h.cultLoyalty < 0) h.cultLoyalty = 0;
    if (h.cultLoyalty > 100) h.cultLoyalty = 100;
  };
  // A row whose Philosophy cell is blank belongs to every commune; a row that
  // names one only fits that one. The filtered draw is tried first.
  auto cultPick = [&](const char* table, const char* weight, const char* phiCol) -> size_t {
    if (phiCol && *phiCol) {
      const size_t r0 = pickRow(table, weight, phiCol, h.cultPhilosophy, "", cultRng_);
      if (r0 != size_t(-1)) return r0;
    }
    return pickRow(table, weight, "", "", "", cultRng_);
  };
  // The followers' devotion drifts a little every year.
  bump(int(cultRng_.below(7)) - 3);

  // --- The commune shop: the followers sell what they make.
  if (title.empty() && cultRng_.chance(0.06)) {
    const size_t r = cultPick("a16_cult-workshop-products-enc", "Engagement Score", "Philosophy");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-workshop-products-enc");
      std::string product = cell(t, r, "Product Name Plural");
      if (product.empty()) product = cell(t, r, "Product Name");
      const std::string place = cell(t, r, "Optimal Location");
      const double cost = t.num(r, "Min Unit Cost");
      const double made = t.num(r, "Average Unit Price") * double(3 + cultRng_.below(10));
      title = "The commune shop";
      say("My followers sold " + (product.empty() ? std::string("plaques") : product) +
          (place.empty() ? std::string() : " at the " + place) + " and brought in " +
          money((long long)(made - cost)) + ".");
      mark(t, r);
      happiness += 1;
    }
  }

  // --- A fast: the reason first, then what the hunger felt like.
  if (title.empty() && cultRng_.chance(0.08)) {
    const size_t r = cultPick("a16_cult-fast-reasons-enc", "Engagement Score", "Philosophy");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-fast-reasons-enc");
      const std::string what = cell(t, r, "Scenario Snippet");
      const std::string why = cell(t, r, "Reason");
      title = "The fast";
      if (!what.empty()) say("I led the commune in fasting " + what + ".");
      const size_t f = pickRow("a16_cult-fast-hunger-foods-enc", "Commonness Score", "", "", "", cultRng_);
      if (f != size_t(-1)) {
        const Table& ft = tbl("a16_cult-fast-hunger-foods-enc");
        const std::string food = cell(ft, f, "Food");
        const std::string art = withArticle(food, ft.flag(f, "Uses Indefinite Article"));
        if (!food.empty()) say("By the end we were all dreaming about " + art + food + ".");
      }
      if (!why.empty()) tell("I called the fast " + why + ".");
      mark(t, r);
      bump(2);
    }
  }

  // --- A film about the commune, or a drum circle with a name act.
  if (title.empty() && cultRng_.chance(0.08)) {
    if (cultRng_.chance(0.5)) {
      const size_t r = cultPick("a16_cult-affirmation-film-titles-enc", "Engagement Score", "Philosophy");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-affirmation-film-titles-enc");
        const std::string film = cell(t, r, "Title");
        const double eng = t.num(r, "Engagement Score");
        title = "A film about the commune";
        say("Film crews followed us for a year and released " + (film.empty() ? std::string("a documentary") : film) + ".");
        const size_t rec = pickWindow("a16_cult-affirmation-receptions-enc", "", "", "", "Min Reception",
                                      "Max Reception", eng * 20.0, cultRng_);
        if (rec != size_t(-1)) {
          const Table& rt = tbl("a16_cult-affirmation-receptions-enc");
          const std::string s = cell(rt, rec, "String");
          if (!s.empty()) say(s);
          mark(rt, rec);
          bump(int(eng) - 1);
          happiness += 1;
        }
        if (tableRef.empty()) mark(t, r);
      }
    } else {
      const size_t ar = pickRow("a16_cult-drum-circle-artists-enc", "Engagement Score", "", "", "", cultRng_);
      if (ar != size_t(-1)) {
        const Table& at = tbl("a16_cult-drum-circle-artists-enc");
        const std::string artist = cell(at, ar, "Artist Name");
        if (!artist.empty()) {
          title = "A drum circle";
          say("We held a drum circle, and " + artist + " played until the fire went out.");
          if (lower(trim(at.at(ar, "Philosophy Engagement Bonus"))) == h.cultPhilosophy) bump(4);
          else bump(1);
          mark(at, ar);
          happiness += 2;
        }
      }
    }
  }

  // --- Someone who left: the ones who shout at the gate, and the locals who nag.
  if (title.empty() && cultRng_.chance(0.08)) {
    if (cultRng_.chance(0.5)) {
      const size_t r = pickWindow("a16_cult-ex-communard-dissent-scenario-enc", "", "", "", "Min Craziness",
                                  "Max Craziness", double(cultRng_.below(101)), cultRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-ex-communard-dissent-scenario-enc");
        const std::string scen = cell(t, r, "Scenario");
        if (!scen.empty()) {
          title = "A former follower";
          say(scen);
          const std::string l = cell(t, r, "Log Version");
          if (!l.empty()) tell(l);
          mark(t, r);
          bump(-3);
          happiness -= 1;
        }
      }
    } else {
      const size_t r = pickWindow("a16_cult-anti-commune-nag-scenario-enc", "", "", "", "Audience Min Age",
                                  "Audience Max Age", double(h.age), cultRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-anti-commune-nag-scenario-enc");
        const std::string who = lower(trim(t.at(r, "Audience")));
        const std::string s = cell(t, r, "Snippet");
        if (!s.empty()) {
          title = "The neighbours";
          pronounOtherSet_ = who.find("male") != std::string::npos && who.find("female") == std::string::npos;
          pronounOtherFemale_ = who.find("female") != std::string::npos;
          say(spyUpperFirst(s));
          const std::string l = cell(t, r, "Log Version");
          if (!l.empty()) tell(l);
          mark(t, r);
          bump(-1);
        }
      }
    }
  }

  // --- A trip: together on the commune's own supply, or alone with it.
  if (title.empty() && cultRng_.chance(0.10)) {
    const bool shared = cultRng_.chance(0.5);
    const char* tt = shared ? "a16_cult-drug-shared-experience-enc" : "a16_cult-drug-solo-experience-enc";
    const size_t r = cultPick(tt, "Commonness Score", "Philosophy");
    if (r != size_t(-1)) {
      const Table& t = tbl(tt);
      const std::string snip = cell(t, r, "String Snippet");
      const std::string logv = cell(t, r, "Log Version");
      std::string drug = cell(t, r, "Drug");
      std::string drugNote;
      if (drug.empty()) {
        const size_t dr = pickRow("a16_cult-spiritual-journey-drugs-enc", "", "", "", "", cultRng_);
        if (dr != size_t(-1)) {
          const Table& dt = tbl("a16_cult-spiritual-journey-drugs-enc");
          drug = cell(dt, dr, "Name");
          if (!drug.empty()) {
            const double risk = cultRng_.below(4) == 0 ? dt.num(dr, "Double Dose Risk Score")
                               : cultRng_.below(2) == 0 ? dt.num(dr, "Dose Risk Score")
                                                        : dt.num(dr, "Microdose Risk Score");
            drugNote = "We paid " + money((long long)dt.num(dr, "Min Price Per Dose")) + " a dose for it.";
            if (risk >= 4 && cultRng_.chance(0.35)) {
              extra.push_back("The dose went wrong: a follower was hospitalised.");
              bump(-2);
            }
          }
        }
      }
      if (!snip.empty() || !logv.empty()) {
        title = shared ? "A shared trip" : "A solo trip";
        // Shared snippets are past-tense clauses, solo snippets gerunds.
        if (!snip.empty()) say(shared ? ("My followers and I " + spySentence(spyLowerFirst(snip)))
                                      : ("I remember " + spySentence(spyLowerFirst(snip))));
        else say(logv);
        if (!drug.empty()) say("The commune supply: " + drug + ".");
        if (!drugNote.empty()) extra.push_back(drugNote);
        mark(t, r);
        if (lower(trim(t.has("Experience Type") ? t.at(r, "Experience Type") : std::string())) == "positive") bump(2);
        else bump(-2);
        happiness += 1;
      }
    }
  }

  // --- The totem: a lecture about it, a sign from it, or an insult to it.
  if (title.empty() && cultRng_.chance(0.10)) {
    const int mode = int(cultRng_.below(3));
    if (mode == 0) {
      const size_t r = pickRow("a16_cult-totem-lecture-enc", "Commonness Score", "", "", "", cultRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-totem-lecture-enc");
        const std::string s = cell(t, r, "String");
        if (!s.empty()) {
          title = "The totem";
          say("I lectured the commune " + s + ".");
          mark(t, r);
          bump(2);
        }
      }
    } else if (mode == 1) {
      const size_t r = pickRow("a16_cult-totem-inspiration-enc", "Commonness Score", "", "", "", cultRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-totem-inspiration-enc");
        const std::string s = cell(t, r, "String");
        if (!s.empty()) {
          title = "The totem speaks";
          say("I sat with " + h.cultTotem + " and " + s + ".");
          if (t.flag(r, "Humorous")) happiness += 2;
          mark(t, r);
          bump(1);
        }
      }
    } else {
      const size_t r = pickWindow("a16_cult-totem-disrespect-enc", "", "", "", "Min Loyalty", "Max Loyalty",
                                  double(h.cultLoyalty), cultRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-totem-disrespect-enc");
        const std::string act = cell(t, r, "String");
        const std::string l = cell(t, r, "Log Version");
        if (!act.empty() || !l.empty()) {
          title = "Disrespecting the totem";
          if (!l.empty()) tell(l);
          else say("I caught a follower " + act + " " + h.cultTotem + ".");
          extra.push_back("The rule was enforced: the follower was punished in front of the commune.");
          mark(t, r);
          bump(-1);
        }
      }
    }
  }

  // --- A ceremony: a healing ritual, and the hands laid on a follower.
  if (title.empty() && cultRng_.chance(0.10)) {
    const size_t r = pickRow("a16_cult-healing-ceremony-enc", "Engagement Score", "", "", "", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-healing-ceremony-enc");
      const std::string kind = cell(t, r, "Ceremony Type");
      const std::string fb = cell(t, r, "Feedback Snippet");
      title = "A ceremony";
      if (!kind.empty()) say("I called a " + kind + " ceremony.");
      if (!fb.empty()) say("A follower came forward and " + fb + ".");
      const std::string power = pickSnippet("a16_cult-empowerments-enc", "String", "Commonness Score", "", "", "", cultRng_);
      if (!power.empty()) say(spySentence(spySwap(spySwap(spySwap(spySwap(power, "You ", "I "), " your ", " my "), "your ", "my "), "you ", "I ")));
      const size_t gr = pickWindow("a16_cult-gratitude-scenarios-enc", "", "", "", "Min Loyalty", "Max Loyalty",
                                   double(h.cultLoyalty), cultRng_);
      if (gr != size_t(-1)) {
        const Table& gt = tbl("a16_cult-gratitude-scenarios-enc");
        const std::string s = cell(gt, gr, "String");
        if (!s.empty()) { say(s); mark(gt, gr); }
      }
      if (tableRef.empty()) mark(t, r);
      bump(int(t.num(r, "Effectiveness Score")) + 1);
      happiness += 2;
    }
  }

  // --- A rumour moving through the commune, in one direction or the other.
  if (title.empty() && cultRng_.chance(0.10)) {
    const bool inbound = cultRng_.chance(0.5);
    const size_t r = pickRow("a16_cult-communard-rumor-enc", "", "", "", "", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-communard-rumor-enc");
      const char* col = inbound ? "Inbound Scenario" : "Outbound Scenario";
      const std::string scen = cell(t, r, col);
      if (!scen.empty()) {
        title = inbound ? "A rumour reaches me" : "A rumour leaves the commune";
        say(scen);
        const std::string l = cell(t, r, "Inbound Scenario Log Version");
        if (inbound && !l.empty()) tell(l);
        mark(t, r);
        bump(inbound ? -2 : 2);
      }
    }
  }

  // --- Doubt, devotion, a warning, or a follower begging not to be expelled.
  if (title.empty() && cultRng_.chance(0.10)) {
    const int mode = int(cultRng_.below(3));
    const char* table = mode == 0 ? "a16_cult-communard-doubts-enc"
                       : mode == 1 ? "a16_cult-communard-apply-color-enc"
                                   : "a16_cult-communard-warning-enc";
    const size_t r = pickWindow(table, "", "", "", "Min Loyalty", "Max Loyalty", double(h.cultLoyalty), cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl(table);
      const std::string s = cell(t, r, mode == 2 ? "Scenario" : "String");
      if (!s.empty()) {
        title = mode == 2 ? "A warning" : "A follower speaks";
        say(spyUpperFirst(s));
        mark(t, r);
        bump(mode == 1 ? 2 : -1);
        if (mode == 2 && t.has("Risk Score") && t.num(r, "Risk Score") >= 4) {
          const size_t br = pickWindow("a16_cult-expel-beg-scenario-enc", "", "", "", "Min Loyalty", "Max Loyalty",
                                       double(h.cultLoyalty), cultRng_);
          if (br != size_t(-1)) {
            const Table& bt = tbl("a16_cult-expel-beg-scenario-enc");
            const std::string beg = cell(bt, br, "String");
            if (!beg.empty()) extra.push_back(beg);
            if (h.communards > 0) --h.communards;
          }
        }
      }
    }
  }

  // --- A building project the commune has not taken on yet.
  if (title.empty() && cultRng_.chance(0.10)) {
    const size_t r = pickRow("a16_cult-commune-structure-enc", "", "", "", "", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-commune-structure-enc");
      const std::string project = cell(t, r, "Project");
      bool owned = false;
      for (size_t i = 0; i < h.cultStructures.size(); ++i)
        if (lower(trim(h.cultStructures[i])) == lower(project)) owned = true;
      if (!project.empty() && !owned) {
        h.cultStructures.push_back(project);
        title = "A new building";
        say("The commune broke ground on " + articleFor(project) + project + ".");
        const std::string sub = cell(t, r, "Secondary Line Text");
        if (!sub.empty()) say(spySentence(sub));
        tell("It set the commune back " + money((long long)t.num(r, "Minimum Cost")) + ".");
        double appeal = t.num(r, "Appeal Impact");
        if (lower(trim(t.at(r, "Bonus Appeal Impact (Philosophy)"))) == h.cultPhilosophy) appeal += 3;
        bump(int(appeal));
        const size_t fr = pickRow("a16_cult-commune-structure-feedback-enc", "Commonness Score", "Structure",
                                  lower(project), "", cultRng_);
        if (fr != size_t(-1)) {
          const Table& ft = tbl("a16_cult-commune-structure-feedback-enc");
          const std::string s = cell(ft, fr, "Scenario");
          if (!s.empty()) { say(spySentence(s)); mark(ft, fr); }
          const std::string l = cell(ft, fr, "Log Version");
          if (!l.empty()) extra.push_back(l);
        }
        if (tableRef.empty()) mark(t, r);
        happiness += 2;
      }
    }
  }

  // --- A demonstration of devotion, and what it cost.
  if (title.empty() && cultRng_.chance(0.12)) {
    const size_t r = cultPick("a16_cult-devotion-demonstration-enc", "Commonness Score", "Philosophy");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-devotion-demonstration-enc");
      const std::string act = cell(t, r, "Scenario Snippet");
      const double risk = t.num(r, "Risk Score");
      if (double(cultRng_.below(100)) < risk * 10.0) {
        const std::string d = cell(t, r, "Death Snippet");
        title = "A demonstration of devotion";
        say("A follower died " + (d.empty() ? std::string("trying to prove their devotion") : d) + ".");
        const size_t bl = pickRow("a16_cult-body-discard-locations-enc", "Commonness Score", "", "", "", cultRng_);
        if (bl != size_t(-1)) {
          const Table& bt = tbl("a16_cult-body-discard-locations-enc");
          const std::string where = cell(bt, bl, "String");
          if (!where.empty()) extra.push_back("The body was left " + where + ".");
          bump(-2);
        }
        if (h.communards > 0) --h.communards;
      } else {
        const std::string s = cell(t, r, "Success Snippet");
        title = "A demonstration of devotion";
        // The Scenario Snippet is a base-form verb phrase and the Success
        // Snippet a gerund or noun phrase, so they cannot share one frame.
        say("A follower volunteered to " + spyLowerFirst(act) + " in my honour.");
        if (!s.empty()) tell("The outcome: " + spySentence(s));
        bump(4);
        happiness += 2;
      }
      mark(t, r);
    }
  }

  // --- Rewarding a follower for something small.
  if (title.empty() && cultRng_.chance(0.12)) {
    const size_t r = pickRow("a16_cult-communard-rewards-enc", "", "", "", "", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-communard-rewards-enc");
      const std::string what = cell(t, r, "Scenario");
      if (!what.empty()) {
        title = "Rewarding a follower";
        const std::string who = cultToken("communard");
        say("I rewarded " + who + " with " + what + ".");
        tell("I gave " + who + " " + what + ".");
        bump(int(t.num(r, "Loyalty Impact")));
        mark(t, r);
      }
    }
  }

  // --- The rules of the commune: one is stated, one is broken, one is punished.
  if (title.empty() && cultRng_.chance(0.15)) {
    const size_t r = pickRow("a16_cult-doctrine-rules-enc", "Commonness Score", "", "", "", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-doctrine-rules-enc");
      const std::string rule = cell(t, r, "List String");
      if (!rule.empty()) h.cultDoctrine = rule;
      const std::string broken = cell(t, r, "Scenario Snippet");
      title = "A rule of the commune";
      if (!rule.empty()) say("The rule in force: " + rule + ".");
      const std::string who = cultToken("communard");
      if (!broken.empty()) say(who + " was caught " + broken + ".");
      const std::string logv = cell(t, r, "Log Version");
      if (!logv.empty()) tell("I caught " + who + " when they " + spyLowerFirst(logv) + ".");
      if (t.flag(r, "Disobedience Can Be Fatal") && cultRng_.chance(0.25)) {
        const std::string d = cell(t, r, "Disobedient Death Snippet");
        extra.push_back(who + " did not survive the lesson" + (d.empty() ? std::string(".") : ": " + d + "."));
        if (h.communards > 0) --h.communards;
        bump(-6);
      } else if (cultRng_.chance(0.5)) {
        const size_t pr = pickRow("a16_cult-communard-punishment-enc", "Commonness Score", "", "", "", cultRng_);
        if (pr != size_t(-1)) {
          const Table& pt = tbl("a16_cult-communard-punishment-enc");
          const std::string fb = cell(pt, pr, "Feedback Scenario");
          if (!fb.empty()) say(fb);
          const std::string pl = cell(pt, pr, "Log Version");
          if (!pl.empty()) extra.push_back(pl);
          bump(int(pt.num(pr, "Loyalty Impact")));
          mark(pt, pr);
        }
      } else {
        const std::string ob = cell(t, r, "Obedience Snippet");
        if (!ob.empty()) extra.push_back("The rest obeyed: " + ob + ".");
        bump(int(t.num(r, "Appeal Score")));
      }
      pronounOtherSet_ = false;
      if (tableRef.empty()) mark(t, r);
    }
  }

  // --- Time with the followers, and the things they want to talk about.
  if (title.empty() && cultRng_.chance(0.15)) {
    if (cultRng_.chance(0.5)) {
      const size_t r = cultPick("a16_cult-communard-spend-time-enc", "", "Philosophy");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-communard-spend-time-enc");
        const std::string pastime = cell(t, r, "Pastime");
        if (!pastime.empty()) {
          title = "Time with my followers";
          say("I took my followers " + pastime + ".");
          bump(2);
          mark(t, r);
        }
      }
    } else {
      const size_t r = cultPick("a16_cult-communard-conversation-enc", "", "Philosophy");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-communard-conversation-enc");
        const std::string conv = cell(t, r, "Conversation");
        if (!conv.empty()) {
          title = "A conversation";
          say(cultToken("communard") + " wanted to talk about " + conv + ".");
          if (t.num(r, "Controversy Score") >= 3) {
            h.cultCharisma += 3;
            bump(1);
          } else {
            bump(2);
          }
          mark(t, r);
        }
      }
    }
  }

  // --- A lesson, and the followers who have heard it too many times.
  if (title.empty() && cultRng_.chance(0.18)) {
    const size_t r = cultPick("a16_cult-commune-teaching-enc", "Engagement Score", "Philosophy");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-commune-teaching-enc");
      const std::string scen = cell(t, r, "Scenario");
      if (!scen.empty()) {
        title = "A lesson";
        say("I taught my followers " + scen + ".");
        const size_t er = cultPick("a16_cult-communard-educate-enc", "", "Philosophy");
        if (er != size_t(-1)) {
          const Table& et = tbl("a16_cult-communard-educate-enc");
          const std::string topic = cell(et, er, "Topic");
          if (!topic.empty()) extra.push_back("The lesson: " + spySentence(topic));
          bump(int(et.num(er, "Loyalty Impact")));
          mark(et, er);
        }
        if (cultRng_.chance(0.30)) {
          const size_t lr = pickRow("a16_cult-communard-leave-scenario-enc", "", "Reason", "educate", "", cultRng_);
          if (lr != size_t(-1)) {
            const Table& lt = tbl("a16_cult-communard-leave-scenario-enc");
            const std::string s = cell(lt, lr, "Scenario Snippet");
            if (!s.empty()) {
              say(cultToken("communard") + " is losing patience: " + s);
              bump(-3);
            }
          }
        }
        if (tableRef.empty()) mark(t, r);
      }
    }
  }

  // --- Recruitment: the pitch, the place, and the odds of the police noticing.
  if (title.empty() && cultRng_.chance(0.20)) {
    const size_t lr = pickRow("a16_cult-recruitment-location-enc", "Commonness Score", "", "", "", cultRng_);
    const size_t mr = cultPick("a16_cult-recruitment-message-enc", "Success Score", "Philosophy");
    if (lr != size_t(-1) && mr != size_t(-1)) {
      const Table& lt = tbl("a16_cult-recruitment-location-enc");
      const Table& mt = tbl("a16_cult-recruitment-message-enc");
      const std::string place = cell(lt, lr, "Location");
      const std::string msg = cell(mt, mr, "Message");
      const double success = mt.num(mr, "Success Score");
      const double police = lt.num(lr, "Police Score");
      title = "Recruiting";
      if (!place.empty())
        say("I stood outside the " + place + " and told anyone who would listen: " + msg + ".");
      if (double(cultRng_.below(100)) < success * 12.0 - police * 4.0 + h.cultCharisma * 0.3) {
        ++h.communards;
        say("One of them followed me home.");
        bump(2);
        mark(mt, mr);
      } else {
        // One heckler, one card: the String column carries the bracket the log
        // line points at with '<rb>', so the String is resolved first.
        const size_t hr = pickRow("a16_cult-outreach-mission-heckles-enc", "Commonness Score",
                                  "", "", "", cultRng_);
        if (hr != size_t(-1)) {
          const Table& ht = tbl("a16_cult-outreach-mission-heckles-enc");
          const std::string heckle = cell(ht, hr, "String");
          if (!heckle.empty()) say(spySentence(heckle));
          const std::string hl = cell(ht, hr, "Log Version");
          if (!hl.empty()) tell(hl);
        }
        bump(-1);
        if (tableRef.empty()) mark(lt, lr);
      }
    }
  }

  // --- The commune's own prompt card: gated on age, philosophy, loyalty and
  //     charisma, and answered with its first choice.
  if (title.empty() && cultRng_.chance(0.25)) {
    const Table& t = tbl("a16_cult-commune-dumb-scenarios-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      const double lo = t.num(r, "Hero Min Age");
      const double hi = t.num(r, "Hero Max Age");
      if (lo > 0 && h.age < lo) continue;
      if (hi > 0 && h.age > hi) continue;
      if (!h.female && !cultGenderFits(t.at(r, "Hero Gender"), true)) continue;
      if (h.female && !cultGenderFits(t.at(r, "Hero Gender"), false)) continue;
      if (!h.loverName.empty() && !cultGenderFits(t.at(r, "Lover Gender"), h.loverFemale)) continue;
      if (h.loverName.empty() && t.flag(r, "Hero Has Lover")) continue;
      if (t.flag(r, "Requires Communard") && h.communards <= 0) continue;
      const std::string phil = lower(trim(t.at(r, "Commune Philosophy")));
      if (!phil.empty() && phil != h.cultPhilosophy) continue;
      const double ll = t.num(r, "Min Loyalty");
      const double hl = t.num(r, "Max Loyalty");
      if (ll > 0 && h.cultLoyalty < ll) continue;
      if (hl > 0 && h.cultLoyalty > hl) continue;
      const double lc = t.num(r, "Min Charisma");
      const double hc = t.num(r, "Max Charisma");
      if (lc > 0 && h.cultCharisma < lc) continue;
      if (hc > 0 && h.cultCharisma > hc) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[cultRng_.below(uint32_t(pool.size()))];
      title = cell(t, r, "Title");
      say(cell(t, r, "Body"));
      const std::string cta = cell(t, r, "CTA");
      if (!cta.empty()) extra.push_back(cta);
      const std::string choice = cell(t, r, "Choice 1");
      if (!choice.empty()) tell("I said: " + choice + ".");
      const std::string narr = cell(t, r, "Narrative 1");
      if (!narr.empty()) say(spySentence(narr));
      bump(int(t.num(r, "Loyalty Impact 1")));
      h.cultCharisma += int(t.num(r, "Charisma Impact 1"));
      mark(t, r);
    }
  }

  // --- The ledger: what the followers make of the year.
  if (title.empty() && cultRng_.chance(0.30)) {
    const size_t r = pickWindow("a16_cult-commune-ledger-color-enc", "", "", "", "Min Loyalty", "Max Loyalty",
                                double(h.cultLoyalty), cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-commune-ledger-color-enc");
      const std::string s = cell(t, r, "String");
      if (!s.empty()) {
        title = "The commune ledger";
        say(s);
        mark(t, r);
      }
    }
  }

  if (h.cultCharisma < 0) h.cultCharisma = 0;
  if (h.cultCharisma > 100) h.cultCharisma = 100;
  if (title.empty() && body.empty() && line.empty()) return false;
  out = AgeUp();
  out.table = tableRef.empty() ? "a16_cult-commune-ledger-color-enc" : tableRef;
  out.row_id = rowRef;
  out.title = title.empty() ? "The commune" : title;
  out.body = body;
  out.narrative = line;
  out.extra = extra;
  out.happiness = happiness;
  return true;
}

void Sim::makeCult(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.cultLeader = true;
  ensureCultFields(life);
  h.communards = 3 + int(cultRng_.below(5));
  h.cultLoyalty = 55;
  h.cultCharisma = 45 + int(cultRng_.below(21));
}

std::string Sim::cultToken(const std::string& key) {
  std::string base = key;
  const std::string psuffix = "-possessive";
  const bool poss = base.size() > psuffix.size() &&
                    base.compare(base.size() - psuffix.size(), psuffix.size(), psuffix) == 0;
  if (poss) base.resize(base.size() - psuffix.size());
  if (!hero_) return std::string();
  const Hero& h = *hero_;
  if (h.cultName.empty() || h.cultTotem.empty())
    ensureCultFields(*const_cast<Life*>(life_));
  if (base == "communard") {
    // One follower per card, and the follower-facing pools address them.
    if (cultCommunard_.empty()) {
      cultCommunardFemale_ = cultRng_.chance(0.5);
      cultCommunard_ = drawFirstName(cultCommunardFemale_) + " " + drawSurname();
    }
    pronounOtherSet_ = true;
    pronounOtherFemale_ = cultCommunardFemale_;
    return poss ? cultCommunard_ + "'s" : cultCommunard_;
  }
  if (base == "ex-communard") {
    if (cultExCommunard_.empty())
      cultExCommunard_ = drawFirstName(cultRng_.chance(0.5)) + " " + drawSurname();
    return poss ? cultExCommunard_ + "'s" : cultExCommunard_;
  }
  if (base == "totem") return h.cultTotem;
  if (base == "honorific") return h.cultHonorific;
  if (base == "cult" || base == "cult-name") return h.cultName;
  if (base == "commune-other") {
    for (int i = 0; i < 6; ++i) {
      const std::string other = pickSnippet("a16_cult-random-commune-names-enc", "Name", "Commonness Score",
                                            "", "", "", cultRng_);
      if (!other.empty() && other != h.cultName) return other;
    }
    return "a rival commune";
  }
  return std::string();
}

std::string Sim::cultHonorificPick() {
  const Table& t = tbl("a16_cult-honorific-name-structure-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < t.size(); ++r) {
    if (!t.flag(r, "Hero Is Leader")) continue;
    if (!cultGenderFits(t.at(r, "Gender"), hero_ ? hero_->female : false)) continue;
    pool.push_back(r);
  }
  if (pool.empty()) return std::string();
  const size_t pick = pool[cultRng_.below(uint32_t(pool.size()))];
  return resolve(trim(t.at(pick, "Honorific")));
}

void Sim::ensureCultFields(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;

  // The name table carries the philosophy in its flag columns, so one draw
  // gives the commune both its name and its beliefs.
  if (h.cultName.empty() || h.cultPhilosophy.empty()) {
    const size_t r = pickRow("a16_cult-random-commune-names-enc", "Commonness Score", "", "",
                             "Hero Founded Cult", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-random-commune-names-enc");
      h.cultName = resolve(trim(t.at(r, "Name")));
      h.cultPhilosophy = kCultPhilosophyNames[cultRng_.below(4)];
      for (int i = 0; i < 4; ++i) {
        if (t.flag(r, kCultPhilosophyFlags[i])) {
          h.cultPhilosophy = kCultPhilosophyNames[i];
          break;
        }
      }
    }
    if (h.cultName.empty()) h.cultName = "the commune";
    if (h.cultPhilosophy.empty()) h.cultPhilosophy = "new age";
  }

  if (h.cultTotem.empty()) {
    const std::string totem = pickSnippet("a16_cult-default-totem-enc", "Item Name", "Commonness Score",
                                          "Philosophy", h.cultPhilosophy, "", cultRng_);
    h.cultTotem = totem.empty() ? std::string("a smooth stone") : totem;
  }

  if (h.cultHonorific.empty()) {
    h.cultHonorific = cultHonorificPick();
    if (h.cultHonorific.empty()) h.cultHonorific = h.female ? "Mother" : "Father";
  }

  // The property the commune lives on, and the street it stands on.
  if (h.cultProperty.empty()) {
    const size_t pr = pickRow("a16_cult-commune-property-types-enc", "Commonness Score", "", "", "", cultRng_);
    if (pr != size_t(-1)) h.cultProperty = resolve(trim(tbl("a16_cult-commune-property-types-enc").at(pr, "Type")));
    if (h.cultProperty.empty()) h.cultProperty = "compound";
  }
  if (h.cultHome.empty()) {
    std::string street = pickSnippet("a16_cult-commune-street-names-enc", "Street Name", "Commonness Score",
                                     "Street Type", "Commune", "", cultRng_);
    if (street.empty())
      street = pickSnippet("a16_cult-commune-street-names-enc", "Street Name", "Commonness Score", "", "", "", cultRng_);
    if (street.empty()) street = "Communal Way";
    h.cultHome = std::to_string(100 + int(cultRng_.below(900))) + " " + street;
  }
}

}  // namespace bl
