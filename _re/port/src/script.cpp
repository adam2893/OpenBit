#include "bl/script.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace bl {
namespace {

std::string slurp(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

// ---- script reader ------------------------------------------------------
// The script is a flat object of scalars plus one number array, so a keyed
// lookup plus a cursor is enough; this is not a general JSON parser.

void skipWs(const std::string& s, size_t& i) {
  while (i < s.size() && (unsigned char)s[i] <= ' ') ++i;
}

// Position of the value for "key", or npos.
// Index of the '}' that closes the object starting at s[i], skipping braces and
// brackets that appear inside strings. npos when the object is not closed.
size_t findObjectEnd(const std::string& s, size_t i) {
  int depth = 0;
  bool inStr = false;
  for (; i < s.size(); ++i) {
    const char c = s[i];
    if (inStr) {
      if (c == '\\') { ++i; continue; }
      if (c == '"') inStr = false;
      continue;
    }
    if (c == '"') inStr = true;
    else if (c == '{') ++depth;
    else if (c == '}' && --depth == 0) return i;
  }
  return std::string::npos;
}

size_t findValue(const std::string& s, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  size_t p = s.find(needle);
  if (p == std::string::npos) return p;
  p = s.find(':', p + needle.size());
  return p == std::string::npos ? p : p + 1;
}

// Reads a JSON string starting at the opening quote in s[i].
bool readString(const std::string& s, size_t& i, std::string& out) {
  if (i >= s.size() || s[i] != '"') return false;
  ++i;
  out.clear();
  while (i < s.size() && s[i] != '"') {
    if (s[i] == '\\' && i + 1 < s.size()) {
      const char e = s[++i];
      if (e == 'n') out += '\n';
      else if (e == 't') out += '\t';
      else if (e == 'r') out += '\r';
      else if (e != 'u') out += e;  // \uXXXX stays unhandled: no script needs it
      ++i;
      continue;
    }
    out += s[i++];
  }
  return i < s.size();
}

// Reads one integer and returns the new position, or npos when none is there.
size_t readInt(const std::string& s, size_t i, long long& out) {
  skipWs(s, i);
  char* end = nullptr;
  const long long v = std::strtoll(s.c_str() + i, &end, 10);
  if (end == s.c_str() + i) return std::string::npos;
  out = v;
  return size_t(end - s.c_str());
}

// ---- output writer ------------------------------------------------------

// '"' + the string + '"', with the escapes JSON requires. The real table
// strings are UTF-8 already, so bytes above 0x1f pass through untouched.
std::string jsonStr(const std::string& v) {
  std::string o = "\"";
  for (unsigned char c : v) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", unsigned(c));
          o += buf;
        } else {
          o += char(c);
        }
    }
  }
  o += '"';
  return o;
}

// A field no real row fills in is null, never an empty guess.
std::string jsonOrNull(const std::string& v) { return v.empty() ? "null" : jsonStr(v); }

std::string statsJson(const Hero& h) {
  std::ostringstream o;
  o << "{\"happiness\":" << h.happiness
    << ",\"health\":" << h.health
    << ",\"smarts\":" << h.smarts
    << ",\"looks\":" << h.looks
    << ",\"karma\":" << h.karma
    << ",\"discipline\":" << h.discipline << "}";
  return o.str();
}

std::string heroName(const Hero& h) {
  if (h.firstName.empty()) return h.lastName;
  if (h.lastName.empty()) return h.firstName;
  return h.firstName + " " + h.lastName;
}

// The lines the demo driver prints for one event, minus the indentation: the
// title/body, the CTA and the branch the hero took, the narrative that branch
// resolves to. Every string is the resolved real table content; the stat deltas
// have their own "deltas" object so they are not repeated here.
std::vector<std::string> logLines(const AgeUp& ev) {
  std::vector<std::string> out;
  // The year's family lifecycle lines come first: they belong to no scenario and
  // the log carries them ahead of the scenario's own line (trace_game9 age 7).
  for (const std::string& s : ev.extra) out.push_back(s);
  // The game's own LifeLog holds the outcome sentence of the branch the hero
  // took, not the prompt it was shown, so a branch's narrative is the whole log
  // line whenever there is one (verified against trace_fresh.json).
  if (!ev.narrative.empty()) {
    out.push_back(ev.narrative);
    return out;
  }
  if (!ev.title.empty() && !ev.body.empty()) out.push_back(ev.title + ": " + ev.body);
  else if (!ev.body.empty()) out.push_back(ev.body);
  else if (!ev.title.empty()) out.push_back(ev.title);
  if (!ev.choice.empty()) {
    out.push_back(ev.cta.empty() ? ev.choice : ev.cta + " -> " + ev.choice);
    if (!ev.narrative.empty()) out.push_back(ev.narrative);
  } else if (!ev.narrative.empty()) {
    out.push_back(ev.narrative);
  }
  return out;
}

