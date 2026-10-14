// bl/deck.h -- RandomDeck, BitLife's probability primitive.
//
// The game does not roll a fresh dice per check: RandomDeckBuilder.Build(percent)
// hands back a shuffled boolean deck whose size is reduced by gcd(step, 100), so
// the long-run success rate is the requested percent *exactly* per full deck
// cycle. Reproduced here because timings/cooldowns (job offers, deaths, crimes)
// inherit that cycle, and a plain rng.chance(p) diverges from the shipped game.
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace bl {

class Rng;

class RandomDeck {
 public:
  RandomDeck(Rng& rng, int deckSize, int successCount);

  // RandomDeck.ComputeDeckParameters: step = round(percent * 100), g = gcd(step,
  // 100), (deckSize, successCount) = (100 / g, step / g). percent is 0..1.
  static std::pair<int, int> ComputeDeckParameters(float percent);
  static int Gcd(int a, int b);

  void SetPercent(float percent);
  // One card; refills and reshuffles after the deck is exhausted.
  bool Draw();

  int deckSize() const { return deckSize_; }
  int successCount() const { return successCount_; }
  int index() const { return index_; }
  const std::vector<unsigned char>& cards() const { return cards_; }

 private:
  void FillAndShuffle(int successCount);
  void ShuffleAll();

  Rng& rng_;
  std::vector<unsigned char> cards_;
  int index_ = 0;
  int deckSize_ = 1;
  int successCount_ = 0;
};

}  // namespace bl
