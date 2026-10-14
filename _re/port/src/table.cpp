#include "bl/table.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace bl {
namespace {

std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open table: " + path);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

std::string baseName(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

// RFC4180-ish scanner: quoted fields may hold the delimiter and newlines, ""
// is a literal quote, a '\r' outside quotes is dropped.
std::vector<std::vector<std::string>> splitRows(const std::string& text, char delim) {
  std::vector<std::vector<std::string>> out;
  std::vector<std::string> row;
  std::string field;
  bool quoted = false, pending = false;
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (quoted) {
      if (c == '"') {
        if (i + 1 < text.size() && text[i + 1] == '"') { field += '"'; ++i; }
        else quoted = false;
      } else {
        field += c;
      }
      continue;
    }
    if (c == '"') { quoted = true; pending = true; continue; }
    if (c == delim) { row.push_back(field); field.clear(); pending = true; continue; }
    if (c == '\n') { row.push_back(field); field.clear(); out.push_back(row); row.clear(); pending = false; continue; }
    if (c == '\r') continue;
    field += c;
    pending = true;
  }
  if (pending) { row.push_back(field); out.push_back(row); }
  return out;
}

bool isComment(const std::vector<std::string>& row) {
  return !row.empty() && !row[0].empty() && row[0][0] == '#';
}

bool isBlank(const std::vector<std::string>& row) {
  if (row.empty()) return true;
  for (const std::string& f : row) if (!f.empty()) return false;
  return true;
}

}  // namespace

Table Table::load(const std::string& path) {
  const std::string text = readFile(path);

  // The header line decides the dialect; the real corpus is either one or the
  // other, never a mix.
  size_t tabs = 0, commas = 0;
  for (size_t i = 0; i < text.size() && text[i] != '\n'; ++i) {
    if (text[i] == '\t') ++tabs;
    if (text[i] == ',') ++commas;
  }
  const char delim = tabs > commas ? '\t' : ',';
  const std::vector<std::vector<std::string>> all = splitRows(text, delim);

  size_t header = 0;
  while (header < all.size() && (isComment(all[header]) || isBlank(all[header]))) ++header;
  if (header >= all.size()) throw std::runtime_error("no header row in table: " + path);

  Table t;
  t.name_ = baseName(path);
  t.columns_ = all[header];
  for (size_t i = header + 1; i < all.size(); ++i) {
    if (isComment(all[i]) || isBlank(all[i])) continue;
    std::vector<std::string> row = all[i];
    // Some real files carry extra trailing cells past the header; pad short rows
    // and keep long ones intact rather than dropping data.
    if (row.size() < t.columns_.size()) row.resize(t.columns_.size());
    t.rows_.push_back(std::move(row));
  }
  return t;
}

int Table::col(const std::string& column) const {
  for (size_t i = 0; i < columns_.size(); ++i) {
    if (columns_[i] == column) return int(i);
  }
  return -1;
}

const std::string& Table::at(size_t row, const std::string& column) const {
  // A column the real table does not have reads as absent, the same way num()
  // already reads it as 0 and flag() as false. Tables differ in shape (the
  // scripted-scenario table has Title/Body/ChoiceList but no CTA, the dumb
  // tables the other way round) and the Phase 1 bench replays rows out of
  // tables the port was never wired to, so a missing column must not abort the
  // run mid-life; the caller sees "" and decides.
  static const std::string kEmpty;
  const int c = col(column);
  if (c < 0) return kEmpty;
  return at(row, c);
}

const std::string& Table::at(size_t row, int column) const {
  if (row >= rows_.size() || column < 0 || size_t(column) >= rows_[row].size()) {
    throw std::out_of_range("row/column out of range in " + name_);
  }
  return rows_[row][size_t(column)];
}

double Table::num(size_t row, const std::string& column) const {
  if (!has(column)) return 0.0;
  std::string v = at(row, column);
  std::string digits;
  for (char c : v) {
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') digits += c;
  }
  return digits.empty() ? 0.0 : std::atof(digits.c_str());
}

bool Table::flag(size_t row, const std::string& column) const {
  if (!has(column)) return false;
  const std::string v = at(row, column);
  return v == "x" || v == "X" || v == "true" || v == "True" || v == "1" || v == "yes";
}



size_t pickFrom(const std::vector<size_t>& candidates, Rng& rng) {
  if (candidates.empty()) return size_t(-1);
  return candidates[rng.below(uint32_t(candidates.size()))];
}

size_t weightedPick(const Table& table, const std::string& weightColumn, Rng& rng) {
  if (table.size() == 0) return size_t(-1);
  double total = 0.0;
  if (table.has(weightColumn)) {
    for (size_t r = 0; r < table.size(); ++r) total += table.num(r, weightColumn);
  }
  if (total <= 0.0) return rng.below(uint32_t(table.size()));
  double roll = rng.unit() * total;
  for (size_t r = 0; r < table.size(); ++r) {
    roll -= table.num(r, weightColumn);
    if (roll <= 0.0) return r;
  }
  return table.size() - 1;
}

}  // namespace bl
