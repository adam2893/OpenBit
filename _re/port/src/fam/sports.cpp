// src/fam/sports.cpp -- moved out of src/sim.cpp (REPORT.en.md 131).
//
// Cut verbatim by tools/split_family.py; the only edits are the includes.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>
#include <vector>

namespace bl {

bool Sim::sportsSeason(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.athlete || !h.sportPro) return false;
  hero_ = &h;
  life_ = &life;
  if (h.age < 18) return false;
  if (!ensureTable("a16_sports-enc")) return false;
  if (!ensureTable("a16_sports-practice-scenarios-enc")) return false;
  if (!sportProRng_.chance(0.58)) return false;

  const std::string gender = h.female ? "female" : "male";
  const Table& sp = tbl("a16_sports-enc");
  size_t spRow = size_t(-1);
  for (size_t r = 0; r < sp.size(); ++r)
    if (lower(trim(sp.at(r, "Sport"))) == h.sport) spRow = r;
  if (spRow == size_t(-1)) return false;

  std::string title, body, line;
  std::string tableRef = "a16_sports-enc";
  int cardRow = int(spRow);
  std::vector<std::string> extra;
  int happy = 0;

  const auto numOf = [&](const Table& t, size_t r, const std::string& col) -> long long {
    if (!t.has(col)) return 0;
    if (trim(t.at(r, col)).empty()) return 0;
    return static_cast<long long>(t.num(r, col));
  };
  const auto flagOf = [&](const Table& t, size_t r, const std::string& col) -> bool {
    if (!t.has(col)) return false;
    const std::string v = lower(trim(t.at(r, col)));
    return v == "x" || v == "yes" || v == "true";
  };
  const auto cell = [&](const Table& t, size_t r, const std::string& col) -> std::string {
    if (!t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  const auto say = [&](std::string s) {
    s = trim(s);
    if (line.empty() && !s.empty()) line = spyUpperFirst(s);
  };
  const auto add = [&](std::string s) {
    s = trim(s);
    if (s.empty()) return;
    const char last = s[s.size() - 1];
    if (last != '.' && last != '!' && last != '?') s += '.';
    extra.push_back(s);
  };
  const auto mark = [&](const std::string& tab, int row) {
    tableRef = tab;
    cardRow = row;
  };
  // The pools carry one flag column per sport ('Soccer', 'Basketball', ...).
  const auto sportCol = [&](const Table& t, const std::string& suffix) -> std::string {
    for (const std::string& c : t.columns())
      if (lower(c) == h.sport + suffix) return c;
    return std::string();
  };
  const auto sportFlag = [&](const Table& t, size_t r) -> bool {
    const std::string c = sportCol(t, "");
    return c.empty() ? true : flagOf(t, r, c);
  };
  const auto fitNum = [&](const Table& t, size_t r, const std::string& loCol,
                          const std::string& hiCol, double v) -> bool {
    if (t.has(loCol) && !trim(t.at(r, loCol)).empty() && v < t.num(r, loCol)) return false;
    if (t.has(hiCol) && !trim(t.at(r, hiCol)).empty() && v > t.num(r, hiCol)) return false;
    return true;
  };
  const auto drawOne = [&](const Table& t, const std::vector<size_t>& pool,
                           const std::string& weight) -> size_t {
    if (pool.empty()) return size_t(-1);
    if (!weight.empty() && t.has(weight)) {
      const size_t r = weightedPickOf(t, weight, pool, sportProRng_);
      if (r != size_t(-1)) return r;
    }
    return pool[sportProRng_.below(uint32_t(pool.size()))];
  };
  const auto clampStat = [](int v) { return v < 0 ? 0 : (v > 99 ? 99 : v); };
  // The style of the familes' other row renderers: the column that owns an
  // alternative is resolved before the column that points back at it.
  const auto scene = [&](const Table& t, size_t r, bool otherFemale, const std::string& col) -> std::string {
    const bool savedSet = pronounOtherSet_;
    const bool savedFemale = pronounOtherFemale_;
    pronounOtherSet_ = true;
    pronounOtherFemale_ = otherFemale;
    const std::string v = cell(t, r, col);
    pronounOtherSet_ = savedSet;
    pronounOtherFemale_ = savedFemale;
    return v;
  };
  // Every pool in the family writes its card column and its journal column from
  // the same row, and both often carry the same '<numN-M>' tag ('12 sets of 30
  // bench presses' / '10 sets'). Draw each tag once and put the same number in
  // both cells - the section-76 actor fix, reused here.
  const auto bindPair = [&](std::string a, std::string c) -> std::pair<std::string, std::string> {
    std::vector<std::string> tags;
    for (size_t p = 0; (p = a.find("<num", p)) != std::string::npos;) {
      const size_t e = a.find('>', p);
      if (e == std::string::npos) break;
      const std::string tag = a.substr(p, e - p + 1);
      bool seen = false;
      for (const std::string& x : tags) if (x == tag) seen = true;
      if (!seen) tags.push_back(tag);
      p = e + 1;
    }
    for (const std::string& tag : tags) {
      std::string v;
      // numberToken takes the inner text, not the bracketed tag.
      if (!numberToken(tag.substr(1, tag.size() - 2), v) || v.empty()) continue;
      size_t q = 0;
      while ((q = a.find(tag, q)) != std::string::npos) { a.replace(q, tag.size(), v); q += v.size(); }
      q = 0;
      while ((q = c.find(tag, q)) != std::string::npos) { c.replace(q, tag.size(), v); q += v.size(); }
    }
    return std::make_pair(a, c);
  };
  const auto scenePair = [&](const Table& t, size_t r, bool otherFemale, const std::string& bodyCol,
                             const std::string& logCol) -> std::pair<std::string, std::string> {
    std::pair<std::string, std::string> pr =
        bindPair(trim(t.at(r, bodyCol)), trim(t.at(r, logCol)));
    const bool savedSet = pronounOtherSet_;
    const bool savedFemale = pronounOtherFemale_;
    pronounOtherSet_ = true;
    pronounOtherFemale_ = otherFemale;
    pr.first = resolve(pr.first);
    pr.second = resolve(pr.second);
    pronounOtherSet_ = savedSet;
    pronounOtherFemale_ = savedFemale;
    return pr;
  };

  // ---- the book ----
  const long long g0 = numOf(sp, spRow, "Season Games");
  const int games = g0 > 0 ? int(g0) : 16;
  long long maxWin = numOf(sp, spRow, "Max Win Percent");
  if (maxWin <= 0) maxWin = 60;
  double edge = 0.30 + (h.sportGreatness - 20) / 160.0 + (h.sportFitness - 40) / 400.0
                - (h.sportInjured ? 0.15 : 0.0);
  edge = std::max(0.20, std::min(double(maxWin) / 100.0, edge));
  const int wins = int(double(games) * edge * (0.65 + 0.35 * sportProRng_.unit()));
  const double winPct = games > 0 ? double(wins) / double(games) : 0.0;
  const long long loSal = numOf(sp, spRow, h.female ? "Female Min Salary" : "Male Min Salary");
  const long long hiSal = numOf(sp, spRow, h.female ? "Female Max Salary" : "Male Max Salary");
  const long long pay = loSal
      + static_cast<long long>(double(hiSal - loSal) * std::min(1.0, h.sportGreatness / 100.0));
  h.money += pay;
  h.sportEarned += pay;
  h.sportSalary = pay;
  h.sportSeasons += 1;
  h.sportGames += games;
  h.sportWins += wins;
  h.sportChampion = winPct >= 0.85 && sportProRng_.chance(0.35);
  h.sportTop5 = winPct >= 0.70;
  h.sportSkill = clampStat(h.sportSkill + 1 + (winPct >= 0.5 ? 1 : 0));
  add("Season " + std::to_string(h.sportSeasons) + ": " + std::to_string(wins) + "-"
      + std::to_string(games - wins) + " for " + money(pay) + ".");
  h.sportRookie = h.sportSeasons <= 1;

  // ---- the stat sheet ----
  if (ensureTable("a16_sports-stats-enc")) {
    const Table& st = tbl("a16_sports-stats-enc");
    static const int kPct[11] = {1, 10, 20, 30, 40, 50, 60, 70, 80, 90, 99};
    std::string sheet;
    int shown = 0;
    for (size_t r = 0; r < st.size() && shown < 5; ++r) {
      if (lower(trim(st.at(r, "Sport"))) != h.sport) continue;
      const int p = std::max(1, std::min(99, h.sportSkill + int(sportProRng_.below(11)) - 5));
      double value = st.num(r, "Percentile 1");
      for (int i = 1; i < 11; ++i) {
        const double hi = st.num(r, std::string("Percentile ") + std::to_string(kPct[i]));
        const double lo = st.num(r, std::string("Percentile ") + std::to_string(kPct[i - 1]));
        if (p <= kPct[i]) {
          const double f = double(p - kPct[i - 1]) / double(kPct[i] - kPct[i - 1]);
          value = lo + (hi - lo) * std::max(0.0, f);
          break;
        }
        value = hi;
      }
      const int frac = int(numOf(st, r, "Fraction Digits"));
      const long long scale = frac > 0 ? 10 : 1;
      const long long iv = static_cast<long long>(value * double(scale) + 0.5);
      std::string shown2 = std::to_string(iv / scale);
      if (frac > 0) shown2 += "." + std::to_string(iv % scale);
      if (!sheet.empty()) sheet += ", ";
      sheet += shown2 + " " + trim(st.at(r, "Stat"));
      shown += 1;
    }
    if (!sheet.empty()) add("Season stats: " + sheet + ".");
    // Greatness tracks the numbers: a season at skill S makes the hero roughly
    // an S-15 player to the people who vote on awards.
    h.sportGreatness = std::max(h.sportGreatness,
        clampStat(h.sportSkill - 10 + int(sportProRng_.below(16))));
  }

  // ---- the year's event ----
  // (0) An injury the hero is still carrying runs its course.
  if (title.empty() && h.sportInjured && sportProRng_.chance(0.55)) {
    h.sportInjuryYears -= 1;
    if (h.sportInjuryYears <= 0) {
      h.sportInjured = false;
      h.sportSkill = clampStat(h.sportSkill - h.sportInjurySeverity);
      title = "Back on the field";
      body = "The rehab is done: my " + h.sportInjury + " is healed.";
      say("I was cleared to play again after my " + h.sportInjury + " healed");
      add("I lost " + std::to_string(h.sportInjurySeverity) + " steps of skill to it.");
      happy += 1;
    } else {
      title = "Still rehabbing";
      body = "My " + h.sportInjury + " still needs " + std::to_string(h.sportInjuryYears)
             + (h.sportInjuryYears == 1 ? " year" : " years") + " of work.";
      say("I spent the year rehabbing my " + h.sportInjury);
      add("I watched most of the season from the training room.");
      happy -= 2;
    }
    mark("a16_sports-injuries-enc", -1);
  }

  // (1) The injury that ends it (2%).
  if (title.empty() && sportProRng_.chance(0.02) && ensureTable("a16_sports-injuries-enc")) {
    const Table& t = tbl("a16_sports-injuries-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!flagOf(t, r, "Can Be Career Ending")) continue;
      if (!sportFlag(t, r)) continue;
      if (t.has("Gender")) {
        const std::string g = lower(trim(t.at(r, "Gender")));
        if (!g.empty() && g != gender) continue;
      }
      pool.push_back(r);
    }
    const size_t r = drawOne(t, pool, std::string());
    if (r != size_t(-1)) {
      const std::string type = cell(t, r, "Type");
      const std::string art = flagOf(t, r, "Uses Indefinite Article") ? articleFor(type) : std::string();
      title = "A career-ending injury";
      body = "The diagnosis: " + art + type + ".";
      say("I " + cell(t, r, "Log Version Snippet"));
      add("My career is over.");
      h.sportCareerEnding = true;
      h.sportInjured = true;
      h.sportInjury = type;
      h.sportInjurySeverity = int(numOf(t, r, "Seriousness Score"));
      h.sportInjuryPain = int(numOf(t, r, "Pain Score"));
      h.sportSkill = clampStat(h.sportSkill - 4 * h.sportInjurySeverity);
      h.athlete = false;
      h.sportPro = false;
      h.sportFame = 0;
      mark("a16_sports-injuries-enc", int(r));
      happy -= 6;
    }
  }

  // (2) An award (4%).
  if (title.empty() && sportProRng_.chance(0.04) && ensureTable("a16_sports-awards-enc")) {
    const Table& t = tbl("a16_sports-awards-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (lower(trim(t.at(r, "Sport"))) != h.sport) continue;
      if (!flagOf(t, r, "Add Award")) continue;
      if (h.sportSeasons < int(numOf(t, r, "Min Seasons"))) continue;
      if (numOf(t, r, "Max Seasons") > 0 && h.sportSeasons > int(numOf(t, r, "Max Seasons"))) continue;
      if (h.sportGreatness < int(numOf(t, r, "Min Greatness"))) continue;
      if (numOf(t, r, "Max Greatness") > 0 && h.sportGreatness > int(numOf(t, r, "Max Greatness"))) continue;
      if (flagOf(t, r, "Requires Championship") && !h.sportChampion) continue;
      if (flagOf(t, r, "Requires Top 5 Finish") && !h.sportTop5) continue;
      const std::string pos = trim(t.at(r, "Position"));
      if (!pos.empty()) {
        bool ok = false;
        std::vector<std::string> ps;
        splitOn(pos, ',', ps);
        for (const std::string& p : ps) if (lower(trim(p)) == lower(h.position)) ok = true;
        if (!ok) continue;
      }
      pool.push_back(r);
    }
    const size_t r = drawOne(t, pool, std::string());
    if (r != size_t(-1)) {
      const std::string award = cell(t, r, "Award");
      const std::string why = cell(t, r, "Description Snippet");
      title = "An award";
      body = "Award: " + award + ". For " + why + ".";
      say("I won the " + award + " for " + why);
      if (flagOf(t, r, "Performance Increase Award")) {
        h.sportSkill = clampStat(h.sportSkill + 3);
        add("It came with a bump in my numbers.");
      }
      h.sportAwards += 1;
      h.sportAwardNames.push_back(award);
      h.sportGreatness = clampStat(h.sportGreatness + 3);
      h.sportRespect = clampStat(h.sportRespect + 2);
      mark("a16_sports-awards-enc", int(r));
      happy += 3;
    }
  }

  // (3) An injury (5%).
  if (title.empty() && !h.sportInjured && sportProRng_.chance(0.05)
      && ensureTable("a16_sports-injuries-enc")) {
    const Table& t = tbl("a16_sports-injuries-enc");
    const std::string cc = sportCol(t, " Commonness Score");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!sportFlag(t, r)) continue;
      if (t.has("Gender")) {
        const std::string g = lower(trim(t.at(r, "Gender")));
        if (!g.empty() && g != gender) continue;
      }
      if (!cc.empty() && t.num(r, cc) <= 0) continue;
      pool.push_back(r);
    }
    const size_t r = drawOne(t, pool, cc);
    if (r != size_t(-1)) {
      const std::string type = cell(t, r, "Type");
      const std::string art = flagOf(t, r, "Uses Indefinite Article") ? articleFor(type) : std::string();
      const int sev = int(numOf(t, r, "Seriousness Score"));
      const int dur = int(numOf(t, r, "Duration Score"));
      title = "An injury";
      body = "The diagnosis: " + art + type + ".";
      say("I " + cell(t, r, "Log Version Snippet"));
      h.sportInjured = true;
      h.sportInjury = type;
      h.sportInjurySeverity = sev;
      h.sportInjuryYears = dur;
      h.sportInjuryPain = int(numOf(t, r, "Pain Score"));
      h.sportDebilitating = flagOf(t, r, "Is Debilitating");
      h.sportSkill = clampStat(h.sportSkill - sev);
      h.sportFitness = clampStat(h.sportFitness - sev / 2 - 1);
      add("It will take " + std::to_string(dur) + (dur == 1 ? " year" : " years") + " of rehab.");
      if (flagOf(t, r, "Can Heal Organically")) add("The doctor says it should heal on its own.");
      else if (flagOf(t, r, "Can Have Surgery")) add("Surgery is an option.");
      mark("a16_sports-injuries-enc", int(r));
      happy -= 2;
    }
  }

