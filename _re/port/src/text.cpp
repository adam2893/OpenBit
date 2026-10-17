// src/text.cpp -- the text helpers bl/text.h declares.
//
// They were file-local to src/sim.cpp until a family moved to src/fam/, where a
// second copy of the rules would have been the alternative.
#include "bl/text.h"

#include <cstddef>
#include <string>

namespace bl {

// Capitalises the first letter: several pool cells are authored lowercase
// because the shipped log line embeds them mid-sentence.
std::string spyUpperFirst(std::string text) {
  for (size_t i = 0; i < text.size(); ++i) {
    const unsigned char c = (unsigned char)text[i];
    if (c == ' ') continue;
    if (c >= 'a' && c <= 'z') text[i] = char(c - 32);
    break;
  }
  return text;
}

}  // namespace bl
