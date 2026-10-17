// src/fam/outdoor.cpp -- moved out of src/sim.cpp (REPORT.en.md 131).
//
// Cut verbatim by tools/split_family.py; the only edits are the includes.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>
#include <vector>

namespace bl {

bool Sim::outdoorYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.outdoors) return false;
  if (h.age < 8) return false;
  if (!ensureTable("a16_outdoor-destination-enc")) return false;
  if (!outdoorRng_.chance(0.5)) return false;
  hero_ = &h;
  life_ = &life;

  std::string title, body, line;
  std::vector<std::string> extra;
  std::string tableRef, rowRef;
  int happy = 0;

  const auto say = [&](const std::string& s) {
    if (line.empty() && !s.empty()) line = spyUpperFirst(s);
  };
  const auto add = [&](const std::string& s) { if (!s.empty()) extra.push_back(spyUpperFirst(s)); };
  const auto cell = [&](const Table& t, size_t r, const char* col) -> std::string {
    if (r == size_t(-1) || !t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  const auto mark = [&](const char* tab, size_t r) { tableRef = tab; rowRef = std::to_string(r + 1); };
  // One draw from a table, filtered on an optional column, windowed on
  // optional Min/Max columns, weighted on an optional Commonness Score.
  const auto window = [&](const char* tab, const char* weight, const char* col,
                          const std::string& want, const char* lo, const char* hi,
                          double value, bool exact) -> size_t {
    if (!ensureTable(tab)) return size_t(-1);
    const Table& t = tbl(tab);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (col != nullptr && *col && t.has(col) && !want.empty()) {
        const std::string v = lower(trim(t.at(r, col)));
        if (!v.empty()) {
          const bool fits = exact ? (v == lower(want)) : llInList(v, lower(want));
          if (!fits) continue;
        }
      }
      if (lo != nullptr && *lo && t.has(lo)) { const double l = t.num(r, lo); if (l > 0 && value < l) continue; }
      if (hi != nullptr && *hi && t.has(hi)) { const double u = t.num(r, hi); if (u > 0 && value > u) continue; }
      pool.push_back(r);
    }
    if (pool.empty()) {
      const Table& all = tbl(tab);
      for (size_t r = 0; r < all.size(); ++r) pool.push_back(r);
    }
    if (pool.empty()) return size_t(-1);
    if (weight != nullptr && *weight && t.has(weight)) {
      const size_t r = weightedPickOf(t, weight, pool, outdoorRng_);
      if (r != size_t(-1)) return r;
    }
    return pool[outdoorRng_.below(uint32_t(pool.size()))];
  };
  // Rows whose log line refers back to an alternative drawn earlier in the row
  // ('<rb>' / '<rb2>' / '<rb3>'): every other bracketed column is resolved
  // first, in column order, which is the order the game draws them in, so the
  // reference lands on the alternative the card really drew (REPORT.en.md 57).
  const auto withBrackets = [&](const Table& t, size_t r, const char* wantCol) -> std::string {
    if (r == size_t(-1)) return std::string();
    for (const std::string& cc : t.columns()) {
      if (cc == wantCol) continue;
      const std::string& s = t.at(r, cc);
      if (s.find('<') == std::string::npos) continue;
      cell(t, r, cc.c_str());
    }
    return cell(t, r, wantCol);
  };
  // A row of one of the family's four dumb-scenario tables whose age and
  // companion gates fit; renderRow plays the rest of the card (report 56).
  const auto cardRow = [&](const char* tab) -> size_t {
    if (!ensureTable(tab)) return size_t(-1);
    const Table& t = tbl(tab);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Hero Min Age")) { const double lo = t.num(r, "Hero Min Age"); if (lo > 0 && h.age < lo) continue; }
      if (t.has("Hero Max Age")) { const double hi = t.num(r, "Hero Max Age"); if (hi > 0 && h.age > hi) continue; }
      if (t.has("Requires Companion") && t.flag(r, "Requires Companion") && h.outdoorCompanion.empty()) continue;
      pool.push_back(r);
    }
    if (pool.empty()) return size_t(-1);
    return pool[outdoorRng_.below(uint32_t(pool.size()))];
  };
  // A landmark row, gated on the age and on whether anyone came along.
  const auto landmarkRow = [&](const char* tab) -> size_t {
    if (!ensureTable(tab)) return size_t(-1);
    const Table& t = tbl(tab);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Min Hero Age")) { const double lo = t.num(r, "Min Hero Age"); if (lo > 0 && h.age < lo) continue; }
      if (t.has("Max Hero Age")) { const double hi = t.num(r, "Max Hero Age"); if (hi > 0 && h.age > hi) continue; }
      if (t.has("Requires Companion") && t.flag(r, "Requires Companion") &&
          h.outdoorCompanion.empty()) continue;
      if (t.has("Companion Gender") && !h.outdoorCompanion.empty()) {
        const std::string g = lower(trim(t.at(r, "Companion Gender")));
        if (!g.empty() && g != "both" && g != (h.outdoorCompanionFemale ? "female" : "male")) continue;
      }
      pool.push_back(r);
    }
    if (pool.empty()) return size_t(-1);
    const size_t r = weightedPickOf(t, "Commonness Score", pool, outdoorRng_);
    return r == size_t(-1) ? pool[outdoorRng_.below(uint32_t(pool.size()))] : r;
  };
  // One animal, plant or bird of the destination's habitat.
  const auto wildlife = [&](const char* tab) -> size_t {
    return window(tab, "Commonness Score", "Habitat", h.outdoorHabitat, "", "", 0, false);
  };

  // One companion for the trip, so every '<outdoor-companion>' on the card is
  // the same person (REPORT.en.md 71.4).
  h.outdoorCompanion.clear();
  if (h.relationshipOn && !h.loverName.empty() && outdoorRng_.chance(0.45)) {
    h.outdoorCompanion = h.loverName;
    h.outdoorCompanionFemale = h.loverFemale;
  } else if (outdoorRng_.chance(0.35)) {
    h.outdoorCompanionFemale = outdoorRng_.chance(0.5);
    h.outdoorCompanion = drawFirstName(h.outdoorCompanionFemale) + " " + drawSurname();
  }
  const bool savedSet = pronounOtherSet_;
  const bool savedFemale = pronounOtherFemale_;
  const auto companionOn = [&]() {
    pronounOtherSet_ = !h.outdoorCompanion.empty();
    pronounOtherFemale_ = h.outdoorCompanionFemale;
  };
  const auto companionOff = [&]() { pronounOtherSet_ = savedSet; pronounOtherFemale_ = savedFemale; };
  const bool hasDog = h.hasPet && !h.petDead && lower(h.petAnimal).find("dog") != std::string::npos;

  // The activity of the year.
  static const struct { const char* name; double weight; } kActivities[] = {
      {"hike", 0.30}, {"camp", 0.22}, {"fish", 0.20},
      {"offroad", 0.13}, {"bike", 0.10}, {"snorkel", 0.05}};
  std::string activity = "hike";
  {
    const double roll = outdoorRng_.unit();
    double acc = 0;
    for (size_t i = 0; i < sizeof(kActivities) / sizeof(kActivities[0]); ++i) {
      acc += kActivities[i].weight;
      if (roll < acc) { activity = kActivities[i].name; break; }
    }
  }

  // The destination: the hike and camp rows are gated on their Activity
  // column, everything else takes any row of the pool (REPORT.en.md 71.2).
  const bool gated = (activity == "hike" || activity == "camp");
  const size_t drow = window("a16_outdoor-destination-enc", "Commonness Score",
                             gated ? "Activity" : "", gated ? activity : "",
                             "", "", 0, false);
  const Table& d = tbl("a16_outdoor-destination-enc");
  h.outdoorPlace = drow == size_t(-1) ? std::string() : cell(d, drow, "Location");
  h.outdoorHabitat = drow == size_t(-1) ? std::string() : lower(trim(d.at(drow, "Habitat")));
  if (h.outdoorPlace.empty()) h.outdoorPlace = "the woods";
  h.outdoorLandmark.clear();
  if (drow != size_t(-1) && d.has("Landmarks")) {
    std::vector<std::string> landmarks;
    splitOn(trim(d.at(drow, "Landmarks")), ',', landmarks);
    if (!landmarks.empty()) h.outdoorLandmark = trim(landmarks[outdoorRng_.below(uint32_t(landmarks.size()))]);
  }
  h.outdoorTrips += 1;
  h.outdoorJoy = std::min(100, h.outdoorJoy + 3);

  if (activity == "hike") {
    title = "A hike";
    body = "Trail: " + h.outdoorPlace + ".";
    say("I hiked through " + h.outdoorPlace + ".");
    happy += 1;
    // A find on the trail (a16_outdoor-hiking-item-discoverie-enc).
    if (outdoorRng_.chance(0.45)) {
      const size_t r = window("a16_outdoor-hiking-item-discoverie-enc", "Commonness Score",
                              "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-hiking-item-discoverie-enc");
        mark("a16_outdoor-hiking-item-discoverie-enc", r);
        const long long lo = static_cast<long long>(t.num(r, "Min Item Value"));
        const long long hi = static_cast<long long>(t.num(r, "Max Item Value"));
        const long long span = hi > lo ? hi - lo + 1 : 1;
        const long long value = lo + static_cast<long long>(outdoorRng_.below(uint32_t(span)));
        const std::string item = cell(t, r, "Item");
        h.money += value;
        h.outdoorEarned += value;
        add("I found " + withArticle(item, true) + item + " worth " + money(value) + ".");
      }
    }
    // Something growing on the trail, and the plants the handbook flags.
    if (outdoorRng_.chance(0.35)) {
      const size_t r = wildlife("a16_outdoor-plant-enc");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-plant-enc");
        mark("a16_outdoor-plant-enc", r);
        const std::string plant = cell(t, r, "Species");
        const std::string plural = cell(t, r, "Plural");
        h.outdoorPlant = plant;
        h.outdoorJoy = std::min(100, h.outdoorJoy + static_cast<int>(t.num(r, "Enjoyment Score")));
        happy += static_cast<int>(t.num(r, "Happiness Impact"));
        h.health = std::max(0, std::min(100, h.health + static_cast<int>(t.num(r, "Health Impact"))));
        add("I came across " + (plural.empty() ? plant : plural) + " growing along the trail.");
        if (t.flag(r, "Hallucinations") && outdoorRng_.chance(0.6)) {
          const size_t hr = window("a16_outdoor-hallucination-enc", "Commonness Score",
                                   "", "", "", "", 0, false);
          if (hr != size_t(-1)) {
            const Table& u = tbl("a16_outdoor-hallucination-enc");
            mark("a16_outdoor-hallucination-enc", hr);
            add(withBrackets(u, hr, "Log Version"));
            h.outdoorJoy = std::min(100, h.outdoorJoy + static_cast<int>(u.num(hr, "Enjoyment Score")));
            const size_t sr = window("a16_outdoor-hallucination-symptom-scenario-enc",
                                     "Commonness Score", "", "", "", "", 0, false);
            if (sr != size_t(-1)) {
              const Table& s = tbl("a16_outdoor-hallucination-symptom-scenario-enc");
              add(withBrackets(s, sr, "Log Version"));
            }
          }
        }
      }
    }
    // The animals of the destination (a16_outdoor-animal-enc).
    if (outdoorRng_.chance(0.25)) {
      const size_t r = wildlife("a16_outdoor-animal-enc");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-animal-enc");
        mark("a16_outdoor-animal-enc", r);
        const std::string species = cell(t, r, "Species");
        const int danger = static_cast<int>(t.num(r, "Danger Score"));
        if (danger >= 3 && outdoorRng_.chance(0.35)) {
          add("A " + species + " " + cell(t, r, "Successful Attack String") + " me on the trail.");
          h.health = std::max(0, h.health - danger);
        } else {
          add("I spotted " + withArticle(species, true) + species + " and backed away slowly.");
        }
      }
    }
    // Birds (a16_outdoor-bird-enc and the behaviour pool behind it).
    if (outdoorRng_.chance(0.20)) {
      const size_t r = wildlife("a16_outdoor-bird-enc");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-bird-enc");
        mark("a16_outdoor-bird-enc", r);
        const std::string plural = cell(t, r, "Plural");
        h.outdoorBird = cell(t, r, "Species");
        add("A pair of " + (plural.empty() ? h.outdoorBird : plural) + " kept me company.");
        const size_t br = window("a16_outdoor-bird-behavior-enc", "Commonness Score", "Habitat",
                                 h.outdoorHabitat, "", "", 0, false);
        if (br != size_t(-1)) add("One of them decided to " + cell(tbl("a16_outdoor-bird-behavior-enc"), br, "String") + ".");
      }
    }
    // The landmark at the end of the trail: a view, a memento or a selfie.
    if (outdoorRng_.chance(0.15) && !h.outdoorLandmark.empty()) {
      if (outdoorRng_.chance(0.5)) {
        const size_t r = landmarkRow("a16_outdoor-landmark-view-reflection-enc");
        if (r != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-landmark-view-reflection-enc");
          mark("a16_outdoor-landmark-view-reflection-enc", r);
          companionOn();
          add(withBrackets(u, r, "Log Version"));
          companionOff();
          happy += static_cast<int>(u.num(r, "Happiness Impact"));
        }
      } else if (outdoorRng_.chance(0.5)) {
        const size_t r = landmarkRow("a16_outdoor-landmark-memento-scenario-enc");
        if (r != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-landmark-memento-scenario-enc");
          mark("a16_outdoor-landmark-memento-scenario-enc", r);
          companionOn();
          add(withBrackets(u, r, "Log Version"));
          companionOff();
          happy += static_cast<int>(u.num(r, "Happiness Impact"));
        }
      } else {
        const size_t r = window("a16_outdoor-landmark-selfie-color-enc", "Commonness Score",
                                "", "", "", "", 0, false);
        if (r != size_t(-1)) add(cell(tbl("a16_outdoor-landmark-selfie-color-enc"), r, "String"));
      }
    }
    // A cave on the way (the cave list plus its own discovery card).
    if (outdoorRng_.chance(0.10)) {
      const size_t cr = window("a16_outdoor-cave-enc", "", "Location", h.country, "", "", 0, false);
      if (cr != size_t(-1)) {
        const Table& c = tbl("a16_outdoor-cave-enc");
        mark("a16_outdoor-cave-enc", cr);
        h.outdoorCave = cell(c, cr, "Name");
        add("I took a detour into " + h.outdoorCave + ".");
        const size_t dr = cardRow("a16_outdoor-cave-discoverie-enc");
        if (dr != size_t(-1)) {
          const std::vector<std::string> keep = out.extra;
          for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
          const bool shown = renderRow(tbl("a16_outdoor-cave-discoverie-enc"), dr, life, out, forcedChoice);
          (void)keep;
          return shown;
        }
      }
    }
    // What the hero drank on the way (a16_outdoor-hiking-drink-enc).
    if (outdoorRng_.chance(0.10)) {
      const size_t r = window("a16_outdoor-hiking-drink-enc", "Commonness Score", "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-hiking-drink-enc");
        mark("a16_outdoor-hiking-drink-enc", r);
        add(toFirstPerson(cell(t, r, "Feedback Color String")));
        happy += static_cast<int>(t.num(r, "Happiness Impact"));
        h.health = std::max(0, std::min(100, h.health + static_cast<int>(t.num(r, "Health Impact"))));
      }
    }
    // A ghost at the campsite or on the trail (a16_outdoor-ghost-encounter-enc).
    if (outdoorRng_.chance(0.08)) {
      const size_t r = window("a16_outdoor-ghost-encounter-enc", "", "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-ghost-encounter-enc");
        mark("a16_outdoor-ghost-encounter-enc", r);
        add(withBrackets(t, r, "Log Version"));
      }
    }
    // The dog, when there is one.
    if (hasDog && outdoorRng_.chance(0.30)) {
      const size_t r = window("a16_outdoor-hiking-dog-discovery-scenario-enc", "Commonness Score",
                              "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-hiking-dog-discovery-scenario-enc");
        mark("a16_outdoor-hiking-dog-discovery-scenario-enc", r);
        add(withBrackets(t, r, "Log Version"));
        h.outdoorJoy = std::min(100, h.outdoorJoy + static_cast<int>(t.num(r, "Hero Enjoyment Score")));
        const size_t in = window("a16_outdoor-hiking-dog-fetch-item-enc", "", "", "", "", "", 0, false);
        if (in != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-hiking-dog-fetch-item-enc");
          const std::string object = withBrackets(u, in, "Object");
          add(h.petName.empty() ? "My dog" : h.petName + " fetched " + withArticle(object, true) + object + ".");
          h.outdoorJoy = std::min(100, h.outdoorJoy + static_cast<int>(u.num(in, "Pride Score")));
        }
      }
    }
  } else if (activity == "camp") {
    title = "A night under the stars";
    body = "Campsite: " + h.outdoorPlace + ".";
    say("I camped at " + h.outdoorPlace + ".");
    happy += 2;
    if (outdoorRng_.chance(0.35)) {
      const size_t r = cardRow("a16_outdoor-camping-dumb-scenario-enc");
      if (r != size_t(-1)) return renderRow(tbl("a16_outdoor-camping-dumb-scenario-enc"), r, life, out, forcedChoice);
    }
    if (outdoorRng_.chance(0.22)) {
      const size_t r = wildlife("a16_outdoor-animal-enc");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-animal-enc");
        mark("a16_outdoor-animal-enc", r);
        add("Something moved through the camp in the dark: " + cell(t, r, "Species") + ".");
      }
    }
    if (outdoorRng_.chance(0.18)) {
      const size_t r = wildlife("a16_outdoor-plant-enc");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-plant-enc");
        mark("a16_outdoor-plant-enc", r);
        h.outdoorPlant = cell(t, r, "Species");
        add("The campsite was surrounded by " + h.outdoorPlant + ".");
      }
    }
    if (outdoorRng_.chance(0.15)) {
      const size_t r = window("a16_outdoor-ghost-encounter-enc", "", "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-ghost-encounter-enc");
        mark("a16_outdoor-ghost-encounter-enc", r);
        add(withBrackets(t, r, "Log Version"));
      }
    }
    if (hasDog && outdoorRng_.chance(0.35)) {
      const size_t r = window("a16_outdoor-dog-behavior-enc", "Max Dog Craziness", "Activity",
                              "camping", "", "", 0, true);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-dog-behavior-enc");
        mark("a16_outdoor-dog-behavior-enc", r);
        add(withBrackets(t, r, "Log Version"));
        const size_t tr = window("a16_outdoor-camping-dog-treat-enc", "", "", "", "", "", 0, false);
        if (tr != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-camping-dog-treat-enc");
          add("I gave " + (h.petName.empty() ? std::string("the dog") : h.petName) + " " +
              cell(u, tr, "Treat") + ".");
          h.outdoorJoy = std::min(100, h.outdoorJoy + static_cast<int>(u.num(tr, "Enjoyment Score")));
        }
      }
    }
  } else if (activity == "fish") {
    say("I went fishing at " + h.outdoorPlace + ".");
    happy += 1;
    static const char* kWaters[] = {"lake", "river", "pond", "ocean"};
    const std::string water = kWaters[outdoorRng_.below(4)];
    const size_t r = window("a16_outdoor-catchable-fish-enc", "Commonness Score", "Habitat",
                            water, "", "", 0, true);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_outdoor-catchable-fish-enc");
      mark("a16_outdoor-catchable-fish-enc", r);
      const std::string species = cell(t, r, "Species");
      const int difficulty = static_cast<int>(t.num(r, "Difficulty Score"));
      // The rod talks back before the fish is landed: one of the two colour
      // pools describes the fight, keyed on the difficulty score (71.5).
      const bool hooked = outdoorRng_.chance(0.5);
      const char* fightTable = hooked ? "a16_outdoor-hooked-fish-color-string-enc"
                                      : "a16_outdoor-lure-fish-color-string-enc";
      const std::string difficultyText = std::to_string(difficulty);
      const size_t fr = window(fightTable, "Commonness Score", "Fish Difficulty",
                               difficultyText, "", "", 0, false);
      // The colour pools are written in the second person (the card's voice);
      // the log keeps the first person the rest of the family uses.
      if (fr != size_t(-1)) add(toFirstPerson(cell(tbl(fightTable), fr, "String")));
      const double lo = t.num(r, "Min Weight (lbs)");
      const double hi = t.num(r, "Max Weight (lbs)");
      const double weight = lo + outdoorRng_.unit() * (hi > lo ? hi - lo : 1.0);
      const long long lbs = static_cast<long long>(weight + 0.5);
      const long long vlo = static_cast<long long>(t.num(r, "Min Value"));
      const long long vhi = static_cast<long long>(t.num(r, "Max Value"));
      const long long span = vhi > vlo ? vhi - vlo + 1 : 1;
      const long long value = vlo + static_cast<long long>(outdoorRng_.below(uint32_t(span)));
      h.outdoorFish = species;
      h.outdoorCatches += 1;
      h.money += value;
      h.outdoorEarned += value;
      title = "A fish on the line";
      body = "Catch: " + species + ".";
      add("I landed a " + species + " that weighed " + std::to_string(lbs) + " lbs and sold it for " +
          money(value) + ".");
      // A trophy fish gets a name and a wall (a16_outdoor-mounted-fish-name-enc).
      if (lbs >= 20) {
        const size_t mr = window("a16_outdoor-mounted-fish-name-enc", "Commonness Score",
                                 "Gender", h.female ? "male" : "female", "", "", 0, false);
        if (mr != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-mounted-fish-name-enc");
          mark("a16_outdoor-mounted-fish-name-enc", mr);
          add("I had " + cell(u, mr, "Fish Name") + " mounted above the fireplace.");
        }
      }
      // And sometimes the hook brings up something older (fishing-treasure).
      if (outdoorRng_.chance(0.15)) {
        const size_t tr = window("a16_outdoor-fishing-treasure-enc", "Commonness Score",
                                 "Location", water, "", "", 0, false);
        if (tr != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-fishing-treasure-enc");
          mark("a16_outdoor-fishing-treasure-enc", tr);
          const long long tlo = static_cast<long long>(u.num(tr, "Min Value"));
          const long long thi = static_cast<long long>(u.num(tr, "Max Value"));
          const long long tsp = thi > tlo ? thi - tlo + 1 : 1;
          const long long worth = tlo + static_cast<long long>(outdoorRng_.below(uint32_t(tsp)));
          const std::string item = cell(u, tr, "List Item Name");
          h.money += worth;
          h.outdoorEarned += worth;
          add("I pulled up " + withArticle(item, true) + item + ", worth " + money(worth) + ".");
        }
      }
    }
  } else if (activity == "offroad") {
    title = "A day off-road";
    body = "Route: " + h.outdoorPlace + ".";
    say("I took the " + h.outdoorVehicle + " out to " + h.outdoorPlace + ".");
    happy += 2;
    // A trick and its aftermath (a16_outdoor-offroading-trick-enc): the
    // Success Feedback column is already the first-person past tense, the
    // String column is the infinitive the failure line needs.
    if (outdoorRng_.chance(0.50)) {
      const size_t r = window("a16_outdoor-offroading-trick-enc", "Commonness Score",
                              "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-offroading-trick-enc");
        mark("a16_outdoor-offroading-trick-enc", r);
        const int difficulty = static_cast<int>(t.num(r, "Difficulty Score"));
        const std::string trick = withBrackets(t, r, "String");
        const std::string trickOk = cell(t, r, "Success Feedback Version");
        if (outdoorRng_.unit() < 0.95 - 0.15 * difficulty) {
          add("I " + trickOk + " in the " + h.outdoorVehicle + ".");
        } else {
          add("I tried to " + toFirstPerson(trick) + " and bent the bumper.");
          h.outdoorBikeCondition = std::max(0, h.outdoorBikeCondition - 5);
        }
      }
    }
    // Where the drive actually went (a16_outdoor-offroading-location-color-string-enc).
    if (outdoorRng_.chance(0.45)) {
      const size_t r = window("a16_outdoor-offroading-location-color-string-enc", "Commonness Score",
                              "Location", cell(tbl("a16_outdoor-offroading-trick-enc"), 0, "Location"),
                              "", "", 0, false);
      if (r != size_t(-1)) add("I drove " + cell(tbl("a16_outdoor-offroading-location-color-string-enc"), r, "String") + ".");
    }
    // Something stuck out there (a16_outdoor-offroading-encounter-enc).
    if (outdoorRng_.chance(0.35)) {
      const size_t r = window("a16_outdoor-offroading-encounter-enc", "Commonness Score",
                              "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-offroading-encounter-enc");
        mark("a16_outdoor-offroading-encounter-enc", r);
        const int danger = static_cast<int>(t.num(r, "Danger Score"));
        add("I came across a vehicle " + cell(t, r, "String") + " and winched it out.");
        if (danger >= 3) h.health = std::max(0, h.health - danger);
      }
    }
    // A little something for the trouble (a16_outdoor-offroading-reward-enc).
    if (outdoorRng_.chance(0.30)) {
      const size_t r = window("a16_outdoor-offroading-reward-enc", "Commonness Score",
                              "", "", "Min Hero Age", "Max Hero Age", h.age, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-offroading-reward-enc");
        mark("a16_outdoor-offroading-reward-enc", r);
        const std::string item = cell(t, r, "Item");
        add("Someone handed me " + withArticle(item, true) + item + ".");
        happy += static_cast<int>(t.num(r, "Happiness Impact"));
      }
    }
    // And the passenger, when there is one (a16_outdoor-offroading-make-love-string-enc).
    if (!h.outdoorCompanion.empty() && outdoorRng_.chance(0.25)) {
      const size_t r = window("a16_outdoor-offroading-make-love-string-enc", "Commonness Score",
                              "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-offroading-make-love-string-enc");
        mark("a16_outdoor-offroading-make-love-string-enc", r);
        add("We found a quiet spot where we could " + toFirstPerson(cell(t, r, "Snippet")) + ".");
        happy += 2;
      }
    }
  } else if (activity == "bike") {
    title = "A bike ride";
    body = "Route: " + h.outdoorPlace + ".";
    say("I rode my " + h.outdoorBike + " around " + h.outdoorPlace + ".");
    happy += 1;
    if (outdoorRng_.chance(0.35)) {
      const size_t r = cardRow("a16_outdoor-bike-ride-dumb-enc");
      if (r != size_t(-1)) return renderRow(tbl("a16_outdoor-bike-ride-dumb-enc"), r, life, out, forcedChoice);
    }
    const size_t r = window("a16_outdoor-bike-ride-location-enc", "Commonness Score",
                            "", "", "", "", 0, false);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_outdoor-bike-ride-location-enc");
      mark("a16_outdoor-bike-ride-location-enc", r);
      add("I rode " + cell(t, r, "String") + " on the " + h.outdoorBike + ".");
    }
    h.outdoorBikeCondition = std::max(0, h.outdoorBikeCondition - static_cast<int>(outdoorRng_.below(6)));
  } else if (activity == "snorkel") {
    title = "A snorkel trip";
    body = "Site: " + h.outdoorPlace + ".";
    say("I went snorkeling off " + h.outdoorPlace + ".");
    happy += 2;
    const size_t dr = window("a16_outdoor-snorkeling-discovery-scenario-enc", "Commonness Score",
                             "", "", "", "", 0, false);
    if (dr != size_t(-1)) {
      const Table& u = tbl("a16_outdoor-snorkeling-discovery-scenario-enc");
      mark("a16_outdoor-snorkeling-discovery-scenario-enc", dr);
      add(cell(u, dr, "Scenario"));
    }
    const size_t r = window("a16_outdoor-marine-life-enc", "Commonness Score", "", "", "", "", 0, false);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_outdoor-marine-life-enc");
      mark("a16_outdoor-marine-life-enc", r);
      h.outdoorMarine = cell(t, r, "Species");
      const int danger = static_cast<int>(t.num(r, "Danger Score"));
      // The play pool is a bare phrase whose own 'Log' column is the recorded
      // first person ("You swam next to" / "I swam next to"), so the log wins.
      const size_t pr = window("a16_outdoor-marine-life-play-string-enc", "Commonness Score",
                               "", "", "", "", 0, false);
      if (pr != size_t(-1)) {
        const Table& u = tbl("a16_outdoor-marine-life-play-string-enc");
        mark("a16_outdoor-marine-life-play-string-enc", pr);
        add(withBrackets(u, pr, "Log") + " a " + h.outdoorMarine + ".");
      }
      const size_t br = window("a16_outdoor-marine-behavior-enc", "Commonness Score",
                               "", "", "", "", 0, false);
      if (br != size_t(-1)) {
        const Table& u = tbl("a16_outdoor-marine-behavior-enc");
        add("It decided to " + withBrackets(u, br, "String") + ".");
      }
      if (danger >= 4 && outdoorRng_.chance(0.40)) {
        add("A " + h.outdoorMarine + " " + cell(t, r, "Attack String") + " me.");
        h.health = std::max(0, h.health - danger);
      }
    }
  }

  if (title.empty()) return false;
  out.table = tableRef.empty() ? "a16_outdoor-destination-enc" : tableRef;
  out.row_id = rowRef.empty() ? std::string("0") : rowRef;
  out.title = title;
  out.body = body;
  out.narrative = line;
  const std::vector<std::string> keep = out.extra;
  out.extra.clear();
  for (size_t i = 0; i < keep.size(); ++i) out.extra.push_back(keep[i]);
  for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
  out.happiness += happy;
  if (h.outdoorBikeCondition <= 20) out.happiness -= 1;
  return true;
}

