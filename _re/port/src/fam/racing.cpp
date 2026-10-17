// src/fam/racing.cpp -- the car and the races (REPORT.en.md 75, 129).
//
// Split out of src/sim.cpp so the family can be worked on without touching the
// twenty-eight thousand lines around it. Nothing here is shared: the only calls
// that leave this file are the text helpers in bl/text.h and the Sim members
// declared in bl/sim.h.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>
#include <vector>

namespace bl {

// ===== The car and the races (REPORT.en.md 75) =====

// 'Bronze' .. 'Platinum', the a16_racing-race-track-enc 'Unlock Rank' and the
// outcome pools' 'Race Level'.
static std::string racRankName(int rank) {
  static const char* kRanks[] = {"Bronze", "Silver", "Gold", "Platinum"};
  if (rank < 0) rank = 0;
  if (rank > 3) rank = 3;
  return kRanks[rank];
}

static const char* const kRacStreetTypes[] = {"Street", "Road", "Avenue", "Boulevard", "Drive", "Lane"};

// One car out of a16_car-models-enc. 'Cost' is a "$44,990" string, which
// Table::num already reads as 44990. The three racing stats come from the
// class ladder (a16_racing-car-class-stat-enc / -stats-enc), so a slower car
// class really is slower.
void Sim::racingBuyCar(long long budget) {
  Hero* h = hero_ == nullptr ? nullptr : const_cast<Hero*>(hero_);
  if (h == nullptr) return;
  if (!ensureTable("a16_car-models-enc")) return;
  const Table& cm = tbl("a16_car-models-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < cm.size(); ++r) {
    if (cm.flag(r, "Unavailable") || cm.flag(r, "Luxury Only") || cm.flag(r, "Race Car Dealership Only")) continue;
    if (cm.num(r, "Cost") > double(budget)) continue;
    pool.push_back(r);
  }
  const size_t row = pool.empty() ? weightedPick(cm, "Cool Score", racingRng_)
                                  : weightedPickOf(cm, "Cool Score", pool, racingRng_);
  if (row == size_t(-1)) return;
  h->carMake = trim(cm.at(row, "Make"));
  h->carModel = trim(cm.at(row, "Model"));
  h->carType = trim(cm.at(row, "Type"));
  h->carName = h->carMake + " " + h->carModel;
  h->carClass = trim(cm.at(row, "Racing Class"));
  if (h->carClass.empty()) h->carClass = "C";
  h->carDurability = int(cm.num(row, "Durability Score"));
  h->carValue = static_cast<long long>(cm.num(row, "Cost"));
  h->carMods = 0;
  std::vector<std::string> cats;
  splitOn(cm.at(row, "Race Type Whitelist"), ',', cats);
  h->racerCategory = (cats.empty() || trim(cats[0]).empty()) ? std::string("street") : trim(cats[0]);
  const char* statTables[2] = {"a16_racing-car-class-stat-enc", "a16_racing-car-class-stats-enc"};
  const char* tab = statTables[h->racerRank & 1];
  h->carHandling = h->carSpeed = h->carAcceleration = 0;
  if (ensureTable(tab)) {
    const Table& cs = tbl(tab);
    for (size_t r = 0; r < cs.size(); ++r) {
      if (trim(cs.at(r, "Class")) != h->carClass) continue;
      h->carHandling = int((cs.num(r, "Min Base Handling") + cs.num(r, "Max Base Handling")) / 2);
      h->carSpeed = int((cs.num(r, "Min Base Speed") + cs.num(r, "Max Base Speed")) / 2);
      h->carAcceleration = int((cs.num(r, "Min Base Acceleration") + cs.num(r, "Max Base Acceleration")) / 2);
      break;
    }
  }
  if (h->carHandling <= 0) { h->carHandling = 15; h->carSpeed = 15; h->carAcceleration = 15; }
}

void Sim::makeRacing(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.racing = true;
  h.racerRank = 0;
  h.racerWins = 0;
  h.racerLosses = 0;
  h.racerWinnings = 0;
  h.racerSpend = 0;
  h.racerFans = 0;
  h.racerProfessionalism = 40 + int(racingRng_.below(21));
  h.carCondition = 100;
  h.racerRivalCraziness = 30 + int(racingRng_.below(51));
  racingBuyCar(90000);
  if (h.money >= h.carValue) h.money -= h.carValue;
  h.racerSpend += h.carValue;
  h.racerRival = racingToken("racer");
  h.racerMechanic = personName("mechanic");
  if (h.racerMechanic.empty()) h.racerMechanic = "the mechanic";
  if (ensureTable("a16_racing-race-sponsor-enc")) {
    const Table& sp = tbl("a16_racing-race-sponsor-enc");
    const size_t row = weightedPick(sp, "Money Score", racingRng_);
    if (row != size_t(-1)) {
      std::string name = resolve(trim(sp.at(row, "Name")));
      if (sp.flag(row, "Use Definite Article")) name = "the " + name;
      h.racerSponsor = name;
    }
  }
  if (h.racerSponsor.empty()) h.racerSponsor = "a local sponsor";
  if (ensureTable("a16_racing-cup-name-enc")) {
    const Table& cp = tbl("a16_racing-cup-name-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < cp.size(); ++r) {
      const std::string c = lower(trim(cp.at(r, "Racing Category")));
      if (!c.empty() && c != lower(h.racerCategory)) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const std::string name = resolve(trim(cp.at(weightedPickOf(cp, "Commonness Score", pool, racingRng_), "Name")));
      h.racerCup = name.empty() ? ("the " + h.racerCategory + " cup") : name + " Cup";
    }
  }
  if (h.racerCup.empty()) h.racerCup = "the " + h.racerCategory + " cup";
  if (ensureTable("a16_racing-race-track-enc")) {
    const Table& tr = tbl("a16_racing-race-track-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < tr.size(); ++r) {
      if (lower(trim(tr.at(r, "Track Type"))) != lower(h.racerCategory)) continue;
      if (lower(trim(tr.at(r, "Unlock Rank"))) != "bronze") continue;
      pool.push_back(r);
    }
    if (!pool.empty())
      h.racerTrack = trim(tr.at(weightedPickOf(tr, "Difficulty", pool, racingRng_), "Track Name"));
  }
  if (h.racerTrack.empty()) h.racerTrack = "the local track";
  if (ensureTable("a16_racing-race-car-garage-street-name-enc")) {
    const Table& g = tbl("a16_racing-race-car-garage-street-name-enc");
    const size_t row = weightedPick(g, "Commonness Score", racingRng_);
    if (row != size_t(-1)) {
      std::string street = resolve(trim(g.at(row, "Street Name")));
      if (g.flag(row, "Add Random Street Type"))
        street += " " + std::string(kRacStreetTypes[racingRng_.below(6)]);
      h.racerGarage = street;
    }
  }
  if (h.racerGarage.empty()) h.racerGarage = "the shop";
  if (ensureTable("a16_racing-pit-crew-name-enc")) {
    const Table& p = tbl("a16_racing-pit-crew-name-enc");
    const size_t row = weightedPick(p, "Commonness Score", racingRng_);
    if (row != size_t(-1)) h.racerPitCrew = resolve(trim(p.at(row, "Name")));
  }
  if (h.racerPitCrew.empty()) h.racerPitCrew = "my pit crew";
}

std::string Sim::racingToken(const std::string& token) {
  Hero* h = hero_ == nullptr ? nullptr : const_cast<Hero*>(hero_);
  if (h == nullptr) return std::string();
  std::string key = trim(token);
  bool poss = false;
  static const std::string suffix = "-possessive";
  if (key.size() > suffix.size() && key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0) {
    key.resize(key.size() - suffix.size());
    poss = true;
  }
  const auto cached = [&](const char* slot) -> std::string {
    const std::map<std::string, std::string>::const_iterator it = racingCache_.find(slot);
    return it == racingCache_.end() ? std::string() : it->second;
  };
  const auto remember = [&](const char* slot, const std::string& v) { racingCache_[slot] = v; };
  const auto own = [&](const std::string& s) -> std::string {
    if (s.empty()) return s;
    return poss ? llPossessive(s) : s;
  };
  const auto freshRacer = [&]() -> std::string {
    if (!ensureTable("a16_racing-racer-name-enc")) return std::string();
    const Table& rn = tbl("a16_racing-racer-name-enc");
    const size_t row = weightedPick(rn, "", racingRng_);
    if (row == size_t(-1)) return std::string();
    return trim(rn.at(row, "First Name")) + " " + trim(rn.at(row, "Last Name"));
  };
  if (key == "car") {
    std::string s = cached("car");
    if (s.empty()) {
      if (h->carName.empty()) racingBuyCar(90000);
      s = h->carName.empty() ? std::string("car") : h->carName;
      remember("car", s);
    }
    return own(s);
  }
  if (key == "racer" || key == "racer-rival") {
    const char* slot = (key == "racer-rival") ? "racer-rival" : "racer";
    std::string s = cached(slot);
    if (s.empty()) {
      if (key == "racer-rival") s = h->racerRival;
      if (s.empty()) s = freshRacer();
      if (s.empty()) s = "the other racer";
      remember(slot, s);
    }
    if (key == "racer-rival") h->racerRival = s;
    return own(s);
  }
  if (key == "mechanic") {
    std::string s = cached("mechanic");
    if (s.empty()) {
      s = h->racerMechanic;
      if (s.empty()) s = personName("mechanic");
      if (s.empty()) s = "the mechanic";
      remember("mechanic", s);
    }
    return own(s);
  }
  if (key == "pit-crew") return own(h->racerPitCrew.empty() ? std::string("my pit crew") : h->racerPitCrew);
  if (key == "passenger") {
    std::string s = cached("passenger");
    if (s.empty()) {
      if (h->relationshipOn && !h->loverName.empty()) s = h->loverName;
      else {
        s = personName("friend");
        if (s.empty()) s = "a friend";
      }
      remember("passenger", s);
    }
    return own(s);
  }
  if (key == "victim") {
    std::string s = cached("victim");
    if (s.empty()) {
      s = h->racerRival.empty() ? freshRacer() : h->racerRival;
      if (s.empty()) s = "the other racer";
      remember("victim", s);
    }
    return own(s);
  }
  return std::string();
}

// One choice card out of the family's pools. The racing pools carry five impact
// columns the generic renderRow does not know (Vehicle Condition, Enjoyment,
// Relationship, Risk, Fault) and their pivotal moments are decided by the car
// rather than by an outcome table, so those two shapes are read here.
bool Sim::racingCardRow(Life& life, AgeUp& out, const std::string& table, size_t row,
                        int forcedChoice, bool moment, int* success) {
  Hero& h = life.hero;
  if (!ensureTable(table)) return false;
  const Table& t = tbl(table.c_str());
  if (row >= t.size()) return false;
  hero_ = &h;
  life_ = &life;
  const bool live0 = racingLive_;
  racingLive_ = true;
  const std::vector<std::string> keep = out.extra;
  out = AgeUp();
  out.extra = keep;
  out.table = t.name();
  out.row_id = std::to_string(row + 1);
  // Every pool in this family is written about somebody else at the wheel:
  // '<racer>', the passenger or the other driver. The pronoun pair follows them
  // while the row renders, and the hero stays 'I'.
  const bool oS = pronounOtherSet_, fS = pronounOtherFemale_;
  pronounOtherSet_ = true;
  pronounOtherFemale_ = racingRng_.chance(0.5);
  out.title = resolve(t.has("Title") ? t.at(row, "Title") : std::string());
  out.body = resolve(t.has("Body") ? t.at(row, "Body") : t.at(row, "Scenario"));
  out.cta = t.has("CTA") ? resolve(t.at(row, "CTA")) : std::string();
  std::vector<int> choices;
  for (int i = 1; i <= 4; ++i) {
    const std::string col = "Choice " + std::to_string(i);
    if (t.has(col) && !trim(t.at(row, col)).empty()) choices.push_back(i);
  }
  if (choices.empty()) {
    pronounOtherSet_ = oS;
    pronounOtherFemale_ = fS;
    racingLive_ = live0;
    return !out.title.empty() || !out.body.empty();
  }
  const int pick = forcedChoice >= 0 && size_t(forcedChoice) < choices.size()
                       ? choices[size_t(forcedChoice)]
                       : choices[racingRng_.below(uint32_t(choices.size()))];
  const std::string sfx = " " + std::to_string(pick);
  out.choice = resolve(t.at(row, "Choice" + sfx));
  const std::string statCol = "Choice" + sfx + " Stat";
  if (moment && t.has(statCol)) {
    const std::string want = lower(trim(t.at(row, statCol)));
    int value = (h.carHandling + h.carSpeed + h.carAcceleration) / 3;
    if (want.find("handling") != std::string::npos) value = h.carHandling;
    else if (want.find("acceleration") != std::string::npos) value = h.carAcceleration;
    else if (want.find("speed") != std::string::npos) value = h.carSpeed;
    else if (want.find("durability") != std::string::npos) value = h.carDurability;
    double odds = 0.35 + value / 160.0 + (h.racerProfessionalism - 50) / 300.0 +
                  (h.carCondition - 60) / 400.0;
    odds = std::max(0.15, std::min(0.9, odds));
    const bool ok = racingRng_.unit() < odds;
    if (success != nullptr) *success = ok ? 1 : 0;
    out.narrative = resolve(t.at(row, (ok ? "Success Narrative" : "Failure Narrative") + sfx));
    const std::string fb = t.has((ok ? "Success Feedback" : "Failure Feedback") + sfx)
                               ? resolve(t.at(row, (ok ? "Success Feedback" : "Failure Feedback") + sfx))
                               : std::string();
    // The Feedback column is the card's own second-person text; the log line
    // is the first-person Narrative. toFirstPerson only rewrites a leading
    // subject, so the card text stays on the card.
    if (!fb.empty()) out.body += " " + spyUpperFirst(fb);
  } else {
    out.narrative = resolve(t.at(row, "Narrative" + sfx));
    if (success != nullptr) *success = -1;
  }
  out.karma = pointsForImpact(int(t.num(row, "Karma Impact" + sfx)));
  out.discipline = pointsForImpact(int(t.num(row, "Discipline Impact" + sfx)));
  out.happiness = pointsForImpact(int(t.num(row, "Happiness Impact" + sfx)));
  out.smarts = pointsForImpact(int(t.num(row, "Intelligence Impact" + sfx)));
  out.looks = pointsForImpact(int(t.num(row, "Appearance Impact" + sfx)));
  const int cond = int(t.num(row, "Vehicle Condition Impact" + sfx));
  if (cond != 0) h.carCondition = std::max(0, std::min(100, h.carCondition + cond));
  const int joy = int(t.num(row, "Enjoyment Impact" + sfx));
  if (joy != 0) out.happiness += joy;
  const int fault = int(t.num(row, "Fault Score" + sfx));
  const int risk = int(t.num(row, "Risk Score" + sfx));
  if (risk > 0 && racingRng_.chance(risk * 0.08)) {
    h.carCondition = std::max(0, h.carCondition - 10);
    out.extra.push_back("The " + (h.carName.empty() ? std::string("car") : h.carName) +
                        " took a knock in the process.");
  }
  if (fault >= 4) h.racerProfessionalism = std::max(0, h.racerProfessionalism - 4);
  pronounOtherSet_ = oS;
  pronounOtherFemale_ = fS;
  racingLive_ = live0;
  return true;
}

bool Sim::racingYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.racing) return false;
  if (h.age < 16) return false;
  if (!ensureTable("a16_racing-race-track-enc")) return false;
  hero_ = &h;
  life_ = &life;

  std::string title, body, line;
  std::vector<std::string> extra;
  std::string tableRef, rowRef;
  int happy = 0;
  long long take = 0;

  const auto say = [&](const std::string& s) { if (line.empty() && !s.empty()) line = spyUpperFirst(s); };
  const auto add = [&](const std::string& s) { if (!s.empty()) extra.push_back(spyUpperFirst(s)); };
  const auto cell = [&](const Table& t, size_t r, const char* col) -> std::string {
    if (r == size_t(-1) || !t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  const auto mark = [&](const char* tab, size_t r) { tableRef = tab; rowRef = std::to_string(r + 1); };
  const auto withBrackets = [&](const Table& t, size_t r, const char* wantCol) -> std::string {
    if (r == size_t(-1)) return std::string();
    for (const std::string& cc : t.columns()) {
      if (cc == wantCol) continue;
      if (t.at(r, cc).find('<') == std::string::npos) continue;
      cell(t, r, cc.c_str());
    }
    return cell(t, r, wantCol);
  };
  const auto flush = [&]() {
    for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
    extra.clear();
  };
  const auto spend = [&](long long n) -> bool {
    if (n <= 0 || h.money < n) return false;
    h.money -= n;
    h.racerSpend += n;
    return true;
  };
  const auto bump = [&](int& stat, int n) { stat = std::max(0, std::min(100, stat + n)); };
  const auto statValue = [&](const std::string& want) -> int {
    const std::string s = lower(trim(want));
    if (s.find("handling") != std::string::npos) return h.carHandling;
    if (s.find("acceleration") != std::string::npos) return h.carAcceleration;
    if (s.find("speed") != std::string::npos) return h.carSpeed;
    if (s.find("durability") != std::string::npos) return h.carDurability;
    if (s.find("condition") != std::string::npos) return h.carCondition;
    return (h.carHandling + h.carSpeed + h.carAcceleration) / 3;
  };
  // A pool row's own text with the pronoun pair pointed at somebody else.
  const auto scene = [&](const Table& t, size_t r, bool otherFemale, const char* wantCol) -> std::string {
    if (r == size_t(-1)) return std::string();
    const bool oSave = pronounOtherSet_, fSave = pronounOtherFemale_;
    pronounOtherSet_ = true;
    pronounOtherFemale_ = otherFemale;
    std::string s = withBrackets(t, r, wantCol);
    pronounOtherSet_ = oSave;
    pronounOtherFemale_ = fSave;
    return s;
  };

  const bool live0 = racingLive_;
  racingLive_ = true;

  if (h.racerCategory.empty()) h.racerCategory = "street";
  if (h.racerMechanic.empty()) h.racerMechanic = personName("mechanic");
  if (h.racerRival.empty()) h.racerRival = racingToken("racer");
  if (h.racerTrack.empty()) h.racerTrack = "the local track";
  if (h.racerGarage.empty()) h.racerGarage = "the shop";
  if (h.racerPitCrew.empty()) h.racerPitCrew = "my pit crew";
  if (h.racerSponsor.empty()) h.racerSponsor = "a local sponsor";
  if (h.racerCup.empty()) h.racerCup = "the " + h.racerCategory + " cup";
  if (h.carName.empty()) racingBuyCar(90000);
  if (h.carName.empty()) { racingLive_ = live0; return false; }
  // A year on the road wears the car down, and a car that is falling apart goes
  // to the shop before anything else happens.
  h.carCondition = std::max(0, h.carCondition - int(racingRng_.below(8)));
  if (h.carCondition < 25) {
    const long long bill = 400 + 60 * (100 - h.carCondition);
    if (spend(bill)) {
      bump(h.carCondition, 40);
      add("A repair at " + h.racerGarage + " cost me " + money(bill) + ".");
    }
  }
  h.racerFans += (h.racerWins > h.racerLosses) ? 1 : 0;
  if (!racingRng_.chance(0.62)) { racingLive_ = live0; return false; }

  // (1) A pink slip race: the papers against a rival's car (0.02).
  if (title.empty() && racingRng_.chance(0.02) &&
      ensureTable("a16_racing-pink-slip-refusal-string-enc")) {
    const Table& t = tbl("a16_racing-pink-slip-refusal-string-enc");
    const bool badCondition = h.carCondition < 60;
    const bool badGrade = h.racerRank <= 0;
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      const std::string reason = lower(trim(t.at(r, "Reason")));
      if (reason.find("car condition") != std::string::npos && !badCondition) continue;
      if (reason.find("car grade") != std::string::npos && !badGrade) continue;
      pool.push_back(r);
    }
    const bool accept = forcedChoice >= 0 ? forcedChoice == 0
                                          : racingRng_.chance((badCondition || badGrade) ? 0.15 : 0.6);
    title = "A pink slip race";
    body = "Someone wants to race me for the " + h.carName + ".";
    if (!accept && !pool.empty()) {
      const size_t r = weightedPickOf(t, "Commonness Score", pool, racingRng_);
      line = spyUpperFirst(scene(t, r, false, "String"));
      mark("a16_racing-pink-slip-refusal-string-enc", r);
    } else {
      double odds = 0.5 + (h.carHandling + h.carSpeed + h.carAcceleration - 90) / 300.0 -
                    h.racerRivalCraziness / 400.0 + (h.carCondition - 60) / 250.0;
      odds = std::max(0.15, std::min(0.9, odds));
      if (!pool.empty()) mark("a16_racing-pink-slip-refusal-string-enc", pool[0]);
      if (racingRng_.unit() < odds) {
        const long long won = h.carValue + 5000 + static_cast<long long>(racingRng_.below(20000));
        h.money += won;
        take += won;
        h.racerWinnings += won;
        h.racerWins += 1;
        happy += 4;
        line = "I won the pink slip race and sold the loser's car for " + money(won) + ".";
      } else {
        const long long lost = h.carValue;
        h.racerLosses += 1;
        happy -= 5;
        racingBuyCar(20000);
        line = "I lost the " + h.carName + " on a pink slip and had to buy a beater.";
        if (lost > 0) add("That car was worth " + money(lost) + ".");
      }
    }
  }

  // (2) The rival gets to the hero's car (0.03).
  if (title.empty() && racingRng_.chance(0.03) &&
      ensureTable("a16_racing-rival-sabotage-string-enc")) {
    const Table& t = tbl("a16_racing-rival-sabotage-string-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      const int lo = int(t.num(r, "Min Rival Craziness"));
      const int hi = int(t.num(r, "Max Rival Craziness"));
      if (lo > 0 && h.racerRivalCraziness < lo) continue;
      if (hi > 0 && h.racerRivalCraziness > hi) continue;
      pool.push_back(r);
    }
    const size_t r = pool.empty() ? size_t(-1) : weightedPickOf(t, "", pool, racingRng_);
    if (r != size_t(-1)) {
      const int impact = int(t.num(r, "Vehicle Condition Impact"));
      title = "Someone got to my car";
      body = "The " + h.carName + " is not driving right.";
      line = spyUpperFirst(scene(t, r, false, "Narrative Version"));
      const std::string part = lower(trim(t.at(r, "Damaged Part")));
      if (!part.empty()) add("The " + part + " is damaged.");
      h.carCondition = std::max(0, std::min(100, h.carCondition + impact * 6));
      happy -= 2;
      mark("a16_racing-rival-sabotage-string-enc", r);
    }
  }

  // (3) The hero gets to the rival's car (0.05).
  if (title.empty() && racingRng_.chance(0.05) &&
      ensureTable("a16_racing-racer-sabotage-method-enc")) {
    const Table& t = tbl("a16_racing-racer-sabotage-method-enc");
    const size_t r = weightedPick(t, "Commonness Score", racingRng_);
    if (r != size_t(-1)) {
      racingCache_["victim"] = h.racerRival;
      title = "A dirty trick";
      body = h.racerRival + " has had the better car all season.";
      const double odds = std::max(0.15, std::min(0.9, 0.85 - t.num(r, "Risk Score") * 0.08 +
                                                           (h.racerProfessionalism - 50) / 250.0));
      if (racingRng_.unit() < odds) {
        line = spyUpperFirst(scene(t, r, false, "Success Log Version"));
        add(scene(t, r, false, "News Report String"));
        h.racerRivalCraziness += 10;
        happy += 2;
      } else {
        line = spyUpperFirst("I was caught " + cell(t, r, "Target Discovers Hero Snippet") + ".");
        h.racerProfessionalism = std::max(0, h.racerProfessionalism - 6);
        h.racerRivalCraziness += 15;
        happy -= 3;
      }
      mark("a16_racing-racer-sabotage-method-enc", r);
    }
  }

  // (4) Pulled over (0.05).
  if (title.empty() && racingRng_.chance(0.05) &&
      ensureTable("a16_racing-drive-police-pull-over-color-enc")) {
    const Table& t = tbl("a16_racing-drive-police-pull-over-color-enc");
    const bool tooFast = racingRng_.chance(0.6);
    const std::string gender = heroToken("gender", false);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (tooFast ? t.flag(r, "Hero Too Slow") : t.flag(r, "Hero Too Fast")) continue;
      const std::string g = lower(trim(t.at(r, "Hero Gender")));
      if (!g.empty() && g != gender) continue;
      const double lo = t.num(r, "Min Professionalism"), hi = t.num(r, "Max Professionalism");
      if (lo > 0 && h.racerProfessionalism < lo) continue;
      if (hi > 0 && h.racerProfessionalism > hi) continue;
      pool.push_back(r);
    }
    const size_t r = pool.empty() ? size_t(-1) : weightedPickOf(t, "", pool, racingRng_);
    if (r != size_t(-1)) {
      title = "Pulled over";
      body = cell(t, r, "Scenario");
      line = "An officer pulled me over.";
      mark("a16_racing-drive-police-pull-over-color-enc", r);
      if (racingRng_.chance(0.6) && ensureTable("a16_racing-drive-police-warning-enc")) {
        const Table& w = tbl("a16_racing-drive-police-warning-enc");
        std::vector<size_t> wp;
        for (size_t x = 0; x < w.size(); ++x) {
          if (tooFast ? w.flag(x, "Hero Going Too Slow") : w.flag(x, "Hero Going Too Fast")) continue;
          wp.push_back(x);
        }
        if (!wp.empty()) {
          const size_t x = weightedPickOf(w, "", wp, racingRng_);
          line = spyUpperFirst(withBrackets(w, x, "Log Version"));
          mark("a16_racing-drive-police-warning-enc", x);
        }
      }
      h.racerProfessionalism = std::max(0, h.racerProfessionalism - 2);
    }
  }

  // (5) The mechanic's work (0.03).
  if (title.empty() && racingRng_.chance(0.03) && ensureTable("a16_racing-bad-mechanic-color-enc")) {
    const Table& t = tbl("a16_racing-bad-mechanic-color-enc");
    const size_t r = weightedPick(t, "Commonness Score", racingRng_);
    if (r != size_t(-1)) {
      h.racerMechanic = personName("mechanic");
      if (h.racerMechanic.empty()) h.racerMechanic = "the mechanic";
      title = "The mechanic's work";
      body = "I took the " + h.carName + " in to " + h.racerMechanic + ".";
      // The pool is written in the second person; the log line is the port's own.
      line = spyUpperFirst(toFirstPerson(resolve(trim(t.at(r, "String")))));
      h.carCondition = std::max(0, h.carCondition - 8);
      happy -= 1;
      mark("a16_racing-bad-mechanic-color-enc", r);
    }
  }

  // (6) A new part for the car (0.06).
  if (title.empty() && racingRng_.chance(0.06) && ensureTable("a16_racing-race-car-mod-enc")) {
    const Table& t = tbl("a16_racing-race-car-mod-enc");
    const size_t r = weightedPick(t, "Commonness Score", racingRng_);
    if (r != size_t(-1)) {
      const std::string part = trim(t.at(r, "Short Name"));
      const long long cost = static_cast<long long>(t.num(r, "Cost"));
      title = "A new part";
      std::string desc = spyUpperFirst(resolve(trim(t.at(r, "Descriptive String"))));
      if (!desc.empty() && desc[desc.size() - 1] != '.') desc += ".";
      body = h.racerGarage + " has a " + part + " for the " + h.carName + ". " + desc;
      if (spend(cost)) {
        bump(h.carHandling, int(t.num(r, "Handling Value Influence")));
        bump(h.carSpeed, int(t.num(r, "Speed Value Influence")));
        bump(h.carAcceleration, int(t.num(r, "Acceleration Value Influence")));
        bump(h.carDurability, int(t.num(r, "Durability Influence")));
        h.carMods += 1;
        h.carValue += cost;
        line = "I put a " + part + " on the " + h.carName + " for " + money(cost) + ".";
        happy += 2;
      } else {
        line = "The " + part + " costs " + money(cost) + " and I cannot afford it.";
      }
      mark("a16_racing-race-car-mod-enc", r);
    }
  }

  // (7) A racing jacket (0.08).
  if (title.empty() && racingRng_.chance(0.08) && ensureTable("a16_racing-racing-jacket-enc")) {
    const Table& t = tbl("a16_racing-racing-jacket-enc");
    const size_t r = weightedPick(t, "Commonness Score", racingRng_);
    if (r != size_t(-1)) {
      const std::string jacket = trim(t.at(r, "Name"));
      const long long lo = static_cast<long long>(t.num(r, "Min Value"));
      const long long hi = static_cast<long long>(t.num(r, "Max Value"));
      const long long cost = lo + (hi > lo ? static_cast<long long>(racingRng_.below(uint32_t(hi - lo + 1))) : 0);
      title = "A racing jacket";
      body = "A shop near " + h.racerGarage + " has a " + jacket + " in the window.";
      if (spend(cost)) {
        h.racerJacket = jacket;
        line = "I bought the " + jacket + " for " + money(cost) + ".";
        happy += 2;
      } else {
        line = "The " + jacket + " costs " + money(cost) + " and I cannot afford it.";
      }
      mark("a16_racing-racing-jacket-enc", r);
    }
  }

  // (8) A fan (0.09).
  if (title.empty() && racingRng_.chance(0.09) &&
      ensureTable("a16_racing-racing-fan-interaction-enc")) {
    const Table& t = tbl("a16_racing-racing-fan-interaction-enc");
    const bool positive = racingRng_.chance(0.65);
    const std::string gender = heroToken("gender", false);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.flag(r, "Is Positive") != positive) continue;
      const std::string g = lower(trim(t.at(r, "Hero Gender")));
      if (!g.empty() && g != gender) continue;
      pool.push_back(r);
    }
    const size_t r = pool.empty() ? size_t(-1) : weightedPickOf(t, "", pool, racingRng_);
    if (r != size_t(-1)) {
      const bool fanFemale = racingRng_.chance(0.5);
      title = positive ? "A fan" : "An angry fan";
      body = scene(t, r, fanFemale, "Scenario");
      line = scene(t, r, fanFemale, "Log Version Snippet");
      // One row of this pool writes a bare '<asked>' tag in its log column,
      // where its own sibling column spells the verb out (data typo), and that
      // same row's log cell is a fragment with no subject: give it the fan.
      for (size_t at = line.find("<asked>"); at != std::string::npos; at = line.find("<asked>"))
        line.replace(at, 7, "asked");
      if (!line.empty() && line[0] >= 'a' && line[0] <= 'z')
        line = (fanFemale ? std::string("She ") : std::string("He ")) + line;
      h.racerFans += positive ? 1 : -1;
      if (h.racerFans < 0) h.racerFans = 0;
      happy += positive ? 2 : -1;
      if (positive && racingRng_.chance(0.4) &&
          ensureTable("a16_racing-fan-encouragement-string-enc")) {
        const Table& f = tbl("a16_racing-fan-encouragement-string-enc");
        std::vector<size_t> fp;
        for (size_t x = 0; x < f.size(); ++x) {
          const std::string c = lower(trim(f.at(x, "Racing Category Referenced")));
          if (!c.empty() && c != lower(h.racerCategory)) continue;
          const std::string g = lower(trim(f.at(x, "Hero Gender")));
          if (!g.empty() && g != gender) continue;
          fp.push_back(x);
        }
        if (!fp.empty()) {
          const size_t x = weightedPickOf(f, "Commonness Score", fp, racingRng_);
          add(scene(f, x, fanFemale, "String"));
        }
      }
      mark("a16_racing-racing-fan-interaction-enc", r);
    }
  }

  // (9) A tune-up at the shop (0.10).
  if (title.empty() && racingRng_.chance(0.10) &&
      ensureTable("a16_racing-race-car-garage-street-name-enc")) {
    const Table& t = tbl("a16_racing-race-car-garage-street-name-enc");
    const size_t r = weightedPick(t, "Commonness Score", racingRng_);
    if (r != size_t(-1)) {
      std::string street = resolve(trim(t.at(r, "Street Name")));
      if (t.flag(r, "Add Random Street Type"))
        street += " " + std::string(kRacStreetTypes[racingRng_.below(6)]);
      h.racerGarage = street;
      const long long cost = 300 + 40 * (100 - h.carCondition);
      title = "A tune-up";
      body = "The car lives at " + street + " with " + h.racerPitCrew + ".";
      if (spend(cost)) {
        bump(h.carCondition, 30);
        line = "I paid " + money(cost) + " for a tune-up at " + street + ".";
        happy += 1;
      } else {
        line = "The tune-up at " + street + " costs " + money(cost) + " and I cannot afford it.";
      }
      mark("a16_racing-race-car-garage-street-name-enc", r);
    }
  }

  // (10) The rival has something to say (0.12).
  if (title.empty() && racingRng_.chance(0.12) && ensureTable("a16_racing-racer-insult-enc")) {
    const Table& t = tbl("a16_racing-racer-insult-enc");
    const size_t r = pickRow("a16_racing-racer-insult-enc", "", "Gender",
                             heroToken("gender", false), "", racingRng_);
    if (r != size_t(-1)) {
      title = "Words with " + h.racerRival;
      body = h.racerRival + " had something to say about the " + h.carName + ".";
      line = h.racerRival + " called me " + resolve(trim(t.at(r, "Insult"))) + ".";
      happy -= 1;
      mark("a16_racing-racer-insult-enc", r);
    }
  }

  // (11) Somebody at the track has a crush (0.12).
  if (title.empty() && racingRng_.chance(0.12) &&
      ensureTable("a16_racing-racer-love-interest-string-enc")) {
    const Table& t = tbl("a16_racing-racer-love-interest-string-enc");
    const std::string gender = heroToken("gender", false);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      const std::string g = lower(trim(t.at(r, "Hero Gender")));
      if (!g.empty() && g != gender) continue;
      const double lo = t.num(r, "Hero Min Age"), hi = t.num(r, "Hero Max Age");
      if (lo > 0 && h.age < lo) continue;
      if (hi > 0 && h.age > hi) continue;
      pool.push_back(r);
    }
    const size_t r = pool.empty() ? size_t(-1) : weightedPickOf(t, "", pool, racingRng_);
    if (r != size_t(-1)) {
      title = "Someone at the track";
      body = h.racerSponsor + " put my face on a poster, and somebody noticed.";
      line = scene(t, r, racingRng_.chance(0.5), "String");
      happy += 2;
      mark("a16_racing-racer-love-interest-string-enc", r);
    }
  }

  // (12) A practice run (0.14).
  if (title.empty() && racingRng_.chance(0.14) && ensureTable("a16_racing-practice-run-feedback-enc")) {
    const Table& t = tbl("a16_racing-practice-run-feedback-enc");
    const size_t r = weightedPick(t, "Commonness Score", racingRng_);
    if (r != size_t(-1)) {
      const std::string best = lower(trim(t.at(r, "Best Vehicle Stat")));
      title = "Practice at " + h.racerTrack;
      body = "I put the " + h.carName + " through its paces before the next race.";
      if (statValue(best) >= 40) {
        line = spyUpperFirst(toFirstPerson(resolve(trim(t.at(r, "String")))));
        if (best.find("handling") != std::string::npos) bump(h.carHandling, 1);
        else if (best.find("acceleration") != std::string::npos) bump(h.carAcceleration, 1);
        else if (best.find("speed") != std::string::npos) bump(h.carSpeed, 1);
        happy += 1;
      } else {
        line = "The " + h.carName + " could not keep up in practice.";
        happy -= 1;
      }
      if (ensureTable("a16_racing-race-car-condition-string-enc")) {
        const Table& c = tbl("a16_racing-race-car-condition-string-enc");
        std::vector<size_t> cp;
        for (size_t x = 0; x < c.size(); ++x) {
          const double lo = c.num(x, "Min Condition"), hi = c.num(x, "Max Condition");
          if (lo > 0 && h.carCondition < lo) continue;
          if (hi > 0 && h.carCondition > hi) continue;
          cp.push_back(x);
        }
        if (!cp.empty())
          add(resolve(trim(c.at(cp[racingRng_.below(uint32_t(cp.size()))], "String"))));
      }
      mark("a16_racing-practice-run-feedback-enc", r);
    }
  }

  // (13) A street race the police interrupt (0.06).
  if (title.empty() && lower(h.racerCategory) == "street" && racingRng_.chance(0.06) &&
      ensureTable("a16_racing-street-race-cop-pivotal-moment-enc")) {
    const Table& t = tbl("a16_racing-street-race-cop-pivotal-moment-enc");
    const std::string gender = heroToken("gender", false);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      const std::string g = lower(trim(t.at(r, "Gender Bracket")));
      if (!g.empty() && g != gender) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[racingRng_.below(uint32_t(pool.size()))];
      h.racerTrack = "a street race";
      title = "A street race";
      // The street pool is a different set of backers from the circuit's.
      if (ensureTable("a16_racing-street-race-sponsor-enc")) {
        const Table& ss = tbl("a16_racing-street-race-sponsor-enc");
        const size_t sr = weightedPick(ss, "Money Score", racingRng_);
        if (sr != size_t(-1)) {
          std::string backer = resolve(trim(ss.at(sr, "Name")));
          if (ss.flag(sr, "Use Definite Article")) backer = "the " + backer;
          add(backer + " put up the money for it.");
        }
      }
      flush();
      int success = -1;
      if (racingCardRow(life, out, "a16_racing-street-race-cop-pivotal-moment-enc", r, forcedChoice,
                        true, &success)) {
        if (out.title.empty()) out.title = "A street race";
        const long long purse = 2000 + static_cast<long long>(racingRng_.below(6000));
        if (success == 1) {
          h.money += purse;
          take += purse;
          h.racerWinnings += purse;
          h.racerWins += 1;
          happy += 3;
          out.extra.push_back("I got away with " + money(purse) + ".");
        } else {
          h.racerLosses += 1;
          happy -= 3;
          out.extra.push_back("I lost the street race.");
        }
        for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
        h.money += take;
        out.happiness += happy;
        racingLive_ = live0;
        return true;
      }
    }
  }

  // (14) A race (0.24).
  if (title.empty() && racingRng_.chance(0.24)) {
    const Table& tr = tbl("a16_racing-race-track-enc");
    const std::string want = racRankName(h.racerRank);
    std::vector<size_t> pool;
    for (size_t r = 0; r < tr.size(); ++r) {
      if (lower(trim(tr.at(r, "Track Type"))) != lower(h.racerCategory)) continue;
      if (lower(trim(tr.at(r, "Unlock Rank"))) != lower(want)) continue;
      pool.push_back(r);
    }
    if (pool.empty())
      for (size_t r = 0; r < tr.size(); ++r)
        if (lower(trim(tr.at(r, "Track Type"))) == lower(h.racerCategory)) pool.push_back(r);
    if (!pool.empty()) {
      const size_t trackRow = weightedPickOf(tr, "Difficulty", pool, racingRng_);
      const std::string track = trim(tr.at(trackRow, "Track Name"));
      const std::string rec = lower(trim(tr.at(trackRow, "Recommended Stat")));
      h.racerTrack = track;
      size_t momentRow = size_t(-1);
      if (ensureTable("a16_racing-race-pivotal-moment-enc")) {
        const Table& mo = tbl("a16_racing-race-pivotal-moment-enc");
        const std::string gender = heroToken("gender", false);
        std::vector<size_t> mp;
        for (size_t r = 0; r < mo.size(); ++r) {
          if (!llInList(mo.at(r, "Race Type"), h.racerCategory)) continue;
          const std::string g = lower(trim(mo.at(r, "Gender Bracket")));
          if (!g.empty() && g != gender) continue;
          mp.push_back(r);
        }
        if (!mp.empty()) momentRow = mp[racingRng_.below(uint32_t(mp.size()))];
      }
      flush();
      int success = -1;
      bool card = false;
      if (momentRow != size_t(-1))
        card = racingCardRow(life, out, "a16_racing-race-pivotal-moment-enc", momentRow, forcedChoice,
                             true, &success);
      if (!card) {
        out.table = "a16_racing-race-track-enc";
        out.row_id = std::to_string(trackRow + 1);
        out.body = "Cup: " + h.racerCup + ".\nTrack: " + track + ".";
      }
      if (out.title.empty()) out.title = "A race at " + track;
      const double difficulty = tr.num(trackRow, "Difficulty");
      double odds = 0.5 + (statValue(rec) - difficulty * 0.5) / 120.0 +
                    (h.carCondition - 60) / 300.0 + (h.racerProfessionalism - 50) / 300.0;
      if (success == 1) odds += 0.15;
      if (success == 0) odds -= 0.15;
      odds = std::max(0.1, std::min(0.92, odds));
      const bool won = racingRng_.unit() < odds;
      out.extra.push_back("Cup: " + h.racerCup + ". Track: " + track + " (" + want + ").");
      if (won) {
        const double wLo = tr.num(trackRow, "Min Winnings"), wHi = tr.num(trackRow, "Max Winnings");
        long long purse = static_cast<long long>(wLo);
        if (wHi > wLo) purse += static_cast<long long>(racingRng_.below(uint32_t(wHi - wLo + 1)));
        if (ensureTable("a16_racing-cup-series-winning-enc")) {
          const Table& cs = tbl("a16_racing-cup-series-winning-enc");
          for (size_t r = 0; r < cs.size(); ++r) {
            if (lower(trim(cs.at(r, "Cup Type"))) != lower(h.racerCategory)) continue;
            const std::string lo = "Min " + want + " Winnings";
            const std::string hi = "Max " + want + " Winnings";
            if (cs.has(lo) && cs.has(hi))
              purse += static_cast<long long>((cs.num(r, lo) + cs.num(r, hi)) / 40);
            mark("a16_racing-cup-series-winning-enc", r);
            break;
          }
        }
        h.money += purse;
        take += purse;
        h.racerWinnings += purse;
        h.racerWins += 1;
        happy += 4;
        if (ensureTable("a16_racing-win-race-outcome-feedback-color-enc")) {
          const Table& w = tbl("a16_racing-win-race-outcome-feedback-color-enc");
          std::vector<size_t> wp;
          for (size_t r = 0; r < w.size(); ++r) {
            const std::string cat = lower(trim(w.at(r, "Race Type")));
            if (!cat.empty() && !llInList(cat, h.racerCategory)) continue;
            const std::string lvl = lower(trim(w.at(r, "Race Level")));
            if (!lvl.empty() && lvl != lower(want)) continue;
            if (w.flag(r, "Rival Racer Involved")) {
              const int lo = int(w.num(r, "Min Racer Craziness"));
              const int hi = int(w.num(r, "Max Racer Craziness"));
              if (lo > 0 && h.racerRivalCraziness < lo) continue;
              if (hi > 0 && h.racerRivalCraziness > hi) continue;
            }
            wp.push_back(r);
          }
          if (!wp.empty()) {
            const size_t x = weightedPickOf(w, "Commonness Score", wp, racingRng_);
            if (x != size_t(-1)) out.body += " " + spyUpperFirst(scene(w, x, false, "String"));
          }
        }
        add("I won " + money(purse) + " at " + track + ".");
        if (h.racerWins % 3 == 0 && h.racerRank < 3) {
          h.racerRank += 1;
          if (ensureTable("a16_racing-car-class-stats-enc")) {
            const Table& cs = tbl("a16_racing-car-class-stats-enc");
            for (size_t r = 0; r < cs.size(); ++r) {
              if (trim(cs.at(r, "Class")) != h.carClass) continue;
              bump(h.carHandling, int((cs.num(r, "Max Base Handling") - cs.num(r, "Min Base Handling")) / 2));
              bump(h.carSpeed, int((cs.num(r, "Max Base Speed") - cs.num(r, "Min Base Speed")) / 2));
              bump(h.carAcceleration,
                   int((cs.num(r, "Max Base Acceleration") - cs.num(r, "Min Base Acceleration")) / 2));
              break;
            }
          }
          add("I am a " + racRankName(h.racerRank) + " racer now.");
        }
      } else {
        h.racerLosses += 1;
        happy -= 3;
        if (ensureTable("a16_racing-lose-race-outcome-feedback-color-enc")) {
          const Table& w = tbl("a16_racing-lose-race-outcome-feedback-color-enc");
          std::vector<size_t> wp;
          for (size_t r = 0; r < w.size(); ++r) {
            const std::string cat = lower(trim(w.at(r, "Race Type")));
            if (!cat.empty() && !llInList(cat, h.racerCategory)) continue;
            const std::string lvl = lower(trim(w.at(r, "Race Level")));
            if (!lvl.empty() && lvl != lower(want)) continue;
            if (w.flag(r, "Rival Racer Involved")) {
              const int lo = int(w.num(r, "Min Racer Craziness"));
              const int hi = int(w.num(r, "Max Racer Craziness"));
              if (lo > 0 && h.racerRivalCraziness < lo) continue;
              if (hi > 0 && h.racerRivalCraziness > hi) continue;
            }
            wp.push_back(r);
          }
          if (!wp.empty()) {
            const size_t x = weightedPickOf(w, "Commonness Score", wp, racingRng_);
            if (x != size_t(-1)) out.body += " " + spyUpperFirst(scene(w, x, false, "String"));
          }
        }
        add("I lost at " + track + ".");
      }
      for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
      h.money += take;
      out.happiness += happy;
      racingLive_ = live0;
      return true;
    }
  }

  // (15) An everyday drive (0.34).
  if (title.empty() && racingRng_.chance(0.34)) {
    const bool hasPassenger = (h.relationshipOn && !h.loverName.empty());
    const std::string gender = heroToken("gender", false);
    const char* tabs[2] = {"a16_racing-drive-dumb-scenario-enc",
                           "a16_racing-drive-traffic-incident-enc"};
    const char* tab = tabs[racingRng_.below(2)];
    bool done = false;
    if (ensureTable(tab)) {
      const Table& t = tbl(tab);
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        const double lo = t.num(r, "Hero Min Age"), hi = t.num(r, "Hero Max Age");
        if (lo > 0 && h.age < lo) continue;
        if (hi > 0 && h.age > hi) continue;
        if (t.flag(r, "Requires Passenger") && !hasPassenger) continue;
        const std::string g = lower(trim(t.at(r, "Gender Bracket")));
        if (!g.empty() && g != gender) continue;
        pool.push_back(r);
      }
      if (!pool.empty()) {
        const size_t r = pool[racingRng_.below(uint32_t(pool.size()))];
        flush();
        if (racingCardRow(life, out, tab, r, forcedChoice, false, nullptr)) {
          if (out.title.empty()) out.title = "On the road";
          for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
          h.money += take;
          out.happiness += happy;
          racingLive_ = live0;
          return true;
        }
        done = true;
      }
    }
    if (!done) {
      std::string unit = "mph";
      if (ensureTable("a16_racing-driving-parameter-enc")) {
        const Table& dp = tbl("a16_racing-driving-parameter-enc");
        for (size_t r = 0; r < dp.size(); ++r) {
          if (lower(trim(dp.at(r, "Country"))) != lower(h.country)) continue;
          unit = dp.flag(r, "Uses MPH") ? "mph" : "km/h";
          mark("a16_racing-driving-parameter-enc", r);
          break;
        }
      }
      title = "On the road";
      body = "I drove the " + h.carName + " around town.";
      line = "I put a few hundred miles on the " + h.carName + " at legal " + unit + " speeds.";
      happy += 1;
    }
  }

  if (title.empty()) { racingLive_ = live0; return false; }
  out.table = tableRef.empty() ? std::string("a16_racing-race-track-enc") : tableRef;
  out.row_id = rowRef.empty() ? std::string("0") : rowRef;
  out.title = title;
  out.body = body;
  out.narrative = line;
  const std::vector<std::string> keep = out.extra;
  out.extra.clear();
  for (size_t i = 0; i < keep.size(); ++i) out.extra.push_back(keep[i]);
  for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
  h.money += take;
  out.happiness += happy;
  racingLive_ = live0;
  return true;
}

}  // namespace bl