  // (4) A pivotal moment (6%).
  if (title.empty() && sportProRng_.chance(0.06)
      && ensureTable("a16_sports-pivotal-moment-scenarios-enc")) {
    const Table& t = tbl("a16_sports-pivotal-moment-scenarios-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!sportFlag(t, r)) continue;
      const std::string pos = t.has("Position") ? trim(t.at(r, "Position")) : std::string();
      if (!pos.empty()) {
        bool ok = false;
        std::vector<std::string> ps;
        splitOn(pos, ',', ps);
        for (const std::string& p : ps) if (lower(trim(p)) == lower(h.position)) ok = true;
        if (!ok) continue;
      }
      pool.push_back(r);
    }
    const size_t r = drawOne(t, pool, std::string());
    if (r != size_t(-1)) {
      std::vector<std::pair<std::string, int> > choices;
      for (int i = 1; i <= 4; ++i) {
        const std::string col = "Choice " + std::to_string(i);
        if (t.has(col) && !trim(t.at(r, col)).empty()) choices.push_back(std::make_pair(cell(t, r, col), i));
      }
      if (!choices.empty()) {
        const int pick = (forcedChoice >= 0 && forcedChoice < int(choices.size()))
                             ? forcedChoice
                             : int(sportProRng_.below(uint32_t(choices.size())));
        const int idx = choices[size_t(pick)].second;
        const int odds = int(numOf(t, r, "Success Odds " + std::to_string(idx)));
        const bool won = int(sportProRng_.below(100)) < odds;
        const std::string sv = won ? "Success " : "Failure ";
        title = "A moment in the game";
        body = cell(t, r, "Body");
        body += " " + choices[size_t(pick)].first + "? " + spyUpperFirst(cell(t, r, sv + "Feedback " + std::to_string(idx)));
        const std::string nar = cell(t, r, sv + "Narrative " + std::to_string(idx));
        say(nar.empty() ? std::string(won ? "I came through when it counted" : "I could not get it done")
                        : spyUpperFirst(nar));
        const long long resp = numOf(t, r, sv + "Respect Impact " + std::to_string(idx));
        h.sportRespect = clampStat(h.sportRespect + int(resp));
        if (won) happy += 3; else happy -= 2;
        add(won ? "The crowd was on its feet." : "The crowd went quiet.");
        mark("a16_sports-pivotal-moment-scenarios-enc", int(r));
      }
    }
  }

