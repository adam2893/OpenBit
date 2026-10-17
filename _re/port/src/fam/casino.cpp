// src/fam/casino.cpp -- moved out of src/sim.cpp (REPORT.en.md 131).
//
// Cut verbatim by tools/split_family.py; the only edits are the includes.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>
#include <vector>

namespace bl {

bool Sim::casinoYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.ownsCasino || h.age < 18) return false;
  if (!casinoRng_.chance(0.50)) return false;
  ensureCasinoFields(life);
  rndCache_.clear(); rbLast_.clear();        // the '<rb>' alternatives belong to one card
  casinoGambler_.clear();  // one punter per card
  casinoRoom_.clear();
  cultCommunard_.clear();   // one follower per cult card
  cultExCommunard_.clear();
  dealerMember_.clear();    // one cartel member / target per trade card
  dealerVictim_.clear();
  dealerCache_.clear();
  mafiaAmount_.clear();     // one amount / family name per family card
  mafiaCache_.clear();
  zooCache_.clear();        // one animal / keeper / park name per zoo card
  llSquatter_.clear();      // one squatter per landlord card
  zooOther_.clear();
  std::string title, body, line, tableRef, rowRef;
  std::vector<std::string> extra;
  int happiness = 0;
  auto say = [&body](const std::string& s) {
    if (!s.empty()) body += body.empty() ? s : " " + s;
  };
  auto tell = [&line](const std::string& s) {
    if (!s.empty()) line += line.empty() ? s : " " + s;
  };
  // The house's own mood drifts a little every year.
  h.casinoPopularity += int(casinoRng_.below(7)) - 3;

  // (14) Entertainment: an empty stage means an audition, a booked act means a
  //      show. The audition lines are graded by the impression score, which the
  //      port reads as the house's own popularity.
  if (title.empty() && casinoRng_.chance(0.35)) {
    if (h.casinoAct.empty()) {
      const size_t ar = pickWindow("a16_casino-casino-audition-string-enc", "", "", "", "Min Impression",
                                   "Max Impression", h.casinoPopularity, casinoRng_);
      if (ar != size_t(-1)) {
        const Table& at = tbl("a16_casino-casino-audition-string-enc");
        const std::string actCat = lower(trim(at.at(ar, "Act Category")));
        const Table& nt = tbl("a16_casino-casino-performance-act-name-enc");
        std::vector<size_t> pool;
        for (size_t r = 0; r < nt.size(); ++r) {
          const std::string ty = lower(trim(nt.at(r, "Act Type")));
          if (!actCat.empty() && !ty.empty() && ty != actCat) continue;
          const std::string th = lower(trim(nt.at(r, "Theme")));
          if (!th.empty() && th != lower(h.casinoTheme)) continue;
          pool.push_back(r);
        }
        std::string actName;
        if (!pool.empty())
          actName = resolve(trim(nt.at(pool[casinoRng_.below(uint32_t(pool.size()))], "Name")));
        const std::string scen = resolve(trim(at.at(ar, "Scenario")));
        if (!scen.empty()) {
          title = "Auditions";
          say(spySentence(scen));
          const std::string l = resolve(trim(at.at(ar, "Log Version")));
          if (!l.empty()) say(spySentence("They " + spyLowerFirst(l)));
          tableRef = at.name();
          rowRef = std::to_string(ar + 1);
          if (casinoRng_.chance(0.40)) {
            const std::string why = pickSnippet("a16_casino-casino-offer-artist-refusal-enc", "String",
                                                "Commonness Score", "", "", "", casinoRng_);
            tell(spySentence(why.empty() ? "I did not book the act." : why));
            happiness -= 1;
          } else {
            h.casinoAct = actName.empty() ? std::string("an unknown act") : actName;
            h.casinoActType = actCat;
            const size_t bp = pickRow("a16_casino-casino-performance-act-enc", "", "Act Type", actCat, "", casinoRng_);
            if (bp != size_t(-1)) h.casinoActPopularity = int(tbl("a16_casino-casino-performance-act-enc").num(bp, "Base Popularity"));
            // A singer act brings a genre with it, and the genre's match against
            // the house's theme is what the popularity impact is for.
            if (h.casinoActType.find("sing") != std::string::npos ||
                h.casinoActType.find("music") != std::string::npos ||
                h.casinoActType.find("band") != std::string::npos) {
              const size_t gr = pickRow("a16_casino-casino-singer-genre-enc", "", "Theme", h.casinoTheme, "", casinoRng_);
              if (gr != size_t(-1)) {
                const Table& g = tbl("a16_casino-casino-singer-genre-enc");
                const double match = g.num(gr, "Match Popularity Impact");
                h.casinoPopularity += int(match > 0 ? match : g.num(gr, "Base Popularity Impact"));
              }
              h.casinoSong = pickSnippet("a16_casino-casino-singer-song-enc", "Song Title", "Commonness Score", "", "", "", casinoRng_);
            }
            h.casinoPopularity += h.casinoActPopularity * 2;
            tell("I booked " + h.casinoAct + (h.casinoActType.empty() ? std::string() : " the " + h.casinoActType) + ".");
            happiness += 2;
          }
        }
      }
    } else if (casinoRng_.chance(0.35)) {
      const size_t sr = pickRow("a16_casino-casino-performance-act-string-enc", "Commonness Score", "Act Type",
                                h.casinoActType, "", casinoRng_);
      if (sr != size_t(-1)) {
        const Table& t = tbl("a16_casino-casino-performance-act-string-enc");
        const std::string scen = resolve(trim(t.at(sr, "Scenario")));
        if (!scen.empty()) {
          title = "The show";
          say(spySentence(scen));
          if (!h.casinoSong.empty()) say("They closed with " + h.casinoSong + ".");
          h.casinoPopularity += 2;
          happiness += 2;
          tableRef = t.name();
          rowRef = std::to_string(sr + 1);
        }
      }
    }
  }

  // (15) A celebrity in the house: the rider first, then the party, then the
  //      fallout. The party pool is gated on the celebrity's own craziness.
  if (title.empty() && casinoRng_.chance(0.20)) {
    if (h.casinoCelebrity.empty()) {
      if (h.casinoPopularity >= 45 && casinoRng_.chance(0.5)) {
        const std::string who = casinoToken("casino-celebrity");
        if (!who.empty()) {
          h.casinoCelebrity = who;
          h.casinoCelebCraziness = 20 + int(casinoRng_.below(71));
          const size_t dr = pickRow("a16_casino-celebrity-demand-enc", "", "", "", "", casinoRng_);
          title = "A celebrity in the house";
          if (dr != size_t(-1)) {
            const Table& t = tbl("a16_casino-celebrity-demand-enc");
            const std::string item = resolve(trim(t.at(dr, "Item")));
            if (!item.empty()) {
              const std::string art = withArticle(item, t.flag(dr, "Uses Indefinite Article"));
              say(who + " arrived with a rider that included " + art + item + ".");
              h.casinoPopularity += int(t.num(dr, "Celebrity Popularity Impact"));
              tableRef = t.name();
              rowRef = std::to_string(dr + 1);
            }
          }
          if (tableRef.empty()) {
            say(who + " arrived to spend the evening at the tables.");
            tableRef = "a16_casino-casino-celebrity-name-enc";
          }
        }
      }
    } else {
      const size_t pr = pickWindow("a16_casino-casino-celebrity-party-scenario-enc", "", "", "",
                                   "Min Celeb Craziness", "Max Celeb Craziness", h.casinoCelebCraziness, casinoRng_);
      if (pr != size_t(-1)) {
        const Table& t = tbl("a16_casino-casino-celebrity-party-scenario-enc");
        const bool positive = t.flag(pr, "Is Positive");
        const std::string scen = resolve(trim(t.at(pr, "Scenario")));
        if (!scen.empty()) {
          title = "The party";
          say(spySentence(scen));
          const std::string l = resolve(trim(t.at(pr, "Log Version")));
          if (!l.empty()) tell(spySentence(l));
          if (positive) {
            h.casinoPopularity += 3;
            happiness += 3;
          } else {
            h.casinoPopularity -= 3;
            happiness -= 3;
            if (casinoRng_.chance(0.40)) {
              const size_t dr2 = pickWindow("a16_casino-casino-celeb-party-death-string-enc", "", "", "",
                                            "Min Celeb Craziness", "Max Celeb Craziness", h.casinoCelebCraziness, casinoRng_);
              if (dr2 != size_t(-1)) {
                const Table& dt = tbl("a16_casino-casino-celeb-party-death-string-enc");
                // The death row is its own card: its '<rb>' refers to the
                // alternatives its own String draws.
                rndCache_.clear(); rbLast_.clear();
                const std::string s = resolve(trim(dt.at(dr2, "String")));
                if (!s.empty()) {
                  const std::string who = h.casinoCelebrity.empty() ? std::string("the guest") : h.casinoCelebrity;
                  say(spySentence(who + " " + spyLowerFirst(s)));
                  const std::string dl = resolve(trim(dt.at(dr2, "Log Version")));
                  if (!dl.empty()) tell(spySentence(who + " " + spyLowerFirst(dl)));
                  tableRef = dt.name();
                  rowRef = std::to_string(dr2 + 1);
                  happiness -= 4;
                }
              }
            }
          }
          if (tableRef.empty()) {
            tableRef = t.name();
            rowRef = std::to_string(pr + 1);
          }
          if (casinoRng_.chance(0.5)) {
            h.casinoCelebrity.clear();
            h.casinoCelebCraziness = 0;
          }
        }
      }
    }
  }

  // (16) Something for the display case.
  if (title.empty() && casinoRng_.chance(0.12)) {
    const size_t r = pickRow("a16_casino-casino-collectible-enc", "Commonness Score", "", "", "", casinoRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_casino-casino-collectible-enc");
      const std::string item = resolve(trim(t.at(r, "Item Name")));
      if (!item.empty()) {
        const std::string art = withArticle(item, t.flag(r, "Indefinite Article"));
        const std::string prefix = art.empty() && t.flag(r, "Definite Article") ? std::string("the ") : art;
        const long long lo = static_cast<long long>(t.num(r, "Min Value"));
        const long long hi = static_cast<long long>(t.num(r, "Max Value"));
        const long long value = hi > lo ? lo + static_cast<long long>(casinoRng_.unit() * double(hi - lo)) : lo;
        title = "A piece for the collection";
        say(spySentence("I bought " + prefix + item + " for " + money(value)));
        tell(spySentence("I bought " + prefix + item + " for " + money(value) + " to display at " + h.casinoName));
        h.casinoPopularity += 1;
        happiness += 1;
        tableRef = t.name();
        rowRef = std::to_string(r + 1);
      }
    }
  }

  // (17) Where the house's own accessories came from; the rows are past-tense
  //      clauses ("you won a hand with 21").
  if (title.empty() && casinoRng_.chance(0.10)) {
    const size_t r = pickRow("a16_casino-casino-accessory-acquisition-method-enc", "", "", "", "", casinoRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_casino-casino-accessory-acquisition-method-enc");
      const std::string type = lower(trim(t.at(r, "Accessory Type")));
      const std::string snip = spySwap(resolve(trim(t.at(r, "Scenario Snippet"))), "you ", "I ");
      if (!type.empty() && !snip.empty()) {
        title = "House swag";
        say(spySentence(snip));
        tell(spySentence("I keep my " + type + " at " + h.casinoName));
        happiness += 1;
        tableRef = t.name();
        rowRef = std::to_string(r + 1);
      }
    }
  }

  // (1) The casino's own prompt-and-choice card. Its gates are the house's
  //     popularity, its theme and the facilities it actually has.
  if (title.empty() && ensureTable("a16_casino-casino-dumb-scenario-enc")) {
    const Table& t = tbl("a16_casino-casino-dumb-scenario-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      const double lo = t.num(r, "Hero Min Age"), hi = t.num(r, "Hero Max Age");
      if (lo > 0 && h.age < lo) continue;
      if (hi > 0 && h.age > hi) continue;
      const std::string g = lower(trim(t.at(r, "Hero Gender")));
      if (!g.empty() && g != (h.female ? "female" : "male")) continue;
      const double plo = t.num(r, "Min Casino Popularity"), phi = t.num(r, "Max Casino Popularity");
      if (plo > 0 && h.casinoPopularity < plo) continue;
      if (phi > 0 && h.casinoPopularity > phi) continue;
      if (t.flag(r, "Requires Casino Amenity") && h.casinoAmenities.empty()) continue;
      if (t.flag(r, "Requires Casino Game Room") && h.casinoGames.empty()) continue;
      if (t.flag(r, "Requires Casino Celebrity") && h.casinoCelebrity.empty()) continue;
      if (t.flag(r, "Requires Casino Performance Act") && h.casinoAct.empty()) continue;
      const std::string theme = lower(trim(t.at(r, "Casino Theme")));
      if (!theme.empty() && theme != lower(h.casinoTheme)) continue;
      pool.push_back(r);
    }
    const size_t row = pool.empty() ? size_t(-1) : pool[casinoRng_.below(uint32_t(pool.size()))];
    if (row != size_t(-1) && casinoRng_.chance(0.15)) {
      title = resolve(trim(t.at(row, "Title")));
      say(resolve(trim(t.at(row, "Body"))));
      say(resolve(trim(t.at(row, "CTA"))));
      // A year-up card has no prompt of its own to route a choice through, so
      // the first choice is the one taken (the same shortcut the UFL card uses).
      say(resolve(trim(t.at(row, "Choice 1"))));
      tell(spySentence(resolve(trim(t.at(row, "Narrative 1")))));
      h.casinoPopularity += int(t.num(row, "Casino Popularity Impact 1"));
      h.casinoGamblerHappiness += int(t.num(row, "Gambler Relationship Impact 1"));
      tableRef = t.name();
      rowRef = std::to_string(row + 1);
    }
  }

  // (2) The ledger, the year's own balance sheet (its String is gated on the
  //     band the house's popularity sits in).
  if (title.empty() && casinoRng_.chance(0.35)) {
    const size_t r = pickWindow("a16_casino-casino-ledger-feedback-enc", "", "", "", "Min Casino Popularity",
                                "Max Casino Popularity", h.casinoPopularity, casinoRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_casino-casino-ledger-feedback-enc");
      const std::string s = resolve(trim(t.at(r, "String")));
      if (!s.empty()) {
        title = "The books";
        tell(spySentence(s));
        h.casinoPopularity += int(casinoRng_.below(5)) - 2;
        h.casinoGamblers += int(casinoRng_.below(9)) - 4;
        happiness += 1;
        tableRef = t.name();
        rowRef = std::to_string(r + 1);
      }
    }
  }

  // (3) What the patrons make of one of the house's amenities. The feedback row
  //     names a Category (one the house has) and sometimes a Theme.
  if (title.empty() && !h.casinoAmenities.empty() && ensureTable("a16_casino-casino-amenity-feedback-scenario-enc")) {
    const std::string cat = [&] {
      const Table& a = tbl("a16_casino-casino-amenity-enc");
      const std::string want = lower(trim(h.casinoAmenities[casinoRng_.below(uint32_t(h.casinoAmenities.size()))]));
      for (size_t r = 0; r < a.size(); ++r)
        if (lower(trim(a.at(r, "Amenity Name"))) == want) return lower(trim(a.at(r, "Category")));
      return std::string();
    }();
    const Table& t = tbl("a16_casino-casino-amenity-feedback-scenario-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      const std::string c = lower(trim(t.at(r, "Category")));
      if (!c.empty() && !cat.empty() && c != cat) continue;
      const std::string th = lower(trim(t.at(r, "Theme")));
      if (!th.empty() && th != lower(h.casinoTheme)) continue;
      pool.push_back(r);
    }
    if (!pool.empty() && casinoRng_.chance(0.30)) {
      const size_t r = pool[casinoRng_.below(uint32_t(pool.size()))];
      const std::string s = resolve(trim(t.at(r, "String")));
      if (!s.empty()) {
        title = "On the floor";
        tell(spySentence(s));
        if (t.has("Enjoyment Score")) {
          const double e = t.num(r, "Enjoyment Score");
          h.casinoGamblerHappiness += int(e) / 10 - 3;
          h.casinoPopularity += int(e) / 20 - 2;
        }
        tableRef = t.name();
        rowRef = std::to_string(r + 1);
      }
    }
  }

  // (4) A punter bends the hero's ear.
  if (title.empty() && casinoRng_.chance(0.25)) {
    const size_t r = pickWindow("a16_casino-casino-gambler-conversation-enc", "", "", "", "Hero Min Age",
                                "Hero Max Age", h.age, casinoRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_casino-casino-gambler-conversation-enc");
      const std::string topic = spyLowerFirst(resolve(trim(t.at(r, "Conversation"))));
      if (!topic.empty()) {
        title = h.casinoName;
        say(spySentence("A punter wanted to talk about " + topic));
        tell(spySentence("I listened to a punter talk about " + topic));
        h.casinoGamblerHappiness += 1;
        tableRef = t.name();
        rowRef = std::to_string(r + 1);
      }
    }
  }

  // (5) The house and one gambler cross paths, for better or worse; the side of
  //     the ledger the crowd's mood is on decides which pool answers.
  if (title.empty() && casinoRng_.chance(0.25)) {
    const bool positive = h.casinoGamblerHappiness >= 50;
    const char* tab = positive ? "a16_casino-positive-gambler-interaction-scenario-enc"
                               : "a16_casino-negative-gambler-interaction-scenario-enc";
    const size_t r = pickWindow(tab, "", "", "", "Min Gambler Happiness", "Max Gambler Happiness",
                                h.casinoGamblerHappiness, casinoRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl(tab);
      const std::string s = resolve(trim(t.at(r, "Scenario")));
      if (!s.empty()) {
        title = positive ? "A happy punter" : "An unhappy punter";
        say(spySentence(s));
        const std::string l = resolve(trim(t.at(r, "Log Version")));
        if (!l.empty()) tell(spySentence(l));
        if (positive) {
          h.casinoGamblerHappiness += 3;
          h.casinoPopularity += 1;
          happiness += 2;
        } else {
          h.casinoGamblerHappiness -= 3;
          h.casinoPopularity -= 2;
          happiness -= 2;
        }
        tableRef = t.name();
        rowRef = std::to_string(r + 1);
      }
    }
  }

  // (6) Nobody complains about a room they are winning in: the mood band picks
  //     between the complaint and the compliment table.
  if (title.empty() && casinoRng_.chance(0.20)) {
    const char* tab = h.casinoGamblerHappiness < 45 ? "a16_casino-casino-game-room-complaint-enc"
                         : h.casinoGamblerHappiness > 70 ? "a16_casino-gambler-game-room-compliment-enc"
                                                         : nullptr;
    if (tab != nullptr) {
      const size_t r = pickWindow(tab, "", "", "", "Min Gambler Happiness", "Max Gambler Happiness",
                                  h.casinoGamblerHappiness, casinoRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl(tab);
        const std::string s = resolve(trim(t.at(r, "String")));
        if (!s.empty()) {
          const bool bad = h.casinoGamblerHappiness < 45;
          title = bad ? "A complaint" : "A good word";
          tell(spySentence(s));
          h.casinoPopularity += bad ? -2 : 2;
          happiness += bad ? -2 : 2;
          tableRef = t.name();
          rowRef = std::to_string(r + 1);
        }
      }
    }
  }

  // (7) New faces at the door.
  if (title.empty() && casinoRng_.chance(0.25)) {
    const std::string s = pickSnippet("a16_casino-gambler-joins-casino-color-enc", "String", "Commonness Score",
                                      "", "", "", casinoRng_);
    if (!s.empty()) {
      title = "The floor";
      tell(spySentence(s));
      const int joined = 1 + int(casinoRng_.below(4));
      h.casinoGamblers += joined;
      h.casinoPopularity += 1;
      tableRef = "a16_casino-gambler-joins-casino-color-enc";
      happiness += 1;
    }
  }

  // (8) Someone who cannot behave is shown the door. The banned-gambler rows are
  //     predicates ("defecated in the casino lobby"), so the port writes the
  //     sentence that carries them.
  if (title.empty() && casinoRng_.chance(0.15)) {
    const int craziness = int(casinoRng_.below(101));
    const std::string punter = casinoToken("casino-gambler");
    const size_t r = pickWindow("a16_casino-banned-gambler-snippet-enc", "", "", "", "Min Gambler Craziness",
                                "Max Gambler Craziness", craziness, casinoRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_casino-banned-gambler-snippet-enc");
      const std::string s = resolve(trim(t.at(r, "Snippet")));
      if (!s.empty()) {
        title = "Bounced at the door";
        say(spySentence(punter + " " + spyLowerFirst(s)));
        const std::string l = resolve(trim(t.at(r, "Log Version")));
        if (!l.empty()) tell("I banned " + punter + " after they " + spyLowerFirst(l) + ".");
        h.casinoGamblers -= 1;
        h.casinoPopularity += 1;
        tableRef = t.name();
        rowRef = std::to_string(r + 1);
      }
    }
  }

  // (9) The mob takes an interest in a house that is doing well.
  if (title.empty() && casinoRng_.chance(0.12)) {
    casinoToken("casino-gambler");
    const size_t r = pickRow("a16_casino-mafia-member-activity-string-enc", "Commonness Score", "", "", "", casinoRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_casino-mafia-member-activity-string-enc");
      const std::string s = resolve(trim(t.at(r, "Snippet")));
      if (!s.empty()) {
        const std::string who = casinoGambler_.empty() ? std::string("a patron") : casinoGambler_;
        title = "The family";
        say(spySentence("A mafia member was " + spyLowerFirst(s) + " " + who));
        tell(spySentence("I caught a member of the mob " + spyLowerFirst(s) + " one of my patrons"));
        h.casinoPopularity -= 1;
        happiness -= 1;
        tableRef = t.name();
        rowRef = std::to_string(r + 1);
      }
    }
  }

  // (10) The meet and greet, where the lines that fit depend on how famous the
  //      hero is and on the house's theme.
  if (title.empty() && casinoRng_.chance(0.15)) {
    casinoToken("casino-gambler");
    const bool positive = casinoRng_.chance(0.5);
    const bool fameBased = h.fame >= 30;
    const char* flag = positive ? (fameBased ? "Positive Fame-Based" : "Positive Theme-Based")
                                : (fameBased ? "Negative Fame-Based" : "Negative Theme-Based");
    const Table& t = tbl("a16_casino-meet-greet-feedback-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r)
      if (t.flag(r, flag)) pool.push_back(r);
    if (pool.empty())
      for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      const size_t r = pool[casinoRng_.below(uint32_t(pool.size()))];
      const std::string s = resolve(trim(t.at(r, "String")));
      if (!s.empty()) {
        title = "The meet and greet";
        tell(spySentence(s));
        h.casinoPopularity += positive ? 2 : -2;
        happiness += positive ? 2 : -2;
        tableRef = t.name();
        rowRef = std::to_string(r + 1);
      }
    }
  }

  // (11) A drink at the bar, and sometimes a refusal. The refusal rows carry
  //      their own alcohol flag, so a soft drink never earns a sober line.
  if (title.empty() && casinoRng_.chance(0.20) && ensureTable("a16_casino-casino-drink-enc")) {
    const std::string punter = casinoToken("casino-gambler");
    const Table& d = tbl("a16_casino-casino-drink-enc");
    const size_t r = pickRow("a16_casino-casino-drink-enc", "", "", "", "", casinoRng_);
    if (r != size_t(-1)) {
      const std::string bev = resolve(trim(d.at(r, "Beverage Type")));
      if (!bev.empty()) {
        const bool alcoholic = d.flag(r, "Drink Has Alcohol");
        const std::string order = "I bought " + punter + " " + withArticle(bev, d.flag(r, "Uses Indefinite Article")) + bev + ".";
        title = "At the bar";
        bool refused = false;
        if (casinoRng_.chance(0.30)) {
          const Table& rr = tbl("a16_casino-drink-refusal-string-enc");
          std::vector<size_t> pool;
          for (size_t i = 0; i < rr.size(); ++i)
            if (rr.flag(i, "Drink Has Alcohol") == alcoholic) pool.push_back(i);
          if (!pool.empty()) {
            const size_t at = pool[casinoRng_.below(uint32_t(pool.size()))];
            tell(spySentence(resolve(trim(rr.at(at, "String")))));
            h.casinoGamblerHappiness -= 1;
            tableRef = rr.name();
            rowRef = std::to_string(at + 1);
            refused = true;
          }
        }
        if (!refused) {
          say(order);
          tell(order);
          h.casinoGamblerHappiness += int(d.num(r, "Happiness Impact")) / 2;
          tableRef = d.name();
          rowRef = std::to_string(r + 1);
        }
      }
    }
  }

  // (12) A gift for the regular, graded by the recipient's own taste.
  if (title.empty() && casinoRng_.chance(0.15)) {
    const std::string punter = casinoToken("casino-gambler");
    const int patronAge = 21 + int(casinoRng_.below(50));
    const size_t r = pickWindow("a16_casino-casino-gift-enc", "Commonness Score", "", "", "Min Age", "Max Age",
                                patronAge, casinoRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_casino-casino-gift-enc");
      const std::string gift = resolve(trim(t.at(r, "Gift")));
      if (!gift.empty()) {
        const double app = casinoGamblerFemale_ ? t.num(r, "Female Appreciation Score")
                                                : t.num(r, "Male Appreciation Score");
        const std::string line = "I gave " + punter + " " + withArticle(gift, t.flag(r, "Is Singular")) + gift + ".";
        title = "A gift for the regulars";
        say(line);
        tell(line);
        h.casinoGamblerHappiness += int(app);
        h.casinoPopularity += 1;
        tableRef = t.name();
        rowRef = std::to_string(r + 1);
      }
    }
  }

  // (13) Keeping the floor sweet: an unhappy punter is encouraged back to the
  //      tables, a happy one is thanked with a compensation the house decides.
  if (title.empty() && casinoRng_.chance(0.15)) {
    const std::string punter = casinoToken("casino-gambler");
    if (h.casinoGamblerHappiness < 50) {
      const size_t r = pickRow("a16_casino-gambler-encouragement-method-enc", "Commonness Score", "", "", "", casinoRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_casino-gambler-encouragement-method-enc");
        const std::string s = resolve(trim(t.at(r, "String")));
        if (!s.empty()) {
          title = "Working the floor";
          say(spySwap(s, "You ", "I "));
          tell(spySwap(s, "You ", "I "));
          h.casinoGamblerHappiness += int(t.num(r, "Effectiveness Score"));
          tableRef = t.name();
          rowRef = std::to_string(r + 1);
        }
      }
    } else {
      const size_t r = pickRow("a16_casino-gambler-compensation-method-enc", "Commonness Score", "", "", "", casinoRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_casino-gambler-compensation-method-enc");
        const std::string f = resolve(trim(t.at(r, "Feedback String")));
        if (!f.empty()) {
          title = "Thanking a patron";
          say(spySwap(f, "You ", "I "));
          const std::string l = resolve(trim(t.at(r, "Log Version")));
          if (!l.empty()) tell(spySentence(l));
          h.casinoGamblerHappiness += int(t.num(r, "Appreciation Score"));
          happiness += 1;
          tableRef = t.name();
          rowRef = std::to_string(r + 1);
        }
      }
    }
  }

  if (title.empty() && body.empty() && line.empty()) return false;
  if (h.casinoPopularity < 1) h.casinoPopularity = 1;
  if (h.casinoPopularity > 100) h.casinoPopularity = 100;
  if (h.casinoGamblerHappiness < 0) h.casinoGamblerHappiness = 0;
  if (h.casinoGamblerHappiness > 100) h.casinoGamblerHappiness = 100;
  if (h.casinoGamblers < 0) h.casinoGamblers = 0;
  // The year's family lifecycle lines were written before the card was picked
  // and belong to no row, so they survive this reset.
  const std::vector<std::string> prior = out.extra;
  out = AgeUp();
  out.extra = prior;
  for (const std::string& s : extra) out.extra.push_back(s);
  out.table = tableRef.empty() ? "a16_casino-casino-ledger-feedback-enc" : tableRef;
  out.row_id = rowRef;
  out.title = title.empty() ? "The casino" : title;
  out.body = body;
  out.narrative = line;
  out.happiness = happiness;
  return true;
}

