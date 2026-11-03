// src/play.cpp -- the interactive terminal driver (tier 2, PORT_PLAN).
//
// The engine already knows how to run a card the player chose: Sim::ageUp takes
// a 0-based button index and every card honours it. What the engine cannot do is
// show the card *before* that index is known. So the driver asks the engine
// twice: once in a forked child, where the state changes die with the child, to
// print the card and its buttons; then for real, in the parent, with the button
// the player pressed. fork() gives the isolation for free -- the parent's Sim and
// Life are untouched until the commit, and no RNG stream is copied or replayed.
#include "bl/play.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace bl {
namespace {

// One shown card, as the forked child reports it back.
struct Probe {
  bool got = false;           // the year produced a card at all
  std::string table;
  std::string row;
  std::string title;
  std::string body;
  std::string cta;
  std::string choice;         // the button this run took, empty for a report card
  std::vector<std::string> options;   // every label the row carries (REPORT.en.md 233)
  std::string narrative;
  std::string extra;          // the year's own log lines, \n separated
  int karma = 0, discipline = 0, happiness = 0, health = 0, smarts = 0, looks = 0;
  int age = 0;
};

// The row's labels travel as one field: \x1e between them, no card text carries it.
std::string joinOptions(const std::vector<std::string>& v) {
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) {
    if (i) out.push_back('\x1e');
    out += v[i];
  }
  return out;
}

std::vector<std::string> splitOptions(const std::string& s) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i <= s.size()) {
    const size_t j = s.find('\x1e', i);
    if (j == std::string::npos) { out.push_back(s.substr(i)); break; }
    out.push_back(s.substr(i, j - i));
    i = j + 1;
  }
  if (!out.empty() && out.back().empty()) out.pop_back();
  return out;
}

// The child's one record. \x1f separates fields; no card text contains it.
std::string encode(const Probe& p) {
  std::ostringstream s;
  s << (p.got ? 1 : 0) << '\x1f' << p.table << '\x1f' << p.row << '\x1f' << p.title
    << '\x1f' << p.body << '\x1f' << p.cta << '\x1f' << p.choice << '\x1f'
    << p.narrative << '\x1f' << joinOptions(p.options) << '\x1f' << p.extra << '\x1f' << p.karma << '\x1f' << p.discipline
    << '\x1f' << p.happiness << '\x1f' << p.health << '\x1f' << p.smarts << '\x1f'
    << p.looks << '\x1f' << p.age;
  return s.str();
}

Probe decode(const std::string& blob) {
  Probe p;
  std::vector<std::string> f;
  size_t i = 0;
  while (i <= blob.size()) {
    const size_t j = blob.find('\x1f', i);
    if (j == std::string::npos) { f.push_back(blob.substr(i)); break; }
    f.push_back(blob.substr(i, j - i));
    i = j + 1;
  }
  if (f.size() < 17) return p;
  p.got = f[0] == "1";
  p.table = f[1]; p.row = f[2]; p.title = f[3]; p.body = f[4]; p.cta = f[5];
  p.choice = f[6]; p.narrative = f[7]; p.options = splitOptions(f[8]); p.extra = f[9];
  p.karma = std::atoi(f[10].c_str());
  p.discipline = std::atoi(f[11].c_str());
  p.happiness = std::atoi(f[12].c_str());
  p.health = std::atoi(f[13].c_str());
  p.smarts = std::atoi(f[14].c_str());
  p.looks = std::atoi(f[15].c_str());
  p.age = std::atoi(f[16].c_str());
  return p;
}

