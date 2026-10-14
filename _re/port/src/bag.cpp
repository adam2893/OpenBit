#include "bl/bag.h"

#include <cstdlib>
#include <sstream>

namespace bl {
namespace {

void skipWs(const std::string& s, size_t& i) {
  while (i < s.size() && (unsigned char)s[i] <= ' ') ++i;
}

size_t findKey(const std::string& s, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  size_t p = s.find(needle);
  if (p == std::string::npos) return p;
  p = s.find(':', p + needle.size());
  return p == std::string::npos ? p : p + 1;
}

bool readStr(const std::string& s, size_t& i, std::string* out) {
  skipWs(s, i);
  if (i >= s.size() || s[i] != '"') return false;
  ++i;
  out->clear();
  while (i < s.size() && s[i] != '"') {
    if (s[i] == '\\' && i + 1 < s.size()) {
      const char e = s[++i];
      if (e == 'n') *out += '\n';
      else if (e == 't') *out += '\t';
      else if (e == 'r') *out += '\r';
      else *out += e;
      ++i;
      continue;
    }
    *out += s[i++];
  }
  if (i >= s.size()) return false;
  ++i;
  return true;
}

bool readNum(const std::string& s, size_t& i, long long* out) {
  skipWs(s, i);
  char* end = nullptr;
  const long long v = std::strtoll(s.c_str() + i, &end, 10);
  if (end == s.c_str() + i) return false;
  *out = v;
  i = size_t(end - s.c_str());
  return true;
}

// Jumps over one JSON value so a scan can continue past keys we do not read.
void skipValue(const std::string& s, size_t& i);

void skipContainer(const std::string& s, size_t& i, char open, char close) {
  int depth = 0;
  bool inStr = false;
  for (; i < s.size(); ++i) {
    const char c = s[i];
    if (inStr) {
      if (c == '\\') ++i;
      else if (c == '"') inStr = false;
      continue;
    }
    if (c == '"') inStr = true;
    else if (c == open) ++depth;
    else if (c == close && --depth <= 0) { ++i; return; }
  }
}

void skipValue(const std::string& s, size_t& i) {
  skipWs(s, i);
  if (i >= s.size()) return;
  if (s[i] == '{') { skipContainer(s, i, '{', '}'); return; }
  if (s[i] == '[') { skipContainer(s, i, '[', ']'); return; }
  if (s[i] == '"') { std::string tmp; readStr(s, i, &tmp); return; }
  while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']') ++i;
}

// Parses "[[name,weight],...]" starting at the opening bracket.
bool parseBag(const std::string& s, size_t i, std::vector<std::pair<std::string, int>>* out) {
  skipWs(s, i);
  if (i >= s.size() || s[i] != '[') return false;
  ++i;
  for (;;) {
    skipWs(s, i);
    if (i >= s.size()) return false;
    if (s[i] == ']') { return true; }
    if (s[i] == ',') { ++i; continue; }
    if (s[i] != '[') return false;
    ++i;
    std::string name;
    if (!readStr(s, i, &name)) return false;
    long long w = 0;
    skipWs(s, i);
    if (i < s.size() && s[i] == ',') ++i;
    if (!readNum(s, i, &w)) return false;
    out->push_back({name, int(w)});
    skipWs(s, i);
    if (i < s.size() && s[i] == ']') ++i;
  }
}

// Splits the top-level object array of "cases".
bool splitCases(const std::string& s, std::vector<std::string>* out) {
  const size_t key = findKey(s, "cases");
  if (key == std::string::npos) return false;
  size_t i = key;
  skipWs(s, i);
  if (i >= s.size() || s[i] != '[') return false;
  ++i;
  for (;;) {
    skipWs(s, i);
    if (i >= s.size()) return false;
    if (s[i] == ']') return true;
    if (s[i] == ',') { ++i; continue; }
    if (s[i] != '{') return false;
    const size_t start = i;
    skipContainer(s, i, '{', '}');
    out->push_back(s.substr(start, i - start));
  }
}

}  // namespace

