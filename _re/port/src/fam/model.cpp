// src/fam/model.cpp -- moved out of src/sim.cpp (REPORT.en.md 131).
//
// Cut verbatim by tools/split_family.py; the only edits are the includes.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>
#include <vector>

namespace bl {

bool Sim::modelYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  const std::string hg = heroToken("gender", false);
  if (!h.model || h.age < 16) return false;
  if (!ensureTable("a16_model-career-progression-enc")) return false;
  if (!modelRng_.chance(0.55)) return false;
  modelLive_ = true;
  modelCache_.clear();
  std::string line, title, body, cardTable;
  int cardRow = -1;
  long long take = 0;
  int happy = 0;
  auto say = [&](const std::string& v) {
    if (!v.empty() && line.empty()) {
      line = spyUpperFirst(v);
      const char last = line[line.size() - 1];
      if (last != '.' && last != '!' && last != '?' && last != '"' && last != ')') line += '.';
    }
  };
  auto add = [&](const std::string& v) {
    if (!v.empty()) out.extra.push_back(spyUpperFirst(v));
  };
  auto cell = [&](const Table& t, size_t r, const std::string& col) {
    return trim(resolve(t.at(r, col)));
  };
  auto numOf = [](const Table& t, size_t r, const std::string& col) -> long long {
    if (!t.has(col) || trim(t.at(r, col)).empty()) return 0;
    return static_cast<long long>(t.num(r, col));
  };
  auto hasCell = [](const Table& t, size_t r, const std::string& col) -> bool {
    return t.has(col) && !trim(t.at(r, col)).empty();
  };
  auto flagOf = [](const Table& t, size_t r, const std::string& col) -> bool {
    return t.has(col) && t.flag(r, col);
  };
  auto mark = [&](const std::string& tab, int r) {
    if (r >= 0) { cardTable = tab; cardRow = r; }
  };
  auto scene = [&](const Table& t, size_t r, bool otherFemale, const std::string& col) {
    const bool keepSet = pronounOtherSet_;
    const bool keepFemale = pronounOtherFemale_;
    pronounOtherSet_ = true;
    pronounOtherFemale_ = otherFemale;
    const std::string v = cell(t, r, col);
    pronounOtherSet_ = keepSet;
    pronounOtherFemale_ = keepFemale;
    return v;
  };
  auto withBrackets = [&](const Table& t, size_t r, const std::string& wantCol, bool otherFemale) {
    const bool keepSet = pronounOtherSet_;
    const bool keepFemale = pronounOtherFemale_;
    pronounOtherSet_ = true;
    pronounOtherFemale_ = otherFemale;
    for (const std::string& col : t.columns()) {
      if (col == wantCol) continue;
      resolve(t.at(r, col));
    }
    const std::string v = resolve(t.at(r, wantCol));
    pronounOtherSet_ = keepSet;
    pronounOtherFemale_ = keepFemale;
    return v;
  };
  auto drawRow = [&](const std::string& tab, const std::string& weight) -> size_t {
    if (!ensureTable(tab)) return size_t(-1);
    const Table& t = tbl(tab.c_str());
    if (!t.size()) return size_t(-1);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    return (weight.empty() || !t.has(weight)) ? pool[modelRng_.below(uint32_t(pool.size()))]
                                              : weightedPickOf(t, weight, pool, modelRng_);
  };
  auto choices = [&](const Table& t, size_t r) {
    std::vector<int> v;
    for (int n = 1; n <= 20; ++n) {
      const std::string col = "Choice " + std::to_string(n);
      if (t.has(col) && !trim(t.at(r, col)).empty()) v.push_back(n);
    }
    return v;
  };
  auto fitNum = [&](const Table& t, size_t r, const std::string& loCol, const std::string& hiCol, long long v) -> bool {
    if (hasCell(t, r, loCol) && v < numOf(t, r, loCol)) return false;
    if (hasCell(t, r, hiCol) && v > numOf(t, r, hiCol)) return false;
    return true;
  };

  // The rank the hero is paid at, and the promotion check that follows the card.
  const Table& cp = tbl("a16_model-career-progression-enc");
  const size_t rank = h.modelRank >= 0 && h.modelRank < int(cp.size()) ? size_t(h.modelRank) : size_t(0);
  h.modelType = trim(resolve(cp.at(rank, "Model Type")));
  const long long payLo = static_cast<long long>(cp.num(rank, "Min Salary"));
  const long long payHi = static_cast<long long>(cp.num(rank, "Max Salary"));
  h.modelSalary = payLo + static_cast<long long>(double(payHi - payLo) * modelRng_.unit());
  take += h.modelSalary;
  if (h.modelRival.empty()) {
    h.modelRival = personName("model");
    if (h.modelRival.empty()) h.modelRival = "another model";
  }

