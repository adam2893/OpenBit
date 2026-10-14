// bl/table.h -- reader for the game's decrypted content tables.
//
// Every file in _re/gamedata_plain/ is a Unity TextAsset exported by
// _re/index_plain.py: no file extension, column header on the first non-'#'
// line, values either RFC4180 comma CSV or tab TSV. Quoted fields may contain
// the delimiter and literal newlines.
#pragma once

#include "bl/dotnet_rng.h"

#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace bl {

class Table {
 public:
  static Table load(const std::string& path);  // throws std::runtime_error

  const std::string& name() const { return name_; }
  const std::vector<std::string>& columns() const { return columns_; }
  size_t size() const { return rows_.size(); }

  // Index of a column by exact header name, or -1 when the real table has none.
  int col(const std::string& column) const;
  bool has(const std::string& column) const { return col(column) >= 0; }

  const std::string& at(size_t row, const std::string& column) const;
  const std::string& at(size_t row, int column) const;
  // Numeric value of a cell; '$' and ',' are stripped so "\$25,000.00" reads as
  // 25000. A missing column reads as 0.
  double num(size_t row, const std::string& column) const;
  // The real tables mark booleans with a bare 'x'.
  bool flag(size_t row, const std::string& column) const;

 private:
  std::string name_;
  std::vector<std::string> columns_;
  std::vector<std::vector<std::string>> rows_;
};

// The game's own streams: Mono System.Random (see bl/dotnet_rng.h). Using it
// here is what makes a ported life comparable to a captured one, draw for draw.
class Rng {
 public:
  explicit Rng(uint64_t seed) : rng_(int32_t(seed)) {}
  uint32_t below(uint32_t n) { return uint32_t(rng_.next(int32_t(n))); }
  double unit() { return rng_.sample(); }
  bool chance(double p) { return unit() < p; }

 private:
  DotNetRng rng_;
};

// Commonness-Score weighted row pick. Falls back to a uniform pick when the
// column is absent or sums to zero -- the real a16_dumb-scenarios-* tables have
// no Commonness Score at all, so uniform is the honest reading there.
size_t weightedPick(const Table& table, const std::string& weightColumn, Rng& rng);

// Uniform pick over an explicit candidate list.
size_t pickFrom(const std::vector<size_t>& candidates, Rng& rng);

}  // namespace bl
