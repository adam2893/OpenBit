// src/fam/casino.cpp -- moved out of src/sim.cpp (REPORT.en.md 131).
//
// Cut verbatim by tools/split_family.py; the only edits are the includes.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <cctype>
#include <string>
#include <vector>

namespace bl {

// The casino's books (REPORT.en.md 319). SimCasino::IncrementAge runs one silent
// pass a year -- the rooms' own games, then the punters' wagers, then the
// amenities' take, then the expenses and the tax on what is left -- and the
// annual review card only prints what that pass stored. The order of the draws is
// the native order: rooms, punters, amenities; expenses and tax draw nothing.
void Sim::casinoRunYear(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (h.casinoLedgerYear == h.age) return;
  h.casinoLedgerYear = h.age;
  // The punters. Native keeps one SimGambler per punter with a style 1..5 that
  // picks his bet bands; the port keeps the styles only.
  // shortcut: how a fresh punter's style is rolled is not recovered, so the port
  // leans the floor towards the low rollers.
  const size_t want = size_t(h.casinoGamblers < 0 ? 0 : h.casinoGamblers);
  while (h.casinoGamblerStyles.size() < want) {
    const uint32_t roll = casinoRng_.below(100);
    int style = 1;
    if (roll >= 97) style = 5;
    else if (roll >= 88) style = 4;
    else if (roll >= 70) style = 3;
    else if (roll >= 40) style = 2;
    h.casinoGamblerStyles.push_back(style);
  }
  if (h.casinoGamblerStyles.size() > want) h.casinoGamblerStyles.resize(want);

  // ---- the rooms play their own hands ----
  // SimCasinoGameRoom::SimulateAgeUpGames (vaddr 0x2abb0f8): one pass per
  // installed unit, four draws each -- RNG(5,10), the room popularity band, the
  // casino type band, then RNG(0,100) for the house-win roll -- and wager =
  // MinBetAmount * type multiplier * that unit count. No draw picks the
  // multiplier. Two more draws per room update its popularity (RNG(1,3) with a
  // random sign) and drift its house win rate (RNG(1,3)).
  long long roomWagers = 0, roomWins = 0;
  for (size_t i = 0; i < h.casinoRooms.size(); ++i) {
    Hero::CasinoRoom& room = h.casinoRooms[i];
    double wagers = 0.0, wins = 0.0;
    for (int unit = 0; unit < room.units; ++unit) {
      int w21 = 5 + int(casinoRng_.below(5));
      if (room.popularity > 80) w21 += 30 + int(casinoRng_.below(40));
      else if (room.popularity > 40) w21 += 10 + int(casinoRng_.below(40));
      else w21 += 5 + int(casinoRng_.below(25));
      double bet = double(room.minBet);
      switch (h.casinoType) {
        case 2: w21 += 10 + int(casinoRng_.below(5)); bet *= 1.1; break;
        case 3: w21 += 15 + int(casinoRng_.below(5)); bet *= 1.2; break;
        case 4: w21 += 20 + int(casinoRng_.below(5)); bet *= 1.25; break;
        case 5: w21 += 25 + int(casinoRng_.below(5)); bet *= 2.0; break;
        default: w21 += int(casinoRng_.below(10)); break;
      }
      const double wager = bet * double(w21);
      wagers += wager;
      if (room.houseWinRate >= int(casinoRng_.below(100))) wins += wager;
    }
    room.lastWagers = (long long)wagers;
    room.lastHouseWins = (long long)wins;
    roomWagers += room.lastWagers;
    roomWins += room.lastHouseWins;
    int d = 1 + int(casinoRng_.below(3));
    if (!casinoRng_.chance(0.5)) d = -d;
    room.popularity += d;
    const int drift = 1 + int(casinoRng_.below(3));
    // shortcut: the house win rate drifts toward the row's Default House Win Rate
    // by an unrecovered amount; the port moves it by the drawn steps, never past
    // the default.
    if (room.houseWinRate < room.defaultHouseWinRate)
      room.houseWinRate = std::min(room.defaultHouseWinRate, room.houseWinRate + drift);
    else if (room.houseWinRate > room.defaultHouseWinRate)
      room.houseWinRate = std::max(room.defaultHouseWinRate, room.houseWinRate - drift);
    if (room.popularity < 0) room.popularity = 0;
    if (room.popularity > 100) room.popularity = 100;
  }

  // ---- the punters at the tables ----
  // GetWagersPlaced (vaddr 0x2aad560) draws exactly ONE RNG(20,70) and then
  // walks the punter list with no further draw, so a punter's stake is a
  // function of the floor's own stats.
  // shortcut: the per-punter expression is unrecovered; the port spreads the
  // one drawn base over the punters by style, weights it by the room's
  // popularity, and lets the room's house win rate take its share of each stake.
  long long gamblerWagers = 0, gamblerHouseWins = 0;
  if (!h.casinoRooms.empty()) {
    const long long base = 20 + int(casinoRng_.below(50));
    for (size_t g = 0; g < h.casinoGamblerStyles.size(); ++g) {
      Hero::CasinoRoom& room = h.casinoRooms[g % h.casinoRooms.size()];
      int stake = 15, spare = 125;   // midpoints of the style's two bet bands
      switch (h.casinoGamblerStyles[g]) {
        case 2: stake = 23; spare = 213; break;
        case 3: stake = 33; spare = 300; break;
        case 4: stake = 50; spare = 500; break;
        case 5: stake = 80; spare = 750; break;
        default: break;
      }
      long long n = (long long)stake + base;
      if (room.popularity <= 40) n /= 4;
      else if (room.popularity <= 80) n /= 2;
      const long long wager = (long long)room.minBet * spare / 100 * n;
      gamblerWagers += wager;
      gamblerHouseWins += wager * room.houseWinRate / 100;
    }
  }

  // ---- the amenities and the house's own running costs ----
  long long amenityRevenue = 0, amenityExpenses = 0;
  for (size_t i = 0; i < h.casinoAmenityMonthly.size(); ++i) {
    const long long monthly = h.casinoAmenityMonthly[i];
    amenityExpenses += monthly * 12;
    // SimCasinoAmenity::GetAnnualRevenue (vaddr 0x2aba13c): theme match is worth
    // +200 or -50, the casino popularity band adds RNG(50,100) / RNG(100,150) /
    // RNG(150,200) at <=40 / <=80 / above, and type 4 or 5 adds its own band.
    double x = i < h.casinoAmenityTheme.size() && lower(h.casinoAmenityTheme[i]) == lower(h.casinoTheme)
                   ? 200.0 : -50.0;
    if (h.casinoPopularity > 80) x += 150 + int(casinoRng_.below(50));
    else if (h.casinoPopularity > 40) x += 100 + int(casinoRng_.below(50));
    else x += 50 + int(casinoRng_.below(50));
    if (h.casinoType == 5) x += 300 + int(casinoRng_.below(200));
    else if (h.casinoType == 4) x += 100 + int(casinoRng_.below(100));
    amenityRevenue += (long long)(x / 100.0) * (monthly * 12);
  }
  long long roomExpenses = 0;
  for (size_t i = 0; i < h.casinoRooms.size(); ++i)
    roomExpenses += (long long)h.casinoRooms[i].costPerUnit * h.casinoRooms[i].units;

  const long long wagersPlaced = roomWagers + gamblerWagers;
  const long long payouts = wagersPlaced - roomWins - gamblerHouseWins;
  const long long gamblingRevenue = wagersPlaced - payouts;
  // shortcut: an act's salary and a celebrity's fee have no table column and no
  // recovered setter, so the port cannot price their share of the year's take.
  const long long performanceRevenue = 0;
  const long long gross = gamblingRevenue + performanceRevenue + amenityRevenue;
  // SimCasino::GetMonthlyExpenses (vaddr 0x2ab2d5c) cost-of-living adjusts the
  // structure row's Base Monthly Cost with a floor of 100 and rounds to 100.
  // shortcut: the adjustment ratio is the hero country's Price Of Coke against
  // the reference country's, and the reference country is not recovered, so the
  // port charges the nominal amount.
  long long monthly = h.casinoBaseMonthlyCost / 100 * 100;
  if (monthly < 100) monthly = 100;
  const long long expenses = amenityExpenses + roomExpenses + monthly * 12;
  const double taxable = double(gross - expenses);
  double taxes = casinoTaxRate(taxable) * taxable;
  if (taxes < 0.0) taxes = 0.0;
  const long long net = gross - (long long)taxes - expenses;

  h.ledgerWagers = wagersPlaced;
  h.ledgerPayouts = payouts;
  h.ledgerGamblingRevenue = gamblingRevenue;
  h.ledgerPerformanceRevenue = performanceRevenue;
  h.ledgerAmenityRevenue = amenityRevenue;
  h.ledgerTaxes = (long long)taxes;
  h.ledgerExpenses = expenses;
  h.ledgerNetIncome = net;
  h.casinoBank += net;
  // SimCasino::IncrementAge counts the losing years itself (REPORT.en.md 320): the
  // counter at SimCasino+0x88 rises while BankBalance[0x28] is negative and fires
  // the card at six, though the card's own text says five.
  if (h.casinoBank < 0) {
    ++h.casinoBankruptYears;
    if (h.casinoBankruptYears >= 6) {
      h.casinoBankruptYears = 0;
      life.pendingCards.push_back(kPendingCasinoBankrupt);
    }
  } else {
    h.casinoBankruptYears = 0;
  }
}

// ScenarioCasinoAgeUpBankrupt (title #12796 "Bankrupt", body #50267). Its handler
// calls ShutDownCasino and logs; it writes no money and no stats, so closing the
// house costs the hero nothing beyond the casino itself (REPORT.en.md 320).
bool Sim::casinoBankruptYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.ownsCasino) return false;
  const std::vector<std::string> prior = out.extra;
  out = AgeUp();
  out.extra = prior;
  out.table = "registry:ScenarioCasinoAgeUpBankrupt";
  out.title = "Bankrupt";
  out.body = "Your casino has been bankrupt for 5 years.\n\nYou were forced to close it for good.";
  out.options = {"Ok"};
  out.choice = out.options[0];
  // ShutDownCasino drops the casino (the pot, the rooms, the amenities) and leaves
  // the hero's own money alone -- the bank balance was never hero money (319).
  h.ownsCasino = false;
  h.casinoBank = 0;
  h.casinoRooms.clear();
  h.casinoGames.clear();
  h.casinoAmenities.clear();
  h.casinoAmenityMonthly.clear();
  h.casinoAmenityTheme.clear();
  h.casinoGamblerStyles.clear();
  h.casinoBankruptYears = 0;
  return true;
}