// One "events" element. age is the hero's age after that age-up.
std::string eventJson(const AgeUp& ev, int age) {
  std::ostringstream o;
  o << "{\n";
  o << "      \"age\": " << age << ",\n";
  o << "      \"table\": " << jsonStr(ev.table) << ",\n";
  o << "      \"row\": " << (ev.row_id.empty() ? std::string("null") : ev.row_id) << ",\n";
  o << "      \"title\": " << jsonOrNull(ev.title) << ",\n";
  o << "      \"body\": " << jsonOrNull(ev.body) << ",\n";
  o << "      \"choice\": " << jsonOrNull(ev.choice) << ",\n";
  const std::vector<std::string> lines = logLines(ev);
  o << "      \"logLines\": [";
  for (size_t i = 0; i < lines.size(); ++i) {
    o << (i ? ",\n" : "\n") << "        " << jsonStr(lines[i]);
  }
  o << (lines.empty() ? "]," : "\n      ],") << "\n";
  // A token the resolver could not answer, with the table it came from (REPORT 207).
  o << "      \"residue\": [";
  for (size_t i = 0; i < ev.residue.size(); ++i) {
    o << (i ? ",\n" : "\n") << "        " << jsonStr(ev.residue[i]);
  }
  o << (ev.residue.empty() ? "],\n" : "\n      ],\n");
  // The +N shorthand in the harness spec is a sign, not JSON: numbers carry it.
  o << "      \"deltas\": {\"karma\":" << ev.karma
    << ",\"discipline\":" << ev.discipline
    << ",\"happiness\":" << ev.happiness
    << ",\"health\":" << ev.health
    << ",\"smarts\":" << ev.smarts
    << ",\"looks\":" << ev.looks << ",\"fame\":" << ev.fame << "},\"ailmentCost\":" << ev.ailmentCost << "\n";
  o << "    }";
  return o.str();
}

// The hero block is taken at birth, so "hero" is the identity + starting
// stats and "final" below is the end state; printing life.hero twice would
// make one of the two redundant.
std::string heroJson(const Hero& h) {
  std::ostringstream o;
  o << "{\n";
  o << "    \"name\": " << jsonOrNull(heroName(h)) << ",\n";
  o << "    \"gender\": " << jsonStr(h.female ? "female" : "male") << ",\n";
  o << "    \"country\": " << jsonOrNull(h.country) << ",\n";
  o << "    \"city\": " << jsonOrNull(h.city) << ",\n";
  o << "    \"region\": " << jsonOrNull(h.region) << ",\n";
  o << "    \"stats\": " << statsJson(h) << "\n";
  o << "  }";
  return o.str();
}

// The ex-lover array (REPORT.en.md 134). The scripted run is the only view a test
// has of the hero's own state, so what the family stores is printed here.
std::string exLoversJson(const Hero& h) {
  std::ostringstream o;
  o << "[";
  for (size_t i = 0; i < h.exLovers.size(); ++i) {
    const Hero::ExLover& ex = h.exLovers[i];
    o << (i ? ",\n      " : "\n      ") << "{\"name\": " << jsonStr(ex.name)
      << ", \"city\": " << jsonOrNull(ex.city)
      << ", \"female\": " << (ex.female ? "true" : "false") << ", \"age\": " << ex.age
      << ", \"status\": " << jsonOrNull(ex.status) << ", \"relationship\": "
      << ex.relationship << ", \"year\": " << ex.year << "}";
  }
  o << (h.exLovers.empty() ? "]" : "\n    ]");
  return o.str();
}

