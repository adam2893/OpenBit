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