// ScenarioCasinoAgeUpGamblerComplaintGame (REPORT.en.md 322): a punter corners the
// hero about a game the floor does not run.
//
// The card's text is recovered as concatenated literals: "Your casino's " #50268, the
// gambler's tier, ', ' #5282, the name, ", is complaining that your casino doesn't have "
// #5667, the possessive, ' favorite game, ' #1953, the game, and a full stop. Title
// #16066 'Complaint'. The three choices are #12770 'Ban ', #11554 'Apologize for the
// inconvenience' and #26366 'Ignore '; the two short labels end in a space because the
// native card appends the gambler's name to them.
//
// ScenarioCasinoAgeUpGamblerComplaintGame speaks about a regular by type, not by a
// five-step rank ladder: the relationship slot is 'casual gambler' or 'high roller' or
// 'Mafia member' (REPORT.en.md 358). The port has no mafia regulars yet (357's ceiling),
// so only the first two appear.
bool Sim::casinoComplaintYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || h.age < 18 || h.inPrison) return false;
  // The native eligibility draws nothing: the card fires when some regular's
  // favourite game has no matching room installed, and names that game
  // (REPORT.en.md 358). Two years of quiet follow a card.
  if (h.casinoComplaintAge >= 0 && h.age - h.casinoComplaintAge <= 2) return false;
  std::string game;
  if (ensureTable("a16_casino-casino-game-room-enc")) {
    const Table& games = tbl("a16_casino-casino-game-room-enc");
    for (size_t r = 0; r < games.size() && game.empty(); ++r) {
      const std::string g = trim(games.at(r, "Game"));
      if (g.empty()) continue;
      bool built = false;
      for (size_t i = 0; i < h.casinoRooms.size(); ++i)
        if (lower(h.casinoRooms[i].game) == lower(g)) { built = true; break; }
      if (!built) game = g;
    }
  }
  if (game.empty()) return false;
  hero_ = &h;
  life_ = &life;

  int style = 3;
  if (!h.casinoGamblerStyles.empty()) {
    const size_t pick = casinoCardRng_.below(uint32_t(h.casinoGamblerStyles.size()));
    style = h.casinoGamblerStyles[pick];
  }
  if (style < 1) style = 1;
  if (style > 5) style = 5;
  const bool female = casinoCardRng_.chance(0.5);
  std::string gambler = resolve("<casino-gambler>");
  if (gambler.empty()) gambler = female ? "Jane Doe" : "John Doe";

  // The row's own line follows the sentence, and the picker filters the table by
  // the floor's mood first (GetRandomCasinoGameRoomComplaintForHappiness): a happy
  // floor never hears the grievance about the missing game.
  std::string rowText;
  {
    const size_t r = pickBandRow("a16_casino-casino-game-room-complaint-enc",
                                 "Min Gambler Happiness", "Max Gambler Happiness",
                                 h.casinoGamblerHappiness, casinoCardRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_casino-casino-game-room-complaint-enc");
      rndCache_.clear();
      rbLast_.clear();
      const bool savedSet = pronounOtherSet_;
      const bool savedFemale = pronounOtherFemale_;
      pronounOtherSet_ = true;
      pronounOtherFemale_ = female;
      // The row reads as its own sentence after the blank line, so it opens
      // capitalised (the same treatment the joined banner and sting sentences get).
      rowText = spyUpperFirst(resolve(trim(t.at(r, "String"))));
      pronounOtherSet_ = savedSet;
      pronounOtherFemale_ = savedFemale;
    }
  }

  const std::string rel = style >= 5 ? "high roller" : "casual gambler";
  out.table = "registry:ScenarioCasinoAgeUpGamblerComplaintGame";
  out.title = "Complaint";
  out.body = std::string("Your casino's ") + rel + ", " + gambler +
             ", is complaining that your casino doesn't have " +
             (female ? "her" : "his") + " favorite game, " + game + ".";
  if (!rowText.empty()) out.body += "\n\n" + rowText;
  out.cta = "What will you do?";  // #146
  // The two short labels carry the punter's pronoun (#12770 'Ban ' and #26366
  // 'Ignore '), and a ban adds the tail #2104 ' from my casino'.
  const std::string them = female ? "her" : "him";
  out.options = {"Ban " + them + " from my casino",
                 "Apologize for the inconvenience", "Ignore " + them};
  const int pick = forcedChoice >= 0 ? (forcedChoice > 2 ? 2 : forcedChoice)
                                     : int(casinoCardRng_.below(3));
  out.choice = out.options[pick];
  h.casinoComplaintAge = h.age;

  if (pick == 1) {
    // Apologising buys the punter a little more appetite for the floor.
    h.casinoGamblerAddiction += 2;
    if (h.casinoGamblerAddiction > 100) h.casinoGamblerAddiction = 100;
    return true;
  }
  // Banning and ignoring both cost the house standing; a ban also reports what the
  // gambler did, banded by how crazy the floor already thinks he is. The native band
  // column is Gambler Craziness, which this port does not track, so the floor's
  // happiness is used for the band instead (ceiling).
  h.casinoPopularity -= 2;
  if (h.casinoPopularity < 0) h.casinoPopularity = 0;
  if (pick == 0 && ensureTable("a16_casino-banned-gambler-snippet-enc")) {
    const size_t r = pickWindow("a16_casino-banned-gambler-snippet-enc", "", "", "",
                                "Min Gambler Craziness", "Max Gambler Craziness",
                                h.casinoGamblerHappiness, casinoCardRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_casino-banned-gambler-snippet-enc");
      const std::string who = female ? "She " : "He ";
      // The snippet's <his/her> tokens follow the gambler, not the hero.
      const bool savedSet = pronounOtherSet_;
      const bool savedFemale = pronounOtherFemale_;
      pronounOtherSet_ = true;
      pronounOtherFemale_ = female;
      const std::string why = resolve(trim(t.at(r, "Snippet")));
      if (!why.empty()) out.body += "\n\n" + spySentence(who + why);
      const std::string logv = resolve(trim(t.at(r, "Log Version")));
      pronounOtherSet_ = savedSet;
      pronounOtherFemale_ = savedFemale;
      if (!logv.empty()) out.extra.push_back(spySentence(who + spySentence(logv)));
      out.table = "a16_casino-banned-gambler-snippet-enc";
      out.row_id = std::to_string(r + 1);
    }
  }
  return true;
}

// ScenarioCasinoAgeUpGamblerComplimentGame (REPORT.en.md 324) is the complaint
// card's positive twin: a punter who likes the floor names his favourite game and
// the hero answers. The sentence is the same nine-part concatenation, with
// 'is complimenting your casino for having ' #5672 in place of the grievance,
// and the row's own line follows it, picked by the floor's mood.
//
// Choices: 'Thank ' #41215, 'Give ' #21649 ... ' some free games' #3411, 'Ignore '
// #26366, each with the punter's object pronoun appended. Title #16100 'Compliment'.
//
// Ceilings: the two casino vtable slots the handler moves (+0x738 and +0x508, both
// integers) are not identified in the dump. The port pairs them with the two bars it
// keeps -- popularity takes the positive slot, the floor's happiness the negative one
// -- so 'thank' ends popularity +1 and happiness -1, 'give' popularity +2 and
// happiness -2, 'ignore' popularity -2 on its own. That pairing is the port's.
// GameReference::CapitalizeAllMajorWordsInString: the game names sit mid-sentence,
// so the card title-cases them itself.
std::string allMajorWords(const std::string& text) {
  std::string s = text;
  bool start = true;
  for (char& c : s) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (u == ' ' || u == '\t' || u == '\n') {
      start = true;
      continue;
    }
    if (start) {
      c = static_cast<char>(::toupper(u));
      start = false;
    }
  }
  return s;
}

bool Sim::casinoComplimentYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || h.age < 18 || h.inPrison) return false;
  // Life::YearsSinceScenario('ScenarioCasinoAgeUpGamblerComplimentGame') > 4.
  if (h.casinoComplimentAge >= 0 && h.age - h.casinoComplimentAge <= 4) return false;
  if (forcedChoice < 0 && !casinoCardRng_.chance(0.10)) return false;
  hero_ = &h;
  life_ = &life;
  casinoGambler_.clear();  // one punter per card
  casinoRoom_.clear();
  static const char* kTiers[] = {"Beginner", "Amateur", "Novice", "Regular",
                                 "High Roller"};
  int style = 3;
  if (!h.casinoGamblerStyles.empty()) {
    const size_t pick = casinoCardRng_.below(uint32_t(h.casinoGamblerStyles.size()));
    style = h.casinoGamblerStyles[pick];
  }
  if (style < 1) style = 1;
  if (style > 5) style = 5;
  const bool female = casinoCardRng_.chance(0.5);
  std::string gambler = resolve("<casino-gambler>");
  if (gambler.empty()) gambler = female ? "Jane Doe" : "John Doe";
  std::string game = resolve("<casino-game>");
  if (game.empty()) game = "blackjack";
  game = allMajorWords(game);

  std::string rowText;
  {
    const size_t r = pickBandRow("a16_casino-gambler-game-room-compliment-enc",
                                 "Min Gambler Happiness", "Max Gambler Happiness",
                                 h.casinoGamblerHappiness, casinoCardRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_casino-gambler-game-room-compliment-enc");
      rndCache_.clear();
      rbLast_.clear();
      const bool savedSet = pronounOtherSet_;
      const bool savedFemale = pronounOtherFemale_;
      pronounOtherSet_ = true;
      pronounOtherFemale_ = female;
      rowText = spyUpperFirst(resolve(trim(t.at(r, "String"))));
      pronounOtherSet_ = savedSet;
      pronounOtherFemale_ = savedFemale;
    }
  }

  h.casinoComplimentAge = h.age;
  const std::string his = female ? "her" : "his";
  const std::string them = female ? "her" : "him";
  const std::string subj = female ? "she" : "he";
  out.table = "registry:ScenarioCasinoAgeUpGamblerComplimentGame";
  out.title = "Compliment";  // #16100
  out.body = std::string("Your casino's ") + kTiers[style - 1] + ", " + gambler +
             ", is complimenting your casino for having " + his +
             " favorite game, " + game + ".";  // #50268 #5282 #5672 #1953 #6312
  if (!rowText.empty()) out.body += "\n\n" + rowText;  // #46
  out.cta = "What will you do?";                          // #146
  out.options = {"Thank " + them, "Give " + them + " some free games",
                 "Ignore " + them};  // #41215 #21649 #3411 #26366
  const int pick = forcedChoice >= 0 ? (forcedChoice > 2 ? 2 : forcedChoice)
                                     : int(casinoCardRng_.below(3));
  out.choice = out.options[pick];
  // The refresh's default journal line, which every branch keeps.
  out.narrative = "I thanked my casino's gambler, " + gambler + ", for " + his +
                  " compliment about my casino.";  // #25215 #5547 #1659

  if (pick == 0) {
    h.casinoGamblerAddiction -= 1;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    if (h.casinoPopularity > 0) h.casinoPopularity -= 1;   // slot A (shortcut)
    if (h.casinoPopularity < 100) h.casinoPopularity += 1; // slot B (shortcut)
  } else if (pick == 1) {
    h.casinoGamblerAddiction += 2;
    if (h.casinoGamblerAddiction > 100) h.casinoGamblerAddiction = 100;
    if (h.casinoPopularity > 1) h.casinoPopularity -= 2;   // slot A (shortcut)
    if (h.casinoPopularity < 99) h.casinoPopularity += 2;  // slot B (shortcut)
    out.narrative += "\nI gave my casino's gambler, " + gambler +
                     ", some free games after " + subj +
                     " complimented my casino.";  // #341 #5857 #1660
  } else {
    out.narrative += "\nI ignored my casino's gambler, " + gambler + ", after " +
                     subj + " complimented my casino.";  // #351 #5350 #1660
    h.casinoPopularity -= 2;  // ImpactPopularity(-2)
    if (h.casinoPopularity < 0) h.casinoPopularity = 0;
  }
  return true;
}