// The hero's own children (REPORT.en.md 146): the SimChild fields the port models.
std::string childrenJson(const Life& life) {
  std::ostringstream o;
  o << "[";
  for (size_t i = 0; i < life.children.size(); ++i) {
    const Child& c = life.children[i];
    o << (i ? ",\n      " : "\n      ") << "{\"name\": " << jsonOrNull(c.name)
      << ", \"female\": " << (c.female ? "true" : "false") << ", \"age\": " << c.age
      << ", \"alive\": " << (c.alive ? "true" : "false")
      << ", \"movedOut\": " << (c.movedOut ? "true" : "false")
      << ", \"otherParent\": " << jsonOrNull(c.otherParent)
      << ", \"withCurrentLover\": " << (c.withCurrentLover ? "true" : "false")
      << ", \"paternityAsked\": " << c.paternityAsked
      << ", \"bornAtHeroAge\": " << c.bornAtHeroAge
      << ", \"isHeroes\": " << (c.isHeroes ? "true" : "false")
      << ", \"city\": " << jsonOrNull(c.city)
      << ", \"city\": " << jsonOrNull(c.city) << "}";
  }
  o << (life.children.empty() ? "]" : "\n    ]");
  return o.str();
}

// The hero's relatives (REPORT.en.md 135). A sibling carries the game's own
// SimSibling state: whether it has moved out, who it married and at what age.
std::string familyJson(const Life& life) {
  std::ostringstream o;
  o << "[";
  for (size_t i = 0; i < life.family.size(); ++i) {
    const Person& p = life.family[i];
    o << (i ? ",\n      " : "\n      ") << "{\"relation\": " << jsonStr(p.relation)
      << ", \"name\": " << jsonOrNull(p.name) << ", \"female\": "
      << (p.female ? "true" : "false") << ", \"age\": " << p.age
      << ", \"alive\": " << (p.alive ? "true" : "false")
      << ", \"disease\": " << jsonOrNull(p.disease)
      << ", \"job\": " << jsonOrNull(p.job)
      << ", \"movedOut\": " << (p.movedOut ? "true" : "false")
      << ", \"spouse\": " << jsonOrNull(p.spouse)
      << ", \"ageAtMarriage\": " << p.ageAtMarriage << "}";
  }
  o << (life.family.empty() ? "]" : "\n    ]");
  return o.str();
}

std::string lifeJson(const ScriptSpec& spec, const std::string& heroAtBirth,
                     const std::string& newbornAtSetup, const Life& life,
                     const std::vector<std::string>& birth,
                     const std::vector<std::string>& events,
                     const std::string& deathType,
                     const std::string& deathSentence) {
  const Hero& h = life.hero;
  std::ostringstream o;
  o << "{\n";
  o << "  \"seed\": " << spec.seed << ",\n";
  o << "  \"hero\": " << heroAtBirth << ",\n";
  // The hero's state the moment SimHero.SetupEverythingElseForNewHero has run,
  // before that year's scenario impacts: the game's age-1 snapshot. Empty when
  // the run never reached an age-up.
  if (!newbornAtSetup.empty()) o << "  \"newborn\": " << newbornAtSetup << ",\n";
  o << "  \"birth\": [";
  for (size_t i = 0; i < birth.size(); ++i) o << (i ? ",\n" : "\n") << "    " << jsonStr(birth[i]);
  o << (birth.empty() ? "],\n" : "\n  ],\n");
  o << "  \"events\": [";
  for (size_t i = 0; i < events.size(); ++i) {
    o << (i ? ",\n" : "\n") << "    " << events[i];
  }
  o << (events.empty() ? "],\n" : "\n  ],\n");
  o << "  \"final\": {\"age\": " << h.age
    << ", \"alive\": " << (life.alive ? "true" : "false")
    << ", \"stats\": " << statsJson(h)
    << ", \"money\": " << h.money
    << ", \"retirementAge\": " << h.retirementAge
    << ", \"birthControl\": " << (h.onBirthControl ? "true" : "false")
    << ", \"employed\": " << (h.employed ? "true" : "false")
    << ", \"job\": " << jsonOrNull(h.job)
    << ", \"hoursPerWeek\": " << h.hoursPerWeek
    << ", \"license\": " << (h.hasLicense ? "true" : "false")
    << ", \"glasses\": " << (h.glasses ? "true" : "false")
    << ", \"city\": " << jsonOrNull(h.city)
    << ", \"loverStatus\": " << jsonOrNull(h.loverStatus)
    << ", \"loverName\": " << jsonOrNull(h.loverName)
    << ", \"relationship\": " << h.relationship
    << ", \"relationshipLow\": " << h.relationshipLow
    << ", \"identityQuestioned\": " << (h.identityQuestioned ? "true" : "false")
    << ", \"sexuality\": " << jsonOrNull(h.sexuality)
    << ", \"fame\": " << h.fame
    << ", \"exLovers\": " << exLoversJson(h)
    << ", \"children\": " << childrenJson(life)
    << ", \"family\": " << familyJson(life);
  // The death sentence is a real table row (a16_death-types-enc) and carries
  // <pronoun> / <body part>: it is resolved where it was drawn, so it must be
  // reported here to be comparable at all.
  if (!deathType.empty() || !deathSentence.empty())
    o << ", \"death\": {\"type\": " << jsonStr(deathType)
      << ", \"sentence\": " << jsonStr(deathSentence) << "}";
  o << "}\n}\n";
  return o.str();
}

}  // namespace