void Sim::makeOutdoor(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.outdoors = true;
  h.outdoorJoy = 55;
  h.outdoorBikeCondition = 85 + static_cast<int>(outdoorRng_.below(16));
  h.outdoorTrips = 0;
  h.outdoorCatches = 0;
  h.outdoorEarned = 0;
  if (ensureTable("a16_outdoor-vehicle-enc")) {
    const Table& v = tbl("a16_outdoor-vehicle-enc");
    std::vector<size_t> offroad, pedal;
    for (size_t r = 0; r < v.size(); ++r) {
      const std::string type = lower(trim(v.at(r, "Type")));
      if (type.find("atv") != std::string::npos) offroad.push_back(r);
      else if (type.find("dirt") == std::string::npos) pedal.push_back(r);
    }
    if (!offroad.empty()) {
      const size_t r = offroad[outdoorRng_.below(uint32_t(offroad.size()))];
      h.outdoorVehicle = trim(v.at(r, "Make")) + " " + trim(v.at(r, "Model"));
    }
    if (!pedal.empty()) {
      const size_t r = pedal[outdoorRng_.below(uint32_t(pedal.size()))];
      h.outdoorBike = trim(v.at(r, "Make")) + " " + trim(v.at(r, "Model"));
    }
  }
  if (h.outdoorVehicle.empty()) h.outdoorVehicle = "ATV";
  if (h.outdoorBike.empty()) h.outdoorBike = "Mountain Bike";
}