  // (1) A rival at the agency is sabotaged.
  if (title.empty() && modelRng_.chance(0.04) && ensureTable("a16_model-agency-model-sabotage-enc")) {
    const Table& t = tbl("a16_model-agency-model-sabotage-enc");
    const size_t row = drawRow("a16_model-agency-model-sabotage-enc", "Commonness Score");
    if (row != size_t(-1)) {
      const double risk = double(numOf(t, row, "Risk Score"));
      const double odds = std::max(0.15, std::min(0.9, 0.85 - risk * 0.08 + double(h.modelSkill - 40) / 300.0));
      title = "A dirty trick";
      body = spyUpperFirst(withBrackets(t, row, "Method String", h.modelRivalFemale));
      if (modelRng_.unit() < odds) {
        say(withBrackets(t, row, "Log Version", h.modelRivalFemale));
        h.modelProfile = std::min(100, h.modelProfile + int(numOf(t, row, "Sabotage Score")) * 2);
        add(h.modelRival + " " + cell(t, row, "Result Snippet") + ".");
      } else {
        say("I was caught " + cell(t, row, "Target Discovers Hero Snippet") + ".");
        h.modelProfile = std::max(0, h.modelProfile - 3);
        happy -= 4;
      }
      mark("a16_model-agency-model-sabotage-enc", int(row));
    }
  }

  // (2) The agency lets the hero go.
  if (title.empty() && modelRng_.chance(0.05) && ensureTable("a16_model-fired-color-enc")) {
    const Table& t = tbl("a16_model-fired-color-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (flagOf(t, r, "Botched Surgery") || flagOf(t, r, "Disease") || flagOf(t, r, "Pregnancy")) continue;
      if (flagOf(t, r, "Age") && h.age < 35) continue;
      if (flagOf(t, r, "Looks") && h.looks > 35) continue;
      if (flagOf(t, r, "Modeling Skill") && h.modelSkill > 30) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t row = pool[modelRng_.below(uint32_t(pool.size()))];
      title = "The agency lets me go";
      body = spyUpperFirst(scene(t, row, false, "String"));
      say("I am no longer with " + h.modelAgency + ".");
      h.modelAgency.clear();
      h.modelRank = 0;
      h.modelProfile = std::max(0, h.modelProfile - 10);
      happy -= 6;
      mark("a16_model-fired-color-enc", int(row));
    }
  }