// --------------------------------------------- the casino's mob favour cards --
// ScenarioCasinoAgeUpMafiaMemberLaunder and ...MafiaMemberInterrogation
// (REPORT.en.md 345). Both are pushed from SimCasino::IncrementAge when the house
// has a mob member attached and the hero has already used one to lean on a
// punter (SimCasino+0x99). The port keeps no per-casino member list, so the hero
// being made and owning the house stands for the attachment (shortcut), and the
// member is a colleague of the hero's family with a nickname out of the mob's
// own table.
void Sim::casinoMobDraw(std::string& name, std::string& first, std::string& nick,
                        bool& female) {
  female = casinoMobRng_.chance(0.5);
  name = mafiaToken("mafia-colleague");
  // The colleague token already scoped the pronoun of the cards' own person; make
  // sure the gender the cards print and the gender the token picked agree.
  pronounOtherSet_ = true;
  pronounOtherFemale_ = female;
  if (name.empty()) name = "a man from the family";
  const size_t space = name.find(' ');
  first = space == std::string::npos ? name : name.substr(0, space);
  const std::string last = space == std::string::npos ? std::string() : name.substr(space + 1);
  nick.clear();
  if (ensureTable("a16_mafia-nicknames-enc")) {
    const Table& t = tbl("a16_mafia-nicknames-enc");
    const std::string country = hero_ == nullptr ? std::string() : hero_->mafiaCountry;
    const auto gather = [&](bool useCountry) {
      std::vector<size_t> out;
      for (size_t i = 0; i < t.size(); ++i) {
        const std::string c = trim(t.at(i, "Mob Country"));
        if (useCountry && !c.empty() && !country.empty() && c != country) continue;
        const std::string g = trim(t.at(i, "Gender"));
        const bool saysFemale = g.find("emale") != std::string::npos;
        const bool saysMale = g.find("ale") != std::string::npos && !saysFemale;
        if (g.empty() || (!saysFemale && !saysMale) || saysFemale == female) out.push_back(i);
      }
      return out;
    };
    // The nicknames are keyed on the mob countries (it, ru, ie, jp, mx, cn), so a
    // hero whose family has no rows there falls back to the whole gender-matched
    // pool rather than to no nickname at all (shortcut).
    std::vector<size_t> pool = gather(true);
    if (pool.empty()) pool = gather(false);
    if (!pool.empty()) {
      const size_t r = pool[size_t(casinoMobRng_.below(uint32_t(pool.size())))];
      std::string cell = trim(t.at(r, "Nickname"));
      // The nickname cells carry the member's own first and last name, so fill
      // both here rather than letting the card's resolver answer them with some
      // other person's name.
      const auto fill = [&](const std::string& token, const std::string& value) {
        size_t at = cell.find(token);
        while (at != std::string::npos) {
          cell.replace(at, token.size(), value);
          at = cell.find(token, at + value.size());
        }
      };
      fill("<first-name>", first);
      fill("<last-name>", last.empty() ? first : last);
      if (cell.find('<') != std::string::npos) cell.clear();  // a token the table does not define
      nick = cell;
    }
  }
  if (nick.empty()) nick = first;
}

bool Sim::casinoMafiaLaunderYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || !h.mafia || h.age < 18 || h.inPrison) return false;
  // Life::YearsSinceScenario('ScenarioCasinoAgeUpMafiaMemberLaunder') > 1.
  if (h.casinoLaunderAge >= 0 && h.age - h.casinoLaunderAge <= 1) return false;
  if (forcedChoice < 0 && !casinoMobRng_.chance(0.12)) return false;
  hero_ = &h;
  life_ = &life;
  std::string name, first, nick;
  bool female = false;
  casinoMobDraw(name, first, nick, female);
  // The offer is a localised 100k-500k band rounded to ten and the cut is 10-20%
  // of it, also rounded to ten. The port has no country pricing, so the dollar
  // band is used as it stands (shortcut).
  const long long amount = (100000 + (long long)casinoMobRng_.below(400001)) / 10 * 10;
  const int pct = 10 + int(casinoMobRng_.below(11));
  const long long cut = ((amount * pct) / 100) / 10 * 10;

  h.casinoLaunderAge = h.age;
  const std::string poss = female ? "her" : "his";
  const std::string subj = female ? "She" : "He";
  out.table = "registry:ScenarioCasinoAgeUpMafiaMemberLaunder";
  out.title = "Favor";  // #19974
  out.body = "\"" + nick + "\" " + name + " is asking for a favor on behalf of " +
             poss + " Mafia family.";  // #2517 #898
  out.body += "\n\n" + subj +
              " asks if you could launder some money through your casino and "
              "promises to give you a cut if you agree.";  // #1352
  out.cta = "What will you do?";
  out.options = {"Do it", "Refuse"};  // #17913 #35505
  const int pick = forcedChoice >= 0 ? (forcedChoice > 1 ? 1 : forcedChoice)
                                     : int(casinoMobRng_.below(2));
  out.choice = out.options[pick];
  // The refresh's journal. Its relationship-status slot (member vtable +0x4f8) is
  // unrecovered, so the punter tier of the floor's own table stands in (shortcut).
  static const char* kTiers[] = {"Beginner", "Amateur", "Novice", "Regular",
                                 "High Roller"};
  int style = 3;
  if (!h.casinoGamblerStyles.empty()) {
    style = h.casinoGamblerStyles[size_t(casinoMobRng_.below(
        uint32_t(h.casinoGamblerStyles.size())))];
  }
  if (style < 1) style = 1;
  if (style > 5) style = 5;
  out.narrative = std::string("My casino's ") + kTiers[style - 1] + ", \"" + nick +
                  "\" " + name + ", asked me to launder $" + std::to_string(amount) +
                  " through my casino and offered me $" + std::to_string(cut) +
                  " to do so.";

  if (pick == 0) {
    h.money += cut;
    h.casinoBank += amount;
    h.casinoMobMood += 2;   // member slot +0x508
    if (h.casinoMobMood > 100) h.casinoMobMood = 100;
    h.casinoMobView -= 2;   // member slot +0x738
    if (h.casinoMobView < 0) h.casinoMobView = 0;
    h.casinoGamblerAddiction -= 2;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    out.narrative += "\nI accepted the bribe.";  // #238
  } else {
    h.casinoMobMood -= 3;
    if (h.casinoMobMood < 0) h.casinoMobMood = 0;
    h.casinoMobView += 2;
    if (h.casinoMobView > 100) h.casinoMobView = 100;
    h.casinoGamblerAddiction -= 2;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    out.narrative += "\nI refused the bribe.";  // #390
  }
  return true;
}

bool Sim::casinoMafiaInterrogYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || !h.mafia || h.age < 18 || h.inPrison) return false;
  // Life::YearsSinceScenario('ScenarioCasinoAgeUpMafiaMemberInterrogation') > 1.
  if (h.casinoInterrogAge >= 0 && h.age - h.casinoInterrogAge <= 1) return false;
  if (forcedChoice < 0 && !casinoMobRng_.chance(0.12)) return false;
  hero_ = &h;
  life_ = &life;
  std::string name, first, nick;
  bool female = false;
  casinoMobDraw(name, first, nick, female);
  const long long offer = (100000 + (long long)casinoMobRng_.below(400001)) / 10 * 10;

  h.casinoInterrogAge = h.age;
  const std::string poss = female ? "her" : "his";
  const std::string subj = female ? "She" : "He";
  const std::string obj = female ? "her" : "him";
  out.table = "registry:ScenarioCasinoAgeUpMafiaMemberInterrogation";
  out.title = "Favor";  // #19974
  out.body = name + " is asking for a favor on behalf of " + poss +
             " Mafia family.";  // #2517 #898
  out.body += "\n\n" + subj + " asks if " + poss +
              " Mafia family members could \"interrogate\" someone in one of your "
              "casino's empty storage rooms and offers you some cash if you "
              "agree.";  // #1350 #897
  out.cta = "What will you do?";
  out.options = {"Do it", "Refuse"};  // #17913 #35505
  const int pick = forcedChoice >= 0 ? (forcedChoice > 1 ? 1 : forcedChoice)
                                     : int(casinoMobRng_.below(2));
  out.choice = out.options[pick];
  out.narrative = name + " asked me to let " + obj +
                  " interrogate someone in my casino's empty storage room. " +
                  subj + " offered me $" + std::to_string(offer) + " to let " +
                  obj + ".";

  if (pick == 0) {
    h.money += offer;
    h.casinoMobMood += 2;   // member slot +0x508
    if (h.casinoMobMood > 100) h.casinoMobMood = 100;
    h.casinoMobView -= 2;   // member slot +0x738
    if (h.casinoMobView < 0) h.casinoMobView = 0;
    h.casinoGamblerAddiction -= 2;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    out.narrative += "\n\nI accepted the bribe";  // #71
  } else {
    h.casinoMobMood -= 3;
    if (h.casinoMobMood < 0) h.casinoMobMood = 0;
    h.casinoMobView += 2;
    if (h.casinoMobView > 100) h.casinoMobView = 100;
    h.casinoGamblerAddiction -= 2;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    out.narrative += "\n\nI refused the bribe";  // #83
  }
  return true;
}

// DataHub::GetTaxRateForUnadjustedIncomeInCountry is the US ladder scaled by the
// owner country's TopTaxRate over the US one. DataCountry has no CSV export in
// this build (no table carries a tax column), so the scale is 1.0 and the hero
// pays the US marginal rate wherever he lives (shortcut).
double Sim::casinoTaxRate(double taxable) const {
  static const double kThreshold[] = {0.0, 9325.0, 37950.0, 91900.0, 191650.0, 416700.0, 418400.0};
  static const double kRate[] = {0.10, 0.15, 0.25, 0.28, 0.33, 0.35, 0.396};
  if (taxable <= 0.0) return 0.0;
  int band = 0;
  for (int i = 0; i < 7; ++i)
    if (taxable >= kThreshold[i]) band = i;
  return kRate[band];
}

