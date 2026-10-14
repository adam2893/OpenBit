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
  // Elected office (REPORT.en.md 55). a16_country-political-systems-enc holds,
  // per country, which office levels exist and what each level is called
  // ('president', 'prime minister', 'mayor'), plus the noun for its region.
  bool politician = false;
  std::string officeLevel;    // national / regional / city / local
  std::string office;         // that country's title for the level
  std::string jurisdiction;   // what '<jurisdiction>' prints
  std::string jurisdictionAdj;  // its adjective: national / provincial / city / local
  // Secret agent (REPORT.en.md 58). a16_secret-agent-random-hero-agency-names-enc
  // names the agency, a16_secret-agent-code-names-enc the code name (gender
  // filtered), a16_secret-agent-potential-agent-color-enc the specialization.
  bool secretAgent = false;
  std::string spyAgency;
  std::string spyCodeName;
  std::string spySpec;         // assassination / stealth / honeypot / hacking
  int spyProficiency = 0;      // 0-100, the gate on mission-summary-feedback
  int spyMissions = 0;
  std::string spyCover;        // the day job that hides the service
  std::string spyAddress;      // the front's street address
  // The casino (report 61): the hero owns the house. The amenities and the game
  // rooms are the two lists the structure's capacity is spent on.
  bool ownsCasino = false;
  std::string casinoName;
  std::string casinoTheme;
  std::string casinoStructure;
  std::string casinoStreet;
  std::string casinoAct;          // the act in residence
  std::string casinoActType;
  std::string casinoSong;
  std::string casinoCelebrity;    // the celebrity the hero last hosted
  std::vector<std::string> casinoAmenities;
  std::vector<std::string> casinoGames;
  int casinoPopularity = 50;
  int casinoGamblers = 0;
  int casinoActPopularity = 0;
  int casinoCelebCraziness = 0;
  int casinoGamblerHappiness = 60;   // what the floor thinks of the house
  int casinoGamblerAddiction = 40;   // how deep in the regulars are

  // The commune (report 62): the hero founds a cult and keeps it believing.
  bool cultLeader = false;
  std::string cultName;
  std::string cultPhilosophy;      // alien / armageddon / new age / antiestablishment
  std::string cultTotem;
  std::string cultHonorific;       // what the followers call the hero
  std::string cultProperty;        // compound / hunting lodge / hermitage ...
  std::string cultHome;            // the commune street address
  std::string cultDoctrine;        // the rule currently in force
  std::vector<std::string> cultStructures;   // the buildings the commune has
  int cultLoyalty = 55;            // what the followers believe
  int cultCharisma = 50;           // how well the hero holds a room
  int communards = 0;              // how many followers there are
  // ---- the drug trade (section 63) ----
  bool dealer = false;             // the hero moves product
  bool dealerKingpin = false;      // and runs the cartel rather than working it
  std::string dealerDrug;          // what is on the shelf right now
  std::string dealerCut;           // what it was cut with, if anything
  std::string dealerCartel;        // the crew the hero answers to
  std::string dealerNick;          // the cartel nickname
  std::string dealerPackage;       // how the doses are wrapped
  std::string dealerStreet;        // where the drop is
  std::string dealerRunStatus;     // complete / ended, read by the flavour pool
  int dealerDemand = 0;            // 0-100, how badly the street wants it
  int dealerStanding = 0;          // 0-100, where the hero sits in the crew
  int dealerWillpower = 0;         // 0-100, holds up under a stop
  int dealerPrice = 0;             // dollars per dose
  int dealerBatch = 0;             // doses on hand
  int dealerDoses = 0;             // doses moved in total
  int dealerInfamy = 0;            // 0-100, how known the hero is
  int dealerHeat = 0;              // 0-100, how closely watched
  // The black market (report 60): the hero deals in items whose provenance is
  // best left out of the paperwork, sells them through their own auction house,
  // and shows the best finds in a museum of their own.
  bool blackMarket = false;
  std::string bmAuctionHouse;
  std::string bmMuseum;
  std::string bmItem;            // the item on the block, article included
  std::string bmItemPossessive;
  std::string bmItemShort;
  bool bmItemPlural = false;
  bool bmFake = false;
  std::string bmCategory;        // the auction category the item belongs to
  long long bmValue = 0;         // what the auction house can get for it
  int bmItems = 0;
  int bmAppeal = 40;             // what the museum's visitors think of it
  // The fight career (REPORT.en.md 59): a level, a league position (1 is the top
  // of the card), a form score, reputation, street cred and a stamina pool.
  bool fighter = false;
  std::string fightStyle;      // striker / submission artist / wrestler
  std::string fightCoach;
  bool fightCoachFemale = false;
  int fightCoachQuality = 0;   // 0-100, the gate on the coach feedback tables
  std::string fightArena;
  int fightLevel = 0;          // the opponent-type Fighter Level band, 1-5
  int fightLeaguePos = 20;     // 1 is the top of the card
  int fightReputation = 0;
  int fightStreetCred = 0;
  int fightWins = 0;
  int fightLosses = 0;
  int fightStrength = 0;
  int fightToughness = 0;
  int fightAgility = 0;
  int fightTechnique = 0;
  int fightStamina = 0;
  // Bit i = the misc dumb-scenario pool i is switched on (REPORT.en.md 56).
  unsigned miscMask = 0;
  // Relationship state (REPORT.en.md 57). The pools in the lover family gate
  // their rows on the score and on the partner's gender, so both are kept.
  bool relationshipOn = false;   // the script asked for the lover subsystem
  std::string loverName;         // given name of the current partner
  bool loverFemale = false;
  int loverAge = 0;
  std::string loverStatus;       // "", "dating", "married"
  int relationship = 60;         // the game's own relationship score, 0-100
  int loverCraziness = 0;
  bool virgin = true;
  std::string exName;            // the most recent ex, kept after a breakup
  bool exFemale = false;
  int exes = 0;
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
