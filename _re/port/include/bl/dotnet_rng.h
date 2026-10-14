// bl/dotnet_rng.h -- the sim's own PRNG (System.Random) reproduced bit for bit.
//
// The game builds its System.Random instances through the parametric .ctor and
// then draws Next()/Next(int)/Next(int,int), so a port that wants the same life
// from the same seed must use Mono/.NET's subtractive generator, not mt19937.
#pragma once

#include <cstdint>
#include <vector>

namespace bl {

class DotNetRng {
 public:
  explicit DotNetRng(int32_t seed);

  // Mono System.Random.Next(): InternalSample().
  int32_t next();
  // Next(maxValue): floor(Sample() * maxValue); maxValue <= 0 returns 0.
  int32_t next(int32_t maxValue);
  // Next(minValue, maxValue): floor(Sample() * (max - min)) + min.
  int32_t next(int32_t minValue, int32_t maxValue);
  double nextDouble() { return sample(); }
  double sample() { return double(internalSample()) * (1.0 / 2147483647.0); }

  // Mono's own Sample() for NextDouble parity: it uses a different scale.
  double sampleDouble() { return double(internalSample()) * (1.0 / 2147483647.0); }

 private:
  int32_t internalSample();
  std::vector<int32_t> seedArray_;
  int inext_ = 0;
  int inextp_ = 21;
};

}  // namespace bl