void WeightedBag::add(const std::string& name, int weight) {
  if (weight <= 0) return;
  const size_t entry = entries_.size();
  entries_.push_back({name, weight});
  for (int k = 0; k < weight; ++k) flat_.push_back(entry);
}

void WeightedBag::clear() {
  entries_.clear();
  flat_.clear();
}

bool WeightedBag::pick(size_t slot, std::string* out) const {
  if (slot >= flat_.size()) return false;
  *out = at(slot);
  return true;
}

bool loadBagVectors(const std::string& json, std::vector<BagCase>* out, std::string* error) {
  std::vector<std::string> objs;
  if (!splitCases(json, &objs)) {
    *error = "no \"cases\" array in bag vector file";
    return false;
  }
  for (const std::string& o : objs) {
    BagCase c;
    size_t p = findKey(o, "bag");
    if (p == std::string::npos || !parseBag(o, p, &c.bag)) {
      *error = "a case has no \"bag\" array";
      return false;
    }
    p = findKey(o, "expect");
    if (p == std::string::npos || !readStr(o, p, &c.expect)) {
      *error = "a case has no \"expect\" name";
      return false;
    }
    long long v = 0;
    p = findKey(o, "age");
    if (p != std::string::npos && readNum(o, p, &v)) c.age = int(v);
    p = findKey(o, "index");
    if (p != std::string::npos && readNum(o, p, &v)) c.index = v;
    p = findKey(o, "rngValue");
    if (p != std::string::npos && readNum(o, p, &v)) c.rngValue = v;
    p = findKey(o, "source");
    if (p != std::string::npos) readStr(o, p, &c.source);
    out->push_back(std::move(c));
  }
  return true;
}

BagCheckResult checkBagVectors(const std::vector<BagCase>& cases) {
  BagCheckResult r;
  for (const BagCase& c : cases) {
    ++r.cases;
    WeightedBag bag;
    for (const auto& e : c.bag) bag.add(e.first, e.second);
    std::ostringstream tag;
    tag << (c.source.empty() ? std::string("case") : c.source) << " age " << c.age;
    if (bag.size() == 0) {
      r.failures.push_back(tag.str() + ": empty bag, nothing to draw");
      continue;
    }
    if (c.index >= 0) {
      ++r.withIndex;
      if (c.index >= static_cast<long long>(bag.size())) {
        r.failures.push_back(tag.str() + ": recorded index " + std::to_string(c.index) +
                             " is outside the bag of " + std::to_string(bag.size()));
        continue;
      }
      const std::string got = bag.at(size_t(c.index));
      if (got == c.expect) {
        ++r.matched;
      } else {
        r.failures.push_back(tag.str() + ": index " + std::to_string(c.index) + " -> " + got +
                             ", game built " + c.expect);
      }
      continue;
    }
    if (c.rngValue >= 0) {
      // The sample must be a legal slot when it indexes the bag directly.
      if (c.rngValue < static_cast<long long>(bag.size()) &&
          bag.at(size_t(c.rngValue)) == c.expect) {
        ++r.withIndex;
        ++r.matched;
      } else {
        r.failures.push_back(tag.str() + ": rng value " + std::to_string(c.rngValue) +
                             " does not select " + c.expect +
                             " (bag " + std::to_string(bag.size()) + ")");
      }
      continue;
    }
    // No draw recorded: the bag itself must still repeat each name by weight.
    bool ok = true;
    size_t slot = 0;
    for (size_t e = 0; e < bag.entries() && ok; ++e) {
      for (int k = 0; k < bag.weightOf(e); ++k) {
        if (bag.at(slot++) != bag.nameOf(e)) ok = false;
      }
    }
    if (ok && slot == bag.size()) ++r.matched;
    else r.failures.push_back(tag.str() + ": weight copies are not contiguous in the bag");
  }
  return r;
}

}  // namespace bl