// One age-up in a throwaway child. forced < 0 lets the port draw its own lane.
bool probeYear(Sim& sim, Life& life, int forced, Probe* out) {
  int fds[2];
  if (pipe(fds) != 0) return false;
  const pid_t pid = fork();
  if (pid < 0) {
    close(fds[0]);
    close(fds[1]);
    return false;
  }
  if (pid == 0) {
    close(fds[0]);
    AgeUp ev;
    Probe p;
    if (sim.ageUp(life, ev, forced)) {
      p.got = true;
      p.table = ev.table;
      p.row = ev.row_id;
      p.title = ev.title;
      p.body = ev.body;
      p.cta = ev.cta;
      p.choice = ev.choice;
      p.options = ev.options;
      p.narrative = ev.narrative;
      for (size_t k = 0; k < ev.extra.size(); ++k) {
        if (k) p.extra += '\n';
        p.extra += ev.extra[k];
      }
      p.karma = ev.karma;
      p.discipline = ev.discipline;
      p.happiness = ev.happiness;
      p.health = ev.health;
      p.smarts = ev.smarts;
      p.looks = ev.looks;
      p.age = life.hero.age;
    }
    const std::string blob = encode(p);
    const ssize_t wrote = ::write(fds[1], blob.data(), blob.size());
    (void)wrote;
    _exit(0);
  }
  close(fds[1]);
  std::string buf;
  char tmp[4096];
  ssize_t n;
  while ((n = ::read(fds[0], tmp, sizeof tmp)) > 0) buf.append(tmp, size_t(n));
  close(fds[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  if (buf.empty()) return false;
  *out = decode(buf);
  return out->got;
}

// The port's own rule for applying a card's stat move, copied from runScript so a
// played life and a scripted life clamp the six stats the same way.
void applyStats(Hero& h, const AgeUp& ev) {
  h.karma += ev.karma;
  h.discipline += ev.discipline;
  h.happiness += ev.happiness;
  h.health += ev.health - ev.ailmentCost;
  h.smarts += ev.smarts;
  h.looks += ev.looks;
  h.fame = std::max(0.0f, std::min(100.0f, h.fame + float(ev.fame)));
  int* stats[] = {&h.karma, &h.discipline, &h.happiness, &h.health, &h.smarts, &h.looks};
  for (int* s : stats) {
    if (*s < 0) *s = 0;
    if (*s > 100) *s = 100;
  }
}

std::string money(long long dollars) {
  std::ostringstream s;
  s << (dollars < 0 ? "-$" : "$") << (dollars < 0 ? -dollars : dollars);
  return s.str();
}

void statsLine(const Hero& h) {
  std::cout << "  " << money(h.money) << "   happiness " << h.happiness << "   health "
            << h.health << "   smarts " << h.smarts << "   looks " << h.looks
            << "   karma " << h.karma << "   discipline " << h.discipline << "\n";
}

void printCard(const Probe& p) {
  if (!p.extra.empty()) {
    std::istringstream lines(p.extra);
    std::string line;
    while (std::getline(lines, line)) std::cout << "  . " << line << "\n";
  }
  if (!p.title.empty()) std::cout << "\n  " << p.title << "\n";
  if (!p.body.empty()) std::cout << "  " << p.body << "\n";
}

}  // namespace

int runPlayMode(const std::string& reDir, const ScriptSpec& spec) {
  Sim sim(reDir, spec.seed);
  std::string error;
  if (!sim.load(&error)) {
    std::cerr << "load failed: " << error << "\n";
    return 1;
  }
  Life life;
  std::string heroAtBirth;
  std::vector<std::string> birth;
  if (!setupLife(sim, spec, life, &heroAtBirth, &birth, &error)) {
    std::cerr << "play: " << (error.empty() ? "life setup failed" : error) << "\n";
    return 1;
  }

  const Hero& h0 = life.hero;
  std::cout << "=== " << h0.firstName << " " << h0.lastName << ", " << h0.country
            << " (" << (h0.female ? "female" : "male") << ")\n";
  for (const std::string& line : birth) std::cout << "  . " << line << "\n";

  const int cap = spec.ages > 0 ? spec.ages : 120;
  std::string deathType, deathSentence;
  for (int year = 0; year < cap && life.alive; ++year) {
    // (1) What the year looks like, from a child that throws its state away.
    Probe base;
    if (!probeYear(sim, life, -1, &base)) {
      std::cout << "\n  the year passes with nothing to show\n";
      break;
    }
    std::cout << "\n--- age " << base.age << " ------------------------------------\n";
    statsLine(life.hero);
    printCard(base);

    // (2) The buttons. A report card has none; every other card's lanes are found
    // by asking the engine for each index in turn, keeping the ones that land on
    // the same table row as the drawn card.
    std::vector<int> lanes;
    std::vector<std::string> labels;
    if (base.choice.empty()) {
      std::cout << "  [enter] continue\n";
    } else if (base.options.size() >= 2) {
      // The row itself says what its buttons are (REPORT.en.md 233), so the menu no
      // longer depends on a probe that consumes a different RNG path.
      for (size_t i = 0; i < base.options.size(); ++i)
        std::cout << "  [" << (i + 1) << "] " << base.options[i]
                  << (i == 0 ? "   (the game's own pick)" : "") << "\n";
      lanes.push_back(-1);
      labels = base.options;
    } else {
      lanes.push_back(-1);
      labels.push_back(base.choice);
      for (int k = 0; k < 6; ++k) {
        Probe alt;
        if (!probeYear(sim, life, k, &alt)) continue;
        if (alt.table != base.table || alt.row != base.row) continue;
        if (alt.choice.empty() || alt.choice == base.choice) continue;
        bool dup = false;
        for (const std::string& s : labels) dup = dup || (s == alt.choice);
        if (dup) continue;
        lanes.push_back(k);
        labels.push_back(alt.choice);
      }
      for (size_t i = 0; i < labels.size(); ++i)
        std::cout << "  [" << (i + 1) << "] " << labels[i]
                  << (i == 0 ? "   (the game's own pick)" : "") << "\n";
    }

    // (3) The player's answer.
    int forced = -1;
    if (!lanes.empty()) {
      std::cout << "  > " << std::flush;
      std::string line;
      if (!std::getline(std::cin, line)) break;
      if (line == "q" || line == "quit") break;
      const int pick = line.empty() ? 1 : std::atoi(line.c_str());
      if (pick >= 1 && pick <= int(lanes.size())) forced = lanes[size_t(pick - 1)];
    } else {
      std::string scratch;
      if (!std::getline(std::cin, scratch)) break;  // EOF ends the session
      if (scratch == "q" || scratch == "quit") break;
    }

    // (4) The same year, for real, with the picked index.
    AgeUp ev;
    if (!sim.ageUp(life, ev, forced)) break;
    applyStats(life.hero, ev);
    if (!ev.choice.empty())
      std::cout << "\n  -> " << ev.choice << "\n";
    if (!ev.narrative.empty()) std::cout << "  " << ev.narrative << "\n";
    if (ev.karma || ev.discipline || ev.happiness || ev.health || ev.smarts || ev.looks) {
      std::cout << "  karma " << std::showpos << ev.karma << "  discipline " << ev.discipline
                << "  happiness " << ev.happiness << "  health " << ev.health << "  smarts "
                << ev.smarts << "  looks " << ev.looks << std::noshowpos << "\n";
    }
    statsLine(life.hero);
    if (sim.checkDeath(life, deathType, deathSentence)) {
      life.alive = false;
      std::cout << "\n*** death at " << life.hero.age << " (" << deathType << "): "
                << deathSentence << "\n";
    }
  }
  std::cout << "\n  lived to " << life.hero.age << "\n";
  return 0;
}

}  // namespace bl
