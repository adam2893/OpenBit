// src/fam/vampire.cpp -- moved out of src/sim.cpp (REPORT.en.md 131).
//
// Cut verbatim by tools/split_family.py; the only edits are the includes.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

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

}  // namespace bl