bool parseScript(const std::string& json, ScriptSpec& spec, std::string* error) {
  auto fail = [error](const std::string& m) {
    if (error) *error = m;
    return false;
  };
  size_t head = 0;
  skipWs(json, head);
  if (head >= json.size() || json[head] != '{') return fail("script is not a JSON object");

  size_t p = findValue(json, "seed");
  if (p != std::string::npos) {
    long long v = 0;
    if (readInt(json, p, v) == std::string::npos || v < 0) return fail("seed is not a positive integer");
    spec.seed = uint64_t(v);
  }

  p = findValue(json, "country");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (!readString(json, p, spec.country)) return fail("country is not a string");
  }

  p = findValue(json, "gender");
  if (p != std::string::npos) {
    skipWs(json, p);
    std::string g;
    if (!readString(json, p, g)) return fail("gender is not a string");
    if (g == "female") spec.female = true;
    else if (g == "male") spec.female = false;
    else return fail("gender must be \"male\" or \"female\", got \"" + g + "\"");
  }

  p = findValue(json, "royal");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.royal = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.royal = false;
    else return fail("royal is not a boolean");
  }

  p = findValue(json, "vampire");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.vampire = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.vampire = false;
    else return fail("vampire is not a boolean");
  }

  p = findValue(json, "military");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.military = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.military = false;
    else return fail("military is not a boolean");
  }

  p = findValue(json, "sports");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.sports = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.sports = false;
    else return fail("sports is not a boolean");
  }

  p = findValue(json, "business");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.business = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.business = false;
    else return fail("business is not a boolean");
  }

  // The car and the races (REPORT.en.md 75).
  {
    size_t q = findValue(json, "racing");
    if (q != std::string::npos) {
      skipWs(json, q);
      if (q + 4 <= json.size() && json.compare(q, 4, "true") == 0) spec.racing = true;
      else if (q + 5 <= json.size() && json.compare(q, 5, "false") == 0) spec.racing = false;
      else return fail("racing is not a boolean");
    }
  }

  p = findValue(json, "mafia");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.mafia = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.mafia = false;
    else return fail("mafia is not a boolean");
  }


  p = findValue(json, "landlord");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.landlord = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.landlord = false;
    else return fail("landlord is not a boolean");
  }

  p = findValue(json, "zoo");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.zoo = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.zoo = false;
    else return fail("zoo is not a boolean");
  }

  p = findValue(json, "model");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.model = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.model = false;
    else return fail("model is not a boolean");
  }

  p = findValue(json, "actor");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.actor = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.actor = false;
    else return fail("actor is not a boolean");
  }

  p = findValue(json, "finance");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.finance = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.finance = false;
    else return fail("finance is not a boolean");
  }

  p = findValue(json, "luxury");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.luxury = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.luxury = false;
    else return fail("luxury is not a boolean");
  }

  p = findValue(json, "boating");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.boating = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.boating = false;
    else return fail("boating is not a boolean");
  }

  p = findValue(json, "prison");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.prison = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.prison = false;
    else return fail("prison is not a boolean");
  }

  p = findValue(json, "office");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.office = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.office = false;
    else return fail("office is not a boolean");
  }

  p = findValue(json, "school");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.school = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.school = false;
    else return fail("school is not a boolean");
  }

  p = findValue(json, "podcast");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.podcast = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.podcast = false;
    else return fail("podcast is not a boolean");
  }

  p = findValue(json, "secret-agent");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.secret_agent = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.secret_agent = false;
    else return fail("secret-agent is not a boolean");
  }

  p = findValue(json, "relationship");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.relationship = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.relationship = false;
    else return fail("relationship is not a boolean");
  }

  p = findValue(json, "spy");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.spy = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.spy = false;
    else return fail("spy is not a boolean");
  }

  p = findValue(json, "fight");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.fight = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.fight = false;
    else return fail("fight is not a boolean");
  }

  p = findValue(json, "black-market");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.black_market = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.black_market = false;
    else return fail("black-market is not a boolean");
  }


  p = findValue(json, "casino");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.casino = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.casino = false;
    else return fail("casino is not a boolean");
  }

  p = findValue(json, "pet");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.pet = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.pet = false;
    else return fail("pet is not a boolean");
  }

  p = findValue(json, "outdoor");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.outdoor = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.outdoor = false;
    else return fail("outdoor is not a boolean");
  }
  p = findValue(json, "hustler");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.hustler = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.hustler = false;
    else return fail("hustler is not a boolean");
  }
  p = findValue(json, "astronaut");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.astronaut = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.astronaut = false;
    else return fail("astronaut is not a boolean");
  }
  p = findValue(json, "cult");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.cult = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.cult = false;
    else return fail("cult is not a boolean");
  }

  p = findValue(json, "dealer");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.dealer = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.dealer = false;
    else return fail("dealer is not a boolean");
  }

  p = findValue(json, "producer");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.producer = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.producer = false;
    else return fail("producer is not a boolean");
  }

  p = findValue(json, "soundcloud");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.soundcloud = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.soundcloud = false;
    else return fail("soundcloud is not a boolean");
  }

  p = findValue(json, "scout");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.scout = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.scout = false;
    else return fail("scout is not a boolean");
  }

  p = findValue(json, "composer");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.composer = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.composer = false;
    else return fail("composer is not a boolean");
  }

  p = findValue(json, "club-takeover");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.club_takeover = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.club_takeover = false;
    else return fail("club-takeover is not a boolean");
  }

  p = findValue(json, "instrument");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.instrument = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.instrument = false;
    else return fail("instrument is not a boolean");
  }

  p = findValue(json, "music");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.music = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.music = false;
    else return fail("music is not a boolean");
  }

  p = findValue(json, "politics");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.politics = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.politics = false;
    else return fail("politics is not a boolean");
  }

  p = findValue(json, "date");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (!readString(json, p, spec.date)) return fail("date is not a string");
  }

  p = findValue(json, "cash");
  if (p != std::string::npos) {
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) ++p;
    long long v = 0;
    if (readInt(json, p, v) == std::string::npos) return fail("cash is not a number");
    if (v < 0) return fail("cash must not be negative");
    spec.cash = v;
  }
  p = findValue(json, "engaged");
  if (p != std::string::npos) {
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) ++p;
    if (json.compare(p, 4, "true") == 0) spec.engaged = true;
    else if (json.compare(p, 5, "false") != 0) return fail("engaged is not a boolean");
  }
  p = findValue(json, "dating");
  if (p != std::string::npos) {
    while (p < json.size() && (json[p] == ' ' || json[p] == '	')) ++p;
    if (json.compare(p, 4, "true") == 0) spec.dating = true;
    else if (json.compare(p, 5, "false") != 0) return fail("dating is not a boolean");
  }
  p = findValue(json, "sexuality");
  if (p != std::string::npos) {
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) ++p;
    const size_t q = json.find('"', p + 1);
    if (p < json.size() && json[p] == '"' && q != std::string::npos) {
      spec.sexuality = json.substr(p + 1, q - p - 1);
    } else {
      return fail("sexuality is not a string");
    }
  }
  p = findValue(json, "ex");
  if (p != std::string::npos) {
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) ++p;
    if (json.compare(p, 4, "true") == 0) spec.ex = true;
    else if (json.compare(p, 5, "false") != 0) return fail("ex is not a boolean");
  }
  p = findValue(json, "children");
  if (p != std::string::npos) {
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) ++p;
    long long v = 0;
    if (readInt(json, p, v) == std::string::npos) return fail("children is not a number");
    if (v < 0 || v > 12) return fail("children must be between 0 and 12");
    spec.children = int(v);
  }
  p = findValue(json, "birthControl");
  if (p != std::string::npos) {
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) ++p;  // findValue stops after ':'
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) spec.birthControl = true;
    else if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) spec.birthControl = false;
    else return fail("birthControl is not a boolean");
  }
  p = findValue(json, "fame");
  if (p != std::string::npos) {
    long long v = 0;
    if (readInt(json, p, v) == std::string::npos) return fail("fame is not a number");
    if (v < 0 || v > 100) return fail("fame must be between 0 and 100");
    spec.fame = int(v);
  }
  p = findValue(json, "ages");
  if (p != std::string::npos) {
    long long v = 0;
    if (readInt(json, p, v) == std::string::npos) return fail("ages is not a number");
    // A life that stops matching rows or dies ends the loop early, so this only
    // bounds a typo.
    if (v < 0 || v > 1000) return fail("ages must be between 0 and 1000");
    spec.ages = int(v);
  }

  p = findValue(json, "choices");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p >= json.size() || json[p] != '[') return fail("choices is not an array");
    ++p;
    while (true) {
      skipWs(json, p);
      if (p >= json.size()) return fail("choices array is not closed");
      if (json[p] == ']') break;
      long long v = 0;
      const size_t next = readInt(json, p, v);
      if (next == std::string::npos) return fail("choices holds something that is not a number");
      if (v < 0) return fail("choices entries are 0-based indexes and cannot be negative");
      spec.choices.push_back(int(v));
      p = next;
      skipWs(json, p);
      if (p < json.size() && json[p] == ',') ++p;
      else if (p < json.size() && json[p] == ']') break;
      else return fail("choices array has a stray value");
    }
  }
  p = findValue(json, "push");
  if (p != std::string::npos) {
    skipWs(json, p);
    if (p >= json.size() || json[p] != '[') return fail("push is not an array");
    ++p;
    while (true) {
      skipWs(json, p);
      if (p >= json.size()) return fail("push array is not closed");
      if (json[p] == ']') break;
      if (json[p] == '{') {
        const size_t end = findObjectEnd(json, p);
        if (end == std::string::npos) return fail("push entry is not a closed object");
        const std::string elem = json.substr(p, end - p + 1);
        PushSpec ps;
        size_t q = findValue(elem, "scenario");
        if (q != std::string::npos) {
          // Scripted-event form: the scenario class the game itself presented.
          skipWs(elem, q);
          if (!readString(elem, q, ps.scenario) || ps.scenario.empty())
            return fail("push \"scenario\" must be a non-empty string");
          q = findValue(elem, "choice");
          long long c = -1;
          if (q != std::string::npos && readInt(elem, q, c) != std::string::npos && c >= 0)
            ps.choice = int(c);
          spec.push.push_back(ps);
          p = end + 1;
          skipWs(json, p);
          if (p < json.size() && json[p] == ',') ++p;
          else if (p < json.size() && json[p] == ']') break;
          else return fail("push array has a stray value");
          continue;
        }
        q = findValue(elem, "table");
        skipWs(elem, q);
        if (q == std::string::npos || !readString(elem, q, ps.table))
          return fail("push entry needs a string \"table\" or \"scenario\"");
        q = findValue(elem, "row");
        long long v = 0;
        if (q == std::string::npos || readInt(elem, q, v) == std::string::npos || v < 1)
          return fail("push entry needs a 1-based \"row\"");
        ps.row = v;
        spec.push.push_back(ps);
        p = end + 1;
      } else if (json.compare(p, 4, "null") == 0) {
        spec.push.push_back(PushSpec());
        p += 4;
      } else {
        return fail("push entry must be an object or null");
      }
      skipWs(json, p);
      if (p < json.size() && json[p] == ',') ++p;
      else if (p < json.size() && json[p] == ']') break;
      else return fail("push array has a stray value");
    }
  }
  return true;
}

