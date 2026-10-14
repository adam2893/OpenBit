// src/deck.cpp -- see include/bl/deck.h for why this exists.
#include "bl/deck.h"

#include <algorithm>
#include <cmath>

#include "bl/table.h"

namespace bl {

int RandomDeck::Gcd(int a, int b) {
  while (b != 0) {
    const int t = a % b;
    a = b;
    b = t;
  }
  return a < 0 ? -a : a;
}

std::pair<int, int> RandomDeck::ComputeDeckParameters(float percent) {
  int step = int(std::lround(percent * 100.0f));
  if (step < 0) step = 0;
  if (step > 100) step = 100;
  const int g = Gcd(step, 100);
  return {100 / g, step / g};
}

RandomDeck::RandomDeck(Rng& rng, int deckSize, int successCount)
    : rng_(rng), deckSize_(deckSize < 1 ? 1 : deckSize) {
  FillAndShuffle(successCount);
}

void RandomDeck::FillAndShuffle(int successCount) {
  if (successCount < 0) successCount = 0;
  if (successCount > deckSize_) successCount = deckSize_;
  successCount_ = successCount;
  cards_.assign(size_t(deckSize_), 0);
  for (int i = 0; i < successCount_; ++i) cards_[size_t(i)] = 1;
  ShuffleAll();
}

void RandomDeck::ShuffleAll() {
  // Fisher-Yates over the same seeded engine the rest of the sim uses; the
  // shipped shuffle is a different PRNG, so only deck composition is matched
  // bit-for-bit, not card order (shortcut: card order, upgrade if a trace needs
  // an identical draw sequence).
  for (size_t i = cards_.size(); i > 1; --i) {
    const size_t j = rng_.below(uint32_t(i));
    std::swap(cards_[i - 1], cards_[j]);
  }
  index_ = 0;
}

void RandomDeck::SetPercent(float percent) {
  const std::pair<int, int> p = ComputeDeckParameters(percent);
  if (p.first == deckSize_ && p.second == successCount_) return;
  deckSize_ = p.first;
  FillAndShuffle(p.second);
}

bool RandomDeck::Draw() {
  if (index_ >= deckSize_) ShuffleAll();
  const bool hit = cards_[size_t(index_)] != 0;
  ++index_;
  return hit;
}

}  // namespace bl