bool Sim::casinoYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.ownsCasino || h.age < 18) return false;
  ensureCasinoFields(life);
  // The books are kept every year, whether or not a card comes up (319).
  casinoRunYear(life);
  if (!casinoRng_.chance(0.50)) return false;
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
            // The act's own theme, which the two act cards weigh against the house:
            // a singer's act brings the genre row's theme (already the house's, since
            // only matching rows are booked), any other act draws one (shortcut;
            // REPORT 350).
            h.casinoActTheme.clear();
            const size_t bp = pickRow("a16_casino-casino-performance-act-enc", "", "Act Type", actCat, "", casinoRng_);
            if (bp != size_t(-1)) h.casinoActPopularity = int(tbl("a16_casino-casino-performance-act-enc").num(bp, "Base Popularity"));
            if (h.casinoActTheme.empty()) {
              h.casinoActTheme = pickSnippet("a16_casino-casino-theme-enc", "Theme",
                                             "Popularity Score", "", "", "", casinoFloorRng_);
            }
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
              h.casinoActTheme = h.casinoTheme;
              h.casinoSong = pickSnippet("a16_casino-casino-singer-song-enc", "Song Title", "Commonness Score", "", "", "", casinoRng_);
            }
            h.casinoPopularity += h.casinoActPopularity * 2;
            tell("I booked " + h.casinoAct + (h.casinoActType.empty() ? std::string() : " the " + h.casinoActType) + ".");
            h.casinoActMetAge = h.age;
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
          // The native reads the guest's own genre off the celebrity record; the
          // port has no celebrity entity, so a genre is drawn from the singer
          // genre table when the guest first appears and kept for the two gambler
          // cards that weigh it against the house theme (shortcut).
          h.casinoCelebrityGenre = pickSnippet("a16_casino-casino-singer-genre-enc", "Genre",
                                               "", "", "", "", casinoRng_);
          h.casinoCelebrityGenreTheme = casinoGenreThemeOf(h.casinoCelebrityGenre);
          h.casinoCelebCraziness = 20 + int(casinoRng_.below(71));
          h.casinoCelebFemale = pronounOtherSet_ ? pronounOtherFemale_ : casinoRng_.chance(0.5);
          h.casinoCelebMetAge = h.age;
          h.casinoCelebRel = 20 + int(casinoRng_.below(61));
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

  // (18) The annual review: the ledger card prints what the year's pass stored
  //      (REPORT.en.md 319). ScenarioIsEligibleForRandomPush (vaddr 0x3564d24)
  //      is a plain gate -- hero age 18 or over and not in prison -- with no RNG,
  //      so the native queue gets this card every year. shortcut: the port deals
  //      one casino card a year, so the ledger only shows when no other casino
  //      card claimed the year.
  if (title.empty() && h.age >= 18 && !h.inPrison) {
    const std::string pop = std::to_string(h.casinoPopularity);
    const size_t fr = pickWindow("a16_casino-casino-ledger-feedback-enc", "", "", "", "Min Casino Popularity",
                                 "Max Casino Popularity", double(h.casinoPopularity), casinoRng_);
    std::string feedback;
    if (fr != size_t(-1)) {
      const Table& ft = tbl("a16_casino-casino-ledger-feedback-enc");
      feedback = resolve(trim(ft.at(fr, "String")));
      tableRef = ft.name();
      rowRef = std::to_string(fr + 1);
    }
    // "HasItemsInstalled": a house with no room and no amenity reports the empty
    // variant and lists its costs only.
    const bool hasItems = !h.casinoRooms.empty() || !h.casinoAmenities.empty();
    std::string rows;
    auto row = [&rows](const std::string& label, const std::string& value) {
      if (!rows.empty()) rows += "\n";
      rows += label + ": " + value;
    };
    if (hasItems) {
      row("Wagers Placed", money(h.ledgerWagers));
      row("Payouts", money(h.ledgerPayouts));
      row("Gambling Revenue", money(h.ledgerGamblingRevenue));
      if (h.casinoRooms.size() > 1) {
        size_t best = 0, worst = 0;
        for (size_t i = 1; i < h.casinoRooms.size(); ++i) {
          if (h.casinoRooms[i].popularity > h.casinoRooms[best].popularity) best = i;
          if (h.casinoRooms[i].popularity < h.casinoRooms[worst].popularity) worst = i;
        }
        row("Most Popular Room", h.casinoRooms[best].game);
        row("Least Popular Room", h.casinoRooms[worst].game);
      }
      row("Performance Revenue", money(h.ledgerPerformanceRevenue));
      row("Amenity Revenue", money(h.ledgerAmenityRevenue));
      row("Taxes", money(h.ledgerTaxes));
    }
    // shortcut: the native card shows the popularity as a bar graph, and the port
    // has no bar in its card model, so the value goes into the body instead.
    row("Annual Expenses", money(h.ledgerExpenses));
    if (hasItems) row("Net Income", money(h.ledgerNetIncome));
    title = "Casino Ledger";
    body = feedback.empty() ? std::string() : feedback;
    body += hasItems ? "\n\nThe annual report for your casino is now available.\n\n"
                     : "\n\nThe annual report for your casino is now available.\n\nYour casino made no money.\n\n"
                       "Install game rooms and amenities, or hire performers in order for your casino to be profitable.\n\n";
    body += rows;
    body += "\n\nPopularity: " + pop;
    const long long revenue = h.ledgerGamblingRevenue + h.ledgerPerformanceRevenue + h.ledgerAmenityRevenue;
    tell("My casino made " + money(revenue) + " in revenue last year.");
    if (h.casinoPopularity > 100) h.casinoPopularity = 100;
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

namespace {
std::string casinoTierWord(const Hero& h, Rng& rng);  // defined below
}

// The act cards: ScenarioCasinoAgeUpGamblerComplaintEntertainmentAct (.ctor
// 0x356e0fc, RefreshScenario 0x356d134, ChoiceSelectedAtIndex 0x356d688,
// ScenarioIsEligibleForRandomPush 0x356df38, pushed at 0x2aacfd4) and
// ScenarioCasinoAgeUpGamblerComplimentEntertainmentAct (.ctor 0x35712c0,
// RefreshScenario 0x3570a00, ChoiceSelectedAtIndex 0x3570ec4,
// ScenarioIsEligibleForRandomPush 0x3571104, pushed at 0x2aad1a0).
//
// Both cards look for an act whose own theme is NOT the house theme - the complaint
// because the act does not fit, the compliment because ... the shipped eligibility
// says the same thing - and both work the same punter. The complaint keeps three
// answers and a journal each; the compliment writes no journal at all (the native
// shared tail skips LifeLog and the UI refresh when the narrative is null), which the
// port reproduces. REPORT 350.
bool Sim::casinoActComplaintYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || h.age < 18 || h.inPrison) return false;
  if (h.casinoAct.empty() || h.casinoGamblers <= 0) return false;
  if (lower(h.casinoActTheme) == lower(h.casinoTheme)) return false;
  if (h.casinoActComplaintAge >= 0 && h.age - h.casinoActComplaintAge <= 2) return false;
  if (forcedChoice < 0 && !casinoFloorRng_.chance(0.10)) return false;
  hero_ = &h;
  life_ = &life;

  const std::string tier = casinoTierWord(h, casinoFloorRng_);
  const std::string punter = resolve("<casino-gambler>");
  const bool female = casinoGamblerFemale_;
  const std::string them = female ? "her" : "him";
  const std::string who = female ? "she" : "he";

  out.table = "registry:ScenarioCasinoAgeUpGamblerComplaintEntertainmentAct";
  out.title = "Complaint";  // #16066
  out.body = "Your casino's " + tier + ", " + punter + ", is complaining that " +
             h.casinoAct + "'s performances ruin your casino's vibe.\n\n" + who +
             " saying their " + h.casinoActTheme + "-themed act doesn't fit the " +
             h.casinoTheme + " theme you're trying to establish.";
  out.cta = "What will you do?";
  out.options = {"Ban " + them + " from my casino", "Apologize for the inconvenience",
                 "Ignore " + them};
  const int pick = forcedChoice >= 0 ? (forcedChoice > 2 ? 2 : forcedChoice)
                                     : int(casinoFloorRng_.below(3));
  out.choice = out.options[size_t(pick)];
  h.casinoActComplaintAge = h.age;

  if (pick == 0) {
    h.casinoGamblers -= 1;
    if (h.casinoGamblers < 0) h.casinoGamblers = 0;
    h.casinoPopularity -= 2;
    if (h.casinoPopularity < 0) h.casinoPopularity = 0;
    // The native spells the pronoun into the literal, as the guest-complaint card does.
    out.narrative = "I banned my casino's gambler, " + punter +
                    ", from my casino after she complained about " + h.casinoAct + ".";
  } else if (pick == 1) {
    h.casinoGamblerAddiction += 2;
    if (h.casinoGamblerAddiction > 100) h.casinoGamblerAddiction = 100;
    h.casinoGamblerHappiness += 1;
    if (h.casinoGamblerHappiness > 100) h.casinoGamblerHappiness = 100;
    out.narrative = "I apologized to my casino's gambler, " + punter + ", for having booked " +
                    h.casinoAct + ".";
  } else {
    h.casinoGamblerAddiction -= 2;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    h.casinoPopularity += 2;
    if (h.casinoPopularity > 100) h.casinoPopularity = 100;
    out.narrative = "I ignored my casino's gambler, " + punter + "'s, complaint about " +
                    h.casinoAct + ".";
  }
  return true;
}

bool Sim::casinoActComplimentYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || h.age < 18 || h.inPrison) return false;
  if (h.casinoAct.empty() || h.casinoGamblers <= 0) return false;
  if (lower(h.casinoActTheme) == lower(h.casinoTheme)) return false;
  if (h.casinoActComplimentAge >= 0 && h.age - h.casinoActComplimentAge <= 4) return false;
  if (forcedChoice < 0 && !casinoFloorRng_.chance(0.10)) return false;
  hero_ = &h;
  life_ = &life;

  const std::string tier = casinoTierWord(h, casinoFloorRng_);
  const std::string punter = resolve("<casino-gambler>");
  const bool female = casinoGamblerFemale_;
  const std::string them = female ? "her" : "him";
  const std::string who = female ? "she" : "he";

  out.table = "registry:ScenarioCasinoAgeUpGamblerComplimentEntertainmentAct";
  out.title = "Compliment";  // #16100
  out.body = "Your casino's " + tier + ", " + punter + ", is complimenting you on booking " +
             h.casinoAct + " to perform at your casino.\n\n" + who +
             " says it's her favorite attraction.";
  out.cta = "What will you do?";
  out.options = {"Thank " + them, "Give " + them + " some free games", "Ignore " + them};
  const int pick = forcedChoice >= 0 ? (forcedChoice > 2 ? 2 : forcedChoice)
                                     : int(casinoFloorRng_.below(3));
  out.choice = out.options[size_t(pick)];
  h.casinoActComplimentAge = h.age;

  if (pick == 0) {
    h.casinoGamblerAddiction -= 1;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    h.casinoGamblerHappiness += 1;
    if (h.casinoGamblerHappiness > 100) h.casinoGamblerHappiness = 100;
  } else if (pick == 1) {
    h.casinoGamblerAddiction -= 2;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    h.casinoGamblerHappiness += 2;
    if (h.casinoGamblerHappiness > 100) h.casinoGamblerHappiness = 100;
  } else {
    h.casinoPopularity -= 2;
    if (h.casinoPopularity < 0) h.casinoPopularity = 0;
  }
  // No narrative: the native card writes none, and its shared tail then skips both
  // the log line and the UI refresh.
  return true;
}