// The life every scripted run and every played run starts from: the setup switches
// (REPORT.en.md 47, 48, 82-85 and the trade families) applied in the order runScript
// has always applied them, then the two draws the loop below depends on.
bool setupLife(Sim& sim, const ScriptSpec& spec, Life& life, std::string* heroAtBirth,
               std::vector<std::string>* birth, std::string* error) {

  life = sim.createLife(spec.female);
  if (!spec.country.empty() && !sim.pinCountry(life, spec.country)) {
    if (error) *error = "no country named \"" + spec.country + "\" in a16_countries-enc.json";
    return false;
  }
  if (spec.royal) sim.makeRoyal(life);
  if (spec.vampire) sim.makeVampire(life);
  if (spec.military) sim.makeMilitary(life);
  if (spec.sports) sim.makeAthlete(life);
  if (spec.business) sim.makeBusiness(life);
  if (spec.racing) sim.makeRacing(life);
  if (spec.actor) sim.makeActor(life);
  if (spec.finance) sim.makeFinance(life);
  if (spec.model) sim.makeModel(life);
  if (spec.luxury) sim.makeLuxury(life);
  if (spec.boating) sim.makeBoating(life);
  if (spec.prison) sim.makePrisoner(life);
  if (spec.office) sim.makeOffice(life);
  if (spec.school) sim.makeSchool(life);
  if (spec.podcast) sim.makePodcast(life);
  if (spec.music) sim.makeMusician(life);
  if (spec.politics) sim.makePolitician(life);
  unsigned miscMask = 0;
  if (spec.mafia) miscMask |= 1u << 0;
  if (spec.dealer) miscMask |= 1u << 1;
  if (spec.hustler) miscMask |= 1u << 2;
  if (spec.landlord) miscMask |= 1u << 3;
  if (spec.zoo) miscMask |= 1u << 4;
  if (spec.model) miscMask |= 1u << 5;
  if (spec.actor) miscMask |= 1u << 6;
  if (spec.secret_agent) miscMask |= 1u << 7;
  if (spec.casino) miscMask |= 1u << 8;
  if (miscMask) sim.makeMisc(life, miscMask);
  if (spec.relationship) sim.makeRelationships(life);
  if (spec.spy) sim.makeSecretAgent(life);
  if (spec.fight) sim.makeFighter(life);
  if (spec.black_market) sim.makeBlackMarketeer(life);
  if (spec.casino) sim.makeCasino(life);
  if (spec.cult) sim.makeCult(life);
  if (spec.dealer) sim.makeDealer(life);
  if (spec.mafia) sim.makeMafia(life);
  if (spec.zoo) sim.makeZoo(life);
  if (spec.landlord) sim.makeLandlord(life);
  if (spec.pet) sim.makePet(life);
  if (spec.outdoor) sim.makeOutdoor(life);
  if (spec.hustler) sim.makeHustler(life);
  if (spec.astronaut) sim.makeAstronaut(life);
  if (spec.producer) sim.makeTrade(life, 1);
  if (spec.soundcloud) sim.makeTrade(life, 2);
  if (spec.scout) sim.makeTrade(life, 3);
  if (spec.composer) sim.makeTrade(life, 4);
  if (spec.club_takeover) sim.makeTrade(life, 5);
  if (spec.instrument) sim.makeInstrument(life);
  if (!spec.date.empty()) {
    // "MM-DD": the calendar/holiday dumb-scenario sets are gated on a real date,
    // so the script has to say which one. Unparsable text leaves the date unset.
    const size_t dash = spec.date.find('-');
    if (dash != std::string::npos)
      sim.setDate(std::atoi(spec.date.substr(0, dash).c_str()),
                  std::atoi(spec.date.substr(dash + 1).c_str()));
  }
  if (spec.fame > 0) life.hero.fame = float(spec.fame);
  if (spec.birthControl) life.hero.onBirthControl = true;
  // The children fixture (REPORT.en.md 146): the array exists before any card that
  // reads it. Each one is a person of the hero's own surname, born before the run.
  if (spec.cash >= 0) life.hero.money = spec.cash;
  life.hero.sexuality = spec.sexuality;
  if (spec.engaged || spec.dating) {
    life.hero.loverName = "Sam Ellery";
    life.hero.loverFemale = true;
    life.hero.loverStatus = spec.engaged ? "engaged" : "dating";
    life.hero.relationship = spec.engaged ? 70 : 75;
    life.hero.relationshipOn = true;
    // The proposal card prints 'Years Together' (REPORT.en.md 303), so the
    // fixture stamps the pair's start age where the sim would have.
    life.hero.loverSinceName = life.hero.loverName;
    life.hero.loverSinceAge = life.hero.age;
  }
  if (spec.ex) {
    bl::Hero::ExLover ended;
    ended.name = "Robin Averill";
    ended.female = true;
    ended.age = life.hero.age + 2;
    ended.status = "dating";
    ended.relationship = 12;
    ended.year = life.hero.age;
    ended.city = life.hero.city;
    life.hero.exLovers.push_back(ended);
  }
  for (int c = 0; c < spec.children; ++c) {
    bl::Child child;
    child.female = (c % 2) == 1;
    // The child is named from the game's own pool (REPORT.en.md 148): the court-order
    // card prints the name in its body, and a card with a hole in it is worse than
    // no card.
    child.name = sim.firstNameFor(child.female);
    child.lastName = life.hero.lastName;
    // Born before the run, and young: a child older than the hero would be absurd,
    // and the court-order card prints its age in the body.
    child.age = 0;
    child.bornAtHeroAge = 20 + 4 * c;  // staggered, so a life has children of two ages
    if (spec.ex) child.otherParent = "Robin Averill";  // the seeded ex is the parent
    child.city = life.hero.city;
    life.children.push_back(child);
  }
  *heroAtBirth = heroJson(life.hero);
  // The age-0 lines belong to the same stream as the age-ups, so they are drawn
  // here, once, before the loop.
  *birth = sim.birthLines(life);
  return true;
}