  // (3) A fashion show, with a pivotal moment on the runway.
  if (title.empty() && modelRng_.chance(0.10) && ensureTable("a16_model-fashion-show-pivotal-moments-enc")) {
    const Table& t = tbl("a16_model-fashion-show-pivotal-moments-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (hasCell(t, r, "Category")) {
        const std::string cat = lower(trim(t.at(r, "Category")));
        if (!h.modelCategory.empty() && cat.find(lower(h.modelCategory)) == std::string::npos
            && lower(h.modelCategory).find(cat) == std::string::npos) continue;
      }
      if (t.has("Requires Model") && !trim(t.at(r, "Requires Model")).empty()) {
        if (lower(trim(t.at(r, "Requires Model"))) != lower(h.modelType)) continue;
      }
      pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      const size_t row = pool[modelRng_.below(uint32_t(pool.size()))];
      std::string venue = "the runway";
      if (ensureTable("a16_model-fashion-show-venues-enc")) {
        const Table& vt = tbl("a16_model-fashion-show-venues-enc");
        std::vector<size_t> vpool;
        for (size_t r = 0; r < vt.size(); ++r) {
          if (!fitNum(vt, r, "Min Profile", "Max Profile", h.modelProfile)) continue;
          vpool.push_back(r);
        }
        if (vpool.empty()) for (size_t r = 0; r < vt.size(); ++r) vpool.push_back(r);
        if (!vpool.empty()) {
          const size_t vr = vpool[modelRng_.below(uint32_t(vpool.size()))];
          const std::string vname = trim(resolve(vt.at(vr, "Venue")));
          venue = flagOf(vt, vr, "Uses Indefinite Article") ? withArticle(vname, true) + vname : vname;
        }
      }
      h.modelVenue = venue;
      const std::vector<int> ch = choices(t, row);
      const int pick = ch.empty() ? -1 : ch[modelRng_.below(uint32_t(ch.size()))];
      const std::string pickName = pick < 0 ? std::string() : trim(resolve(t.at(row, "Choice " + std::to_string(pick))));
      const double odds = pick < 0 ? 0.5 : double(numOf(t, row, "Success Odds " + std::to_string(pick))) / 100.0;
      const bool won = modelRng_.unit() < std::max(0.1, std::min(0.9, odds + double(h.modelPoise - 50) / 300.0));
      const std::string sfx = std::to_string(pick < 0 ? 1 : pick);
      title = "A fashion show";
      body = "Venue: " + venue + ". " + spyUpperFirst(withBrackets(t, row, "Body", h.modelRivalFemale))
             + (pickName.empty() ? std::string() : " I chose to " + pickName + ".");
      const std::string fb = won ? "Success Feedback " + sfx : "Failure Feedback " + sfx;
      if (t.has(fb)) add(spyUpperFirst(withBrackets(t, row, fb, h.modelRivalFemale)));
      const std::string nar = won ? "Success Narrative " + sfx : "Failure Narrative " + sfx;
      if (t.has(nar)) say(withBrackets(t, row, nar, h.modelRivalFemale));
      const int imp = int(numOf(t, row, (won ? "Success Performance Impact " : "Failure Performance Impact ") + sfx));
      h.modelProfile = std::max(0, std::min(100, h.modelProfile + imp));
      if (won) { h.modelShows += 1; happy += 3; } else happy -= 3;
      mark("a16_model-fashion-show-pivotal-moments-enc", int(row));
    }
  }

  // (4) An ad campaign, and the word from the client.
  if (title.empty() && modelRng_.chance(0.08) && ensureTable("a16_model-publication-titles-enc")) {
    const Table& t = tbl("a16_model-publication-titles-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!fitNum(t, r, "Min Profile", "Max Profile", h.modelProfile)) continue;
      if (hasCell(t, r, "Gender") && lower(trim(t.at(r, "Gender"))) != lower(hg)) continue;
      if (hasCell(t, r, "Industry") && !h.modelIndustry.empty()
          && lower(trim(t.at(r, "Industry"))) != lower(h.modelIndustry)) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t row = hasCell(t, pool[0], "Commonness Score") ? weightedPickOf(t, "Commonness Score", pool, modelRng_)
                                                                 : pool[modelRng_.below(uint32_t(pool.size()))];
      if (row != size_t(-1)) {
        h.modelPublication = trim(resolve(t.at(row, "Name")));
        const std::string prod = h.modelProduct.empty() ? "the product" : h.modelProduct;
        std::string summary;
        if (ensureTable("a16_model-campaign-summary-color-enc")) {
          const Table& st = tbl("a16_model-campaign-summary-color-enc");
          std::vector<size_t> spool;
          for (size_t r = 0; r < st.size(); ++r) {
            if (hasCell(st, r, "Min Reception") && h.modelSkill < numOf(st, r, "Min Reception")) continue;
            if (hasCell(st, r, "Max Reception") && h.modelSkill > numOf(st, r, "Max Reception")) continue;
            spool.push_back(r);
          }
          if (spool.empty()) for (size_t r = 0; r < st.size(); ++r) spool.push_back(r);
          if (!spool.empty()) {
            const size_t sr = spool[modelRng_.below(uint32_t(spool.size()))];
            summary = trim(resolve(st.at(sr, "String")));
            mark("a16_model-campaign-summary-color-enc", int(sr));
          }
        }
        title = "An ad campaign";
        body = "Campaign: " + h.modelPublication + ". " + spyUpperFirst(summary);
        say("I shot a campaign for " + prod + " that ran in " + h.modelPublication + ".");
        h.modelProfile = std::min(100, h.modelProfile + 4);
        take += 5000 + static_cast<long long>(modelRng_.below(12000));
        happy += 2;
      }
    }
  }

  // (5) The word from fashion week.
  if (title.empty() && modelRng_.chance(0.08) && ensureTable("a16_model-fashion-week-feedback-enc")) {
    const Table& t = tbl("a16_model-fashion-week-feedback-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!fitNum(t, r, "Min Performance", "Max Performance", h.modelProfile)) continue;
      pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      const size_t row = pool[modelRng_.below(uint32_t(pool.size()))];
      title = "Fashion week";
      const std::string verdict = scene(t, row, true, "String");
      body = spyUpperFirst(verdict);
      say(toFirstPerson(verdict));
      h.modelProfile = std::min(100, h.modelProfile + 2);
      happy += 1;
      mark("a16_model-fashion-week-feedback-enc", int(row));
    }
  }

  // (6) A photo shoot.
  if (title.empty() && modelRng_.chance(0.12) && ensureTable("a16_model-photo-shoot-poses-enc")) {
    const Table& t = tbl("a16_model-photo-shoot-poses-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (flagOf(t, r, "Body Part Model Only") && lower(h.modelType).find("body part") == std::string::npos) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t row = pool[modelRng_.below(uint32_t(pool.size()))];
      title = "A photo shoot";
      body = "I had to " + trim(resolve(t.at(row, "Body Snippet"))) + ".";
      if (t.has("Lover Argument Snippet") && !trim(t.at(row, "Lover Argument Snippet")).empty()
          && life.hero.loverName.empty() == false) {
        const bool agree = forcedChoice == 0 ? true : (forcedChoice == 1 ? false : modelRng_.chance(0.5));
        body += " " + spyUpperFirst(scene(t, row, false, "Lover Argument Snippet"));
        const int imp = int(numOf(t, row, agree ? "Agree Performance Impact" : "Disagree Performance Impact"));
        h.modelProfile = std::max(0, std::min(100, h.modelProfile + imp));
        say(agree ? ("I did what my partner asked at the shoot.")
                  : ("I refused my partner at the shoot."));
      } else {
        say("I " + trim(resolve(t.at(row, "Feedback Log Snippet"))) + ".");
      }
      const int hh = int(numOf(t, row, "Happiness Impact"));
      happy += hh;
      mark("a16_model-photo-shoot-poses-enc", int(row));
    }
  }

  // (6b) A facial-expression shoot.
  if (title.empty() && modelRng_.chance(0.06) && ensureTable("a16_model-photo-shoot-facial-expression-enc")) {
    const Table& t = tbl("a16_model-photo-shoot-facial-expression-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      const size_t row = pool[modelRng_.below(uint32_t(pool.size()))];
      const std::vector<int> ch = choices(t, row);
      if (!ch.empty()) {
        const int pick = ch[modelRng_.below(uint32_t(ch.size()))];
        const std::string sfx = std::to_string(pick);
        title = "A facial-expression shoot";
        body = "They wanted me to " + trim(resolve(t.at(row, "Snippet"))) + ". I went with "
               + trim(resolve(t.at(row, "Choice " + sfx))) + ".";
        const std::string nar = "Narrative " + sfx;
        if (t.has(nar)) say(resolve(t.at(row, nar)));
        h.modelProfile = std::max(0, std::min(100, h.modelProfile + int(numOf(t, row, "Impact Score " + sfx))));
        mark("a16_model-photo-shoot-facial-expression-enc", int(row));
      }
    }
  }

  // (7) A go-see: the interview, the poses, the review.
  if (title.empty() && modelRng_.chance(0.10) && ensureTable("a16_model-go-see-interview-questions-enc")) {
    const size_t roll = modelRng_.below(3);
    if (roll == 0) {
      const Table& t = tbl("a16_model-go-see-interview-questions-enc");
      const size_t row = modelRng_.below(uint32_t(t.size()));
      const std::vector<int> ch = choices(t, row);
      int pick = -1, best = -1, bestImp = -999;
      for (size_t i = 0; i < ch.size(); ++i) {
        const int imp = int(numOf(t, row, "Choice " + std::to_string(ch[i]) + " Impact"));
        if (imp > bestImp) { bestImp = imp; best = ch[i]; }
      }
      if (forcedChoice >= 0 && forcedChoice < int(ch.size())) pick = ch[size_t(forcedChoice)];
      else if (h.modelAttitude < 50 && !ch.empty()) pick = ch[modelRng_.below(uint32_t(ch.size()))];
      else pick = best;
      title = "A go-see";
      body = spyUpperFirst(trim(resolve(t.at(row, "Question"))))
             + (pick < 0 ? std::string() : " I answered: " + trim(resolve(t.at(row, "Choice " + std::to_string(pick)))) + ".");
      const int imp = pick < 0 ? int(numOf(t, row, "Default Choice Impact")) : int(numOf(t, row, "Choice " + std::to_string(pick) + " Impact"));
      h.modelProfile = std::max(0, std::min(100, h.modelProfile + imp));
      h.modelAttitude = std::max(0, std::min(100, h.modelAttitude + (imp >= 3 ? 2 : -1)));
      say(imp >= 4 ? "I nailed the interview at the go-see." : (imp >= 2 ? "I got through the go-see." : "The go-see did not go well."));
      mark("a16_model-go-see-interview-questions-enc", int(row));
    } else if (roll == 1) {
      const Table& t = tbl("a16_model-go-see-action-poses-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
      const size_t row = hasCell(t, pool[0], "Commonness Score") ? weightedPickOf(t, "Commonness Score", pool, modelRng_)
                                                                : pool[modelRng_.below(uint32_t(pool.size()))];
      if (row != size_t(-1)) {
        const bool agree = forcedChoice == 0 ? true : (forcedChoice == 1 ? false : modelRng_.chance(1.0 - double(h.modelQuirkiness) / 200.0));
        title = "A go-see";
        body = "They asked me to " + trim(resolve(t.at(row, "Snippet"))) + "."
               + (agree ? std::string() : " I refused.");
        say(agree ? ("I " + trim(resolve(t.at(row, "Log Snippet"))) + ".")
                  : ("I would not " + trim(resolve(t.at(row, "Log Refusal Snippet"))) + "."));
        const int imp = int(numOf(t, row, agree ? "Agree Performance Impact" : "Refuse Performance Impact"));
        h.modelProfile = std::max(0, std::min(100, h.modelProfile + imp));
        mark("a16_model-go-see-action-poses-enc", int(row));
      }
    } else {
      const Table& t = tbl("a16_model-go-see-performance-review-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        if (!fitNum(t, r, "Min Impression", "Max Impression", h.modelProfile)) continue;
        pool.push_back(r);
      }
      if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
      if (!pool.empty()) {
        const size_t row = pool[modelRng_.below(uint32_t(pool.size()))];
        title = "The go-see review";
        const std::string verdict = scene(t, row, modelRng_.chance(0.5), "String");
        body = spyUpperFirst(verdict);
        say(toFirstPerson(verdict));
        mark("a16_model-go-see-performance-review-enc", int(row));
      }
    }
  }

  // (8) Headshots.
  if (title.empty() && modelRng_.chance(0.10) && ensureTable("a16_model-headshot-outfits-enc")) {
    const Table& t = tbl("a16_model-headshot-outfits-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (hasCell(t, r, "Hero Gender") && lower(trim(t.at(r, "Hero Gender"))) != lower(hg)) continue;
      pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      const size_t row = hasCell(t, pool[0], "Commonness Score") ? weightedPickOf(t, "Commonness Score", pool, modelRng_)
                                                                : pool[modelRng_.below(uint32_t(pool.size()))];
      if (row != size_t(-1)) {
        const std::string outfit = trim(resolve(t.at(row, "Outfit Snippet")));
        const int prof = int(numOf(t, row, "Professionalism Score"));
        std::string expr = "smiling", bg = "a plain backdrop";
        int extra = 0;
        if (ensureTable("a16_model-headshot-expressions-enc")) {
          const Table& et = tbl("a16_model-headshot-expressions-enc");
          std::vector<size_t> epool;
          for (size_t r = 0; r < et.size(); ++r) epool.push_back(r);
          const size_t er = epool.empty() ? size_t(-1)
                          : (hasCell(et, epool[0], "Commonness Score") ? weightedPickOf(et, "Commonness Score", epool, modelRng_)
                                                                      : epool[modelRng_.below(uint32_t(epool.size()))]);
          if (er != size_t(-1)) { expr = trim(resolve(et.at(er, "Expression"))); extra += int(numOf(et, er, "Professionalism Score")); }
        }
        if (ensureTable("a16_model-headshot-background-enc")) {
          const Table& bt = tbl("a16_model-headshot-background-enc");
          std::vector<size_t> bpool;
          for (size_t r = 0; r < bt.size(); ++r) bpool.push_back(r);
          if (!bpool.empty()) {
            const size_t br = hasCell(bt, bpool[0], "Commonness Score") ? weightedPickOf(bt, "Commonness Score", bpool, modelRng_)
                                                                       : bpool[modelRng_.below(uint32_t(bpool.size()))];
            if (br != size_t(-1)) { bg = trim(resolve(bt.at(br, "Background"))); extra += int(numOf(bt, br, "Professionalism Score")); }
          }
        }
        h.modelOutfit = outfit;
        h.modelExpression = expr;
        h.modelBackground = bg;
        title = "Headshots";
        body = "Outfit: " + outfit + ". Expression: " + expr + ". Background: " + bg + ".";
        say("I sat for new headshots.");
        const int score = (prof + extra) / 2;
        h.modelProfile = std::max(0, std::min(100, h.modelProfile + (score >= 4 ? 3 : (score >= 3 ? 1 : -1))));
        mark("a16_model-headshot-outfits-enc", int(row));
      }
    }
  }

  // (9) An audition, and the booking that follows it.
  if (title.empty() && modelRng_.chance(0.12) && ensureTable("a16_model-audition-snippets-enc")) {
    const Table& t = tbl("a16_model-audition-snippets-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (hasCell(t, r, "Hero Gender") && lower(trim(t.at(r, "Hero Gender"))) != lower(hg)) continue;
      if (!fitNum(t, r, "Min Attitude", "Max Attitude", h.modelAttitude)) continue;
      if (!fitNum(t, r, "Min Poise", "Max Poise", h.modelPoise)) continue;
      if (!fitNum(t, r, "Min Quirkiness", "Max Quirkiness", h.modelQuirkiness)) continue;
      pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      const size_t row = pool[modelRng_.below(uint32_t(pool.size()))];
      const double difficulty = double(numOf(t, row, "Difficulty Score"));
      h.modelSkill = std::min(100, h.modelSkill + 1);
      const double odds = std::max(0.08, std::min(0.92, 0.55 + double(h.modelSkill - 40) / 200.0
                                                             + double(h.looks - 40) / 300.0 - difficulty * 0.02));
      const bool booked = modelRng_.unit() < odds;
      title = booked ? "A booking" : "An audition";
      body = "They asked me to " + trim(resolve(t.at(row, "Snippet"))) + ".";
      if (ensureTable("a16_model-audition-feedback-enc")) {
        const Table& ft = tbl("a16_model-audition-feedback-enc");
        std::vector<size_t> fpool;
        for (size_t r = 0; r < ft.size(); ++r) {
          if (!fitNum(ft, r, "Min Looks", "Max Looks", h.looks)) continue;
          if (!fitNum(ft, r, "Min Modeling Skill", "Max Modeling Skill", h.modelSkill)) continue;
          fpool.push_back(r);
        }
        if (fpool.empty()) for (size_t r = 0; r < ft.size(); ++r) fpool.push_back(r);
        if (!fpool.empty()) {
          const size_t fr = fpool[modelRng_.below(uint32_t(fpool.size()))];
          add(spyUpperFirst(scene(ft, fr, false, "Reason")));
          mark("a16_model-audition-feedback-enc", int(fr));
        }
      }
      if (booked) {
        const long long fee = 1000 + static_cast<long long>(modelRng_.below(9000)) + h.modelProfile * 120;
        take += fee;
        h.modelJobs += 1;
        h.modelProfile = std::min(100, h.modelProfile + 3);
        happy += 3;
        say("I booked the job.");
        add("I was paid " + money(fee) + " for it.");
        mark(cardTable.empty() ? "a16_model-audition-snippets-enc" : cardTable, cardRow < 0 ? int(row) : cardRow);
      } else {
        h.modelProfile = std::max(0, h.modelProfile - 1);
        happy -= 2;
        say("I did not get the booking.");
        mark("a16_model-audition-snippets-enc", int(row));
      }
    }
  }

  // (10) A gift from a client.
  if (title.empty() && modelRng_.chance(0.07) && ensureTable("a16_model-gifts-enc")) {
    const Table& t = tbl("a16_model-gifts-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (hasCell(t, r, "Gender") && lower(trim(t.at(r, "Gender"))) != lower(hg)) continue;
      if (flagOf(t, r, "Elite") && h.modelRank < 3) continue;
      pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      const size_t row = hasCell(t, pool[0], "Commonness Score") ? weightedPickOf(t, "Commonness Score", pool, modelRng_)
                                                                : pool[modelRng_.below(uint32_t(pool.size()))];
      if (row != size_t(-1)) {
        const long long lo = numOf(t, row, "Min Value");
        const long long hi = numOf(t, row, "Max Value");
        const long long value = lo + static_cast<long long>(double(hi - lo) * modelRng_.unit());
        h.modelGift = trim(resolve(t.at(row, "Item Name")));
        title = "A gift";
        body = "A client sent me a " + h.modelGift + ".";
        say("I was gifted a " + h.modelGift + " worth " + money(value) + ".");
        take += value;
        happy += 2;
        mark("a16_model-gifts-enc", int(row));
      }
    }
  }

  // (11) A day of work for a brand.
  if (title.empty() && modelRng_.chance(0.14) && ensureTable("a16_model-gig-types-enc")) {
    const Table& t = tbl("a16_model-gig-types-enc");
    const size_t row = hasCell(t, 0, "Commonness Score") ? weightedPickOf(t, "Commonness Score", std::vector<size_t>(), modelRng_) : size_t(-1);
    const size_t gr = row == size_t(-1) ? modelRng_.below(uint32_t(t.size())) : row;
    const std::string gig = trim(resolve(t.at(gr, "Job Type")));
    const std::string company = modelName("a16_model-companies-enc");
    const long long fee = 500 + static_cast<long long>(modelRng_.below(4000)) + h.modelProfile * 60;
    title = "A day on set";
    body = "I shot a " + gig + " for " + (company.empty() ? std::string("a brand") : company) + ".";
    say("I worked a " + gig + " for " + (company.empty() ? std::string("a brand") : company) + " and was paid " + money(fee) + ".");
    take += fee;
    h.modelJobs += 1;
    h.modelProfile = std::min(100, h.modelProfile + 2);
    h.modelSkill = std::min(100, h.modelSkill + 1);
    happy += 1;
    mark("a16_model-gig-types-enc", int(gr));
  }

  // (12) A quiet year on the catwalk.
  if (title.empty()) {
    title = "A year on the catwalk";
    body = "I worked the " + (h.modelCategory.empty() ? std::string("fashion") : h.modelCategory)
           + " circuit as a " + (h.modelType.empty() ? std::string("model") : lower(h.modelType)) + ".";
    say("I spent the year modelling.");
    h.modelProfile = std::min(100, h.modelProfile + 1);
    mark("a16_model-career-progression-enc", int(rank));
  }

  // The promotion at the end of the year, and the stress of the work.
  const int jobsToPromotion = int(numOf(cp, rank, "Jobs To Promotion"));
  const long long needLooks = numOf(cp, rank, "Minimum Looks");
  const long long needProfile = numOf(cp, rank, "Minimum Profile");
  const long long needHealth = numOf(cp, rank, "Minimum Health");
  if (rank + 1 < cp.size() && h.modelJobs >= jobsToPromotion && h.looks >= needLooks
      && h.modelProfile >= needProfile && h.health >= needHealth && h.age <= int(numOf(cp, rank, "Max Age"))) {
    h.modelRank += 1;
    const size_t next = size_t(h.modelRank);
    h.modelType = trim(resolve(cp.at(next, "Model Type")));
    if (flagOf(cp, next, "Can Be Famous")) h.modelFamous = true;
    add("I was promoted to " + h.modelType + ".");
  }
  const int stress = int(numOf(cp, rank, "Stress Score"));
  h.modelStress = std::max(0, h.modelStress + stress - 2);
  if (h.modelStress >= 6) { happy -= 2; h.modelStress = 3; }

  if (title.empty()) { modelLive_ = false; return false; }
  if (body.empty()) body = title;
  out.table = cardTable.empty() ? "a16_model-career-progression-enc" : cardTable;
  out.row_id = cardRow >= 0 ? std::to_string(cardRow) : "0";
  out.title = title;
  out.body = body;
  out.narrative = line;
  h.money += take;
  h.modelEarned += take;
  out.happiness += happy;
  modelLive_ = false;
  return true;
}