// The amenity cards: ScenarioCasinoAgeUpGamblerComplaintAmenity (.ctor 0x356c074,
// RefreshScenario 0x356b0d8, ChoiceSelectedAtIndex 0x356b5c4,
// ScenarioIsEligibleForRandomPush 0x356beb4, pushed at 0x2aad250) and
// ScenarioCasinoAgeUpGamblerComplimentAmenity (.ctor 0x35701a8, RefreshScenario
// 0x356f7ac, ChoiceSelectedAtIndex 0x356fda8, ScenarioIsEligibleForRandomPush
// 0x356ffe8, pushed at 0x2aad288).
//
// The pair works the same way as the act cards next door, but the amenity's theme is
// weighed the other way round: the complaint wants an amenity that does NOT match the
// house theme, the compliment wants one that does. The compliment writes no journal
// either, and its 'give' answer raises the punter's addiction instead of lowering it -
// an inconsistency in the shipped card, kept here. REPORT 351.
bool Sim::casinoAmenityComplaintYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || h.age < 18 || h.inPrison) return false;
  if (h.casinoGamblers <= 0) return false;
  if (h.casinoAmenityComplaintAge >= 0 && h.age - h.casinoAmenityComplaintAge <= 2)
    return false;
  // The native searches the amenity list for one whose theme differs from the house's.
  size_t which = size_t(-1);
  for (size_t i = 0; i < h.casinoAmenities.size(); ++i) {
    if (i < h.casinoAmenityTheme.size() &&
        lower(h.casinoAmenityTheme[i]) != lower(h.casinoTheme)) {
      which = i;
      break;
    }
  }
  if (which == size_t(-1)) return false;
  if (forcedChoice < 0 && !casinoAmenityRng_.chance(0.10)) return false;
  hero_ = &h;
  life_ = &life;

  const std::string tier = casinoTierWord(h, casinoAmenityRng_);
  const std::string punter = resolve("<casino-gambler>");
  const bool female = casinoGamblerFemale_;
  const std::string them = female ? "her" : "him";
  const std::string amenity = h.casinoAmenities[which];

  out.table = "registry:ScenarioCasinoAgeUpGamblerComplaintAmenity";
  out.title = "Complaint";  // #16066
  out.body = "Your casino's " + tier + ", " + punter +
             ", is complaining that your casino is tacky.\n\n" +
             (female ? "she" : "he") + " saying your casino's " + amenity +
             " is the worst part.";
  out.cta = "What will you do?";
  out.options = {"Ban " + them + " from my casino", "Apologize for the inconvenience",
                 "Ignore " + them};
  const int pick = forcedChoice >= 0 ? (forcedChoice > 2 ? 2 : forcedChoice)
                                     : int(casinoAmenityRng_.below(3));
  out.choice = out.options[size_t(pick)];
  h.casinoAmenityComplaintAge = h.age;

  if (pick == 0) {
    h.casinoGamblers -= 1;
    if (h.casinoGamblers < 0) h.casinoGamblers = 0;
    h.casinoPopularity -= 2;
    if (h.casinoPopularity < 0) h.casinoPopularity = 0;
    // The native spells the pronoun into the literal, as its twin cards do.
    out.narrative = "I banned my casino's gambler, " + punter +
                    ", from my casino after she complained about my casino's " +
                    amenity + ".";
  } else if (pick == 1) {
    h.casinoGamblerAddiction += 2;
    if (h.casinoGamblerAddiction > 100) h.casinoGamblerAddiction = 100;
    h.casinoGamblerHappiness += 1;
    if (h.casinoGamblerHappiness > 100) h.casinoGamblerHappiness = 100;
    out.narrative = "I apologized to my casino's gambler, " + punter +
                    ", for having a " + amenity + " in my casino.";
  } else {
    h.casinoGamblerAddiction -= 2;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    h.casinoPopularity += 2;
    if (h.casinoPopularity > 100) h.casinoPopularity = 100;
    out.narrative = "I ignored my casino's gambler, " + punter +
                    "'s, complaint about my casino's " + amenity + ".";
  }
  return true;
}

bool Sim::casinoAmenityComplimentYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || h.age < 18 || h.inPrison) return false;
  if (h.casinoGamblers <= 0) return false;
  if (h.casinoAmenityComplimentAge >= 0 && h.age - h.casinoAmenityComplimentAge <= 4)
    return false;
  // This one wants an amenity that matches the house theme (String::Equals, no negation).
  size_t which = size_t(-1);
  for (size_t i = 0; i < h.casinoAmenities.size(); ++i) {
    if (i < h.casinoAmenityTheme.size() &&
        lower(h.casinoAmenityTheme[i]) == lower(h.casinoTheme)) {
      which = i;
      break;
    }
  }
  if (which == size_t(-1)) return false;
  if (forcedChoice < 0 && !casinoAmenityRng_.chance(0.10)) return false;
  hero_ = &h;
  life_ = &life;

  const std::string tier = casinoTierWord(h, casinoAmenityRng_);
  const std::string punter = resolve("<casino-gambler>");
  const bool female = casinoGamblerFemale_;
  const std::string them = female ? "her" : "him";
  std::string game = resolve("<casino-game>");
  if (game.empty()) game = "blackjack";
  const std::string amenity = h.casinoAmenities[which];

  out.table = "registry:ScenarioCasinoAgeUpGamblerComplimentAmenity";
  out.title = "Compliment";  // #16100
  out.body = "Your casino's " + tier + ", " + punter +
             ", is complimenting you on the quality of your casino's " + amenity + ".";
  out.body += std::string("\n\n") + (female ? "She" : "He") + " says it's " +
              (female ? "her" : "his") + " favorite place to go when " +
              (female ? "she's" : "he's") + " not playing " + game + ".";
  out.cta = "What will you do?";
  out.options = {"Thank " + them, "Give " + them + " some free games", "Ignore " + them};
  const int pick = forcedChoice >= 0 ? (forcedChoice > 2 ? 2 : forcedChoice)
                                     : int(casinoAmenityRng_.below(3));
  out.choice = out.options[size_t(pick)];
  h.casinoAmenityComplimentAge = h.age;

  if (pick == 0) {
    h.casinoGamblerAddiction -= 1;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    h.casinoGamblerHappiness += 1;
    if (h.casinoGamblerHappiness > 100) h.casinoGamblerHappiness = 100;
  } else if (pick == 1) {
    // Up, not down: the shipped card raises addiction on the 'some free games' answer.
    h.casinoGamblerAddiction += 2;
    if (h.casinoGamblerAddiction > 100) h.casinoGamblerAddiction = 100;
    h.casinoGamblerHappiness += 2;
    if (h.casinoGamblerHappiness > 100) h.casinoGamblerHappiness = 100;
  } else {
    h.casinoPopularity -= 2;
    if (h.casinoPopularity < 0) h.casinoPopularity = 0;
  }
  // No narrative: this card's narrative slot is never written either.
  return true;
}

void Sim::seedCasinoAmenity(Life& life, bool sameTheme) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  ensureCasinoFields(life);
  if (h.casinoGamblers <= 0) h.casinoGamblers = 3;
  if (!ensureTable("a16_casino-casino-amenity-enc")) return;
  const Table& t = tbl("a16_casino-casino-amenity-enc");
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string theme = trim(t.at(r, "Theme"));
    const std::string name = resolve(trim(t.at(r, "Amenity Name")));
    if (name.empty()) continue;
    if ((lower(theme) == lower(h.casinoTheme)) != sameTheme) continue;
    h.casinoAmenities.push_back(name);
    h.casinoAmenityMonthly.push_back(0);
    h.casinoAmenityTheme.push_back(theme.empty() ? h.casinoTheme : theme);
    return;
  }
}

bool Sim::casinoCelebResignYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.ownsCasino || h.casinoCelebrity.empty() || h.age < 18 || h.inPrison) return false;
  if (h.casinoCelebMetAge < 0 || h.age - h.casinoCelebMetAge < 4) return false;
  if (h.casinoCelebRel > 40) return false;
  if (h.casinoCelebResignAge >= 0 && h.age - h.casinoCelebResignAge <= 2) return false;
  if (forcedChoice < 0 && !casinoResignRng_.chance(0.25)) return false;
  const std::string tier = casinoTierWord(h, casinoResignRng_);
  const bool female = h.casinoCelebFemale;
  const std::string them = female ? "her" : "him";
  const std::string poss = female ? "her" : "his";
  const std::string she = female ? "She" : "He";
  const std::string shes = female ? "she's" : "he's";
  h.casinoCelebResignAge = h.age;
  out.table = "registry:ScenarioCasinoAgeUpCelebrityResignation";
  out.title = "Resignation";  // #36027
  out.body = "Your casino's " + tier + ", " + h.casinoCelebrity + ", is informing you that " +
             shes + " resigning from performing at your casino.\n\n" + she + " says " + shes +
             " booked a better gig at a bigger casino across town.";
  out.cta = "What will you do?";
  out.options = {"Offer " + them + " more money", "Beg " + them + " to stay",
                 "Wish " + them + " well"};
  int pick = forcedChoice < 0 ? int(casinoResignRng_.below(3)) : forcedChoice;
  if (pick > 2) pick = 2;
  out.choice = out.options[size_t(pick)];
  std::string narrative = "My casino's " + tier + ", " + h.casinoCelebrity + ", put in " + poss +
                          " resignation from " + poss + " position at my casino.";
  const auto drop = [&]() {
    h.casinoCelebrity.clear();
    h.casinoCelebrityGenre.clear();
    h.casinoCelebrityGenreTheme.clear();
    h.casinoCelebCraziness = 0;
    h.casinoCelebMetAge = -1;
    h.casinoCelebRel = 0;
  };
  if (pick == 0) {
    // The native chains ScenarioCasinoAgeUpCelebrityResignationOffer, whose body is
    // not recovered: the offer costs nothing here and the follow-up is not pushed
    // (ceiling).
  } else if (pick == 1) {
    if (h.casinoCelebRel >= int(casinoResignRng_.below(101))) {
      narrative += "\nI begged " + them + " to stay and " + (female ? "she" : "he") + " agreed.";
      h.casinoCelebRel = 90;
      h.casinoFeedbackTitle = "Fine, I guess";  // #20245
      h.casinoFeedbackBody = she + " agreed to stay on as your casino's " + tier + "!";
    } else {
      drop();
      h.casinoPopularity -= 2;
      if (h.casinoPopularity < 0) h.casinoPopularity = 0;
      narrative += "\n" + she + " refused to stay, even though I begged.";
      h.casinoFeedbackTitle = "No dice";  // #31612
      h.casinoFeedbackBody = she + " turned you down and resigned from " + poss +
                             " position as your casino's " + tier + ".";
    }
  } else {
    drop();
    narrative += "\nI wished " + them + " well.";
  }
  out.narrative = narrative;
  if (!h.casinoFeedbackTitle.empty()) life.pendingCards.push_back(kPendingCasinoFeedback);
  return true;
}