bool runScript(Sim& sim, const ScriptSpec& spec, std::string* json, std::string* error) {
  Life life;
  std::string heroAtBirth;
  std::vector<std::string> birth;
  if (!setupLife(sim, spec, life, &heroAtBirth, &birth, error)) return false;

  std::vector<std::string> events;
  std::string newbornAtSetup;
  std::string deathType, deathSentence;
  // `ages` counts years lived, not cards shown (REPORT.en.md 133): a year that owes
  // a second card drains it on its own ageUp call, which does not advance the age.
  // `i` stays the card index, so choices[]/push[] keep their one-entry-per-card
  // alignment with the bench's traces.
  int lived = 0;
  for (int i = 0; lived < spec.ages && life.alive; ++i) {
    const int forced = i < int(spec.choices.size()) ? spec.choices[i] : -1;
    // push[i] (when present and non-empty) replays one exact real row; a clear
    // call keeps the override from leaking into later years.
    if (i < int(spec.push.size())) {
      const PushSpec& ps = spec.push[size_t(i)];
      if (!ps.scenario.empty()) sim.forceScenarioClass(ps.scenario, ps.choice);
      else sim.forceScenario(ps.table, ps.row);
    }
    else sim.forceScenario(std::string(), 0);
    AgeUp ev;
    const int ageBefore = life.hero.age;
    if (!sim.ageUp(life, ev, forced)) break;  // no real row matched this year
    if (life.hero.age != ageBefore) ++lived;  // a pending card leaves the age alone
    // The first age-up also runs SimHero.SetupEverythingElseForNewHero, a real
    // state change that belongs to no scenario. It is reported once as the
    // 'newborn' block; folding it into the deltas too would double-count it.
    if (i == 0) newbornAtSetup = heroJson(life.hero);
    Hero& h = life.hero;
    h.karma += ev.karma;
    h.discipline += ev.discipline;
    h.happiness += ev.happiness;
    // The hero's own ailments cost health outside the card deltas (REPORT 247).
    h.health += ev.health - ev.ailmentCost;
    h.smarts += ev.smarts;
    h.looks += ev.looks;
    h.fame = std::max(0.0f, std::min(100.0f, h.fame + float(ev.fame)));
    int* stats[] = {&h.karma, &h.discipline, &h.happiness, &h.health, &h.smarts, &h.looks};
    for (int* s : stats) {
      if (*s < 0) *s = 0;
      if (*s > 100) *s = 100;
    }

    events.push_back(eventJson(ev, h.age));

    if (sim.checkDeath(life, deathType, deathSentence)) life.alive = false;
    // The game closes the hero's last line with the estate (REPORT.en.md 252):
    // "I died while sleeping peacefully. My net worth at the time of my death was
    // $388,327."
    if (!life.alive && !deathSentence.empty()) {
      const long long worth = life.hero.money;
      std::string digits = std::to_string(worth < 0 ? -worth : worth);
      for (long long at = (long long)digits.size() - 3; at > 0; at -= 3)
        digits.insert(size_t(at), ",");
      deathSentence += std::string(" My net worth at the time of my death was ") +
                       (worth < 0 ? "-$" : "$") + digits + ".";
    }
  }

  if (json)
    *json = lifeJson(spec, heroAtBirth, newbornAtSetup, life, birth, events,
                     deathType, deathSentence);
  return true;
}

int runScriptMode(const std::string& reDir, const std::string& scriptPath) {
  std::string text;
  try {
    text = slurp(scriptPath);
  } catch (const std::exception& e) {
    std::cerr << "script: " << e.what() << "\n";
    return 2;
  }

  ScriptSpec spec;
  std::string error;
  if (!parseScript(text, spec, &error)) {
    std::cerr << "script: " << error << "\n";
    return 2;
  }

  Sim sim(reDir, spec.seed);
  if (!sim.load(&error)) {
    std::cerr << "load failed: " << error << "\n";
    return 1;
  }

  std::string json;
  if (!runScript(sim, spec, &json, &error)) {
    std::cerr << "script: " << error << "\n";
    return 2;
  }
  std::cout << json;
  return 0;
}

}  // namespace bl
