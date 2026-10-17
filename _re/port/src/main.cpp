// src/main.cpp -- demo driver for the BitLife port scaffold.
//
//   blsim [RE_DIR]
//   blsim --script SCRIPT.json [--data RE_DIR]
//
// RE_DIR defaults to /Users/Adam/Documents/bitlife/_re and must contain
// gamedata_plain/ and gamedata_json/. With --script the demo below is not run:
// the scripted life is aged up and one JSON object goes to stdout.
#include "bl/bag.h"
#include "bl/deck.h"
#include "bl/dotnet_rng.h"
#include "bl/game.h"
#include "bl/mod.h"
#include "bl/play.h"
#include "bl/script.h"
#include "bl/sim.h"
#include "bl/table.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::string join(const std::vector<std::string>& v, const char* sep, size_t limit) {
  std::string out;
  for (size_t i = 0; i < v.size() && i < limit; ++i) {
    if (i) out += sep;
    out += v[i];
  }
  if (v.size() > limit) out += ",...";
  return out;
}

const char* genderWord(bool female) { return female ? "female" : "male"; }

}  // namespace

namespace {

// --deckcheck: the deck maths must reproduce the game's decomposition of a
// percent into (deckSize, successCount). Small percents are the ones the port
// actually draws from, so they are asserted rather than merely printed.
int runDeckCheck() {
  const double ps[] = {0.0, 0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0};
  bl::Rng rng(1);
  int failures = 0;
  for (double p : ps) {
    const std::pair<int, int> d = bl::RandomDeck::ComputeDeckParameters(float(p));
    bl::RandomDeck deck(rng, d.first, d.second);
    std::cout << "    p=" << p << " -> deckSize=" << d.first
              << " success=" << d.second << " cards=" << deck.cards().size() << "\n";
    if (deck.cards().size() != size_t(d.first)) ++failures;
    size_t ones = 0;
    for (unsigned char c : deck.cards()) ones += c;
    if (int(ones) != d.second) ++failures;
  }
  // Exact rational cases the game's GCD decomposition implies.
  struct Case { float p; int size; int success; };
  const Case cases[] = {{0.25f, 4, 1}, {0.1f, 10, 1}, {0.01f, 100, 1}, {0.5f, 2, 1},
                        {0.75f, 4, 3}, {1.0f, 1, 1}, {0.0f, 1, 0}};
  for (const Case& c : cases) {
    const std::pair<int, int> d = bl::RandomDeck::ComputeDeckParameters(c.p);
    if (d.first != c.size || d.second != c.success) {
      std::cerr << "deck mismatch p=" << c.p << " got (" << d.first << ","
                << d.second << ") want (" << c.size << "," << c.success << ")\n";
      ++failures;
    }
  }
  std::cout << "  deckcheck failures=" << failures << "\n";
  return failures ? 3 : 0;
}

// Extracts the {"slot": "Name"} pairs templatize.py writes into Slots.
void slotsOf(const std::string& json, std::vector<std::pair<std::string, std::string>>* out) {
  size_t i = 0;
  while ((i = json.find('"', i)) != std::string::npos) {
    const size_t keyEnd = json.find('"', i + 1);
    if (keyEnd == std::string::npos) break;
    const std::string key = json.substr(i + 1, keyEnd - i - 1);
    const size_t valStart = json.find('"', json.find(':', keyEnd) + 1);
    if (valStart == std::string::npos) break;
    const size_t valEnd = json.find('"', valStart + 1);
    if (valEnd == std::string::npos) break;
    out->push_back({key, json.substr(valStart + 1, valEnd - valStart - 1)});
    i = valEnd + 1;
  }
}

// --templates: the observed corpus is the port's text ground truth. Every
// {slot} in a template must resolve against that row's Slots column.
int runTemplates(const std::string& reDir) {
  const std::string path = reDir + "/obs_corpus/templates.csv";
  bl::Table t = bl::Table::load(path);
  size_t scenario = 0, log = 0, withSlots = 0, unresolved = 0, tokens = 0;
  std::string bestText, bestOut;
  size_t bestTokens = 0;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string kind = t.at(r, "Kind");
    if (kind == "scenario") ++scenario; else ++log;
    const std::string text = t.at(r, "Text");
    std::vector<std::pair<std::string, std::string>> slots;
    slotsOf(t.at(r, "Slots"), &slots);
    if (!slots.empty()) ++withSlots;
    size_t hit = 0;
    for (size_t i = 0; i < text.size();) {
      if (text[i] != '{') { ++i; continue; }
      const size_t close = text.find('}', i);
      if (close == std::string::npos) break;
      const std::string key = text.substr(i + 1, close - i - 1);
      if (t.col(key) >= 0) { ++i; continue; }
      bool found = false;
      for (const auto& kv : slots) if (kv.first == key) { found = true; break; }
      if (!found) ++unresolved;
      ++hit;
      i = close + 1;
    }
    tokens += hit;
    if (hit > bestTokens) { bestTokens = hit; bestText = text; }
  }
  std::cout << "    templates=" << t.size() << " scenarios=" << scenario
            << " logs=" << log << " withSlots=" << withSlots
            << " slots=" << tokens << " unresolved=" << unresolved << "\n";
  std::cout << "    widest: " << bestText.substr(0, 120) << "\n";
  return unresolved ? 4 : 0;
}