std::string Sim::outdoorToken(const std::string& key) {
  if (hero_ == nullptr) return std::string();
  Hero& h = *const_cast<Hero*>(hero_);
  std::string k = trim(key);
  const std::string psuffix = "-possessive";
  bool poss = false;
  if (k.size() > psuffix.size() && k.compare(k.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    poss = true;
    k = k.substr(0, k.size() - psuffix.size());
  }
  const auto own = [&](const std::string& name) { return poss ? llPossessive(name) : name; };
  const auto drawPerson = [&](bool female) { return drawFirstName(female) + " " + drawSurname(); };
  const auto remembered = [&](const std::string& slot) -> std::string {
    std::map<std::string, std::string>::iterator it = outdoorCache_.find(slot);
    return it == outdoorCache_.end() ? std::string() : it->second;
  };
  const auto remember = [&](const std::string& slot, const std::string& value) {
    if (!value.empty()) outdoorCache_[slot] = value;
    return value;
  };
  const auto drawSpecies = [&](const std::string& slot, const char* table, const char* column) -> std::string {
    const std::string have = remembered(slot);
    if (!have.empty()) return have;
    if (!ensureTable(table)) return std::string();
    const Table& t = tbl(table);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Habitat") && !h.outdoorHabitat.empty()) {
        const std::string v = lower(trim(t.at(r, "Habitat")));
        if (!v.empty() && !llInList(v, h.outdoorHabitat)) continue;
      }
      pool.push_back(r);
    }
    if (pool.empty()) {
      const Table& all = tbl(table);
      for (size_t r = 0; r < all.size(); ++r) pool.push_back(r);
    }
    if (pool.empty()) return std::string();
    return remember(slot, resolve(trim(t.at(pool[outdoorRng_.below(uint32_t(pool.size()))], column))));
  };

  if (k == "outdoor-location")
    return own(h.outdoorPlace.empty() ? std::string("the trail") : h.outdoorPlace);
  if (k == "outdoor-landmark")
    return own(h.outdoorLandmark.empty() ? std::string("the view") : h.outdoorLandmark);
  if (k == "bicycle")
    return own(h.outdoorBike.empty() ? std::string("bicycle") : h.outdoorBike);
  if (k == "outdoor-plant") {
    const std::string plant = drawSpecies("plant", "a16_outdoor-plant-enc", "Species");
    return own(plant.empty() ? std::string("wildflower") : plant);
  }
  if (k == "outdoor-bird") {
    const std::string bird = drawSpecies("bird", "a16_outdoor-bird-enc", "Species");
    return own(bird.empty() ? std::string("bird") : bird);
  }
  if (k == "outdoor-marine-life") {
    const std::string animal = drawSpecies("marine", "a16_outdoor-marine-life-enc", "Species");
    return own(animal.empty() ? std::string("fish") : animal);
  }
  if (k == "outdoor-cave") {
    if (h.outdoorCave.empty()) h.outdoorCave = remembered("cave");
    if (h.outdoorCave.empty() && ensureTable("a16_outdoor-cave-enc")) {
      const Table& t = tbl("a16_outdoor-cave-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        if (lower(trim(t.at(r, "Location"))) == lower(h.country)) pool.push_back(r);
      }
      if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
      if (!pool.empty()) h.outdoorCave = trim(t.at(pool[outdoorRng_.below(uint32_t(pool.size()))], "Name"));
      remember("cave", h.outdoorCave);
    }
    return own(h.outdoorCave.empty() ? std::string("the cave") : h.outdoorCave);
  }
  if (k == "outdoor-companion") {
    if (h.outdoorCompanion.empty()) {
      h.outdoorCompanionFemale = outdoorRng_.chance(0.5);
      h.outdoorCompanion = drawPerson(h.outdoorCompanionFemale);
    }
    return own(h.outdoorCompanion);
  }
  if (k == "pet") return own(h.petName.empty() ? h.petAnimal : h.petName);
  return std::string();
}

}  // namespace bl