void Sim::makeModel(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.model = true;
  h.modelRank = 0;
  h.modelProfile = 20;
  h.modelSkill = 40;
  h.modelAttitude = 50;
  h.modelPoise = 50;
  h.modelQuirkiness = 50;
  h.modelJobs = 0;
  h.modelShows = 0;
  h.modelStress = 0;
  h.modelElite = false;
  h.modelFamous = false;
  h.modelIndustry = "fashion";
  if (!ensureTable("a16_model-career-progression-enc")) return;
  const Table& cp = tbl("a16_model-career-progression-enc");
  const size_t rank = h.modelRank < int(cp.size()) ? size_t(h.modelRank) : size_t(0);
  h.modelType = trim(resolve(cp.at(rank, "Model Type")));
  const long long lo = static_cast<long long>(cp.num(rank, "Min Salary"));
  const long long hi = static_cast<long long>(cp.num(rank, "Max Salary"));
  h.modelSalary = lo + static_cast<long long>(double(hi - lo) * modelRng_.unit());
  if (ensureTable("a16_model-industries-enc")) {
    const Table& it = tbl("a16_model-industries-enc");
    std::vector<size_t> ipool;
    for (size_t r = 0; r < it.size(); ++r) {
      if (it.has("Country Blacklist") && !trim(it.at(r, "Country Blacklist")).empty()
          && lower(trim(it.at(r, "Country Blacklist"))).find(lower(h.country)) != std::string::npos) continue;
      ipool.push_back(r);
    }
    if (ipool.empty()) for (size_t r = 0; r < it.size(); ++r) ipool.push_back(r);
    const size_t irow = ipool.empty() ? size_t(-1)
                      : (it.has("Commonness Score") ? weightedPickOf(it, "Commonness Score", ipool, modelRng_)
                                                    : ipool[modelRng_.below(uint32_t(ipool.size()))]);
    if (irow != size_t(-1)) {
      const std::string ind = trim(resolve(it.at(irow, "Industry")));
      if (!ind.empty()) h.modelIndustry = ind;
    }
  }
  if (ensureTable("a16_model-categories-enc")) {
    const Table& ct = tbl("a16_model-categories-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < ct.size(); ++r) {
      if (ct.has("Industry") && !trim(ct.at(r, "Industry")).empty()
          && lower(trim(ct.at(r, "Industry"))) != lower(h.modelIndustry)) continue;
      pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < ct.size(); ++r) pool.push_back(r);
    const size_t row = ct.has("Commonness Score") ? weightedPickOf(ct, "Commonness Score", pool, modelRng_)
                                                  : pool[modelRng_.below(uint32_t(pool.size()))];
    if (row != size_t(-1)) {
      h.modelCategory = trim(resolve(ct.at(row, "Category")));
      const std::string ind = trim(resolve(ct.at(row, "Industry")));
      if (!ind.empty()) h.modelIndustry = ind;
    }
  }
  h.modelAgency = modelName("a16_model-agency-names-enc");
  if (h.modelAgency.empty()) h.modelAgency = "the agency";
  h.modelRival = personName("model");
  if (h.modelRival.empty()) h.modelRival = "another model";
  h.modelRivalFemale = modelRng_.chance(0.5);
  if (ensureTable("a16_model-products-enc")) {
    const Table& pt = tbl("a16_model-products-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < pt.size(); ++r) {
      if (!h.modelCategory.empty() && pt.has("Category") && !trim(pt.at(r, "Category")).empty()
          && lower(trim(pt.at(r, "Category"))) != lower(h.modelCategory)) continue;
      pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < pt.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      const size_t row = pool[modelRng_.below(uint32_t(pool.size()))];
      std::vector<std::string> parts;
      splitOn(pt.at(row, "Product Name"), ',', parts);
      std::vector<std::string> keep;
      for (size_t i = 0; i < parts.size(); ++i) if (!trim(parts[i]).empty()) keep.push_back(trim(parts[i]));
      if (!keep.empty()) h.modelProduct = trim(resolve(keep[modelRng_.below(uint32_t(keep.size()))]));
    }
  }
}