// --rngcheck: the sim's own PRNG must reproduce the draws captured from the
// game (rng_vectors.json, written by rng_vectors.py from the frida
// System.Random hooks). Consecutive Next() captures map to consecutive internal
// samples, so the k-th captured value must equal the k-th sample.
int runRngCheck(const std::string& reDir) {
  std::ifstream in(reDir + "/rng_vectors.json");
  if (!in) { std::cerr << "cannot open rng_vectors.json\n"; return 5; }
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

  size_t good = 0, checked = 0;
  size_t pos = 0;
  while ((pos = text.find('"', pos)) != std::string::npos) {
    const size_t keyEnd = text.find('"', pos + 1);
    if (keyEnd == std::string::npos) break;
    const std::string key = text.substr(pos + 1, keyEnd - pos - 1);
    pos = keyEnd + 1;
    if (key.empty() || key.find_first_not_of("0123456789") != std::string::npos) continue;
    const size_t open = text.find('[', pos);
    if (open == std::string::npos) break;
    // Values are the second number of each [seq, value] pair at this nesting.
    std::vector<int64_t> values;
    size_t p = open + 1;
    int depth = 1;
    while (p < text.size() && depth > 0) {
      if (text[p] == '[') {
        ++depth;
        size_t q = text.find(']', p);
        if (q == std::string::npos) break;
        const std::string inner = text.substr(p + 1, q - p - 1);
        const size_t comma = inner.find(',');
        if (comma != std::string::npos) {
          const std::string rhs = inner.substr(comma + 1);
          const size_t b = rhs.find_first_not_of(" \t\r\n");
          if (b != std::string::npos) values.push_back(std::atoll(rhs.c_str() + b));
        }
        p = q + 1;
        --depth;
        continue;
      }
      if (text[p] == ']') --depth;
      ++p;
    }
    if (values.size() < 8) continue;
    bl::DotNetRng rng(int32_t(std::atoll(key.c_str())));
    size_t hits = 0;
    for (size_t i = 0; i < values.size() && i < 32; ++i) {
      if (rng.next() == values[i]) ++hits; else break;
    }
    ++checked;
    std::cout << "    seed " << key << " pairs=" << values.size()
              << " consecutive-matches=" << hits << "\n";
    if (hits >= 8) ++good;
  }
  std::cout << "  rngcheck instances=" << checked << " verified=" << good << "\n";
  if (good < 2) { std::cerr << "rngcheck: fewer than two instances verified\n"; return 6; }
  return 0;
}

