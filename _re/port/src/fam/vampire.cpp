// src/fam/vampire.cpp -- moved out of src/sim.cpp (REPORT.en.md 131).
//
// Cut verbatim by tools/split_family.py; the only edits are the includes.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <algorithm>
#include <string>
#include <vector>

namespace bl {

bool Sim::vampireYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.vampire) return false;
  if (!ensureTable("a16_vampire-dumb-scenario-enc")) return false;
  hero_ = &h;
  life_ = &life;

  // One vampire NPC for the whole year, so the card and its log line agree on
  // the name '<vampire>' renders (REPORT.en.md 48).
  h.vampireName = vampireNpcName();
  h.vampireNpcFemale = vampireRng_.chance(0.5);
  if (h.vampireFamiliarDesc.empty() && ensureTable("a16_vampire-familiar-color-enc")) {
    const size_t fr = pickRow("a16_vampire-familiar-color-enc", "", "", "", "", vampireRng_);
    if (fr != size_t(-1)) h.vampireFamiliarDesc = resolve(trim(tbl("a16_vampire-familiar-color-enc").at(fr, "Descriptive String")));
  }

  std::string title, body, line;
  std::vector<std::string> extra;
  std::string tableRef, rowRef;
  int happy = 0;

  const auto say = [&](const std::string& s) { if (line.empty() && !s.empty()) line = spyUpperFirst(s); };
  const auto tell = [&](const std::string& s) { if (!s.empty()) extra.push_back(spyUpperFirst(s)); };
  const auto cell = [&](const Table& t, size_t r, const std::string& col) -> std::string {
    if (!t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  const auto num = [&](const Table& t, size_t r, const std::string& col) -> int {
    if (!t.has(col)) return 0;
    return int(t.num(r, col));
  };
  const auto mark = [&](const std::string& tb, size_t r) {
    tableRef = tb;
    rowRef = std::to_string(r + 1);
  };
  const auto warm = [&](const Table& t, size_t r, const std::string& skip) {
    const std::vector<std::string>& cols = t.columns();
    for (size_t c = 0; c < cols.size(); ++c) {
      if (skip.size() && cols[c] == skip) continue;
      const std::string v = t.at(r, cols[c]);
      if (v.find('<') != std::string::npos) resolve(v);
    }
  };
  const auto rowOf = [&](const std::string& tab, const std::string& weight, const std::string& fcol,
                         const std::string& fval, const std::string& flag, bool fallbackAll) -> size_t {
    size_t r = pickRow(tab, weight, fcol, fval, flag, vampireRng_);
    if (r == size_t(-1) && fallbackAll) r = pickRow(tab, weight, "", "", flag, vampireRng_);
    return r;
  };
  const auto inWindow = [&](const Table& t, size_t r, const std::string& lo, const std::string& hi, int value) {
    if (t.has(lo)) { const std::string s = trim(t.at(r, lo)); if (!s.empty() && value < std::atoi(s.c_str())) return false; }
    if (t.has(hi)) { const std::string s = trim(t.at(r, hi)); if (!s.empty() && value > std::atoi(s.c_str())) return false; }
    return true;
  };
  const auto drawPerson = [&](bool female) { return drawFirstName(female) + " " + drawSurname(); };
  const auto genderFits = [&](const Table& t, size_t r, const std::string& col, bool female) {
    if (!t.has(col)) return true;
    const std::string g = lower(trim(t.at(r, col)));
    if (g.empty() || g == "both" || g == "all" || g == "any") return true;
    if (col == "Hero Gender") return g == (h.female ? "female" : "male");
    return g == (female ? "female" : "male");
  };

  // --- 21. A council decree: a code, its result and a member's opinion.
  if (line.empty() && vampireRng_.chance(0.06) && ensureTable("a16_vampire-code-data-enc")) {
    const Table& t = tbl("a16_vampire-code-data-enc");
    const size_t r = rowOf("a16_vampire-code-data-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-code-data-enc", r);
      title = "A decree of the council";
      body = "The code: " + trim(t.at(r, "Code"));
      say(cell(t, r, "Vampire NPC String"));
      tell(cell(t, r, "Results String"));
      h.vampireStanding += num(t, r, "Approval Impact");
      h.vampireTitle = trim(t.at(r, "Code"));
    }
  }
  // --- 20. V. Coli and the other indignities of the body.
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-vcoli-string-enc")) {
    const Table& t = tbl("a16_vampire-vcoli-string-enc");
    const size_t r = rowOf("a16_vampire-vcoli-string-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-vcoli-string-enc", r);
      warm(t, r, "Log Version");
      title = "The body keeps its receipts";
      say(cell(t, r, "Log Version"));
      tell(cell(t, r, "String"));
      h.vampireEssence -= 3;
    }
  }
  // --- 19. A vampire title, offered by the council (a16_vampire-titles).
  if (line.empty() && vampireRng_.chance(0.10) && ensureTable("a16_vampire-titles-enc")) {
    const Table& t = tbl("a16_vampire-titles-enc");
    const size_t r = rowOf("a16_vampire-titles-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-titles-enc", r);
      static const char* kOpts[] = {"Option 1", "Option 2", "Option 3", "Option 4", "Option 5"};
      std::vector<std::string> opts;
      for (int i = 0; i < 5; ++i) { const std::string o = trim(t.at(r, kOpts[i])); if (!o.empty()) opts.push_back(o); }
      const std::string tag = trim(t.at(r, "Scenario Tag"));
      title = opts.empty() ? trim(t.at(r, "Og Title")) : opts[vampireRng_.below(uint32_t(opts.size()))];
      body = "The council's ruling: " + tag + ".";
      say("The council now calls me " + title + ".");
      h.vampireTitle = title;
    }
  }
  // --- 18. An insult, thrown at the hero by another vampire.
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-insult-enc")) {
    const Table& t = tbl("a16_vampire-insult-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Gender")) {
        const std::string g = lower(trim(t.at(r, "Gender")));
        if (!g.empty() && g != "both" && g != "all" && g != "any" && g != (h.female ? "female" : "male")) continue;
      }
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      mark("a16_vampire-insult-enc", r);
      std::string word = trim(t.at(r, "Insult"));
      title = "An insult";
      body = "The word: " + word + ".";
      say(h.vampireName + " called me " + word + ".");
      happy -= 2;
    }
  }
  // --- 17. The lover asks to be turned (a16_vampire-lover-convert-nag).
  if (line.empty() && !h.loverName.empty() && vampireRng_.chance(0.10) &&
      ensureTable("a16_vampire-lover-convert-nag-enc")) {
    const Table& t = tbl("a16_vampire-lover-convert-nag-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!genderFits(t, r, "Hero Gender", h.female)) continue;
      if (!genderFits(t, r, "Lover Gender", h.loverFemale)) continue;
      if (!inWindow(t, r, "Min Relationship", "Max Relationship", h.relationship)) continue;
      const std::string ma = t.has("Married") ? lower(trim(t.at(r, "Married"))) : std::string();
      if (!ma.empty() && (ma == "x") != (h.loverStatus == "married")) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      mark("a16_vampire-lover-convert-nag-enc", r);
      warm(t, r, "Log Version");
      title = "A request";
      body = cell(t, r, "Scenario");
      say(cell(t, r, "Log Version"));
      h.relationship -= 4;
    }
  }
  // --- 16. Time spent with a person who bleeds (a16_vampire-spend-time and the
  // hunger the small accidents stir up).
  if (line.empty() && vampireRng_.chance(0.12) && ensureTable("a16_vampire-spend-time-enc")) {
    const Table& t = tbl("a16_vampire-spend-time-enc");
    const size_t r = rowOf("a16_vampire-spend-time-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-spend-time-enc", r);
      const std::string spent = toFirstPerson(cell(t, r, "String"));
      title = "An evening out";
      body = "I went " + spent + ".";
      say("I went " + spent + ".");
      h.vampireEssence += 1;
      if (vampireRng_.chance(0.45) && ensureTable("a16_vampire-spend-time-hunger-color-enc")) {
        const Table& u = tbl("a16_vampire-spend-time-hunger-color-enc");
        std::vector<size_t> pool;
        for (size_t k = 0; k < u.size(); ++k) {
          if (!genderFits(u, k, "Hero Gender", h.female)) continue;
          pool.push_back(k);
        }
        if (!pool.empty()) {
          const size_t k = pool[vampireRng_.below(uint32_t(pool.size()))];
          warm(u, k, "Log String");
          tell(cell(u, k, "Log String"));
          h.vampireHunger += 8;
        }
      }
    }
  }
  // --- 15. A night out, and the stranger who wants to turn the hero.
  if (line.empty() && vampireRng_.chance(0.10) && ensureTable("a16_vampire-clubbing-approache-enc")) {
    const Table& t = tbl("a16_vampire-clubbing-approache-enc");
    const size_t r = rowOf("a16_vampire-clubbing-approache-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-clubbing-approache-enc", r);
      warm(t, r, "Log Version");
      title = "A night out";
      body = cell(t, r, "String");
      say(cell(t, r, "Log Version"));
    }
  }
  // --- 14. The witch doctor's remedy, kept for the days the blood bites back.
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-witch-doctor-remedie-enc")) {
    const Table& t = tbl("a16_vampire-witch-doctor-remedie-enc");
    const size_t r = rowOf("a16_vampire-witch-doctor-remedie-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-witch-doctor-remedie-enc", r);
      const std::string item = trim(t.at(r, "Item"));
      const int eff = num(t, r, "Effectiveness Score");
      const int risk = num(t, r, "Risk Score");
      title = "The witch doctor";
      body = "The remedy: " + item + ".";
      if (eff * 20 > int(vampireRng_.below(100))) {
        say("I treated myself with " + item + " and it worked.");
        if (!h.vampireDisease.empty()) { h.vampireDisease.clear(); }
        h.vampireEssence += 4;
      } else {
        say("I tried " + item + " and it did nothing.");
        h.vampireEssence -= risk;
      }
    }
  }
  // --- 13. Disposing of what is left (a16_vampire-body-dump-color).
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-body-dump-color-enc")) {
    const Table& t = tbl("a16_vampire-body-dump-color-enc");
    const size_t r = rowOf("a16_vampire-body-dump-color-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-body-dump-color-enc", r);
      h.vampireVictim = drawPerson(vampVictimFemale_ = vampireRng_.chance(0.5));
      title = "Cleaning up";
      body = "Left at: " + cell(t, r, "Location") + ".";
      say(toFirstPerson(cell(t, r, "String")));
      h.vampireNotoriety += 2;
    }
  }
  // --- 12. The coffin (a16_vampire-coffin-string).
  if (line.empty() && vampireRng_.chance(0.10) && ensureTable("a16_vampire-coffin-string-enc")) {
    const Table& t = tbl("a16_vampire-coffin-string-enc");
    const size_t r = rowOf("a16_vampire-coffin-string-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-coffin-string-enc", r);
      warm(t, r, "Log Version");
      title = "Daylight hours";
      body = toFirstPerson(cell(t, r, "Scenario"));
      say(cell(t, r, "Log Version"));
      happy += 1;
    }
  }
  // --- 11. Blood offered freely, or refused (offering string and pact
  // rejection).
  if (line.empty() && vampireRng_.chance(0.10) && ensureTable("a16_vampire-blood-offering-string-enc") &&
      vampireRng_.chance(0.65)) {
    const Table& t = tbl("a16_vampire-blood-offering-string-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!genderFits(t, r, "Hero Gender", h.female)) continue;
      if (t.has("Offering Gender")) {
        const std::string g = lower(trim(t.at(r, "Offering Gender")));
        if (!g.empty() && g != "both" && g != "all" && g != "any") continue;   // the cell names the offerer
      }
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      mark("a16_vampire-blood-offering-string-enc", r);
      h.vampireVictim = drawPerson(vampVictimFemale_ = vampireRng_.chance(0.5));
      title = "An offering";
      body = "From: " + h.vampireVictim + ".";
      say(toFirstPerson(cell(t, r, "String")));
      h.vampireEssence += 6;
      h.vampireHunger -= 10;
    }
  }
  if (line.empty() && vampireRng_.chance(0.10) && ensureTable("a16_vampire-blood-pact-rejection-string-enc")) {
    const Table& t = tbl("a16_vampire-blood-pact-rejection-string-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!inWindow(t, r, "Min Relationship", "Max Relationship", h.relationship)) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      mark("a16_vampire-blood-pact-rejection-string-enc", r);
      const std::string who = t.has("Person Type") ? trim(t.at(r, "Person Type")) : std::string();
      title = "A refusal";
      body = who.empty() ? "Someone said no." : ("The " + who + " said no.");
      say(toFirstPerson(cell(t, r, "String")));
      h.vampireEssence -= 2;
    }
  }
  // --- 10. A blood bath with the council.
  if (line.empty() && vampireRng_.chance(0.06) && ensureTable("a16_vampire-blood-bath-scenario-enc")) {
    const Table& t = tbl("a16_vampire-blood-bath-scenario-enc");
    const size_t r = rowOf("a16_vampire-blood-bath-scenario-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-blood-bath-scenario-enc", r);
      warm(t, r, "Log Version");
      title = "A blood bath";
      body = "The activity: " + trim(t.at(r, "Activity")) + ".";
      say(cell(t, r, "Log Version"));
      h.vampireEssence += num(t, r, "Max Essence Impact");
      happy += num(t, r, "Max Happiness Impact");
      h.relationship += num(t, r, "Max Relationship Impact");
    }
  }
  // --- 9. Council business: a member is disciplined, and the hero may be the
  // one holding the whip.
  if (line.empty() && vampireRng_.chance(0.06) && ensureTable("a16_vampire-council-discipline-scenario-enc")) {
    const Table& t = tbl("a16_vampire-council-discipline-scenario-enc");
    const size_t r = rowOf("a16_vampire-council-discipline-scenario-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-council-discipline-scenario-enc", r);
      warm(t, r, "Log Version");
      title = "The council sits";
      body = cell(t, r, "String");
      const bool kill = t.has("Kill") && t.flag(r, "Kill");
      say(cell(t, r, "Log Version") + (kill ? " The council put them down for it." : ""));
      h.vampireStanding += kill ? 3 : 1;
      if (vampireRng_.chance(0.5) && ensureTable("a16_vampire-council-punishment-enc")) {
        const size_t p = rowOf("a16_vampire-council-punishment-enc", "Commonness Score", "", "", "", false);
        if (p != size_t(-1)) {
          warm(tbl("a16_vampire-council-punishment-enc"), p, "Log Version");
          tell(cell(tbl("a16_vampire-council-punishment-enc"), p, "Log Version"));
        }
      }
    }
  }
  if (line.empty() && vampireRng_.chance(0.05) && ensureTable("a16_vampire-council-reward-enc")) {
    const Table& t = tbl("a16_vampire-council-reward-enc");
    const size_t r = rowOf("a16_vampire-council-reward-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-council-reward-enc", r);
      warm(t, r, "Log Version");
      title = "A reward";
      body = cell(t, r, "Scenario");
      say(cell(t, r, "Log Version"));
      h.vampireStanding += 2;
    }
  }
  // --- 8. A council gathering, and whatever the guests overdosed on.
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-council-party-string-enc")) {
    const Table& t = tbl("a16_vampire-council-party-string-enc");
    const size_t r = rowOf("a16_vampire-council-party-string-enc", "Commonness Score", "", "", "", false);
    const size_t pt = rowOf("a16_vampire-council-party-type-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1) && pt != size_t(-1) && ensureTable("a16_vampire-council-party-type-enc")) {
      mark("a16_vampire-council-party-string-enc", r);
      warm(t, r, "Log Version");
      title = "A gathering";
      body = "The party: " + trim(tbl("a16_vampire-council-party-type-enc").at(pt, "Party Type")) + ".";
      say(cell(t, r, "Log Version"));
      h.vampireStanding += num(t, r, "Max Standing Impact");
      happy += num(t, r, "Max Looks Impact") + num(t, r, "Max Smarts Impact");
    }
  }
  // --- 7. Enticing and assaulting: the two ways a hunt ends.
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-entice-method-enc")) {
    const Table& t = tbl("a16_vampire-entice-method-enc");
    const size_t r = rowOf("a16_vampire-entice-method-enc", "", "", "", "", false);
    if (r != size_t(-1) && !h.vampireVictim.empty()) {
      mark("a16_vampire-entice-method-enc", r);
      warm(t, r, "Success Log Version");
      const bool ok = num(t, r, "Effectiveness Score") * 18 > int(vampireRng_.below(100));
      title = ok ? "The lure worked" : "The lure failed";
      pronounOtherSet_ = true;
      pronounOtherFemale_ = vampVictimFemale_;
      body = "Method: " + cell(t, r, "Method") + ".";
      pronounOtherSet_ = false;
      say(cell(t, r, ok ? "Success Log Version" : "Failure Log Version"));
      h.vampireEssence += ok ? 5 : -2;
      if (!ok) h.vampireNotoriety += 1;
    }
  }
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-assault-method-enc")) {
    const Table& t = tbl("a16_vampire-assault-method-enc");
    const size_t r = rowOf("a16_vampire-assault-method-enc", "", "", "", "", false);
    if (r != size_t(-1) && !h.vampireVictim.empty()) {
      mark("a16_vampire-assault-method-enc", r);
      warm(t, r, "Success Log Version");
      const bool ok = num(t, r, "Effectiveness Score") * 18 > int(vampireRng_.below(100));
      title = ok ? "Taken by force" : "It went badly";
      pronounOtherSet_ = true;
      pronounOtherFemale_ = vampVictimFemale_;
      body = "Method: " + cell(t, r, "Method") + ".";
      pronounOtherSet_ = false;
      say(cell(t, r, ok ? "Success Log Version" : "Failure Log Version"));
      h.vampireEssence += ok ? 6 : -3;
      h.vampireNotoriety += ok ? 1 : 3;
    }
  }
  // --- 6. Hunted in turn: a hunter finds the hero.
  if (line.empty() && vampireRng_.chance(0.12) && ensureTable("a16_vampire-hunter-encounter-scenario-enc")) {
    const Table& t = tbl("a16_vampire-hunter-encounter-scenario-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!genderFits(t, r, "Hero Gender", h.female)) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      mark("a16_vampire-hunter-encounter-scenario-enc", r);
      const bool female = vampireRng_.chance(0.5);
      const std::string hunter = drawFirstName(female) + " " + drawSurname();
      pronounOtherSet_ = true;
      pronounOtherFemale_ = female;
      title = "A hunter";
      body = "They gave a name: " + hunter + ".";
      say(cell(t, r, "Scenario"));
      pronounOtherSet_ = false;
      h.vampireNotoriety += 2;
      if (ensureTable("a16_vampire-hunter-appreciation-string-enc")) {
        const Table& a = tbl("a16_vampire-hunter-appreciation-string-enc");
        std::vector<size_t> apool;
        for (size_t k = 0; k < a.size(); ++k) {
          if (!genderFits(a, k, "Hero Gender", h.female)) continue;
          if (a.has("Vampire Hero") && a.flag(k, "Vampire Hero") && !h.vampire) continue;
          apool.push_back(k);
        }
        if (!apool.empty()) {
          const size_t k = apool[vampireRng_.below(uint32_t(apool.size()))];
          tell(cell(a, k, "String"));
        }
      }
    }
  }
  // --- 5. Hypnosis: an animal, a wallet, a lover or a breakup.
  if (line.empty() && vampireRng_.chance(0.12) && !h.vampireVictim.empty()) {
    const Table* ht = nullptr;
    const char* htable = nullptr;
    const int roll = int(vampireRng_.below(4));
    if (roll == 0 && ensureTable("a16_vampire-hypnosis-animal-scenario-enc")) { ht = &tbl("a16_vampire-hypnosis-animal-scenario-enc"); htable = "a16_vampire-hypnosis-animal-scenario-enc"; }
    else if (roll == 1 && ensureTable("a16_vampire-hypnosis-money-string-enc")) { ht = &tbl("a16_vampire-hypnosis-money-string-enc"); htable = "a16_vampire-hypnosis-money-string-enc"; }
    else if (roll == 2 && ensureTable("a16_vampire-hypnosis-love-string-enc")) { ht = &tbl("a16_vampire-hypnosis-love-string-enc"); htable = "a16_vampire-hypnosis-love-string-enc"; }
    else if (ensureTable("a16_vampire-hypnosis-breakup-string-enc")) { ht = &tbl("a16_vampire-hypnosis-breakup-string-enc"); htable = "a16_vampire-hypnosis-breakup-string-enc"; }
    if (ht != nullptr) {
      const Table& t = *ht;
      const size_t r = rowOf(htable, "Commonness Score", "", "", "", false);
      if (r != size_t(-1)) {
        mark(htable, r);
        warm(t, r, "Log Version");
        title = "A compulsion";
        body = "The victim: " + h.vampireVictim + ".";
        say(cell(t, r, "Log Version"));
        if (std::string(htable).find("money") != std::string::npos) {
          const int lo = num(t, r, "Min Money"), hi = num(t, r, "Max Money");
          const long long got = (lo + int(vampireRng_.below(uint32_t(hi > lo ? hi - lo + 1 : 1)))) * 100;
          h.money += got;
          tell("That was worth " + money(got) + " to me.");
        }
        h.vampireEssence -= 1;
      }
    }
  }
  // --- 4. The familiar: ordered about, confused, threatened or dumped.
  if (line.empty() && vampireRng_.chance(0.15)) {
    const int roll = int(vampireRng_.below(4));
    const char* ftable = nullptr;
    if (roll == 0 && ensureTable("a16_vampire-familiar-command-enc")) ftable = "a16_vampire-familiar-command-enc";
    else if (roll == 1 && ensureTable("a16_vampire-familiar-confused-string-enc")) ftable = "a16_vampire-familiar-confused-string-enc";
    else if (roll == 2 && ensureTable("a16_vampire-familiar-threat-enc")) ftable = "a16_vampire-familiar-threat-enc";
    else if (ensureTable("a16_vampire-familiar-dump-enc")) ftable = "a16_vampire-familiar-dump-enc";
    if (ftable != nullptr) {
      const Table& t = tbl(ftable);
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        if (!genderFits(t, r, "Familiar Gender", h.vampireFamiliarFemale)) continue;
        if (!inWindow(t, r, "Min Familiar Smarts", "Max Familiar Smarts", h.vampireFamiliarSmarts)) continue;
        pool.push_back(r);
      }
      if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
      if (!pool.empty()) {
        const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
        mark(ftable, r);
        warm(t, r, "Log Version");
        title = "My familiar";
        body = h.vampireFamiliarDesc.empty() ? ("My familiar: " + h.familiar + ".") : ("My familiar " + h.vampireFamiliarDesc + ".");
        const std::string lg = t.has("Log Version") ? cell(t, r, "Log Version") : std::string();
        say(lg.empty() ? cell(t, r, "String") : lg);
        const int rel = t.has("Relationship Impact") ? num(t, r, "Relationship Impact") : 0;
        h.vampireFamiliarRel += rel;
        if (std::string(ftable).find("dump") != std::string::npos) {
          h.vampireNotoriety += num(t, r, "Risk Score");
          h.familiar.clear();
        }
      }
    }
  }
  // --- 3. A feed, the taste of it, and whatever the blood carried.
  if (line.empty() && vampireRng_.chance(0.20) && ensureTable("a16_vampire-feeding-location-color-enc")) {
    const Table& t = tbl("a16_vampire-feeding-location-color-enc");
    const size_t r = rowOf("a16_vampire-feeding-location-color-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-feeding-location-color-enc", r);
      warm(t, r, "Log Version");
      // The risk of feeding where the hero fed (a16_vampire-feeding-location).
      int risk = 1;
      if (ensureTable("a16_vampire-feeding-location-enc")) {
        const Table& l = tbl("a16_vampire-feeding-location-enc");
        const std::string want = lower(trim(t.at(r, "Location")));
        for (size_t k = 0; k < l.size(); ++k) {
          if (lower(trim(l.at(k, "Location"))) == want) { risk = int(l.num(k, "Risk Score")); break; }
        }
      }
      h.vampireVictim = drawPerson(vampVictimFemale_ = vampireRng_.chance(0.5));
      title = "A feed";
      body = "The victim: " + h.vampireVictim + " (risk " + std::to_string(risk) + ").";
      say(cell(t, r, "Log Version"));
      h.vampireEssence += 8;
      h.vampireHunger = 0;
      // The taste, and whether it carried something (REPORT.en.md 70).
      if (ensureTable("a16_vampire-blood-taste-color-enc")) {
        const Table& b = tbl("a16_vampire-blood-taste-color-enc");
        const size_t tr = b.size() ? vampireRng_.below(uint32_t(b.size())) : size_t(-1);
        if (tr != size_t(-1)) {
          tell("The blood tasted " + resolve(trim(b.at(tr, "Taste"))) + ".");
          if (b.has("Disease Indicator") && b.flag(tr, "Disease Indicator") && ensureTable("a16_vampire-disease-enc")) {
            const Table& d = tbl("a16_vampire-disease-enc");
            if (d.size()) {
              h.vampireDisease = trim(d.at(vampireRng_.below(uint32_t(d.size())), "Disease"));
              tell("I caught " + h.vampireDisease + " from it.");
              h.vampireEssence -= 10;
            }
          }
        }
      }
      // How healthy the victim was, and whether the hero should stop.
      if (ensureTable("a16_vampire-victim-status-color-enc")) {
        const Table& v = tbl("a16_vampire-victim-status-color-enc");
        const int health = 30 + int(vampireRng_.below(71));
        std::vector<size_t> pool;
        for (size_t k = 0; k < v.size(); ++k) if (inWindow(v, k, "Min Victim Health", "Max Victim Health", health)) pool.push_back(k);
        if (!pool.empty()) {
          pronounOtherSet_ = true;
          pronounOtherFemale_ = vampVictimFemale_;
          tell(cell(v, pool[vampireRng_.below(uint32_t(pool.size()))], "String"));
          pronounOtherSet_ = false;
        }
      }
      if (risk >= 3) h.vampireNotoriety += 1;
    }
  }
  // --- 2. A hunt: where it happened, what the hero preyed on and who joined.
  if (line.empty() && vampireRng_.chance(0.25) && ensureTable("a16_vampire-solo-hunt-location-enc")) {
    const Table& t = tbl("a16_vampire-solo-hunt-location-enc");
    const size_t r = rowOf("a16_vampire-solo-hunt-location-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1) && ensureTable("a16_vampire-hunt-string-enc")) {
      const Table& u = tbl("a16_vampire-hunt-string-enc");
      const size_t hrow = rowOf("a16_vampire-hunt-string-enc", "", "", "", "", false);
      if (hrow != size_t(-1)) {
        mark("a16_vampire-hunt-string-enc", hrow);
        warm(u, hrow, "Log Version");
        title = "The hunt";
        body = "The night: " + trim(t.at(r, "String Version")) + ".";
        say(cell(u, hrow, "Log Version"));
        h.vampireEssence += num(u, hrow, "Essence Impact");
        h.relationship += num(u, hrow, "Relationship Impact");
        happy += num(u, hrow, "Enjoyment Score");
        if (ensureTable("a16_vampire-hunt-encounter-color-enc")) {
          const Table& e = tbl("a16_vampire-hunt-encounter-color-enc");
          std::vector<size_t> pool;
          for (size_t k = 0; k < e.size(); ++k) {
            if (!inWindow(e, k, "Min Vampire Notoriety", "Max Vampire Notoriety", h.vampireNotoriety)) continue;
            pool.push_back(k);
          }
          if (!pool.empty()) tell(cell(e, pool[vampireRng_.below(uint32_t(pool.size()))], "String"));
        }
      }
    }
  }
  // --- 1. The pool the hero already had: a16_vampire-dumb-scenario-enc.
  if (line.empty()) {
    const Table& t = tbl("a16_vampire-dumb-scenario-enc");
    // scenarioEligible covers the table's own age / gender / lover gates; its
    // Notoriety, Essence and Hero Has Familiar gates belong to the vampire
    // attribute system the port models with the hero's own numbers, so the
    // window is applied here (REPORT.en.md 48, 70).
    std::vector<size_t> rows;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!scenarioEligible(t, r, life)) continue;
      if (!inWindow(t, r, "Min Notoriety", "Max Notoriety", h.vampireNotoriety)) continue;
      if (t.has("Hero Has Familiar") && t.flag(r, "Hero Has Familiar") && h.familiar.empty()) continue;
      rows.push_back(r);
    }
    const size_t r = pickFrom(rows, vampireRng_);
    if (r != size_t(-1)) {
      const std::vector<std::string> incoming = out.extra;
      if (!renderRow(t, r, life, out, forcedChoice)) return false;
      const std::vector<std::string> rendered = out.extra;
      out.extra = incoming;
      for (size_t k = 0; k < rendered.size(); ++k) out.extra.push_back(rendered[k]);
      for (size_t k = 0; k < extra.size(); ++k) out.extra.push_back(extra[k]);
      return true;
    }
    return false;
  }

  out.table = tableRef.empty() ? "a16_vampire-dumb-scenario-enc" : tableRef;
  out.row_id = rowRef;
  out.title = title;
  out.body = body;
  out.narrative = line;
  const std::vector<std::string> keep = out.extra;
  out.extra.clear();
  for (size_t k = 0; k < keep.size(); ++k) out.extra.push_back(keep[k]);
  for (size_t k = 0; k < extra.size(); ++k) out.extra.push_back(extra[k]);
  out.happiness += happy;
  return true;
}