  // (5) Trash talk (6%).
  if (title.empty() && sportProRng_.chance(0.06)
      && ensureTable("a16_sports-trash-talk-scenarios-enc")) {
    const Table& t = tbl("a16_sports-trash-talk-scenarios-enc");
    const bool inbound = sportProRng_.chance(0.55);
    const bool themFemale = sportProRng_.chance(0.5);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!sportFlag(t, r)) continue;
      if (!fitNum(t, r, "Min Greatness", "Max Greatness", double(h.sportGreatness))) continue;
      const std::string want = inbound ? gender : std::string(themFemale ? "female" : "male");
      if (t.has("Recipient Gender")) {
        const std::string g = lower(trim(t.at(r, "Recipient Gender")));
        if (!g.empty() && g != want) continue;
      }
      pool.push_back(r);
    }
    const size_t r = drawOne(t, pool, std::string());
    if (r != size_t(-1)) {
      title = "Trash talk";
      body = scene(t, r, themFemale, inbound ? "Scenario Inbound" : "Scenario Outbound");
      say(inbound ? std::string("A player from the other team ran his mouth at me")
                  : std::string("I told a player on the other team exactly what I thought"));
      add(inbound ? "I did not let it get to me." : "It fired the team up.");
      h.sportRespect = clampStat(h.sportRespect + (inbound ? -1 : 1));
      happy += inbound ? -1 : 1;
      mark("a16_sports-trash-talk-scenarios-enc", int(r));
    }
  }

  // (6) A word from the coach (8%).
  if (title.empty() && sportProRng_.chance(0.08)
      && ensureTable("a16_sports-coach-scenarios-enc")) {
    const Table& t = tbl("a16_sports-coach-scenarios-enc");
    const bool good = sportProRng_.chance(0.5);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!sportFlag(t, r)) continue;
      if (flagOf(t, r, "Positive") != good) continue;
      if (t.has("Hero Gender")) {
        const std::string g = lower(trim(t.at(r, "Hero Gender")));
        if (!g.empty() && g != gender) continue;
      }
      if (!fitNum(t, r, "Min Fitness", "Max Fitness", double(h.sportFitness))) continue;
      if (!fitNum(t, r, "Min Greatness", "Max Greatness", double(h.sportGreatness))) continue;
      pool.push_back(r);
    }
    const size_t r = drawOne(t, pool, std::string());
    if (r != size_t(-1)) {
      const bool coachFemale = sportProRng_.chance(0.5);
      title = good ? "The coach is happy" : "The coach is not happy";
      const std::pair<std::string, std::string> pr =
          scenePair(t, r, coachFemale, "Scenario", "Log Version");
      body = pr.first;
      say(pr.second);
      if (good) {
        h.sportGreatness = clampStat(h.sportGreatness + 1);
        happy += 2;
      } else {
        h.sportFitness = clampStat(h.sportFitness - 1);
        happy -= 2;
      }
      mark("a16_sports-coach-scenarios-enc", int(r));
    }
  }

  // (7) A fan (10%).
  if (title.empty() && sportProRng_.chance(0.10)
      && ensureTable("a16_sports-fan-interaction-scenarios-enc")) {
    const Table& t = tbl("a16_sports-fan-interaction-scenarios-enc");
    const bool good = sportProRng_.chance(0.5);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!sportFlag(t, r)) continue;
      if (flagOf(t, r, "Positive") != good) continue;
      if (t.has("Hero Gender")) {
        const std::string g = lower(trim(t.at(r, "Hero Gender")));
        if (!g.empty() && g != gender) continue;
      }
      if (!fitNum(t, r, "Min Greatness", "Max Greatness", double(h.sportGreatness))) continue;
      if (!fitNum(t, r, "Hero Min Age", "Hero Max Age", double(h.age))) continue;
      if (!fitNum(t, r, "Min Respect", "Max Respect", double(h.sportRespect))) continue;
      pool.push_back(r);
    }
    const size_t r = drawOne(t, pool, std::string());
    if (r != size_t(-1)) {
      bool fanFemale = sportProRng_.chance(0.5);
      if (t.has("Fan Gender")) {
        const std::string g = lower(trim(t.at(r, "Fan Gender")));
        if (g == "male" || g == "female") fanFemale = g == "female";
      }
      title = good ? "A fan" : "An angry fan";
      std::pair<std::string, std::string> pr =
          scenePair(t, r, fanFemale, "Scenario", "Log Version Snippet");
      body = pr.first;
      say("A fan " + pr.second);
      if (good) {
        h.sportGreatness = clampStat(h.sportGreatness + 1);
        h.sportRespect = clampStat(h.sportRespect + 2);
        happy += 2;
      } else {
        h.sportRespect = clampStat(h.sportRespect - 2);
        happy -= 2;
      }
      mark("a16_sports-fan-interaction-scenarios-enc", int(r));
    }
  }

  // (8) Practice (14%) and (9) training (16%): the two skill pools. Their Log
  // Version columns point back at the Scenario's alternatives, so the Scenario
  // is resolved first.
  if (title.empty() && sportProRng_.chance(0.14)
      && ensureTable("a16_sports-practice-scenarios-enc")) {
    const Table& t = tbl("a16_sports-practice-scenarios-enc");
    const std::string skills = sportCol(t, " Skills");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!skills.empty() && trim(t.at(r, skills)).empty()) continue;
      pool.push_back(r);
    }
    const size_t r = drawOne(t, pool, std::string());
    if (r != size_t(-1)) {
      const int impact = int(numOf(t, r, "Skill Impact"));
      const int risk = int(numOf(t, r, "Injury Score"));
      const std::string what = skills.empty() ? std::string() : cell(t, r, skills);
      title = "Practice";
      const std::pair<std::string, std::string> pr =
          bindPair(trim(t.at(r, "Scenario")), trim(t.at(r, "Log Version")));
      body = resolve(pr.first);
      say(resolve(pr.second));
      h.sportSkill = clampStat(h.sportSkill + impact);
      if (!what.empty()) add("Focused on: " + lower(what) + ".");
      add("My " + h.sport + " skill is up " + std::to_string(impact) + " to "
          + std::to_string(h.sportSkill) + ".");
      if (risk > 0 && sportProRng_.chance(0.02 * double(risk))) {
        add("I tweaked something late in the session.");
        h.sportFitness = clampStat(h.sportFitness - 2);
      }
      mark("a16_sports-practice-scenarios-enc", int(r));
      happy += 1;
    }
  }
  if (title.empty() && sportProRng_.chance(0.16)
      && ensureTable("a16_sports-training-scenarios-enc")) {
    const Table& t = tbl("a16_sports-training-scenarios-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    const size_t r = drawOne(t, pool, std::string());
    if (r != size_t(-1)) {
      const int fit = int(numOf(t, r, "Fitness Impact"));
      const int risk = int(numOf(t, r, "Injury Score"));
      title = "Training";
      const std::pair<std::string, std::string> pr =
          bindPair(trim(t.at(r, "Scenario")), trim(t.at(r, "Log Version")));
      body = resolve(pr.first);
      say(resolve(pr.second));
      if (flagOf(t, r, "Balance")) h.sportBalance = clampStat(h.sportBalance + 2);
      if (flagOf(t, r, "Endurance")) h.sportEndurance = clampStat(h.sportEndurance + 2);
      if (flagOf(t, r, "Flexibility")) h.sportFlexibility = clampStat(h.sportFlexibility + 2);
      if (flagOf(t, r, "Strength")) h.sportStrength = clampStat(h.sportStrength + 2);
      h.sportFitness = clampStat(h.sportFitness + fit);
      add("Fitness is up " + std::to_string(fit) + " to " + std::to_string(h.sportFitness) + ".");
      if (risk > 0 && sportProRng_.chance(0.02 * double(risk))) {
        add("I felt a twinge and shut it down early.");
        h.sportFitness = clampStat(h.sportFitness - 2);
      }
      mark("a16_sports-training-scenarios-enc", int(r));
      happy += 1;
    }
  }

  // ---- the season without a story of its own ----
  if (title.empty()) {
    title = "The season";
    const std::string club = h.team.empty() ? std::string("the team") : resolve(h.team);
    body = "A year of " + h.sport + " with " + club + ".";
    say("I played " + std::to_string(games) + " games for " + club + " and won "
        + std::to_string(wins));
  }

  // ---- the age curve, then retirement ----
  if (h.age >= 30)
    h.sportSkill = clampStat(h.sportSkill - 1 - (h.age - 30) / 4 - int(sportProRng_.below(2)));
  h.sportGreatness = clampStat(h.sportGreatness
      + (winPct >= 0.7 ? 4 : (winPct >= 0.5 ? 2 : -1)) + h.sportSkill / 25);
  h.sportRespect = clampStat(h.sportRespect + (winPct >= 0.5 ? 1 : -1));
  if (h.age >= 28) h.sportFitness = clampStat(h.sportFitness - 1);
  h.sportFame = int(numOf(sp, spRow, "Fame Score")) * 4 + h.sportGreatness / 4;

  if (h.sportPro && h.age >= 33 && (h.sportSkill < 15 || sportProRng_.chance(0.12))) {
    const bool hall = h.sportGreatness >= 80 || h.sportAwards >= 5 || h.sportSeasons >= 12;
    h.sportPro = false;
    h.athlete = false;
    h.sportHall = hall;
    title = hall ? "The Hall of Fame" : "The end of the road";
    body = hall ? ("After " + std::to_string(h.sportSeasons) + " seasons the Hall called.")
                : ("After " + std::to_string(h.sportSeasons) + " seasons, that was it.");
    line = spyUpperFirst(hall ? ("I retired after " + std::to_string(h.sportSeasons)
                                  + " seasons and went into the Hall of Fame")
                               : ("I retired after " + std::to_string(h.sportSeasons) + " seasons"));
    mark("a16_sports-enc", int(spRow));
    if (!h.sportAwardNames.empty()) {
      std::string list;
      for (size_t i = 0; i < h.sportAwardNames.size() && i < 4; ++i) {
        if (!list.empty()) list += ", ";
        list += h.sportAwardNames[i];
      }
      add("The trophy case: " + list + ".");
    }
    happy += hall ? 6 : 1;
  }

  out.table = tableRef;
  out.row_id = std::to_string(cardRow);
  out.title = title;
  out.body = spyUpperFirst(body);   // a card opening on <he/she> arrives lower-case
  out.narrative = line;
  const std::vector<std::string> keep = out.extra;
  out.extra.clear();
  for (const std::string& s : keep) out.extra.push_back(s);
  for (const std::string& s : extra) out.extra.push_back(s);
  out.happiness += happy;
  return true;
}