// --bagcheck: the push-scenario selector builds a flat bag by appending each
// eligible type name once per unit of weight, then one uniform index into that
// bag picks the type. tools/bag_vectors.json carries captures from the device;
// each case must select the type the game actually constructed.
int runBagCheck(const std::string& reDir) {
  const std::string path = reDir + "/tools/bag_vectors.json";
  std::ifstream in(path);
  if (!in) { std::cerr << "cannot open " << path << "\n"; return 7; }
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::vector<bl::BagCase> cases;
  std::string error;
  if (!bl::loadBagVectors(text, &cases, &error)) {
    std::cerr << "bagcheck: " << error << "\n";
    return 7;
  }
  const bl::BagCheckResult r = bl::checkBagVectors(cases);
  for (const bl::BagCase& c : cases) {
    bl::WeightedBag bag;
    for (const auto& kv : c.bag) bag.add(kv.first, kv.second);
    std::cout << "    " << (c.source.empty() ? std::string("capture") : c.source)
              << " age=" << c.age << " entries=" << bag.entries()
              << " slots=" << bag.size()
              << (c.index >= 0 ? " index=" + std::to_string(c.index) : "")
              << " expect=" << c.expect << "\n";
  }
  for (const std::string& f : r.failures) std::cerr << "bagcheck: " << f << "\n";
  std::cout << "  bagcheck cases=" << r.cases << " withIndex=" << r.withIndex
            << " matched=" << r.matched << " failures=" << r.failures.size() << "\n";
  return r.failures.empty() ? 0 : 7;
}

}  // namespace

