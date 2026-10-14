#pragma once

#include <string>
#include <vector>

namespace bl {

// The push-scenario selector, as observed on the device. Each eligible scenario
// type is appended to the bag once per unit of weight, then one uniform index
// into the flattened bag selects the type that gets constructed:
//
//     ValidateAndSetScenario("ScenarioSocialMediaJoinPush", 70, bag)
//         bag.size 0 -> 70
//
// so the bag is a multiset of type names, never a set of distinct candidates.
class WeightedBag {
 public:
  void add(const std::string& name, int weight);
  void clear();
  size_t size() const { return flat_.size(); }
  size_t entries() const { return entries_.size(); }
  int weightOf(size_t entry) const { return entries_[entry].second; }
  const std::string& nameOf(size_t entry) const { return entries_[entry].first; }
  // Flat slot view: slot i holds a copy of the name at entry flat_[i].
  const std::string& at(size_t slot) const { return entries_[flat_[slot]].first; }
  // Uniform draw: slot comes from the RNG, the bag only maps it to a name.
  bool pick(size_t slot, std::string* out) const;

 private:
  std::vector<std::pair<std::string, int>> entries_;
  std::vector<size_t> flat_;
};

// One captured draw: the full bag as built on the device plus the type the game
// actually constructed. Either index (the slot the RNG produced) or rngValue
// (a raw System.Random sample taken just before the draw) may be absent.
struct BagCase {
  int age = -1;
  std::vector<std::pair<std::string, int>> bag;
  long long index = -1;
  long long rngValue = -1;
  std::string expect;
  std::string source;
};

// Reads a bag-vector file: {"cases":[{"age":53,"bag":[["ScenarioX",70]],
// "index":123,"expect":"ScenarioX","source":"run5"}]}. Returns false and fills
// error when the file is missing or malformed.
bool loadBagVectors(const std::string& json, std::vector<BagCase>* out, std::string* error);

// Checks every case against the bag model and reports the mismatches.
struct BagCheckResult {
  size_t cases = 0;
  size_t withIndex = 0;
  size_t matched = 0;
  std::vector<std::string> failures;
};
BagCheckResult checkBagVectors(const std::vector<BagCase>& cases);

}  // namespace bl
