#include "bl/dotnet_rng.h"

#include <cmath>
#include <limits>

namespace bl {

// Mono/.NET reference implementation, including the Int32.MinValue clamp: the
// game seeds Random from a hash of the save, so negative seeds do occur.
DotNetRng::DotNetRng(int32_t seed) : seedArray_(56, 0) {
  const int32_t subtraction =
      (seed == std::numeric_limits<int32_t>::min()) ? std::numeric_limits<int32_t>::max()
                                                   : std::abs(seed);
  int32_t mj = 161803398 - subtraction;
  seedArray_[55] = mj;
  int32_t mk = 1;
  for (int i = 1; i < 55; ++i) {
    const int ii = (21 * i) % 55;
    seedArray_[ii] = mk;
    mk = mj - mk;
    if (mk < 0) mk += std::numeric_limits<int32_t>::max();
    mj = seedArray_[ii];
  }
  for (int k = 1; k < 5; ++k) {
    for (int i = 1; i < 56; ++i) {
      seedArray_[i] -= seedArray_[1 + (i + 30) % 55];
      if (seedArray_[i] < 0) seedArray_[i] += std::numeric_limits<int32_t>::max();
    }
  }
}

int32_t DotNetRng::internalSample() {
  int locINext = inext_;
  int locINextp = inextp_;
  if (++locINext >= 56) locINext = 1;
  if (++locINextp >= 56) locINextp = 1;
  int32_t retVal = seedArray_[locINext] - seedArray_[locINextp];
  if (retVal == std::numeric_limits<int32_t>::max()) --retVal;
  if (retVal < 0) retVal += std::numeric_limits<int32_t>::max();
  seedArray_[locINext] = retVal;
  inext_ = locINext;
  inextp_ = locINextp;
  return retVal;
}

int32_t DotNetRng::next() { return internalSample(); }

int32_t DotNetRng::next(int32_t maxValue) {
  if (maxValue < 0) return 0;  // the game never passes a negative bound
  if (maxValue <= 1) return 0;
  return int32_t(sample() * maxValue);
}

int32_t DotNetRng::next(int32_t minValue, int32_t maxValue) {
  if (minValue > maxValue) return minValue;
  const int64_t range = int64_t(maxValue) - int64_t(minValue);
  if (range <= 1) return minValue;
  return minValue + int32_t(sample() * double(range));
}

}  // namespace bl