int main(int argc, char** argv) {
  std::string reDir = "/Users/Adam/Documents/bitlife/_re";
  std::string scriptPath;
  bool deckCheck = false, templates = false, rngCheck = false, bagCheck = false;
  bool play = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--script" && i + 1 < argc) scriptPath = argv[++i];
    else if (arg == "--deckcheck") deckCheck = true;
    else if (arg == "--templates") templates = true;
    else if (arg == "--rngcheck") rngCheck = true;
    else if (arg == "--bagcheck") bagCheck = true;
    else if (arg == "--play") play = true;
    else if (arg == "--data" && i + 1 < argc) reDir = argv[++i];
    else reDir = arg;  // the original blsim [RE_DIR] form
  }
  // --play: the same script spec, driven interactively instead of batched.
  if (play) {
    bl::ScriptSpec spec;
    std::string scriptError;
    std::string text;
    if (!scriptPath.empty()) {
      std::ifstream in(scriptPath);
      if (!in) {
        std::cerr << "play: cannot open " << scriptPath << "\n";
        return 2;
      }
      text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    } else {
      text = "{}";  // a seed-only life: a random country and a random gender roll
    }
    if (!bl::parseScript(text, spec, &scriptError)) {
      std::cerr << "play: " << scriptError << "\n";
      return 2;
    }
    return bl::runPlayMode(reDir, spec);
  }
  if (!scriptPath.empty()) return bl::runScriptMode(reDir, scriptPath);
  if (deckCheck) return runDeckCheck();
  if (templates) return runTemplates(reDir);
  if (rngCheck) return runRngCheck(reDir);
  if (bagCheck) return runBagCheck(reDir);

  std::cout << "=== BitLife port (blsim) -- data: " << reDir << "\n";

  // ---------------------------------------------------------------- data layer
  bl::Sim sim(reDir, 0xB17C0DEULL);
  std::string error;
  if (!sim.load(&error)) {
    std::cerr << "load failed: " << error << "\n";
    return 1;
  }

  std::cout << "\n[1] content tables loaded from gamedata_plain/\n";
  for (const std::string& name : sim.loadedTableNames()) {
    const bl::Table* t = sim.table(name);
    std::cout << "    " << name << "  rows=" << t->size()
              << " cols=" << t->columns().size() << "\n";
    std::cout << "      header: " << join(t->columns(), ", ", 8) << "\n";
  }
  std::cout << "    gamedata_json/a16_countries-enc.json  countries="
            << sim.countries().size() << "\n";

  // ------------------------------------------------------------- life creation
  std::cout << "\n[2] create life (country + gender from real data)\n";
  bl::Life life = sim.createLife(/*female=*/false);
  const bl::Hero& h = life.hero;
  std::cout << "    " << h.firstName << " " << h.lastName << "  " << genderWord(h.female)
            << "  born in " << h.country << " (" << h.continent << ")\n";
  std::cout << "    money=" << h.money << " happiness=" << h.happiness
            << " health=" << h.health << " smarts=" << h.smarts
            << " looks=" << h.looks << " karma=" << h.karma << " fame=" << h.fame << "\n";

  // ------------------------------------------------------------------ age loop
  std::cout << "\n[3] age-up loop (scenario text is real table content)\n";
  int printed = 0;
  for (int i = 0; i < 100 && life.alive && printed < 30; ++i) {
    bl::AgeUp ev;
    if (!sim.ageUp(life, ev)) break;
    ++printed;

    bl::Hero& hh = life.hero;
    hh.karma += ev.karma;
    hh.discipline += ev.discipline;
    hh.happiness += ev.happiness;
    hh.health += ev.health;
    hh.smarts += ev.smarts;
    hh.looks += ev.looks;
    int* stats[] = {&hh.karma, &hh.discipline, &hh.happiness, &hh.health, &hh.smarts, &hh.looks};
    for (int* s : stats) {
      if (*s < 0) *s = 0;
      if (*s > 100) *s = 100;
    }

    std::cout << "  age " << hh.age << "  [" << ev.table << " row " << ev.row_id << "]\n";
    if (!ev.title.empty() || !ev.body.empty()) {
      if (!ev.title.empty()) std::cout << "    " << ev.title << ": ";
      std::cout << ev.body << "\n";
    }
    if (!ev.choice.empty()) {
      if (!ev.choice.empty() || !ev.cta.empty())
        std::cout << "    " << ev.cta << " -> " << ev.choice << "\n";
      if (!ev.narrative.empty()) std::cout << "    " << ev.narrative << "\n";
      std::cout << "    karma" << (ev.karma >= 0 ? "+" : "") << ev.karma
                << " discipline" << (ev.discipline >= 0 ? "+" : "") << ev.discipline
                << " happiness" << (ev.happiness >= 0 ? "+" : "") << ev.happiness
                << " health" << (ev.health >= 0 ? "+" : "") << ev.health
                << " smarts" << (ev.smarts >= 0 ? "+" : "") << ev.smarts
                << " looks" << (ev.looks >= 0 ? "+" : "") << ev.looks << "\n";
    } else if (!ev.narrative.empty()) {
      std::cout << "    " << ev.narrative << "\n";
    }

    std::string type, sentence;
    if (sim.checkDeath(life, type, sentence)) {
      std::cout << "  *** death at " << hh.age << " (" << type << "): " << sentence << "\n";
      life.alive = false;
    }
  }
  {
    const bl::Hero& hh = life.hero;
    std::cout << "  -> age " << hh.age << " money=" << hh.money
              << " happiness=" << hh.happiness << " smarts=" << hh.smarts
              << " looks=" << hh.looks << " karma=" << hh.karma
              << " discipline=" << hh.discipline
              << (life.alive ? "  (alive)\n" : "  (dead)\n");
  }

  // ------------------------------------------------------------------ mod menu
  std::cout << "\n[4] mod menu tree (verbatim LGL grammar strings)\n";
  std::cout << bl::buildMenu().render();

  bl::GameState state;
  bl::Mod mod(state);
  std::cout << "    toggles: saveFeature=" << (mod.saveFeature ? "on" : "off")
            << "  autoMenuSize=" << (mod.autoMenuSize ? "on" : "off") << "\n";
  size_t grants = 0, initiators = 0;
  for (const bl::Entitlement& e : state.entitlements) {
    if (e.grantMethod) ++grants;
    if (e.purchaseMethod) ++initiators;
  }
  std::cout << "    entitlement catalogue: " << grants << " *Purchased grant points + "
            << initiators << " Purchase* initiators ("
            << state.entitlements.size() << " products)\n";

  std::cout << "\n[5] grant() the mod's free-in-app-purchase products\n";
  const char* keys[] = {"BitizenshipPurchased", "GodMode", "NonConsume_RemoveAds",
                        "PurchaseExpansionPackCasino", "HollywoodStarPurchased"};
  for (const char* k : keys) {
    const bool ok = mod.grant(k);
    std::cout << "    grant(\"" << k << "\") -> " << (ok ? "ok" : "unknown product")
              << "\n";
  }
  std::cout << "    bitizenship=" << state.vars.userBoughtBitizenship
            << " godMode=" << state.vars.userBoughtGodMode
            << " adsActive=" << (state.adsActive() ? "yes" : "no") << "\n";
  std::cout << "    owned " << state.ownedCount() << "/" << state.entitlements.size()
            << " entitlements\n";

  std::cout << "\n[6] grantAll() -- the whole product list\n";
  const int n = mod.grantAll();
  std::cout << "    granted " << n << " products, owned " << state.ownedCount() << "/"
            << state.entitlements.size() << ", numTimesPurchasesMade="
            << state.vars.numTimesPurchasesMade << "\n";
  std::cout << "\nok\n";
  return 0;
}
