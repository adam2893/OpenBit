#include "bl/game.h"

#include <cstring>

namespace bl {
namespace {

struct Entry {
  const char* grant;
  const char* purchase;
  const char* iap;
};

const Entry kEntitlements[] = {
  // { grantMethod, purchaseMethod, iapField } -- generated from il2cpp_dump.txt.
  // grantMethod: Game.UIIAPManager method the original mod ShadowHooks (REPORT.en.md s.13).
  // iapField: IAPManager System.String field holding that product id (nullptr = no field matched).
      {"BitizenshipPurchased", "PurchaseBitizenship", "NonConsume_Bitizenship"},
      {"LegacyBitizenshipPurchased", nullptr, "NonConsume_LegacyBitizenship"},
      {"GodModePurchased", "PurchaseGodMode", "NonConsume_GodMode"},
      {"BitizenshipAndGodModePurchased", "PurchaseBitizenshipAndGodMode", "NonConsume_BitizenshipAndGodMode"},
      {"RemoveAdsPurchased", "PurchaseRemoveAds", "NonConsume_RemoveAds"},
      {"TimeMachinePurchased", "PurchaseTimeMachine", "Consume_TimeMachine"},
      {"StreakSaverPurchased", "PurchaseStreakSaver", "Consume_StreakSaver"},
      {"BattlePassPurchased", "PurchaseBattlePass", "Consume_BattlePass"},
      {"BossModePurchased", "PurchaseBossMode", "NonConsume_BossMode"},
      {"FighterModePurchased", "PurchaseFighterMode", "NonConsume_FighterMode"},
      {"MovieDirectorPurchased", nullptr, "NonConsume_MovieDirector"},
      {"BundleMovieDirectorHollywoodStarPurchased", "PurchaseBundleMovieDirectorHollywoodStar", "NonConsume_BundleMovieDirectorHollywoodStar"},
      {"CareerPoliticianPurchased", "PurchaseCareerPolitician", nullptr},
      {"CareerAthletePurchased", "PurchaseCareerAthlete", "NonConsume_CareerAthlete"},
      {"CareerMusicianPurchased", "PurchaseCareerMusician", "NonConsume_CareerMusician"},
      {"CareerMusicProducerPurchased", "PurchaseCareerMusicProducer", "NonConsume_CareerMusicProducer"},
      {"CareerMafiaPurchased", "PurchaseCareerMafia", "NonConsume_CareerMafia"},
      {"CareerActorPurchased", "PurchaseCareerActor", "NonConsume_CareerActor"},
      {"CareerHustlerPurchased", "PurchaseCareerHustler", "NonConsume_CareerHustler"},
      {"CareerBusinessPurchased", "PurchaseCareerBusiness", "NonConsume_CareerBusiness"},
      {"CareerAstronautPurchased", "PurchaseCareerAstronaut", "NonConsume_CareerAstronaut"},
      {"CareerModelPurchased", "PurchaseCareerModel", "NonConsume_CareerModel"},
      {"ChallengeVaultPurchased", "PurchaseChallengeVault", "NonConsume_ChallengeVault"},
      {"UnlimitedTimeMachinePurchased", "PurchaseUnlimitedTimeMachine", "NonConsume_UnlimitedTimeMachine"},
      {"GoldThemePurchased", "PurchaseGoldTheme", "NonConsume_GoldTheme"},
      {"GoldenPacifierPurchased", "PurchaseGoldenPacifier", "NonConsume_GoldenPacifier"},
      {"GoldenDiplomaPurchased", "PurchaseGoldenDiploma", "NonConsume_GoldenDiploma"},
      {"GoldenNekoStatuePurchased", "PurchaseGoldenNekoStatue", "NonConsume_GoldenNekoStatue"},
      {"FriendshipBraceletPurchased", "PurchaseFriendshipBracelet", "NonConsume_FriendshipBracelet"},
      {"AuraRingPurchased", "PurchaseAuraRing", "NonConsume_AuraRing"},
      {"GoldenPassportPurchased", "PurchaseGoldenPassport", "NonConsume_GoldenPassport"},
      {"PromiscuityPotionPurchased", "PurchasePromiscuityPotion", "NonConsume_PromiscuityPotion"},
      {"AssassinsBladePurchased", "PurchaseAssassinsBlade", "NonConsume_AssassinsBlade"},
      {"HollywoodStarPurchased", "PurchaseHollywoodStar", "NonConsume_HollywoodStar"},
      {"SpikedKnucklesPurchased", "PurchaseSpikedKnuckles", "NonConsume_SpikedKnuckles"},
      {"BundleFameAndFortunePurchased", "PurchaseBundleFameAndFortune", nullptr},
      {"GoldenPiggyBankPurchased", "PurchaseGoldenPiggyBank", "NonConsume_GoldenPiggyBank"},
      {"LuxuryAccessPurchased", "PurchaseLuxuryAccess", "NonConsume_LuxuryAccess"},
      {"OutdoorLifestylePurchased", "PurchaseOutdoorLifestyle", "NonConsume_OutdoorLifestyle"},
      {"GoldenWrenchPurchased", "PurchaseGoldenWrench", "NonConsume_GoldenWrench"},
      {"GoldenResumePurchased", "PurchaseGoldenResume", "NonConsume_GoldenResume"},
      {"GetOutJailCardPurchased", "PurchaseGetOutJailCard", "NonConsume_GetOutJailCard"},
      {"ExpansionPackInvestorPurchased", "PurchaseExpansionPackInvestor", "NonConsume_ExpansionPackInvestor"},
      {"ExpansionPackLandlordPurchased", "PurchaseExpansionPackLandlord", "NonConsume_ExpansionPackLandlord"},
      {"ExpansionPackBlackMarketPurchased", "PurchaseExpansionPackBlackMarket", "NonConsume_ExpansionPackBlackMarket"},
      {"ExpansionPackCultPurchased", "PurchaseExpansionPackCult", "NonConsume_ExpansionPackCult"},
      {"ExpansionPackSecretAgentPurchased", "PurchaseExpansionPackSecretAgent", "NonConsume_ExpansionPackSecretAgent"},
      {"ExpansionPackZooPurchased", "PurchaseExpansionPackZoo", "NonConsume_ExpansionPackZoo"},
      {"ExpansionPackRacingPurchased", "PurchaseExpansionPackRacing", "NonConsume_ExpansionPackRacing"},
      {"ExpansionPackCasinoPurchased", "PurchaseExpansionPackCasino", "NonConsume_ExpansionPackCasino"},
      {"ExpansionPackVampirePurchased", "PurchaseExpansionPackVampire", "NonConsume_ExpansionPackVampire"},
      {"CareerDealerPurchased", "PurchaseCareerDealer", "NonConsume_CareerDealer"},
      {"BundleBillionairePurchased", "PurchaseBundleBillionaire", nullptr},
      {"BundleBirthdayStarterPurchased", "PurchaseBundleBirthdayStarter", "NonConsume_BundleBirthdayStarter"},
      {"BundleBirthdayLegendsPurchased", "PurchaseBundleBirthdayLegends", nullptr},
      {"BundleTimeIsMoneyPurchased", "PurchaseBundleTimeIsMoney", "NonConsume_BundleTimeIsMoney"},
      {"BundleStarter1Purchased", "PurchaseBundleStarter1", "NonConsume_BundleStarter1"},
      {"BundleStarter2Purchased", "PurchaseBundleStarter2", "NonConsume_BundleStarter2"},
      {"BundlePartyAnimalPurchased", "PurchaseBundlePartyAnimal", nullptr},
      {"BundleCrimePurchased", "PurchaseBundleCrime", nullptr},
};

// True when `grant` without its "Purchased" suffix equals `short_name`.
bool matchesShort(const char* grant, const std::string& short_name) {
  const size_t len = std::strlen(grant);
  if (len <= 9) return false;
  return std::string(grant, len - 9) == short_name;
}

}  // namespace

GameState::GameState() {
  for (const Entry& e : kEntitlements) {
    Entitlement ent;
    ent.grantMethod = e.grant;
    ent.purchaseMethod = e.purchase;
    ent.iapField = e.iap;
    entitlements.push_back(ent);
  }
  vars.sessionsNumber = 1;
}

Entitlement* GameState::findEntitlement(const std::string& key) {
  for (Entitlement& e : entitlements) {
    if ((e.grantMethod && key == e.grantMethod) ||
        (e.purchaseMethod && key == e.purchaseMethod) ||
        (e.iapField && key == e.iapField) ||
        (e.grantMethod && matchesShort(e.grantMethod, key))) {
      return &e;
    }
  }
  return nullptr;
}

bool GameState::owns(const std::string& shortName) const {
  for (const Entitlement& e : entitlements) {
    if (e.owned && e.grantMethod && matchesShort(e.grantMethod, shortName)) return true;
  }
  return false;
}

size_t GameState::ownedCount() const {
  size_t n = 0;
  for (const Entitlement& e : entitlements) if (e.owned) ++n;
  return n;
}

}  // namespace bl