void Sim::makeVampire(Life& life) {
  Hero& h = life.hero;
  if (!ensureTable("a16_vampire-name-enc")) return;
  hero_ = &h;
  life_ = &life;
  h.vampire = true;
  h.vampireEssence = 60;
  h.vampireNotoriety = 20;
  h.vampireStanding = 50;
  h.vampireHunger = 40;
  h.vampireFamiliarRel = 60;
  // The familiar is a character the player meets in the vampire screens; there
  // is no familiar-name table, so the name comes from the port's own person
  // pool and the description and the numbers from a16_vampire-familiar-color-enc
  // (REPORT.en.md 70).
  if (h.familiar.empty()) {
    h.vampireFamiliarFemale = vampireRng_.chance(0.5);
    h.familiar = drawFirstName(h.vampireFamiliarFemale);
  }
  if (ensureTable("a16_vampire-familiar-color-enc")) {
    const Table& t = tbl("a16_vampire-familiar-color-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Familiar Gender")) {
        const std::string g = lower(trim(t.at(r, "Familiar Gender")));
        if (!g.empty() && g != "both" && g != "all" && g != "any" &&
            g != (h.vampireFamiliarFemale ? "female" : "male")) continue;
      }
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      h.vampireFamiliarDesc = resolve(trim(t.at(r, "Descriptive String")));
      const auto roll = [&](const char* lo, const char* hi, int fallback) -> int {
        const std::string los = t.has(lo) ? trim(t.at(r, lo)) : std::string();
        const std::string his = t.has(hi) ? trim(t.at(r, hi)) : std::string();
        const int l = los.empty() ? fallback : std::atoi(los.c_str());
        const int hh = his.empty() ? fallback : std::atoi(his.c_str());
        if (hh <= l) return l;
        return l + int(vampireRng_.below(uint32_t(hh - l + 1)));
      };
      h.vampireFamiliarLooks = roll("Min Familiar Looks", "Max Familiar Looks", 50);
      h.vampireFamiliarSmarts = roll("Min Familiar Smarts", "Max Familiar Smarts", 50);
    }
  }
  // The address off a16_vampire-street-name-enc (the street cells are templates
  // of their own, so they are resolved before the house number goes on).
  if (ensureTable("a16_vampire-street-name-enc")) {
    const size_t r = pickRow("a16_vampire-street-name-enc", "Commonness Score", "", "", "", vampireRng_);
    if (r != size_t(-1)) {
      const Table& st = tbl("a16_vampire-street-name-enc");
      std::string street = resolve(trim(st.at(r, "Street Name")));
      if (st.flag(r, "Add Random Street Type")) street += " Street";
      h.vampireHome = std::to_string(100 + int(vampireRng_.below(900))) + " " + street;
    }
  }
  h.vampireMaker = vampireNpcName();
}