// The two injury menu cards (REPORT.en.md 394). The literal table has the whole row for
// each: body #21923 "Go to physical therapy for a sports injury today!" with the answer
// #21922 "Go to physical therapy", and body #44719 "Undergo surgery for a sports injury
// today!" with the answer #44718 "Undergo surgery". The native arithmetic behind either
// was not recovered, so the port models the two obvious effects and says so below
// (shortcut).
bool Sim::sportTherapyYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.sportInjured || h.sportInjuryClearAge >= 0) return false;
  if (h.age < 16 || h.inPrison) return false;
  if (h.sportTherapyAge >= 0 && h.age - h.sportTherapyAge < 1) return false;
  if (forcedChoice < 0 && !sportInjuryRng_.chance(0.50)) return false;
  const std::vector<std::string> extra = out.extra;
  out = AgeUp();
  out.extra = extra;
  h.sportTherapyAge = h.age;
  // A year of therapy is worth a year of rehab, and the fitness the layoff cost.
  if (h.sportInjuryYears > 0) h.sportInjuryYears -= 1;
  h.sportFitness = h.sportFitness > 100 - 2 ? 100 : h.sportFitness + 2;
  const bool healed = h.sportInjuryYears <= 0;
  if (healed) {
    h.sportInjured = false;
    h.sportInjuryClearAge = h.age;
  }
  out.table = "registry:ScenarioSportsPhysicalTherapy";
  out.title = "Physical Therapy";
  out.body = "Go to physical therapy for a sports injury today!";  // #21923
  out.body += "\n\nInjury: " + h.sportInjury + "\nRehab Left: " +
              std::to_string(h.sportInjuryYears) + (h.sportInjuryYears == 1 ? " year" : " years");
  out.options = {"Go to physical therapy"};  // #21922
  out.choice = out.options[0];
  out.narrative = healed
      ? "A year of physical therapy finally cleared up my " + h.sportInjury + "."
      : "I spent the year in physical therapy for my " + h.sportInjury + ".";
  return true;
}