std::string Sim::modelToken(const std::string& token) {
  Hero* h = const_cast<Hero*>(hero_);
  if (!h) return std::string();
  std::string k = trim(token);
  const std::string psuffix = "-possessive";
  bool possessive = false;
  if (k.size() > psuffix.size() && k.compare(k.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    k.resize(k.size() - psuffix.size());
    possessive = true;
  }
  if (k == "model") {
    std::string v = h->modelRival;
    if (v.empty()) v = "another model";
    if (possessive) v += "'s";
    return v;
  }
  if (k == "model-product" || k == "model-product-name") {
    std::string v = h->modelProduct;
    if (v.empty()) {
      if (ensureTable("a16_model-products-enc")) {
        const Table& pt = tbl("a16_model-products-enc");
        if (pt.size()) {
          const size_t row = modelRng_.below(uint32_t(pt.size()));
          std::vector<std::string> parts;
          splitOn(pt.at(row, "Product Name"), ',', parts);
          for (size_t i = 0; i < parts.size(); ++i) {
            if (!trim(parts[i]).empty()) { v = trim(resolve(parts[i])); break; }
          }
        }
      }
      if (v.empty()) v = "the product";
    }
    return v;
  }
  return std::string();
}

std::string Sim::modelName(const char* table) {
  Hero* h = const_cast<Hero*>(hero_);
  const std::string hg = heroToken("gender", false);
  if (!ensureTable(table)) return std::string();
  const Table& t = tbl(table);
  const std::string ind = h ? lower(h->modelIndustry) : std::string();
  std::vector<size_t> pool;
  for (size_t r = 0; r < t.size(); ++r) {
    if (t.has("Industry") && !ind.empty()) {
      const std::string v = lower(trim(t.at(r, "Industry")));
      if (!v.empty() && v != ind) continue;
    }
    if (t.has("Country Blacklist")) {
      const std::string v = lower(trim(t.at(r, "Country Blacklist")));
      if (!v.empty() && h && !h->country.empty() && v.find(lower(h->country)) != std::string::npos) continue;
    }
    pool.push_back(r);
  }
  if (pool.empty()) {
    for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
  }
  if (pool.empty()) return std::string();
  const size_t row = t.has("Commonness Score") ? weightedPickOf(t, "Commonness Score", pool, modelRng_)
                                              : pool[modelRng_.below(uint32_t(pool.size()))];
  if (row == size_t(-1) || !t.has("Format")) return std::string();
  auto oneOf = [&](const std::string& cellText) -> std::string {
    std::vector<std::string> parts;
    splitOn(cellText, ',', parts);
    std::vector<std::string> keep;
    for (size_t i = 0; i < parts.size(); ++i) { if (!trim(parts[i]).empty()) keep.push_back(trim(parts[i])); }
    if (keep.empty()) return std::string();
    return keep[modelRng_.below(uint32_t(keep.size()))];
  };
  const std::string fmt = oneOf(t.at(row, "Format"));
  std::string out = fmt;
  const char* tags[6] = {"<prefix>", "<suffix>", "<palette>", "<color>", "<random-first-name-possessive>", "<random-first-name>"};
  for (int i = 0; i < 6; ++i) {
    const std::string tag = tags[i];
    while (out.find(tag) != std::string::npos) {
      std::string v;
      if (tag == "<prefix>" || tag == "<suffix>") {
        v = t.has(tag == "<prefix>" ? "Prefix" : "Suffix") ? oneOf(t.at(row, tag == "<prefix>" ? "Prefix" : "Suffix")) : std::string();
      } else if (tag == "<palette>" || tag == "<color>") {
        if (ensureTable("a16_company-name-color-enc")) {
          const Table& ct = tbl("a16_company-name-color-enc");
          if (ct.size()) v = trim(resolve(ct.at(modelRng_.below(uint32_t(ct.size())), "Color")));
        }
        if (v.empty()) v = "Ivory";
      } else {
        const bool fem = hg == "female";
        v = drawFirstName(fem);
        if (v.empty()) v = "Alex";
        if (tag == "<random-first-name-possessive>") v += "'s";
      }
      out.replace(out.find(tag), tag.size(), v);
    }
  }
  if (out.find("<random-last-name>") != std::string::npos) {
    std::string v = drawSurname();
    if (v.empty()) v = "Vance";
    while (out.find("<random-last-name>") != std::string::npos) out.replace(out.find("<random-last-name>"), 17, v);
  }
  // Anything the table left in brackets is dropped, then the blanks close up.
  std::string cleaned;
  for (size_t i = 0; i < out.size(); ++i) {
    if (out[i] == '<') {
      const size_t close = out.find('>', i);
      if (close != std::string::npos) { i = close; continue; }
    }
    cleaned += out[i];
  }
  cleaned = trim(resolve(cleaned));
  while (cleaned.find("  ") != std::string::npos) cleaned.replace(cleaned.find("  "), 2, " ");
  return cleaned;
}

int Sim::modelRankIndex() const {
  const Hero* h = hero_;
  return h ? h->modelRank : 0;
}

}  // namespace bl