std::string Sim::vampireNpcName() {
  if (!ensureTable("a16_vampire-name-enc")) return std::string();
  const Table& t = tbl("a16_vampire-name-enc");
  const std::string want = hero_ != nullptr && hero_->female ? "female" : "male";
  std::vector<size_t> firsts, surnames;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string type = lower(trim(t.at(r, "Type")));
    if (type == "surname") surnames.push_back(r);
    else if (type == want) firsts.push_back(r);
  }
  if (firsts.empty() || surnames.empty()) return std::string();
  const size_t f = firsts[vampireRng_.below(uint32_t(firsts.size()))];
  const size_t s = surnames[vampireRng_.below(uint32_t(surnames.size()))];
  return trim(t.at(f, "Name")) + " " + trim(t.at(s, "Name"));
}

// ---------------------------------------------------------------------------
// REPORT.en.md 333. The vampire hunt's animal (ScenarioVampireHuntAnimal).
//
// Native: the solo hunt's location card (ScenarioVampireHuntLocation) hands its
// sentence to VampireHuntActivity::SelectScenarioByLocation (0x2969b8c), which sorts
// the location's three appearance likelihoods (animal / human / vampire) in descending
// order and walks them with GetRandomNumber(0,100) < 50 (VampireBalancing
// HuntLikelihoodPercent), falling to the last one. An animal builds this card over a
// DataHub::GetRandomAnimalForContinent row (Europe when the hero's continent has none).
// The port has no hunt activity, so the hunt arrives as its own annual family for a
// vampire (shortcut) and the location is drawn uniformly instead of being picked by the
// player (shortcut). Human and vampire appearances are other classes and end the turn.
std::string allMajorWords(const std::string& text);

