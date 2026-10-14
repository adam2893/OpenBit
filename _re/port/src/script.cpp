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
  // The +N shorthand in the harness spec is a sign, not JSON: numbers carry it.
  o << "      \"deltas\": {\"karma\":" << ev.karma
    << ",\"discipline\":" << ev.discipline
    << ",\"happiness\":" << ev.happiness
    << ",\"smarts\":" << ev.smarts
    << ",\"looks\":" << ev.looks << "}\n";
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
  o << "    \"stats\": " << statsJson(h) << "\n";
  o << "  }";
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
    << ", \"money\": " << h.money;
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

bool runScript(Sim& sim, const ScriptSpec& spec, std::string* json, std::string* error) {
  Life life = sim.createLife(spec.female);
  if (!spec.country.empty() && !sim.pinCountry(life, spec.country)) {
    if (error) *error = "no country named \"" + spec.country + "\" in a16_countries-enc.json";
    return false;
  }
  if (spec.royal) sim.makeRoyal(life);
  if (spec.vampire) sim.makeVampire(life);
  if (spec.military) sim.makeMilitary(life);
  if (spec.sports) sim.makeAthlete(life);
  if (spec.business) sim.makeBusiness(life);
  if (spec.music) sim.makeMusician(life);
  if (spec.politics) sim.makePolitician(life);
  if (!spec.date.empty()) {
    // "MM-DD": the calendar/holiday dumb-scenario sets are gated on a real date,
    // so the script has to say which one. Unparsable text leaves the date unset.
    const size_t dash = spec.date.find('-');
    if (dash != std::string::npos)
      sim.setDate(std::atoi(spec.date.substr(0, dash).c_str()),
                  std::atoi(spec.date.substr(dash + 1).c_str()));
  }
  const std::string heroAtBirth = heroJson(life.hero);
  // The age-0 lines belong to the same stream as the age-ups, so they are drawn
  // here, once, before the loop.
  const std::vector<std::string> birth = sim.birthLines(life);

  std::vector<std::string> events;
  std::string newbornAtSetup;
  std::string deathType, deathSentence;
  for (int i = 0; i < spec.ages && life.alive; ++i) {
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
    if (!sim.ageUp(life, ev, forced)) break;  // no real row matched this year
    // The first age-up also runs SimHero.SetupEverythingElseForNewHero, a real
    // state change that belongs to no scenario. It is reported once as the
    // 'newborn' block; folding it into the deltas too would double-count it.
    if (i == 0) newbornAtSetup = heroJson(life.hero);
    Hero& h = life.hero;
    h.karma += ev.karma;
    h.discipline += ev.discipline;
    h.happiness += ev.happiness;
    h.smarts += ev.smarts;
    h.looks += ev.looks;
    int* stats[] = {&h.karma, &h.discipline, &h.happiness, &h.smarts, &h.looks};
    for (int* s : stats) {
      if (*s < 0) *s = 0;
      if (*s > 100) *s = 100;
    }

    events.push_back(eventJson(ev, h.age));

    if (sim.checkDeath(life, deathType, deathSentence)) life.alive = false;
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