bool Sim::sportSurgeryYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.sportInjured || !h.sportDebilitating) return false;
  if (h.age < 16 || h.inPrison) return false;
  if (h.sportSurgeryAge >= 0 && h.age - h.sportSurgeryAge < 3) return false;
  if (forcedChoice < 0 && !sportInjuryRng_.chance(0.50)) return false;
  const std::vector<std::string> extra = out.extra;
  out = AgeUp();
  out.extra = extra;
  h.sportSurgeryAge = h.age;
  // shortcut: the native success rate and recovery schedule were not recovered. Two
  // thirds of these operations take; a failed one costs another step of skill.
  const bool worked = forcedChoice >= 0 ? forcedChoice == 0 : sportInjuryRng_.chance(0.66);
  if (worked) {
    h.sportInjured = false;
    h.sportDebilitating = false;
    h.sportInjuryYears = 0;
    h.sportInjuryClearAge = h.age;
  } else {
    h.sportSkill = h.sportSkill > 4 ? h.sportSkill - 2 : 0;
  }
  out.table = "registry:ScenarioSportsSurgery";
  out.title = "Surgery";
  out.body = "Undergo surgery for a sports injury today!";  // #44719
  out.body += "\n\nInjury: " + h.sportInjury;
  out.options = {"Undergo surgery"};  // #44718
  out.choice = out.options[0];
  out.narrative = worked
      ? "Surgery repaired my " + h.sportInjury + " and I am cleared to train again."
      : "The surgery on my " + h.sportInjury + " did not take and cost me more strength.";
  return true;
}
// Test hook (REPORT.en.md 394): a script spec can hand the hero an injury so the two
// injury cards can be driven without waiting for a season to roll one.
void Sim::seedSportInjury(Life& life, bool debilitating) {
  Hero& h = life.hero;
  if (!h.sportPro) makeAthlete(life);
  h.sportInjured = true;
  h.sportInjury = debilitating ? "torn ACL" : "sprained ankle";
  h.sportInjurySeverity = debilitating ? 8 : 3;
  h.sportInjuryYears = debilitating ? 3 : 2;
  h.sportInjuryPain = debilitating ? 8 : 3;
  h.sportDebilitating = debilitating;
}