namespace {
// DataHub::GetTitleForVampire: one of the tag row's six cells, uniformly.
std::string vampireTitleFor(const Table& t, const std::string& tag, Rng& rng) {
  static const char* kCols[] = {"Og Title", "Option 1", "Option 2", "Option 3", "Option 4", "Option 5"};
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Scenario Tag")) != tag) continue;
    std::vector<std::string> cells;
    for (const char* c : kCols) {
      const std::string v = trim(t.at(r, c));
      if (!v.empty()) cells.push_back(v);
    }
    if (cells.empty()) return std::string();
    return cells[rng.below(uint32_t(cells.size()))];
  }
  return std::string();
}
}  // namespace

bool Sim::vampireHuntAnimalYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.vampire || h.age < 18 || h.inPrison) return false;
  if (forcedChoice < 0 && !huntAnimalRng_.chance(0.15)) return false;
  if (!ensureTable("a16_vampire-solo-hunt-location-enc") || !ensureTable("a16_animals-enc") ||
      !ensureTable("a16_vampire-titles-enc"))
    return false;
  hero_ = &h;
  life_ = &life;
  Rng& g = huntAnimalRng_;
  const Table& lt = tbl("a16_vampire-solo-hunt-location-enc");
  std::vector<size_t> locs;
  for (size_t r = 0; r < lt.size(); ++r)
    if (!trim(lt.at(r, "String Version")).empty()) locs.push_back(r);
  if (locs.empty()) return false;
  const size_t lr = locs[g.below(uint32_t(locs.size()))];
  const std::string inLoc = trim(lt.at(lr, "String Version"));
  struct App { int like; int kind; };
  App apps[3] = {{int(lt.num(lr, "Animal Appearance Likelihood")), 1},
                 {int(lt.num(lr, "Human Appearance Likelihood")), 2},
                 {int(lt.num(lr, "Vampire Appearance Likelihood")), 3}};
  // ponytail: native Array.Sort is unstable, so tied likelihoods have no fixed order;
  // the port keeps table order for ties.
  std::stable_sort(apps, apps + 3, [](const App& a, const App& b) { return a.like > b.like; });
  int kind = apps[2].kind;
  for (const App& a : apps) {
    if (int(g.below(100)) < 50) {
      kind = a.kind;
      break;
    }
  }
  if (kind != 1) return false;

  const Table& t = tbl("a16_animals-enc");
  auto poolFor = [&](const std::string& want) {
    std::vector<size_t> pool;
    const std::string cont = lower(trim(want));
    for (size_t r = 0; r < t.size(); ++r) {
      const std::string cell = lower(trim(t.at(r, "Continent")));
      // Same reading as the walk's card (REPORT 326): 'all 7 continents' matches all.
      if (cell.find("all 7 continents") != std::string::npos) {
        pool.push_back(r);
        continue;
      }
      size_t pos = 0;
      while (true) {
        const size_t comma = cell.find(',', pos);
        const size_t len = comma == std::string::npos ? std::string::npos : comma - pos;
        if (trim(cell.substr(pos, len)) == cont) {
          pool.push_back(r);
          break;
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
      }
    }
    return pool;
  };
  std::vector<size_t> pool = poolFor(h.continent);
  if (pool.empty()) pool = poolFor("Europe");  // #19103 fallback
  if (pool.empty()) return false;
  const size_t row = pool[g.below(uint32_t(pool.size()))];
  const std::string animal = trim(t.at(row, "Animal"));
  const int danger = int(t.num(row, "Danger Score"));
  const int aggression = int(t.num(row, "Aggression Score"));
  const int skittish = int(t.num(row, "Skittishness Score"));
  const int gain = int(t.num(row, "Vampire Essence Gain"));
  const std::string verb = trim(t.at(row, "Attack String"));
  const std::string ind = articleFor(animal) + animal;  // <animal-indefinite>
  const Table& titles = tbl("a16_vampire-titles-enc");

  const std::vector<std::string> extra = out.extra;
  out = AgeUp();
  out.extra = extra;
  out.table = "a16_animals-enc";
  out.row_id = std::to_string(row + 1);
  out.title = allMajorWords(animal);  // #8533 <animal-title>
  out.body = "A juicy-looking " + animal + " you discovered while hunting " + inLoc +
             " is tempting you.\n\nWhat will you do?";  // #10080
  out.cta = "What will you do?";
  out.options = {"Feed on it", "Let it go", "Attempt to rescue it"};  // #20004 #28743 #12363
  const int lane = forcedChoice >= 0 ? (forcedChoice > 2 ? 2 : forcedChoice) : int(g.below(3));
  out.choice = out.options[size_t(lane)];
  out.narrative = "I encountered " + ind + ".";  // #24092, logged by RefreshScenario
  // The follow-up cards the native choice pushes are appended to this card's body as
  // a title line and a body (shortcut: the port shows one card per year).
  auto follow = [&](const std::string& title, const std::string& body) {
    out.body += "\n\n" + title + "\n" + body;
  };
  // IsAnimalAttacking / IsAnimalFleeing: GetRandomNumber(0,100) below
  // 100 * clamp(score / 5, 0, 1) (both multipliers 1.0).
  auto rolls = [&](int score) {
    const double c = score <= 0 ? 0.0 : (score >= 5 ? 100.0 : 20.0 * score);
    return c > double(g.below(100));
  };
  // <body-part>: the parser's SelectedBodyPart is never set in this build, so the
  // native token source is unrecovered; the port draws a16_body-parts-enc (shortcut).
  auto bodyPart = [&]() {
    if (!ensureTable("a16_body-parts-enc")) return std::string("arm");
    const Table& b = tbl("a16_body-parts-enc");
    std::vector<size_t> ok;
    for (size_t r = 0; r < b.size(); ++r) {
      const std::string gd = lower(trim(b.at(r, "Gender")));
      if (gd == "both" || gd == (h.female ? "female" : "male")) ok.push_back(r);
    }
    if (ok.empty()) return std::string("arm");
    return trim(b.at(ok[g.below(uint32_t(ok.size()))], "Body Part"));
  };

  if (lane == 1) return true;  // let it go: no narrative of its own, no deltas
  if (lane == 0) {
    if (!rolls(aggression)) {
      follow(vampireTitleFor(titles, "BLOOD_IS_BLOOD_1", g),
             "You drained " + ind + " " + inLoc + " and discarded its carcass.");  // #48404
      h.vampireEssence += gain;  // SimVampire::AdjustEssence(Vampire Essence Gain)
      h.vampireNotoriety -= 1;   // ImpactNotoriety(-1)
      out.happiness = pointsForImpact(1);
      out.narrative += "\nI fed on it for some essence.";  // #24154
      return true;
    }
    // Attacking: a second attack roll, then a flee roll, decide fled vs hit.
    if (!rolls(aggression) || rolls(skittish)) {
      follow(vampireTitleFor(titles, "LATER", g), "It fled the scene after you attacked.");  // #27507
      out.narrative += "\nIt fled after I failed to attack it.";  // #27506
      return true;
    }
    // AnimalAttackHero(max 2): 1 + clamp(Danger/5) truncated, so 2 only at Danger 5.
    const int damage = danger >= 5 ? 2 : 1;
    const std::string part = bodyPart();
    follow(vampireTitleFor(titles, "BLOOD_IS_BLOOD_2", g),
           "You tried to attack the " + animal + ", but missed!");  // #49619
    follow("Ouch", "The " + animal + " charged you!\n\nIt " + verb + " your " + part + ".");  // #33037 #41507
    out.health = pointsForImpact(-damage);
    out.happiness = pointsForImpact(-2);
    h.vampireEssence -= damage;  // SimVampire::ImpactEssence(-damage)
    out.narrative += "\nI attacked and missed.\nIt " + verb + " my " + part + ".";  // #23577
    return true;
  }
  // Attempt to rescue it.
  if (int(g.below(20)) + 1 < 5 - skittish) {
    static const char* kWin[] = {"Big heart", "Jane Goodall mode", "Star citizen",
                                 "It lives to see another day"};  // #13463 #27886 #39831 #27549
    follow(kWin[g.below(4)], "You successfully rescued the " + animal + "!");  // #49430
    out.happiness = pointsForImpact(3);
    out.karma += 3;  // SimPerson::ImpactKarma(hero, 3)
    return true;
  }
  if (int(g.below(8)) + 1 >= aggression) {
    static const char* kMiss[] = {"Big heart", "Animal magnetism", "Brave but naive",
                                  "Hands to yourself"};  // #13463 #11440 #13976 #22480
    const std::string title = danger > 3 ? std::string("Are you freaking crazy") : kMiss[g.below(4)];  // #11716
    follow(title, "Your attempt to rescue the " + animal + " was unsuccessful.");  // #50215
    out.narrative = "I encountered " + ind + " and tried unsuccessfully to rescue it.";  // #24091
    return true;
  }
  out.narrative = "I encountered " + ind + " and tried to rescue it. The " + animal + " " + verb +
                  " me!";  // #24090 + #966
  out.health = pointsForImpact(-(danger + 1));
  if (h.health + out.health < 1) {
    h.animalKill = animal;  // SimDeath::UpdateWithKillerAnimal
    h.animalKillVerb = verb;
    return true;
  }
  follow("Help!", "The " + animal + " " + verb + " you!");  // #22821 #41506
  return true;
}

}  // namespace bl
