// bl/game.h -- port of the game's state structs.
//
// Field set derived from the live IL2CPP inventory _re/il2cpp_dump.txt:
//   "class Game.GameVars : System.Object"        (dump line 105337, 157 fields)
//   "class IAPManager : UnityEngine.MonoBehaviour" (dump line 447, ~110 products)
//   "class Game.UIIAPManager : System.Object"    (dump line 128447, 267 methods)
#pragma once

#include <string>
#include <vector>

namespace bl {

// One purchasable product.
//
// grantMethod / purchaseMethod are the real IL2CPP methods Game.UIIAPManager
// exposes and that the original mod ShadowHooks by name at runtime
// (_re/REPORT.en.md section 13: 59 hook sites, ~50 of them "*Purchased" grant
// points plus a matching batch of "Purchase*" initiators). iapField is the
// verbatim IAPManager System.String field that holds that product id -- the
// store SKU literals themselves are resolved by Unity IAP at runtime and are
// not present in the IL2CPP dump.
struct Entitlement {
  const char* grantMethod = nullptr;     // "BitizenshipPurchased"
  const char* purchaseMethod = nullptr;  // "PurchaseBitizenship", may be absent
  const char* iapField = nullptr;        // "NonConsume_Bitizenship", may be absent
  bool owned = false;
};

// Port of Game.GameVars, restricted to the fields the port reads or writes.
// Each comment is the verbatim IL2CPP backing field and its dump offset.
struct GameVars {
  bool gameLaunchedBefore = false;      // <GameLaunchedBefore>          0x20
  bool isFirstSession = true;           // <IsFirstSession>              0x21
  bool gdprAccept = false;              // <GDPRAccept>                  0x22
  bool userBoughtBitizenship = false;   // <UserBoughtBitizenship>       0x23
  bool userGivenBitizenship = false;    // <UserGivenBitizenship>        0x24
  bool userBoughtGodMode = false;       // <UserBoughtGodMode>           0x25
  bool userGivenGodMode = false;        // <UserGivenGodMode>            0x26
  bool userHasPurchasedBefore = false;  // <UserHasPurchasedBefore>      0x27
  bool ads = true;                      // <Ads>                         0x30
  bool settingAgeDownButton = false;    // <SettingAgeDownButton>        0x54
  bool settingBiggerText = false;       // <SettingBiggerText>           0x56
  bool settingBoosts = true;            // <SettingBoosts>               0x57
  bool settingDarkMode = false;         // <SettingDarkMode>             0x58
  bool settingIntraYearAging = false;   // <SettingIntraYearAging>       0x59
  bool hapticEffects = true;            // <HapticEffects>               0x5a
  bool soundEffects = true;             // <SoundEffects>                0x5e
  bool miniGames = true;                // <MiniGames>                   0x5f
  int theme = 0;                        // <SettingTheme>                0x68 (Int32)
  bool onboardingCompleted = false;     // <OnboardingCompleted>         0x169
  int livesStarted = 0;                 // <LivesStarted>                0x14c
  int livesCompleted = 0;               // <LivesCompleted>              0x150
  int sessionsNumber = 0;               // <SessionsNumber>              0x154
  int numTimesPurchasesMade = 0;        // <NumTimesPurchasesMade>       0x25c
};

// The hero ("Life") the game ages up. BitLife keeps the hero inside the save
// object graph rather than Game.GameVars, so this is the port's own struct; the
// stat names come from the real scenario impact columns (Karma / Discipline /
// Happiness / Intelligence / Appearance).
struct Hero {
  bool female = false;
  std::string firstName;
  std::string lastName;
  std::string country;
  std::string continent;
  // Birth city, drawn from the country's own 'Cities' array in
  // gamedata_json/a16_countries-enc.json. Only Sim::pinCountry fills it in.
  std::string city;
  int age = 0;
  long long money = 0;
  int happiness = 100;
  int health = 100;
  int smarts = 100;
  int looks = 100;
  int karma = 0;
  int discipline = 0;
  int fame = 0;
  bool employed = false;
  std::string job;  // port-side only: drives which scenario pools apply
  // Prison state. The real game reaches it through the interactive Crime menu
  // and its court; the port draws a yearly conviction instead (REPORT.en.md 45).
  bool inPrison = false;
  int prisonYearsLeft = 0;
  std::string prisonJob;  // the 'as a laundry attendant' half of the job line
  // Set when the hero is left with no surviving parent before 18; the orphan
  // dumb-scenario set is gated on it (REPORT.en.md 46).
  bool orphan = false;
  // Born into royalty (REPORT.en.md 47). The game decides this at life setup
  // (SimHero.Royal, offset 0x178, then a SimRoyal object); the port drives it
  // from the script's "royal" switch because the roll's probability was not
  // read out of the binary, and the royal dumb-scenario set is gated on it.
  bool royal = false;
  int royalRank = 0;       // 1 = the country's top seat
  std::string royalSeat;   // 'King' / 'Emperor' / 'Prince', from the seat table
  std::string royalHouse;  // a16_royal-house-names-enc 'Name'
  // Vampire state (REPORT.en.md 48). SimPerson carries a SimVampire object at
  // offset 0x1e0 (plus VampireDiseaseCarrier 0x1e8 and IsDestinedToBecomeVampire
  // 0x1e9); the port models only "is one" and the two people a vampire card
  // names, and takes the state from the script's "vampire" switch.
  bool vampire = false;
  std::string vampireName;  // the NPC '<vampire>' names, drawn once per year
  std::string familiar;     // '<familiar>', the hero's companion
  // Military service (REPORT.en.md 50). The game keeps the branch, status and
  // rank on the SimCareer side (SimPerson field 0x10 <MilitaryBranch>) and
  // builds its lines in code, so the port takes the state from the script's
  // "military" switch and walks a16_military-ranks-enc for the ladder.
  bool military = false;
  bool militaryPending = false;  // the switch asked for it; enlistment happens at 18
  std::string militaryBranch;  // Air Force / Army / Coast Guard / Marines / Navy
  std::string militaryRank;    // that branch's longhand for the current rank row
  std::string militaryPay;     // Salary Average of that row, verbatim ('$19,200')
  bool militaryOfficer = false;
  int militaryYears = 0;
  // The hero's own job (REPORT.en.md 51). a16_careers-enc gives the Seniority
  // Track ladder; the port walks it and writes the family line shape in the
  // first person ('I started a new position as {job} for {company}.', literal
  // #9408 with <person> = 'I').
  // Professional athlete (REPORT.en.md 52). a16_sports-enc is seven rows of
  // sport plus that sport's own bracket words (Ball / Stadium / Field / Referee
  // / Game Bracket); the league, team and position come from their own tables.
  // Business owner (REPORT.en.md 53). a16_business-industries-enc gives the
  // industry and its facility words, a16_business-products-enc the products,
  // a16_business-name-structure-enc the rival companies' names.
  // Recording artist (REPORT.en.md 54). a16_music-genres-enc names the genre,
  // a16_music-names-albums-enc and a16_music-names-songs-enc the works (both
  // carry per-genre flag columns), a16_music-names-bands-enc the band.
  bool musician = false;
  std::string musicGenre;  // a16_music-genres-enc 'Genre'
  std::string bandName;    // a16_music-names-bands-enc 'Title'
  bool ownsBusiness = false;
  std::string industry;  // a16_business-industries-enc 'Industry'
  bool athlete = false;
  std::string sport;     // a16_sports-enc 'Sport'
  std::string league;    // a16_sports-leagues-enc 'Name'
  std::string team;      // a16_sports-teams-enc 'Name', alternates resolved
  std::string position;  // a16_sports-positions-enc 'Position'
  std::string careerTrack;  // the raw Seniority Track cell
  int careerStep = 0;       // 0-based index into that track
  int careerYears = 0;
};

class GameState {
 public:
  GameState();  // builds the full entitlement catalogue

  GameVars vars;
  Hero hero;
  std::vector<Entitlement> entitlements;

  // Look an entitlement up by grant method ("AssassinsBladePurchased"),
  // purchase initiator ("PurchaseAssassinsBlade"), IAPManager field
  // ("NonConsume_AssassinsBlade") or short name ("AssassinsBlade").
  Entitlement* findEntitlement(const std::string& key);
  bool owns(const std::string& shortName) const;
  size_t ownedCount() const;

  // Derived: owning RemoveAds switches the whole ad layer off.
  bool adsActive() const { return vars.ads && !owns("RemoveAds"); }
};

}  // namespace bl