void Sim::makeCasino(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.ownsCasino = true;
  ensureCasinoFields(life);
  h.casinoGamblers = 20 + int(casinoRng_.below(80));
}

std::string Sim::casinoToken(const std::string& key) {
  std::string base = key;
  const std::string psuffix = "-possessive";
  if (base.size() > psuffix.size() &&
      base.compare(base.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    base.resize(base.size() - psuffix.size());
  const Hero& h = *hero_;
  if (base == "casino-gambler") {
    // One gambler per card: the pools talk about a single punter at a time, and
    // the name has to survive the body and the log line of that card.
    if (casinoGambler_.empty()) {
      casinoGamblerFemale_ = casinoRng_.chance(0.5);
      casinoGambler_ = drawFirstName(casinoGamblerFemale_) + " " + drawSurname();
    }
    // The gambler-facing pools speak about the punter, not the hero.
    pronounOtherSet_ = true;
    pronounOtherFemale_ = casinoGamblerFemale_;
    return casinoGambler_;
  }
  if (base == "casino-celebrity") {
    if (!h.casinoCelebrity.empty()) return h.casinoCelebrity;
    const std::string who = referenceToken("celebrity");
    if (!who.empty()) return who;
    return drawFirstName(casinoRng_.chance(0.5)) + " " + drawSurname();
  }
  if (base == "casino-name" || base == "casino" || base == "hero-casino") {
    if (!h.casinoName.empty()) return h.casinoName;
    return pickSnippet("a16_casino-random-hero-casino-name-enc", "Name", "Commonness Score", "", "", "", casinoRng_);
  }
  if (base == "casino-room" || base == "casino-game") {
    if (!h.casinoGames.empty()) {
      if (casinoRoom_.empty()) casinoRoom_ = h.casinoGames[casinoRng_.below(uint32_t(h.casinoGames.size()))];
      return casinoRoom_;
    }
    return pickSnippet("a16_casino-casino-game-room-enc", "Game", "", "", "", "", casinoRng_);
  }
  return std::string();
}

}  // namespace bl