bool Sim::casinoActResignYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.ownsCasino || h.casinoAct.empty() || h.age < 18 || h.inPrison) return false;
  if (h.casinoActMetAge < 0 || h.age - h.casinoActMetAge <= 3) return false;
  if (h.casinoActResignAge >= 0 && h.age - h.casinoActResignAge <= 2) return false;
  if (forcedChoice < 0 && !casinoResignRng_.chance(0.25)) return false;
  h.casinoActResignAge = h.age;
  out.table = "registry:ScenarioCasinoAgeUpEntertainmentActResignation";
  out.title = "Resignation";  // #36027
  out.body = h.casinoAct + "'s manager is informing you that the act will no longer perform "
                           "at your casino.\n\nThey booked a better gig at a bigger casino "
                           "across town.";
  out.cta = "What will you do?";
  out.options = {"Offer them more money", "Beg them to stay", "Wish them well"};
  int pick = forcedChoice < 0 ? int(casinoResignRng_.below(3)) : forcedChoice;
  if (pick > 2) pick = 2;
  out.choice = out.options[size_t(pick)];
  std::string narrative = h.casinoAct + " put in their resignation from their position at my casino.";
  const auto drop = [&]() {
    h.casinoAct.clear();
    h.casinoActType.clear();
    h.casinoActTheme.clear();
    h.casinoActPopularity = 0;
    h.casinoActMetAge = -1;
  };
  if (pick == 0) {
    // The native chains ScenarioCasinoAgeUpEntertainmentActResignationOffer, whose body
    // is not recovered: the offer costs nothing here and the follow-up is not pushed
    // (ceiling).
  } else if (pick == 1) {
    if (h.casinoPopularity >= int(casinoResignRng_.below(101))) {
      narrative += "\nI begged them to stay and they agreed.";
      h.casinoFeedbackTitle = "Fine, I guess";  // #20245
      h.casinoFeedbackBody = "They agreed to continue performing at your casino!";
    } else {
      drop();
      h.casinoPopularity -= 2;
      if (h.casinoPopularity < 0) h.casinoPopularity = 0;
      narrative += "\nThey refused to stay, even though I begged.";
      h.casinoFeedbackTitle = "No dice";  // #31612
      h.casinoFeedbackBody = "They turned you down and resigned anyway.";
    }
  } else {
    drop();
    narrative += "\nI wished them well.";
  }
  out.narrative = narrative;
  if (!h.casinoFeedbackTitle.empty()) life.pendingCards.push_back(kPendingCasinoFeedback);
  return true;
}

bool Sim::casinoFeedbackYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (h.casinoFeedbackTitle.empty()) return false;
  out.table = "registry:Scenario";  // a plain Scenario(title, body), no choices
  out.title = h.casinoFeedbackTitle;
  out.body = h.casinoFeedbackBody;
  out.cta = "What will you do?";
  out.options = {"OK"};
  out.choice = "OK";
  h.casinoFeedbackTitle.clear();
  h.casinoFeedbackBody.clear();
  return true;
}

bool Sim::casinoGamblerDeathYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  life_ = &life;
  if (!h.ownsCasino || h.age < 18 || h.inPrison) return false;
  if (h.casinoGamblers <= 0) return false;
  // (shortcut) The native walks a persistent gambler list and asks each gambler's
  // script for a death age (SimThirdPersonScript[+0x18][+0x60]). The port keeps a
  // count of regulars and one lazily drawn regular instead, the way 354 does for
  // the zoo keeper; nothing is drawn until the card can actually fire.
  if (h.casinoPunterAge0 < 0) {
    h.casinoPunterFemale = casinoDeathRng_.chance(0.5);
    h.casinoPunterName = drawFirstName(h.casinoPunterFemale) + " " + drawSurname();
    h.casinoPunterAge0 = 18 + int(casinoDeathRng_.below(45));
    h.casinoPunterLifespan = 55 + int(casinoDeathRng_.below(31));
    h.casinoPunterMetAge = h.age;
  }
  const int punterAge = h.casinoPunterAge0 + (h.age - h.casinoPunterMetAge);
  if (punterAge < h.casinoPunterLifespan) return false;
  const std::string tier = casinoTierWord(h, casinoDeathRng_);
  const bool female = h.casinoPunterFemale;
  const std::string she = female ? "She" : "He";
  const std::string poss = female ? "her" : "his";
  const std::string name = h.casinoPunterName;
  // #57524 / #57525 carry their own verb, and #50270 reads "{3} {4}.", so both
  // causes read naturally here (unlike the zoo keeper's 354 pair).
  const std::string deathString =
      punterAge > 69 ? "died of old age" : "died of natural causes";
  out.table = "registry:ScenarioCasinoAgeUpGamblerDeath";
  out.title = "Death";  // #17288
  out.body = "Your casino's " + tier + ", " + name + ", passed away at the age of " +
             std::to_string(punterAge) + ".\n\n" + she + " " + deathString + ".";
  out.cta = "What will you do?";
  out.options = {"Attend " + poss + " funeral", "Skip it"};  // #12413 + #2153, #39181
  int pick = forcedChoice;
  if (pick < 0) pick = int(casinoDeathRng_.below(2));
  if (pick > 1) pick = 1;
  out.choice = out.options[size_t(pick)];
  std::string narrative = name + " died.";  // #1771
  if (pick == 0) out.karma += 1;
  out.narrative = narrative;
  h.casinoGamblers -= 1;
  if (h.casinoGamblers < 0) h.casinoGamblers = 0;
  h.casinoPunterAge0 = -1;
  h.casinoPunterLifespan = 0;
  h.casinoPunterMetAge = -1;
  h.casinoPunterName.clear();
  return true;
}

bool Sim::casinoCelebDeathYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  life_ = &life;
  if (!h.ownsCasino || h.casinoCelebrity.empty() || h.age < 18 || h.inPrison) return false;
  if (h.casinoCelebMetAge < 0) return false;
  // (shortcut) Same substitution as the gambler card above: the native reads the
  // act's death age off its own script, the port draws a lifespan on first reach.
  if (h.casinoCelebAge0 < 0) {
    h.casinoCelebAge0 = 18 + int(casinoDeathRng_.below(45));
    h.casinoCelebLifespan = 55 + int(casinoDeathRng_.below(31));
  }
  const int celebAge = h.casinoCelebAge0 + (h.age - h.casinoCelebMetAge);
  if (celebAge < h.casinoCelebLifespan) return false;
  const std::string tier = casinoTierWord(h, casinoDeathRng_);
  const bool female = h.casinoCelebFemale;
  const std::string she = female ? "She" : "He";
  const std::string poss = female ? "her" : "his";
  const std::string name = h.casinoCelebrity;
  const std::string deathString =
      celebAge > 69 ? "died of old age" : "died of natural causes";
  out.table = "registry:ScenarioCasinoCelebrityDeath";
  out.title = "Death";  // #17288
  out.body = "Your casino's " + tier + ", " + name + ", passed away at the age of " +
             std::to_string(celebAge) + ".\n\n" + she + " " + deathString + ".";
  out.cta = "What will you do?";
  out.options = {"Attend " + poss + " funeral", "Skip it"};
  int pick = forcedChoice;
  if (pick < 0) pick = int(casinoDeathRng_.below(2));
  if (pick > 1) pick = 1;
  out.choice = out.options[size_t(pick)];
  // The celebrity's journal is the ctor narrative: pronoun, " died ", cause, ".".
  std::string narrative = she + " died " + deathString + ".";
  if (pick == 0) out.karma += 1;
  out.narrative = narrative;
  h.casinoCelebrity.clear();
  h.casinoCelebrityGenre.clear();
  h.casinoCelebrityGenreTheme.clear();
  h.casinoCelebCraziness = 0;
  h.casinoCelebRel = 60;
  h.casinoCelebMetAge = -1;
  h.casinoCelebAge0 = -1;
  h.casinoCelebLifespan = 0;
  return true;
}