bool Sim::sportsYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.athlete) return false;
  hero_ = &h;
  life_ = &life;
  if (h.age < 18) return false;
  if (!ensureTable("a16_dumb-scenarios-sports-enc")) return false;
  if (!sportsRng_.chance(0.45)) return false;
  const Table& t = tbl("a16_dumb-scenarios-sports-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < t.size(); ++r) {
    if (!t.has("Hero Min Age") || h.age >= int(t.num(r, "Hero Min Age"))) pool.push_back(r);
  }
  if (pool.empty()) return false;
  return renderRow(t, pool[sportsRng_.below(uint32_t(pool.size()))], life, out, forcedChoice);
}

void Sim::makeAthlete(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!ensureTable("a16_sports-enc")) return;
  const Table& sp = tbl("a16_sports-enc");
  if (!sp.size()) return;
  h.sport = trim(sp.at(sportsRng_.below(uint32_t(sp.size())), "Sport"));
  if (h.sport.empty()) return;
  h.athlete = true;
  h.sportPro = true;
  h.sportSkill = 20 + int(sportsRng_.below(21));
  h.sportGreatness = 5 + int(sportsRng_.below(16));
  h.sportRespect = 15 + int(sportsRng_.below(21));
  h.sportFitness = 35 + int(sportsRng_.below(21));
  h.sportBalance = 30 + int(sportsRng_.below(31));
  h.sportEndurance = 30 + int(sportsRng_.below(31));
  h.sportFlexibility = 30 + int(sportsRng_.below(31));
  h.sportStrength = 30 + int(sportsRng_.below(31));
  h.sportSeasons = 0;
  const std::string gender = h.female ? "female" : "male";
  const auto pickWhere = [this](const char* table, const char* sportCol, const std::string& want,
                                const char* nameCol) {
    ensureTable(table);
    const Table& t = tbl(table);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has(sportCol) && lower(trim(t.at(r, sportCol))) != want) continue;
      pool.push_back(r);
    }
    if (pool.empty()) {
      for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    }
    if (pool.empty()) return std::string();
    return trim(t.at(pool[sportsRng_.below(uint32_t(pool.size()))], nameCol));
  };
  // Leagues are per sport and per gender; teams are per league index, so the
  // league name is stored on the hero and the team is drawn from the same sport.
  if (ensureTable("a16_sports-leagues-enc")) {
    const Table& lg = tbl("a16_sports-leagues-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < lg.size(); ++r) {
      if (lower(trim(lg.at(r, "Sport"))) != h.sport) continue;
      if (lg.has("Gender") && lower(trim(lg.at(r, "Gender"))) != gender) continue;
      pool.push_back(r);
    }
    if (pool.empty()) {
      for (size_t r = 0; r < lg.size(); ++r) pool.push_back(r);
    }
    if (!pool.empty()) h.league = trim(lg.at(pool[sportsRng_.below(uint32_t(pool.size()))], "Name"));
  }
  h.team = pickWhere("a16_sports-teams-enc", "Sport", h.sport, "Name");
  h.position = pickWhere("a16_sports-positions-enc", "Sport", h.sport, "Position");
  h.employed = true;
  if (!h.position.empty()) h.job = h.position;
}

}  // namespace bl
