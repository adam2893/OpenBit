// bl/script.h -- deterministic scripted-run mode: blsim --script <file.json>.
//
// Port side of the Phase 1 differential harness (PORT_PLAN.en.md section 3,
// "Port side: blsim --script emits the same JSON"): one seed in, one JSON
// object out, byte-identical on every rerun, so tools/diff.py has something
// stable to compare against the original game's trace.
#pragma once

#include "bl/sim.h"

#include <cstdint>
#include <string>
#include <vector>

namespace bl {

// One scripted run, read from the script JSON:
//
//   {"seed": 12345, "country": "Finland", "gender": "male",
//    "ages": 3, "choices": [0, 1, 2]}
//
// Every key is optional. seed defaults to 0, an empty country means the seed
// picks one the way createLife() always has, gender defaults to "male", ages to
// 0. choices[i] is the 0-based index of the branch taken at age-up i -- an
// index into the choices a16_dumb-scenarios-universal-enc actually fills in for
// that row, not a Choice column number. A missing entry, an out-of-range index
// or a row with no filled choice falls back to the seeded RNG pick.
// "push": [ {"table": "a16_dumb-scenarios-universal-enc", "row": 130} | null, ... ]
// "date": "10-31"
//
// push[i] names the exact real table row the port must replay for age-up i
// instead of drawing one, which is how the Phase 1 bench feeds the port the
// scenario the game actually picked. A null entry (or a shorter array) draws as
// usual, so a script can override only the years that matter.
//
// The same entry may instead name a scenario CLASS the game presented as a
// scripted event rather than a table row:
//   {"scenario": "ScenarioChildhoodVaccination", "choice": 2}
// choice is the 0-based button index; -1 (or absent) lets the RNG pick one.
struct PushSpec {
  std::string table;
  long long row = 0;  // 1-based data row in that table
  std::string scenario;  // real il2cpp scenario class, e.g. "ScenarioFriendMeet"
  int choice = -1;       // 0-based button index for that scenario, -1 = RNG
};

struct ScriptSpec {
  uint64_t seed = 0;
  std::string country;
  bool female = false;
  // Born into royalty (REPORT.en.md 47): the game rolls this at life setup and
  // the port exposes it as a switch instead of guessing the probability.
  bool royal = false;
  // An undead life (REPORT.en.md 48). The game reaches it through the vampire
  // encounter and conversion screens; the port takes it from the switch.
  bool vampire = false;
  bool military = false;
  bool sports = false;
  bool business = false;
  bool music = false;
  bool politics = false;
  bool mafia = false;
  bool dealer = false;
  bool hustler = false;
  bool astronaut = false;
  // The car and the races (REPORT.en.md 75).
  bool racing = false;
  bool landlord = false;
  bool zoo = false;
  bool model = false;
  // The luxuries (REPORT.en.md 79).
  bool luxury = false;
  // The boat (REPORT.en.md 81).
  bool boating = false;
  bool prison = false;    // force a sentence from 18 (REPORT.en.md 82)
  bool office = false;    // a managerial job from 18 (REPORT.en.md 83)
  bool school = false;    // a pupil from age 5 (REPORT.en.md 84)
  bool podcast = false;   // a show of the hero's own (REPORT.en.md 85)
  bool actor = false;
  bool finance = false;    // the market (REPORT.en.md 77)
  bool secret_agent = false;
  bool relationship = false;
  bool spy = false;
  bool fight = false;
  bool black_market = false;
  bool casino = false;
  bool cult = false;
  bool pet = false;
  // The outdoors (REPORT.en.md 71).
  bool outdoor = false;
  // The five studio trades (REPORT.en.md 69).
  bool producer = false;
  bool soundcloud = false;
  bool scout = false;
  bool composer = false;
  bool club_takeover = false;
  bool instrument = false;
  // A life that starts famous (REPORT.en.md 139). The game reaches it through a
  // career and its own fame impacts; the port takes it from the spec so the Fame
  // family's gate can be exercised at all.
  int fame = 0;
  // The hero starts on birth control (REPORT.en.md 141); the game gets there
  // through the lover family's own cards, which need the flag to exist first.
  bool birthControl = false;
  // Children at the start of the life (REPORT.en.md 146). The game gets there
  // through the baby scenarios; the port needs the array to exist for the cards
  // that read it, and a fixture is the honest way to reach it.
  int children = 0;
  // An ex-lover at the start of the life (REPORT.en.md 149). The court-order card
  // needs one, and a break-up inside two years of a birth is too rare to test on.
  bool ex = false;
  int ages = 0;
  std::vector<int> choices;
  std::vector<PushSpec> push;
  // Optional real-world date "MM-DD" (default ""). The calendar and holiday
  // dumb-scenario sets are gated on one, so a seed-only run leaves them out of
  // the yearly pool; see Sim::setDate.
  std::string date;
};

// Parses the script JSON. False when the text is not a flat JSON object, when a
// value has the wrong type, or when gender is neither "male" nor "female".
bool parseScript(const std::string& json, ScriptSpec& spec, std::string* error);

// Builds the life the spec describes: createLife, the country pin and every setup
// switch, then the two draws (the hero snapshot and the age-0 lines) that share
// the age-up stream. Both the scripted run and the played run start here.
// False when the script names a country a16_countries-enc.json does not have.
bool setupLife(Sim& sim, const ScriptSpec& spec, Life& life, std::string* heroAtBirth,
               std::vector<std::string>* birth, std::string* error);

// Runs the life spec describes and returns the JSON object the port prints for
// it. False when the script names a country a16_countries-enc.json does not have.
bool runScript(Sim& sim, const ScriptSpec& spec, std::string* json, std::string* error);

// Loads the real tables from reDir, runs the script at scriptPath and prints the
// one JSON object on stdout. Returns the process exit code: 0 ok, 1 data/table
// error, 2 unreadable or malformed script.
int runScriptMode(const std::string& reDir, const std::string& scriptPath);

}  // namespace bl