// ScenarioCasinoAgeUpCelebrityDemand (REPORT.en.md 356) is the guest act's own
// demand card: the artist-in-residence wants a perk provided, or refuses to
// perform. Native .ctor 0x3568614, RefreshScenario 0x3567260,
// ChoiceSelectedAtIndex 0x3567ac8, ScenarioIsEligibleForRandomPush 0x356845c,
// push site SimCasino::IncrementAge bl @0x2aacac8 (the second rung of the ordered
// cascade). Fields: _narrative +0x170, _casino +0x178, _celebrity +0x180,
// _perk +0x188, _perkData +0x190, _perkString +0x198.
//
// Eligibility: the celebrity must be alive, the hero must own a casino, its
// demand list must not already be satisfied, and a roll of GetRandomNumber(0,101)
// has to land under 80 when the celebrity's craziness is above its fame and under
// 50 otherwise; then hero age >= 18, not in prison, and
// YearsSinceScenario #37039 'ScenarioCasinoAgeUpCelebrityDemand' > 1. shortcut: the
// native roll happens before the age/prison/year tests and its exclusion list is
// the celebrity's own _requiredIdPerks (+0x348); the port tests the plain gates
// first and keeps the satisfied rows in casinoCelebPerks.
//
// A row of a16_casino-celebrity-demand-enc (loader DataHub::LoadCasinoCelebrityDemandSet
// 0x26c7ae0, picker GetRandomCasinoCelebrityDemand 0x26c8094) is picked
// automatically; the item goes through the game's bracket parser with
// DoNotCapitalize, takes 'some ' #65874 when its Plural cell is set and an
// indefinite article when its Uses Indefinite Article cell is set.
bool Sim::casinoCelebDemandYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || h.casinoCelebrity.empty() || h.casinoCelebMetAge < 0) return false;
  if (h.age < 18 || h.inPrison) return false;
  if (h.casinoCelebDemandAge >= 0 && h.age - h.casinoCelebDemandAge <= 1) return false;

  // The celeb's fame is not modelled, so it is drawn once from this card's own
  // stream; craziness is the booking's value (REPORT.en.md 347).
  if (h.casinoCelebFame < 0) h.casinoCelebFame = 20 + int(casinoDemandRng_.below(71));
  const int threshold = h.casinoCelebCraziness > h.casinoCelebFame ? 80 : 50;
  if (forcedChoice < 0 && int(casinoDemandRng_.below(101)) >= threshold) return false;

  ensureTable("a16_casino-celebrity-demand-enc");
  const Table& dt = tbl("a16_casino-celebrity-demand-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < dt.size(); ++r) {
    const int idx = int(dt.num(r, "Index"));
    bool done = false;
    for (size_t k = 0; k < h.casinoCelebPerks.size(); ++k)
      if (h.casinoCelebPerks[k] == idx) { done = true; break; }
    if (!done) pool.push_back(r);
  }
  if (pool.empty()) return false;
  const size_t drow = pool[casinoDemandRng_.below(uint32_t(pool.size()))];

  std::string perk = resolve(trim(dt.at(drow, "Item")));
  if (dt.flag(drow, "Plural")) perk = "some " + perk;
  if (dt.flag(drow, "Uses Indefinite Article")) perk = withArticle(perk, true) + perk;

  const bool female = h.casinoCelebFemale;
  const std::string obj = female ? "her" : "him";        // PronounStringForType 2
  const std::string sub = female ? "she" : "he";         // 0
  const std::string poss = female ? "her" : "his";       // 1
  const std::string cont = female ? "She's" : "He's";    // 9
  const std::string possCap = female ? "Her" : "His";    // 6

  // The status string SimCasinoCelebrity::RelationshipStatusString 0x2c277a0
  // returns is the single literal #54414 'artist-in-residence'.
  out.table = "registry:ScenarioCasinoAgeUpCelebrityDemand";
  out.title = "Demand";  // #17489
  out.body = std::string("Your casino's artist-in-residence, ") + h.casinoCelebrity +
             ", is demanding you provide " + obj + " with a perk, or " + sub +
             " won't perform.";
  out.body += "\n\n" + cont + " demanding you have " + perk + " on-hand before each of " +
              poss + " performances.";
  // TableDictionary (+0xa0) and TableOrderArray (+0xa8): 'Additional Expenses'
  // #10793 = GetYearlyExpenseForPerk (0x2c276d0 -> DataCountry local cost of
  // row.Cost x 12; the country scaling is not modelled here) and 'Request'
  // #35890 = the perk with CapitalizeAllMajorWords.
  std::string rows;
  auto row = [&rows](const std::string& label, const std::string& value) {
    if (!rows.empty()) rows += "\n";
    rows += label + ": " + value;
  };
  row("Additional Expenses", money(int(dt.num(drow, "Cost")) * 12));
  row("Request", allMajorWords(perk));
  out.body += "\n\n" + rows;
  // BarGraphArray (+0xb8): [0] {pron6} ' Fame' #822 over Att_fame (+0xb4),
  // [1] {pron6} ' Popularity' #922 over the celebrity's own popularity (+0x338).
  out.body += "\n\n" + possCap + " Fame: " + std::to_string(h.casinoCelebFame);
  out.body += "\n" + possCap + " Popularity: " + std::to_string(h.casinoCelebPop);
  out.cta = "What will you do?";  // #146
  out.options = {"Oblige " + obj, "Refuse"};

  int pick = forcedChoice < 0 ? int(casinoDemandRng_.below(2)) : forcedChoice;
  if (pick > 1) pick = 1;
  out.choice = out.options[size_t(pick)];
  h.casinoCelebDemandAge = h.age;

  // Both lanes chain a plain Scenario whose title is the single-element list
  // PickRandomFromStringArray over #27654 "It's a NEED, not a  want" (two spaces in
  // the literal); the port's existing feedback card renders it (REPORT.en.md 352).
  if (pick == 0) {
    // SimCasinoCelebrity::AddPerk 0x2c275a0(celebrity, row.Index, perk) and
    // ImpactPopularity 0x2c27830(celebrity, row['Celebrity Popularity Impact']).
    h.casinoCelebPerks.push_back(int(dt.num(drow, "Index")));
    h.casinoCelebPop += int(dt.num(drow, "Celebrity Popularity Impact"));
    out.narrative = "I began providing " + perk + " for my casino's artist-in-residence, " +
                    h.casinoCelebrity + ".";
    h.casinoFeedbackTitle = "It's a NEED, not a  want";  // #27654
    h.casinoFeedbackBody = "You began providing " + perk + " for " + h.casinoCelebrity +
                           " before each of " + poss + " performances.";
  } else {
    // The refusal lane insults the hero with DataHub::GetRandomInsultForGender
    // 0x2808580(hero.Gender) and writes no popularity change.
    const std::string insult = insultTokenForGender(h.female);
    const std::string msg = spyUpperFirst(sub) + " called you " + insult + ".";
    out.narrative = "I refused to give in to my casino's artist-in-residence, " +
                    h.casinoCelebrity + "'s, demand for " + perk + ".";
    h.casinoFeedbackTitle = "It's a NEED, not a  want";  // #27654
    h.casinoFeedbackBody = spyUpperFirst(sub) + " yelled at you for refusing " + poss +
                           " request.\n\n" + msg + ".";
  }
  if (h.casinoCelebPop < 0) h.casinoCelebPop = 0;
  if (h.casinoCelebPop > 100) h.casinoCelebPop = 100;
  life.pendingCards.push_back(kPendingCasinoFeedback);
  return true;
}

// ScenarioCasinoAgeUpGamblerNew (REPORT.en.md 357): a new regular starts
// frequenting the casino. Native .ctor 0x3573804, RefreshScenario 0x3572d4c,
// ScenarioIsEligibleForRandomPush 0x357373c, pushed from SimCasino::IncrementAge
// bl @0x2aacea0. Fields: _narrative +0x170, _casino +0x178. The card overrides no
// ChoiceSelectedAtIndex -- it is informational, with a single OK button and no
// dismiss button.
//
// Eligibility draws nothing: casino present, hero age >= 18, not in prison, and
// YearsSinceScenario(#37053 'ScenarioCasinoAgeUpGamblerNew') > 4. Refresh builds a
// whole SimCasinoGambler with PersonGenerator::GenerateRandomCasinoGambler
// 0x338bba8 (its second argument suppresses mafia-member generation while the
// casino has no mobsters), retries the name up to fifty times against the hero's
// known people, then SimCasino::AddGambler 0x2ab3a90 and ImpactPopularity(+2).
//
// Bodies: 'A ' #9953 + GamblerTypeString + #2805 ' named ' + name + #2583
// ' is now frequently gambling at your casino.'; the table rows are Age #10954,
// Occupation #32523, Gambling Style #20993 and Favorite Games #19976, the two
// bars are Money #30360 and Addiction #10777, the single choice is #32432 'OK',
// and the journal is 'A ' + GetGamblingStyleString + #2157 ' gambler named ' +
// name + #1478 ' began frequenting my casino.'.
//
// shortcut: the port has no gambler entity, so the newcomer's details are drawn
// from this card's own stream and printed, and the regular is only counted (its
// style goes onto casinoGamblerStyles so later cards can draw it). The mafia
// branch of the native generator is not modelled -- every newcomer is an ordinary
// gambler -- the fifty-attempt name collision retry collapses to one draw, and
// the two bar graphs are body lines.
bool Sim::casinoGamblerNewYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.ownsCasino || h.age < 18 || h.inPrison) return false;
  if (h.casinoGamblerNewAge >= 0 && h.age - h.casinoGamblerNewAge <= 4) return false;

  static const char* kStyles[] = {"Beginner", "Amateur", "Novice", "Regular",
                                  "High Roller"};
  const int style = int(casinoNewRng_.below(5));
  const bool female = casinoNewRng_.chance(0.5);
  std::string name = resolve("<casino-gambler>");
  if (name.empty()) name = female ? "Jane Doe" : "John Doe";
  const int age = 21 + int(casinoNewRng_.below(45));
  const long long cash = 500 + static_cast<long long>(casinoNewRng_.below(250000));
  const int addiction = int(casinoNewRng_.below(101));

  ensureTable("a16_careers-enc");
  const Table& careers = tbl("a16_careers-enc");
  std::string occupation =
      careers.size() ? trim(careers.at(casinoNewRng_.below(uint32_t(careers.size())), "Career"))
                     : std::string();
  if (occupation.empty()) occupation = "Office Worker";

  ensureTable("a16_casino-casino-game-room-enc");
  const Table& games = tbl("a16_casino-casino-game-room-enc");
  std::string favourites;
  const int nGames = 1 + int(casinoNewRng_.below(2));
  for (int g = 0; g < nGames && games.size(); ++g) {
    std::string game = trim(games.at(casinoNewRng_.below(uint32_t(games.size())), "Game"));
    if (game.empty()) continue;
    if (favourites.find(game) != std::string::npos) continue;
    if (!favourites.empty()) favourites += ", ";
    favourites += allMajorWords(game);
  }
  if (favourites.empty()) favourites = "Blackjack";

  // GamblerTypeString 0x2c28130 prints 'casual gambler' / 'high roller' /
  // 'Mafia member'; only a five-style regular counts as a high roller here.
  const std::string type = style == 4 ? "high roller" : "casual gambler";

  out.table = "registry:ScenarioCasinoAgeUpGamblerNew";
  out.title = "New Gambler";  // #31317
  out.body = std::string("A ") + type + " named " + name +
             " is now frequently gambling at your casino.";
  std::string rows;
  auto row = [&rows](const std::string& label, const std::string& value) {
    if (!rows.empty()) rows += "\n";
    rows += label + ": " + value;
  };
  row("Age", std::to_string(age));
  row("Occupation", allMajorWords(occupation));
  row("Gambling Style", allMajorWords(kStyles[style]));
  row("Favorite Games", favourites);
  out.body += "\n\n" + rows;
  // BarGraphArray: Money #30360 over the gambler's own money, Addiction #10777
  // over its addiction.
  out.body += "\n\nMoney: " + money(cash);
  out.body += "\nAddiction: " + std::to_string(addiction);
  out.cta = "What will you do?";  // #146
  out.options = {"OK"};           // #32432
  out.choice = "OK";
  out.narrative = std::string("A ") + kStyles[style] + " gambler named " + name +
                  " began frequenting my casino.";

  h.casinoGamblerNewAge = h.age;
  h.casinoGamblers += 1;
  h.casinoGamblerStyles.push_back(style + 1);
  h.casinoPopularity += 2;
  if (h.casinoPopularity > 100) h.casinoPopularity = 100;
  return true;
}

std::string Sim::casinoGenreThemeOf(const std::string& genre) {
  if (genre.empty() || !ensureTable("a16_casino-casino-singer-genre-enc")) return std::string();
  const Table& t = tbl("a16_casino-casino-singer-genre-enc");
  for (size_t r = 0; r < t.size(); ++r) {
    if (lower(trim(t.at(r, "Genre"))) == lower(genre)) return trim(t.at(r, "Theme"));
  }
  return std::string();
}

void Sim::seedCasinoGuest(Life& life, bool sameTheme) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (h.casinoCelebrity.empty()) h.casinoCelebrity = casinoToken("casino-celebrity");
  // The house theme is drawn by the casino's own field setup, so the hook runs it
  // first: a pinned family never sees that pass, and the genre comparison is the
  // whole point of the two cards (harness hook only).
  ensureCasinoFields(life);
  if (!ensureTable("a16_casino-casino-singer-genre-enc")) return;
  const Table& t = tbl("a16_casino-casino-singer-genre-enc");
  std::string pick;
  std::string pickTheme;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string theme = trim(t.at(r, "Theme"));
    const std::string genre = resolve(trim(t.at(r, "Genre")));
    if (genre.empty()) continue;
    if ((lower(theme) == lower(h.casinoTheme)) != sameTheme) continue;
    pick = genre;
    pickTheme = theme;
    break;
  }
  h.casinoCelebrityGenre = pick;
  // A house theme the table does not carry has no genre to borrow: the hook then
  // hands the guest the theme word as its theme so the comparison still works
  // (harness hook only).
  h.casinoCelebrityGenreTheme = pick.empty() ? h.casinoTheme : pickTheme;
  // The resignation card needs an age at which the guest was last met, and the
  // relationship it weighs; a pinned guest never went through the booking pass
  // (harness hook only).
  h.casinoCelebMetAge = h.age;
  h.casinoCelebRel = 20;
  if (h.casinoCelebFemale != true && h.casinoCelebFemale != false) h.casinoCelebFemale = false;
}

