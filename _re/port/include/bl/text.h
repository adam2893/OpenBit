// bl/text.h -- the small text and table helpers more than one family needs.
//
// These are defined in src/sim.cpp beside the tables they were written for; the
// declarations are here so a family that has moved to its own translation unit
// (src/fam/*.cpp) can call them without a second copy of the rules.
#pragma once

#include "bl/dotnet_rng.h"
#include "bl/table.h"

#include <cstddef>
#include <string>
#include <vector>

namespace bl {

// Cell hygiene: the tables are full of stray spaces and mixed case.
std::string lower(std::string s);
std::string trim(const std::string& s);

// A comma or slash separated cell split in place.
void splitOn(const std::string& s, char sep, std::vector<std::string>& out);

// One weighted draw from a pool of rows against a score column.
size_t weightedPickOf(const Table& t, const std::string& col,
                      const std::vector<size_t>& pool, Rng& rng);

// 'the hero' -> 'I' rewrites used by the cards that quote the hero back at
// himself (racing's own post-race line among them).
std::string toFirstPerson(const std::string& s);

// First letter up, rest untouched ('the dog' -> 'The dog').
std::string spyUpperFirst(std::string text);

// Possessive of a name: 'Alex' -> "Alex's", 'James' -> "James'".
std::string llPossessive(const std::string& name);

// Membership in one of the comma list cells ('Dog, Cat, Horse').
bool llInList(const std::string& list, const std::string& value);

// 'a'/'an' for a word, and a quantifier-safe article for a resolved cell that may
// already carry its own ('a pack of cigarettes').
std::string articleFor(const std::string& word);
std::string withArticle(const std::string& text, bool wantsArticle);

}  // namespace bl