void Sim::seedCasinoAct(Life& life, bool sameTheme) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  ensureCasinoFields(life);
  if (h.casinoGamblers <= 0) h.casinoGamblers = 3;
  if (!ensureTable("a16_casino-casino-performance-act-name-enc")) return;
  const Table& t = tbl("a16_casino-casino-performance-act-name-enc");
  std::string name;
  std::string theme;
  std::string type;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string rowTheme = trim(t.at(r, "Theme"));
    const std::string rowName = resolve(trim(t.at(r, "Name")));
    if (rowName.empty()) continue;
    if ((lower(rowTheme) == lower(h.casinoTheme)) != sameTheme) continue;
    name = rowName;
    theme = rowTheme;
    type = trim(t.at(r, "Act Type"));
    break;
  }
  if (name.empty()) return;
  h.casinoAct = name;
  h.casinoActType = type;
  h.casinoActTheme = theme.empty() ? (sameTheme ? h.casinoTheme : std::string("medieval")) : theme;
  if (sameTheme) h.casinoActTheme = h.casinoTheme;
  h.casinoActMetAge = h.age;  // harness hook only
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

namespace {

// The punter's relationship status the casino cards print. The native reads it
// off its gambler record; the port keeps one gambling style per punter instead
// (REPORT.en.md 322), so the word is drawn from the same list.
std::string casinoTierWord(const Hero& h, Rng& rng) {
  static const char* kTiers[] = {"Beginner", "Amateur", "Novice", "Regular", "High Roller"};
  int style = 3;
  if (!h.casinoGamblerStyles.empty())
    style = h.casinoGamblerStyles[rng.below(uint32_t(h.casinoGamblerStyles.size()))];
  if (style < 1) style = 1;
  if (style > 5) style = 5;
  return kTiers[style - 1];
}

// The first name only: the complaint card reaches for the celebrity's first
// name, not the full name (RefreshScenario 0x356c0f8 reads +0x18 of the name).
std::string casinoFirstName(const std::string& full) {
  const size_t space = full.find(' ');
  return space == std::string::npos ? full : full.substr(0, space);
}

}  // namespace

// ScenarioCasinoAgeUpGamblerLost (.ctor 0x3572d40, RefreshScenario 0x35725c4,
// ScenarioIsEligibleForRandomPush 0x3572af4, pushed from SimCasino::IncrementAge
// at 0x2aacecc). A regular the hero has not dealt to for five years stops coming:
// the floor loses him and the house loses a little face.
//
// The card has a single 'OK' choice and no handler, so nothing follows it.
// Ceilings: the native shows the punter's gambling style as a table row; the
// style, money and addiction it prints come off the gambler record the port does
// not keep, so the row is dropped (REPORT.en.md 322 keeps the same ceiling).
bool Sim::casinoGamblerLostYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || h.age < 18 || h.inPrison) return false;
  if (h.casinoGamblers <= 0) return false;
  if (h.casinoGamblerLostAge >= 0 && h.age - h.casinoGamblerLostAge <= 4) return false;
  if (forcedChoice < 0 && !casinoGuestRng_.chance(0.10)) return false;
  hero_ = &h;
  life_ = &life;

  const std::string punter = resolve("<casino-gambler>");
  out.table = "registry:ScenarioCasinoAgeUpGamblerLost";
  out.title = "Gambler Lost";
  out.body = punter + " has stopped gambling at your casino.";
  out.cta = "OK";
  out.options = {"OK"};
  out.choice = "OK";
  out.narrative = "My casino's gambler, " + punter + ", stopped playing at my casino.";
  h.casinoGamblerLostAge = h.age;
  h.casinoGamblers -= 1;
  h.casinoPopularity -= 1;
  if (h.casinoPopularity < 0) h.casinoPopularity = 0;
  return true;
}

// ScenarioCasinoAgeUpGamblerComplaintCelebrity (.ctor 0x356d128, RefreshScenario
// 0x356c0f8, ChoiceSelectedAtIndex 0x356c61c, ScenarioIsEligibleForRandomPush
// 0x356cfec, pushed at 0x2aacf50). A regular complains that the celebrity the
// house booked does not fit the theme it is trying to establish - so the card
// only fires while the guest's genre is NOT the house theme.
//
// Choices in stored order: 'Ban ' + object pronoun + ' from my casino' (#12770
// and #2104), 'Apologize for the inconvenience' (#11554), 'Ignore ' + object
// pronoun (#26366).
//
// Ceilings: the ban's feedback card (title "You're outta here", body over the
// banned-gambler snippet pool) is not pushed, so the journal carries it; the
// celebrity's own genre is drawn from the singer-genre table when the guest
// first appears (no celebrity record in the port); the two gambler vtable slots
// are paired with popularity and the floor's happiness as in REPORT 322.
bool Sim::casinoCelebComplaintYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || h.casinoCelebrity.empty() || h.age < 18 || h.inPrison) return false;
  if (h.casinoGamblers <= 0) return false;
  if (h.casinoCelebrityGenreTheme.empty() ||
      lower(h.casinoCelebrityGenreTheme) == lower(h.casinoTheme))
    return false;
  if (h.casinoCelebComplaintAge >= 0 && h.age - h.casinoCelebComplaintAge <= 2) return false;
  if (forcedChoice < 0 && !casinoGuestRng_.chance(0.12)) return false;
  hero_ = &h;
  life_ = &life;

  const std::string tier = casinoTierWord(h, casinoGuestRng_);
  const std::string punter = resolve("<casino-gambler>");
  const bool female = casinoGamblerFemale_;
  const std::string them = female ? "her" : "him";
  const std::string who = female ? "she" : "he";
  const std::string celeb = h.casinoCelebrity;
  const std::string celebFirst = casinoFirstName(celeb);

  out.table = "registry:ScenarioCasinoAgeUpGamblerComplaintCelebrity";
  out.title = "Complaint";
  out.body = "Your casino's " + tier + ", " + punter + ", is complaining that " + celeb +
             "'s \n\n" + who + " saying " + celebFirst + "'s " + h.casinoCelebrityGenre +
             " musical stylings don't fit the " + h.casinoTheme + " theme you're trying to establish.";
  out.cta = "What will you do?";
  out.options = {"Ban " + them + " from my casino", "Apologize for the inconvenience",
                 "Ignore " + them};
  const int pick = forcedChoice >= 0 ? (forcedChoice > 2 ? 2 : forcedChoice)
                                     : int(casinoGuestRng_.below(3));
  out.choice = out.options[size_t(pick)];
  h.casinoCelebComplaintAge = h.age;

  if (pick == 0) {
    h.casinoGamblers -= 1;
    if (h.casinoGamblers < 0) h.casinoGamblers = 0;
    h.casinoPopularity -= 2;
    // The native journal spells the pronoun into the literal and ends ',.'.
    out.narrative = "I banned my casino's gambler, " + punter + ", from my casino after she "
                    "complained about my casino's " + tier + ", " + celeb + ",.";
  } else if (pick == 1) {
    h.casinoGamblerAddiction += 2;
    if (h.casinoGamblerAddiction > 100) h.casinoGamblerAddiction = 100;
    h.casinoPopularity -= 2;
    h.casinoGamblerHappiness += 1;
    if (h.casinoGamblerHappiness > 100) h.casinoGamblerHappiness = 100;
    out.narrative = "I apologized to my casino's gambler, " + punter + ", for booking my "
                    "casino's " + tier + ", " + celeb + ".";
  } else {
    h.casinoGamblerAddiction -= 2;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    h.casinoPopularity += 2;
    h.casinoGamblerHappiness -= 1;
    if (h.casinoGamblerHappiness < 0) h.casinoGamblerHappiness = 0;
    out.narrative = "I ignored my casino's gambler, " + punter + "'s, complaint about my "
                    "casino's " + tier + ", " + celeb + ".";
  }
  if (h.casinoPopularity < 0) h.casinoPopularity = 0;
  if (h.casinoPopularity > 100) h.casinoPopularity = 100;
  return true;
}

// ScenarioCasinoAgeUpGamblerComplimentCelebrity (.ctor 0x35709f4, RefreshScenario
// 0x3570214, ChoiceSelectedAtIndex 0x3570678, ScenarioIsEligibleForRandomPush
// 0x35708b8, pushed at 0x2aacf7c). The same regular, this time pleased: the guest
// act fits the house, so the card fires only while the guest's genre IS the theme.
//
// Choices: 'Thank ' + object pronoun (#41215), 'Give ' + pronoun + ' some free
// games' (#21649 and #3411), 'Ignore ' + pronoun (#26366). The card writes no
// journal at all, so the port logs nothing.
bool Sim::casinoCelebComplimentYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsCasino || h.casinoCelebrity.empty() || h.age < 18 || h.inPrison) return false;
  if (h.casinoGamblers <= 0) return false;
  if (h.casinoCelebrityGenreTheme.empty() ||
      lower(h.casinoCelebrityGenreTheme) != lower(h.casinoTheme))
    return false;
  if (h.casinoCelebComplimentAge >= 0 && h.age - h.casinoCelebComplimentAge <= 4) return false;
  if (forcedChoice < 0 && !casinoGuestRng_.chance(0.12)) return false;
  hero_ = &h;
  life_ = &life;

  const std::string tier = casinoTierWord(h, casinoGuestRng_);
  const std::string punter = resolve("<casino-gambler>");
  const bool female = casinoGamblerFemale_;
  const std::string them = female ? "her" : "him";
  const std::string who = female ? "she" : "he";
  const std::string celeb = h.casinoCelebrity;

  out.table = "registry:ScenarioCasinoAgeUpGamblerComplimentCelebrity";
  out.title = "Compliment";
  // The native reads Pronoun(5) here, the capitalised subject.
  const std::string whoCap = female ? "She" : "He";
  out.body = "Your casino's " + tier + ", " + punter + ", is complimenting you for booking " +
             celeb + " to perform at your casino.\n\n" + whoCap +
             " says each performance is somehow better than the previous.";
  out.cta = "What will you do?";
  out.options = {"Thank " + them, "Give " + them + " some free games", "Ignore " + them};
  const int pick = forcedChoice >= 0 ? (forcedChoice > 2 ? 2 : forcedChoice)
                                     : int(casinoGuestRng_.below(3));
  out.choice = out.options[size_t(pick)];
  h.casinoCelebComplimentAge = h.age;

  if (pick == 0) {
    h.casinoGamblerAddiction -= 1;
    if (h.casinoGamblerAddiction < 0) h.casinoGamblerAddiction = 0;
    h.casinoPopularity -= 1;
    h.casinoGamblerHappiness += 1;
    if (h.casinoGamblerHappiness > 100) h.casinoGamblerHappiness = 100;
  } else if (pick == 1) {
    h.casinoGamblerAddiction += 2;
    if (h.casinoGamblerAddiction > 100) h.casinoGamblerAddiction = 100;
    h.casinoPopularity -= 2;
    h.casinoGamblerHappiness += 2;
    if (h.casinoGamblerHappiness > 100) h.casinoGamblerHappiness = 100;
  } else {
    h.casinoPopularity -= 2;
  }
  if (h.casinoPopularity < 0) h.casinoPopularity = 0;
  return true;
}

}  // namespace bl
