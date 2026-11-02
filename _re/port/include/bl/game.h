// bl/game.h -- port of the game's state structs.
//
// Field set derived from the live IL2CPP inventory _re/il2cpp_dump.txt:
//   "class Game.GameVars : System.Object"        (dump line 105337, 157 fields)
//   "class IAPManager : UnityEngine.MonoBehaviour" (dump line 447, ~110 products)
//   "class Game.UIIAPManager : System.Object"    (dump line 128447, 267 methods)
#pragma once

#include <map>
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
  // The state, province or parish the city sits in (REPORT.en.md 95). Drawn
  // from a16_city-regions-enc by country code and city, and the answer to the
  // '<region>' token a16_country-political-systems-enc asks for 67 times
  // ('governor of <region>'). Empty when the table does not carry the country.
  std::string region;
  int age = 0;
  long long money = 0;
  int happiness = 100;
  int health = 100;
  int smarts = 100;
  int looks = 100;
  int karma = 0;
  int murders = 0;  // confirmed kills (REPORT.en.md 300)
  // The hitman chain (REPORT.en.md 301): the name the found card quoted, the
  // target, the quote and both genders, carried to the outcome card.
  std::string hitmanName, hitmanTarget;
  bool hitmanFemale = false, hitmanTargetFemale = false;
  long long hitmanPrice = 0;
  // The current bank robbery's picks, held between the picker and the
  // result card (REPORT.en.md 302).
  std::string robDisguise, robWeapon, robGetaway;
  int robPower = 0;
  // The horse-races chain (REPORT.en.md 306): the picker stores the
  // field and the pick, the result card settles them against the winner.
  std::vector<std::string> horseField;
  std::vector<int> horseScores;
  std::string horsePicked;
  long long horseWager = 0;
  int discipline = 0;
  // Fame is a float 0..100 at hero+0xb4 (REPORT.en.md 136):
  // SimPerson::ImpactFame @0x73f30e4d88 adds
  // AttributeAdjustmentPointsForImpactScore(impact) and clamps to [0, 100]. The
  // Fame family's gates read it as a float -- ScenarioFamePaparazzi's is
  // `fame >= 75.0f` -- so it is kept as one.
  float fame = 0.0f;
  // The country's retirement age for the hero's gender (REPORT.en.md 137), pinned
  // with the country at birth: 50-67 depending on both.
  int retirementAge = 65;
  // Birth control (REPORT.en.md 141). The game keeps it on the person
  // (Person.OnBirthControl) with two stamps on the life
  // (Life.AgeAtLastWentOnBirthControl / ...Off). Three cards of the lover family
  // are written around it: LoverDemandsBirthControl, LoverStopsBirthControl and
  // ScenarioBirthControlHero.
  bool onBirthControl = false;
  // The partner's pregnancy (REPORT.en.md 151). The game keeps a SimPregnancy object;
  // the port keeps the two facts its own birth card needs.
  bool loverPregnant = false;
  bool loverPregnantByHero = false;
  // A resolved pregnancy keeps loverPregnant until its own card drains, so no second
  // pregnancy can start behind a queued birth; this says that card is already owed
  // (REPORT.en.md 338).
  bool loverBirthQueued = false;
  // The hero as the mother (REPORT.en.md 342). The game carries the hero's own pregnancy
  // on the same SimPerson+0x48 slot the lover's uses, so the port needs a second lane for
  // the case where the hero is the one carrying it.
  bool heroPregnant = false;
  bool heroBirthQueued = false;
  int heroFertility = -1;      // drawn once: the hero has no Att_fertility field
  std::string heroPregFather;  // the lover who fathered it
  // The partner's fertility, drawn once (REPORT.en.md 338). The native roll reads the
  // mother's SimPerson+0xa4 float; the port's lover has no person model, so the draw
  // stands in for it and stays fixed for that partner.
  int loverFertility = -1;
  // The partner is owed an argue card (REPORT.en.md 153): set when the hero cheats
  // and the partner finds out, drained by the next age-up.
  bool loverArguesCheating = false;

  int ageWentOnBirthControl = 0;
  int ageWentOffBirthControl = 0;
  bool employed = false;
  std::string job;  // port-side only: drives which scenario pools apply
  // The working week (REPORT.en.md 248). The overtime family raises it and a later
  // year brings it back to 40: the played-life trace shows 46 hours at 32 and
  // "my supervisor told me I can go back to working 40 hours a week again" at 55.
  int hoursPerWeek = 40;
  int overtimeYearsLeft = 0;
  std::string employer;  // the company the job line names ("at Impact Racetrack")
  std::string employerType;  // the careers row's Company Type, named lazily
  long long jobSalary = 0;   // the salary a job line names (REPORT.en.md 286)
  bool onDiet = false;       // the year the diet card is spent (REPORT 290)
  std::string martialArt;    // the art the belt belongs to (REPORT 291)
  int martialBelt = -1;      // index into the art's Belt Hierarchy
  int flyingLessons = 0;     // the pilot test is taken after one lesson (REPORT 295)
  int flyingAttempts = 0;    // three rides at the test, then the licence waits
  int surgeries = 0;         // plastic surgery count, capped by the row (REPORT 297)
  bool pilotLicense = false;
  // Prison state. The real game reaches it through the interactive Crime menu
  // and its court; the port draws a yearly conviction instead (REPORT.en.md 45).
  bool inPrison = false;
  int prisonYearsLeft = 0;
  std::string prisonJob;  // the 'as a laundry attendant' half of the job line
  // Prison life (REPORT.en.md 82): the nickname the yard gives you, the gang,
  // the inmate across the yard, the letters, the canteen gifts and the parole
  // board. The sentence itself (inPrison / prisonYearsLeft) is section 45's.
  // Mischief (REPORT.en.md 318): SimHero.IncrementCrimesAttempted. Only the
  // pull half's wiliness ladder reads it; the push half just increments it.
  int crimesAttempted = 0;
  bool prisonPrompt = false;    // the script asked for a sentence, as a probe hook
  bool prisonServed = false;    // the forced sentence has been served
  std::string prisonNickname;
  std::string prisonGang, prisonGangType, prisonGangShort;
  int prisonGangPower = 0, prisonGangViolence = 0;
  bool prisonGangMember = false;
  int prisonRespect = 10;       // the yard's opinion of the hero, 0-100
  std::string prisonEnemy;      // the inmate the year's card is about
  bool prisonEnemyFemale = false;
  int prisonFights = 0, prisonWins = 0;
  std::string prisonLetterFrom, prisonLetterTopic;
  std::string prisonGift;
  int prisonParoleTries = 0;
  long long prisonCanteen = 0, prisonEarned = 0;

  // ---------------------------------------------------------------------------
  // The office (REPORT.en.md 83): the hero as somebody's manager. The employee
  // pools are written from the boss's chair ("You threatened to fire <employee>",
  // "You sent an email to all <company> employees that lauded the service of
  // <employee>"), so the family needed one thing above all others: a staff.
  // ---------------------------------------------------------------------------
  bool ofcManager = false;
  std::string ofcJob;        // the hero's own title, from a16_employee-types-enc
  std::string ofcIndustry;
  std::string ofcCompany;    // the employer, from companyName()
  std::string ofcEmployee;   // the one report the year is about
  std::string ofcEmployeeJob;
  bool ofcEmployeeFemale = false;
  int ofcStaff = 3;          // how many people report to the hero
  int ofcMorale = 55;        // 0..100, what the staff think of the hero
  int ofcEmpRel = 60;        // 0..100, the named employee's bond with the hero (329)
  int ofcEmpRespect = 60;    // 0..100, the named employee's respect for the hero (330)
  int ofcEmpWarned = 0;      // warnings the named employee has been given (330)
  int ofcEmpYelledAt = 0;    // how often the hero has shouted at them (330)
  int ofcEmpLastRecognizeAge = -1;  // the age the last recognition paid out (330)
  int ofcEmpLastBonusAge = -1;      // the age the last bonus paid out (330)
  int ofcPerf = 50;          // 0..100, the named employee's performance
  int ofcWarnings = 0;       // written warnings handed out to them
  int ofcYears = 0, ofcHires = 0, ofcFires = 0;
  long long ofcPay = 0, ofcEarned = 0;

  // The school (REPORT.en.md 84). level: 0 none, 1 elementary, 2 middle,
  // 3 high, 4 college. The hero is a pupil, not a manager, so the year is
  // about a class, a teacher and the crowd around the locker.
  bool schooling = false;
  // A relative died this year and the funeral card is owed (REPORT.en.md 249).
  // The trace writes the death line and the funeral line in the same year.
  bool pendingFuneral = false;
  // The death card's "Plan his funeral" option hands the year to the
  // disposition screen, so its card is owed (REPORT.en.md 307).
  bool pendingFuneralPlan = false;
  // The hero offered to take an ex-lover's paternity test (REPORT.en.md 251): the
  // result card then names the ex and the child the way the trace does.
  bool paternityProactive = false;
  // The will (REPORT.en.md 252): the trace writes "I created a will. I'm leaving
  // everything to my son, Cleveland." once in a life, so the hero carries the flag.
  bool hasWill = false;
  // The school ladder the year lines read (REPORT.en.md 254): the level the hero was
  // in last year and whether the high-school line has been written.
  int lastSchoolLevel = 0;
  bool graduatedHighSchool = false;
  int schoolAnnouncedLevel = 0;  // the level whose start line has been written
  // University application (REPORT 313). Life+0x218 in the game is the age the
  // accepted application was written; universityStage is the port's own small
  // state machine: 0 not offered, 1 enrolled, 2 rejected, 3 accepted and waiting
  // for the tuition screen (REPORT.en.md 317).
  int ageAtUniversityApplication = -1;
  int universityStage = 0;
  // SimLoan (REPORT.en.md 317). The game's own layout is YearsCompleted +0x10,
  // YearsLeft +0x14, LoanType +0x18, InterestRate +0x1c, Years +0x20,
  // DownPaymentPercent +0x24, Active +0x28, DownPayment +0x2c, FinancedAmount +0x30,
  // Balance +0x34; the port only ever creates the student loan (LoanType 1), whose
  // TermsForLoanType default is 5% over 10 years with no down payment.
  struct StudentLoan {
    bool exists = false;
    bool active = false;      // SimLoan.Active: set when the payback years begin
    int years = 10;
    int yearsCompleted = 0;
    int yearsLeft = 10;
    double rate = 0.05;       // InterestRate
    double downPaymentPercent = 0.0;
    double financed = 0.0;    // FinancedAmount
    double balance = 0.0;     // Balance
    int collegeYears = 4;     // the college row's Years, the cap on tuition adds
    int tuitionYears = 0;     // how many annual tuitions have been added
    double annualTuition = 0.0;
    std::string college;      // the college row's School, named by the log lines
  };
  StudentLoan studentLoan;
  // The school each level is at (REPORT.en.md 282): the played life says
  // 'I started high school at Adams High School, a private school.', and the
  // named forms are 'Adams High School', 'Allure Middle School', 'Virgil
  // Hardwood High School' and 'St. Rian's High School'.
  std::string schoolName[3];   // elementary, middle, high
  bool schoolPrivate[3] = {false, false, false};
  bool graduationPending = false;  // the graduation card owns that year (REPORT 262)
  // The optometrist chain (REPORT.en.md 267): worsening eyesight, the exam, and the
  // glasses the failure buys.
  bool glasses = false;
  // The exam is owed the year after the eyesight first worsens (REPORT.en.md 267).
  bool eyeExamPending = false;
  // The crime chain's own state (REPORT.en.md 273): the crime the hero was
  // arrested for and the sentence the court handed down, carried between cards.
  bool parentsPetGiven = false;  // the family gives one pet, once (REPORT 275)
  bool parentsCarGiven = false;  // the parents' car gift, once (REPORT 280)
  std::string crimeName;
  int crimeYears = 0;
  // The driver's licence (REPORT.en.md 255). ScenarioPushLicenseCar offers the test
  // at 16, ScenarioGameLicenseCar asks the question, and the outcome is one of two
  // literals from metadata (#24138 failed / #24778 passed).
  bool hasLicense = false;
  // The sibling squabble (REPORT.en.md 256): one card per fight, and the topic comes
  // from a16_sibling-arguments-enc's 191 rows.
  int siblingFeuds = 0;
  // The safe-sex card owed after a hookup (REPORT.en.md 257).
  bool safeSexDue = false;
  std::string safeSexWith;
  std::string funeralRelation;   // "father" / "mother" / "big sister"
  bool funeralFemale = false;
  // Cremation queues a same-year ashes card, asking what to do with them
  // before the next annual increment (REPORT.en.md 307).
  bool pendingFuneralAshes = false;
  std::string ashesRelation;   // "father": "my father's ashes"
  bool ashesFemale = false;
  int schoolLevel = 0, schoolYears = 0;
  std::string schoolTeacher, schoolClassmate, schoolPrincipal, schoolActivity;
  std::string schoolActivityRef, schoolClique, schoolFrat, schoolFratLetters, schoolFratNickname;
  bool schoolTeacherFemale = false, schoolClassmateFemale = false, schoolPrincipalFemale = false;
  int schoolPopularity = 50, schoolGrades = 70, schoolTrouble = 0;
  int schoolActUps = 0, schoolDetentions = 0, schoolSuckups = 0;
  bool schoolCliqueMember = false, schoolFratMember = false;
  // The podcast (REPORT.en.md 85): a show of the hero's own, the ladder it
  // climbs and the money it pays.
  bool podcaster = false;
  std::string podcastName, podcastGenre, podcastSubgenre, podcastContentType, podcastSponsor;
  int podcastListeners = 0, podcastEpisodes = 0, podcastPopularity = 50, podcastCredibility = 50;
  int podcastSponsors = 0;
  long long podcastEarned = 0;
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
  // The royal year (REPORT.en.md 87). Section 47 gave the hero a house and a
  // seat and stopped there, so the family's twelve scenario pools were dead
  // weight. These are the numbers those pools window on.
  std::string royalCastle;   // the a16_royal-house-names-enc 'Castle' of the house
  int royalRespect = 50;     // 0..100, what the subjects think of the hero
  int royalYears = 0;        // years spent on the throne
  int royalOutrages = 0;     // public disservices that stuck to him
  int royalSentences = 0;    // subjects punished
  int royalGifts = 0;        // subjects rewarded
  int royalExecutions = 0;   // subjects executed on his order
  std::string royalNewspaper;  // the paper that covers him
  std::string royalSubject;
  // The anniversary (REPORT.en.md 88): the score ledger of a married life.
  int anniversaries = 0;
  int annGifts = 0;
  // The gift family's own ledger (REPORT.en.md 89): what the hero handed over
  // and what he carried out of the shop without paying for.
  int giftsGiven = 0;
  int giftsLifted = 0;
  // The conversations family's own ledger (REPORT.en.md 90): the talks held
  // and the ones the listener did not want to hear.
  int talks = 0;
  int talksAwkward = 0;
  // The social media year (REPORT.en.md 91): the account the hero posts from,
  // the handle the username family gives him, and the counters the year
  // reports back. The follower count is the only number the account keeps.
  std::string socialChannel;
  std::string socialUsername;
  long long socialFollowers = 0;
  int socialPosts = 0;
  int socialTrolls = 0;
  int socialPromotions = 0;
  int socialCollabs = 0;
  int socialHacks = 0;
  // The school year (REPORT.en.md 92): the trip, the dance, the
  // prom and the party, and the one prom vote a life gets.
  int b2sTrips = 0; int b2sDances = 0; int b2sParties = 0; bool b2sPromDone = false;
  int b2sSleepovers = 0;  // REPORT.en.md 93
    // the person a sentence, gift or execution names
  // Vampire state (REPORT.en.md 48). SimPerson carries a SimVampire object at
  // offset 0x1e0 (plus VampireDiseaseCarrier 0x1e8 and IsDestinedToBecomeVampire
  // 0x1e9); the port models only "is one" and the two people a vampire card
  // names, and takes the state from the script's "vampire" switch.
  bool vampire = false;
  std::string vampireName;  // the NPC '<vampire>' names, drawn once per year
  std::string familiar;     // '<familiar>', the hero's companion

  // The vampire's own life (REPORT.en.md 70). The family is forty-four tables
  // that describe a hunt, a feed, a familiar, a council, a pack of hunters and
  // the diseases the blood carries, so the hero carries the numbers those rows
  // window on.
  int vampireNotoriety = 20;   // how much the world has noticed
  int vampireEssence = 60;     // the strength the blood buys
  int vampireStanding = 50;    // the hero's place in the council
  int vampireHunger = 40;      // how long since the last feed
  std::string vampireVictim;   // the person this year's card names
  std::string vampireMaker;    // the vampire who turned the hero
  std::string vampireDisease;  // what the blood carried, if anything
  std::string vampireTitle;    // the last decree or title the council gave
  std::string vampireHome;     // a16_vampire-street-name-enc and a house number
  std::string vampireFamiliarDesc;  // 'a hobo' / 'a former colleague'
  bool vampireFamiliarFemale = false;
  bool vampireNpcFemale = false;
  int vampireFamiliarRel = 60;      // how the familiar feels about the hero
  int vampireFamiliarLooks = 50;
  int vampireFamiliarSmarts = 50;
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
  // The politician's year (REPORT.en.md 86). a16_political-parties-enc is the
  // party the hero stands for, a16_political-laws-enc what he signs,
  // a16_political-bribes-enc what he is offered, a16_political-rumors-enc what
  // is said about him and a16_political-headlines-enc what the press prints.
  // The standing map is the office pool's own model: one entry per issue the
  // a16_political-headlines-enc Issue column names, and the approval rating is
  // their average.
  std::string politicalParty;  // the party's name in the hero's country
  std::string politicalRival;  // the name a rumour is planted about
  int politicalApproval = 50;
  int politicalServed = 0;
  int politicalLaws = 0;
  int politicalScandals = 0;
  long long politicalBribe = 0;
  std::map<std::string, int> politicalStanding;
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
  std::string casinoCelebrityGenre;
  // The theme the act's genre belongs to. The two gambler cards weigh the act
  // against the house theme, and the shipped singer-genre table keeps genre
  // words and theme words disjoint, so the genre's own theme is what is
  // compared (REPORT 347).
  std::string casinoCelebrityGenreTheme;   // the guest act's genre (346, 347)
  int casinoGamblerHappiness = 60;   // what the floor thinks of the house
  int casinoGamblerAddiction = 40;   // how deep in the regulars are
  // The casino economy (REPORT.en.md 319). The native casino keeps one
  // SimGameRoom per installed unit string -- a unit count, the bet band, the
  // house win rate and a popularity of its own -- and a running ledger that the
  // annual review card prints. casinoGames stays the list of names the older
  // cards talk about; casinoRooms is the state behind it.
  struct CasinoRoom {
    std::string game;
    std::string name;   // the room's own name, from the menu's rename card (384)
    std::string unitString;
    int units = 0;
    int baseCapacity = 0;
    int costPerUnit = 0;
    int installationCost = 0;
    int houseWinRate = 0;   // percent, 0..100
    int defaultHouseWinRate = 0;  // the row's Default House Win Rate, drift target
    int minBet = 10;
    int maxBet = 100;
    int popularity = 50;
    long long lastWagers = 0;
    long long lastHouseWins = 0;
  };
  std::vector<CasinoRoom> casinoRooms;
  long long casinoBank = 0;              // SimCasino bankBalance[0x28]
  long long ledgerWagers = 0;
  long long ledgerPayouts = 0;
  long long ledgerGamblingRevenue = 0;
  long long ledgerPerformanceRevenue = 0;
  long long ledgerAmenityRevenue = 0;
  long long ledgerTaxes = 0;
  long long ledgerExpenses = 0;
  long long ledgerNetIncome = 0;
  int casinoType = 1;                    // 1..5, the structure's own rung in a16_casino-casino-structure-enc
  long long casinoBaseMonthlyCost = 0;   // that structure's Base Monthly Cost
  int casinoLedgerYear = -1;             // the last year the books were run
  int casinoBankruptYears = 0;           // consecutive years the casino pot ended negative
  int casinoComplaintAge = -1;           // age the complaint card last fired (358)
  int casinoComplimentAge = -1;          // age the compliment card last fired (5-year cooldown)
  // The mob's man at the house (REPORT.en.md 345). The native cards hold one
  // SimCasinoMafiaMember with its own person record; the port has no member
  // entity, so the two stats his virtual slots carry (0x508 and 0x738) live on
  // the hero, and the two cards keep their own one-year cooldowns.
  int casinoMobMood = 50;                // the member's own mood, +0x508
  int casinoMobView = 50;                // how the member sees the hero, +0x738
  int casinoLaunderAge = -1;             // the launder card last fired
  int casinoInterrogAge = -1;            // the interrogation card last fired
  int casinoGamblerLostAge = -1;        // the year a regular stopped coming
  int casinoCelebComplaintAge = -1;     // the year the guest act drew a complaint
  int casinoCelebComplimentAge = -1;
  std::string casinoActTheme;           // the act in residence's own theme
  int casinoActComplaintAge = -1;        // the year the act drew a complaint
  int casinoActComplimentAge = -1;       // ... and the year it drew a compliment
  int casinoCelebRel = 60;               // how the guest act feels about the hero (0-100, 352)
  bool casinoCelebFemale = false;        // ... and which pronoun it takes
  int casinoCelebMetAge = -1;            // the hero's age when the guest act was booked
  int casinoActMetAge = -1;              // ... and when the act in residence was booked
  int casinoCelebResignAge = -1;
  int casinoCelebAge0 = -1;        // the guest act's age when it was booked (355)
  int casinoCelebLifespan = 0;     // the age the guest act dies at
  int casinoCelebFame = -1;              // the guest act's own fame, drawn on its first demand (356)
  int casinoCelebPop = 50;               // ... and its own popularity, moved by an obliged demand
  int casinoCelebDemandAge = -1;         // the year the guest act last made a demand
  std::vector<int> casinoCelebPerks;     // the demand rows already satisfied (356)
  long long casinoCelebSalary = 0;
  int casinoCelebOfferLength = 0;  // REPORT.en.md 371       // the guest act yearly fee (362)
  int casinoCelebContract = -1;          // years left on its contract, ticked every year (362)
  int casinoGamblerNewAge = -1;          // the year the last new regular walked in (357)
  int casinoPunterAge0 = -1;       // the regular's age when they started coming (355)
  int casinoPunterLifespan = 0;    // the age that regular dies at
  int casinoPunterMetAge = -1;     // the hero's age when the regular turned up
  std::string casinoPunterName;    // the regular the death card is about
  bool casinoPunterFemale = false;         // the year the guest act last handed in notice
  int casinoActResignAge = -1;           // ... and the act in residence
  std::string casinoFeedbackTitle;       // the follow-up a resignation's answer chains (352)
  std::string casinoFeedbackBody;
  int casinoAmenityComplaintAge = -1;    // the year an amenity drew a complaint
  int casinoAmenityComplimentAge = -1;   // ... and the year one drew a compliment    // ... and the year it drew a compliment
  int casinoWithdrawAge = -1;            // the year the withdraw card fired (374)
  int casinoThemeAge = -1;               // the year the theme card fired (375)
  int casinoRenameAge = -1;             // the year the rename card fired (376)
  int casinoInvestAge = -1;  // the invest card's cooldown (REPORT.en.md 382)
  int casinoInstallGameAge = -1;  // the install card's cooldown (383)
  int casinoRoomNameAge = -1;    // the room rename card's cooldown (384)
  int casinoRoomRemoveAge = -1;  // the room remove card's cooldown (384)
  int casinoAmenityInstallAge = -1;   // the amenity install card's cooldown (385)
  int casinoAmenityDemolishAge = -1;  // the amenity demolish card's cooldown (385)
  int zooAttractionAge = -1;      // the attraction install card's cooldown (383)
  int casinoExpandAge = -1;             // the year the expand card fired (378)
  std::vector<int> casinoGamblerStyles;  // one style per punter, 1..5
  std::vector<long long> casinoAmenityMonthly;   // the same amenities' Monthly Expenses
  std::vector<std::string> casinoAmenityTheme;   // and the theme each was built for

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
  // The mafia (REPORT.en.md 64): a made man at one of five ranks, with the
  // family and the rival family the mob table assigns to his country.
  bool mafia = false;
  std::string mafiaFamily;
  std::string mafiaBoss;
  std::string mafiaNick;
  std::string mafiaRival;
  std::string mafiaRankName;
  std::string mafiaCountry;        // the ISO code the mob tables key on
  std::string mafiaJoin;           // how he got in, written once
  int mafiaRank = 0;               // 1 associate ... 5 godfather
  int mafiaStanding = 0;           // 0-100, how the family rates him
  int mafiaMoney = 0;              // what the family's business is worth
  int mafiaNotoriety = 0;
  int mafiaHeat = 0;
  int mafiaHits = 0;
  long long mafiaEarned = 0;
  // A zoo the hero owns (REPORT.en.md 65): the park, the animal on the card,
  // the habitat it lives in and the keeper looking after it.
  bool ownsZoo = false;
  std::string zooName;
  std::string zooStreet;
  std::string zooSupplier;
  int zooFoodPerAnimal = 2500;   // the current supplier's annual cost per animal (386)
  int zooSupplierAge = -1;       // the year the food supplier card last fired (386)
  int zooCommentAge = -1;        // the year the comment card last fired (387)
  std::string zooHabitat;        // the habitat the card is about
  std::string zooAnimalSpecies;  // the species ("Big Cat")
  std::string zooAnimalBreed;    // the breed inside the species
  std::string zooAnimalName;     // the individual the tables name
  bool zooAnimalFemale = false;
  int zooAnimalHappiness = 60;
  int zooAnimalHealth = 90;
  std::string zooEmployee;       // the keeper / vet on the card
  bool zooEmployeeFemale = false;
  std::string zooEmployeeRole;
  int zooEmployeeCompetence = 50;   // 0-100, gates the staff tables
  int zooEmployeeRel = 60;          // 0-100, how the keeper feels about the hero (328)
  int zooStaff = 0;
  int zooSize = 2;                  // 1 small / 2 medium / 3 large; the pool is 10/20/40 (364)
  int zooStaffCustodial = 0;        // the three counters SimZoo::.ctor splits its pool into
  int zooStaffSecurity = 0;
  int zooStaffCustomerRel = 0;
  int zooAnimals = 0;
  int zooAttractions = 0;
  int zooEngagement = 50;        // 0-100, what the patrons make of it
  int zooTicket = 20;            // gate price
  int zooAdmissionAge = -1;      // the year the admission fee card was offered (373)
  int zooRenameAge = -1;          // the year the zoo rename card fired (376)
  int zooInvestAge = -1;          // the year the zoo invest card fired (377)
  int zooHabitatAge = -1;         // the year the habitat card fired (379)
  std::vector<std::string> zooHabitats;  // '<name> (<type>)' per habitat built (379, 380)
  std::vector<std::string> zooAttractionNames;  // one name per installed attraction (383)
  int zooDemolishAge = -1;        // the year the demolish card fired (380)
  bool zooSellArmed = false;          // harness hook: offer the zoo sale card (381)
  bool casinoSellArmed = false;       // harness hook: offer the casino sale card (381)
  int zooVisitorsLastYear = 0;   // the visitors the last ledger year's gate sold for (373)
  long long zooMoney = 0;        // the takings this year
  // The zoo's own books (REPORT.en.md 321): a pot the takings and the upkeep are
  // settled against, the run of losing years, and the age whose pass already ran.
  long long zooBank = 0;
  int zooNegativeYears = 0;
  int zooLedgerYear = -1;
  bool zooBankruptQueued = false;

  // The zoo's animals as individuals (REPORT.en.md 344): the herd the lifecycle
  // cards act on, the age the park was bought (SimZoo+0x20 YearsOwned) and the
  // age of the last pregnancy card (its four-year cooldown).
  struct ZooHerdAnimal {
    std::string species;
    std::string breed;
    std::string name;
    bool female = false;
    int age = 0;
    bool pregnant = false;
    int health = 90;
    // The stats SimZooAnimal::GetPopularity (0x2b661f8) weighs (REPORT.en.md 366).
    bool rare = false;
    int cool = 60;        // 20 x the catalogue's Cool Score
    int happiness = 45;
    int craziness = 55;
    int charisma = 8;
  };
  std::vector<ZooHerdAnimal> zooHerd;
  bool zooHerdSeeded = false;   // the park's animals are only put there once
  int zooOwnedSince = -1;
  int zooPregAge = -1;
  int zooTheftAge = -1;    // the year the park was last burgled (346)
  int zooCleanAge = -1;    // the year the park's cleanliness was last flagged (348)
  int zooCustAge = -1;     // the year the operations manager last complained (349)
  int zooEmployeeHiredAge = -1;   // the hero's age when the keeper was hired (353)
  int zooEmployeeResignAge = -1;  // the year the keeper last handed in notice
  int zooEmployeeAge0 = -1;        // the keeper's age on the day they were hired (354)
  int zooEmployeeLifespan = 0;     // the age the keeper dies at
  // The gift shop (REPORT.en.md 363). The native park opens with empty shelves and the
  // player stocks them through the shop menu, which the port has no screen for, so the
  // park stocks three lines when it is bought (shortcut: 363).
  struct GiftShopItem {
    std::string product;      // the catalogue's Product column
    std::string plural;       // the catalogue's Plural column
    std::string animal;       // the featured animal's name
    std::string breed;
    std::string name;         // '<animal> <product>'
    long long pricePerUnit = 0;
    long long sellPrice = 0;
    long long minSale = 0;
    long long maxSale = 0;
    long long units = 0;
    long long unitsSold = 0;
    long long revenue = 0;
    int popScore = 0;
  };
  std::vector<GiftShopItem> giftShop;
  long long zooGiftRevenue = 0;   // what the shop took this year (363)
  int zooMerchAge = -1;           // the year the merch order card last fired (363)
  std::string zooFeedbackTitle;   // the plain card a merch order answers with (363)
  std::string zooFeedbackBody;
  // The shop's stocking chain (REPORT.en.md 368): the add-product card hands its line to
  // the unit card, which hands the units and the animal to the price card.
  GiftShopItem zooShopStaged;
  int zooShopStockAge = -1;       // the year the add-product card last fired (368)
  size_t zooShopProfileIdx = 0;   // the shelf the product card opened on (369)
  int zooShopProfileAge = -1;     // the year the product card last fired (369)
  size_t zooShopAnswerIdx = 0;    // the shelf the product card's answer is about (370)

  // The rental property (REPORT.en.md 66): a house the hero lets out, the
  // tenant in it, and the squatter who may replace them.
  bool landlord = false;
  std::string propType;          // a16_house-types-enc 'Type'
  std::string propAddress;       // what '<address>' and '<tenant-address>' print
  std::string propCity;          // the city the property stands in
  int propBeds = 0;
  int propBaths = 0;
  int propCondition = 0;         // 0-100, what the repair bills are about
  int propHaunted = 0;           // the type's Haunted Score
  long long propValue = 0;
  int propRent = 0;              // dollars a month
  std::vector<std::string> propFeatures;   // remodelled projects and amenities
  std::string tenantName;
  bool tenantFemale = false;
  std::string tenantJob;         // from the employment check
  int tenantIncome = 0;
  int tenantSatisfaction = 60;   // 0-100, the gate on most tenant tables
  int tenantCraziness = 20;      // 0-100, drawn with the job
  bool tenantSquatting = false;
  int tenantYears = 0;
  long long rentCollected = 0;

  // The household pet (REPORT.en.md 67): one animal, its health, and the year
  // it either dies or is let go.
  bool hasPet = false;
  std::string petAnimal;         // a16_pets-enc 'Animal'
  std::string petBreed;          // a16_pets-enc 'Breed'
  std::string petName;           // a16_pet-names-enc 'Name' for that animal
  bool petFemale = false;
  int petAge = 0;
  int petHealth = 95;            // 0-100
  int petHappiness = 75;         // 0-100
  bool petDead = false;
  std::string petCause;          // the death type, once it is gone
  long long petVetSpend = 0;

  // The great outdoors (REPORT.en.md 71): the off-roader, the bicycle and the
  // six activities of the a16_outdoor family.
  bool outdoors = false;
  std::string outdoorVehicle;    // a16_outdoor-vehicle-enc, "Make Model"
  // The car the hero owns (REPORT.en.md 323): make/model from a16_car-models-enc, the
  // reliability score the damage roll reads (1 = most breakable), the condition, and the
  // part that is currently broken (a16_car-parts-enc).
  // (make/model/condition are shared with the racing car below, which also draws a car
  // out of a16_car-models-enc.)
  bool ownsCar = false;
  int carCost = 0;               // the model's own Cost, the scale for part prices
  int carReliability = 3;        // Reliability Score, 1..5
  bool carNeedsRepair = false;
  std::string carPart;
  std::string carBrokenTerms;
  long long carRepairCost = 0;
  int carYearAtLastRepair = -1;
  int carYearsOwned = 0;
  std::string outdoorBike;
  std::string outdoorPlace;      // the destination's Location
  std::string outdoorHabitat;    // its Habitat, lowercased
  std::string outdoorCompanion;  // whoever came along this year
  bool outdoorCompanionFemale = false;
  std::string outdoorCave;       // the place '<outdoor-cave>' names
  std::string outdoorLandmark;
  std::string outdoorPlant;
  std::string outdoorBird;
  std::string outdoorMarine;
  std::string outdoorFish;       // the species on the line this year
  int outdoorJoy = 55;           // 0-100
  int outdoorBikeCondition = 85; // 0-100
  int outdoorTrips = 0;
  int outdoorCatches = 0;
  long long outdoorEarned = 0;

  // The hustle (REPORT.en.md 72): panhandling, busking or scamming for a living.
  // The kind and the props are drawn once and kept; the person, the mark and the
  // take belong to one year's card.
  bool hustle = false;
  std::string hustlerKind;        // panhandle / busk / scam
  std::string hustlerAttire;      // a16_hustler-attire-enc
  std::string hustlerAct;         // the perform act, or the scam in the year's card
  std::string hustlerInstrument;  // the busk instrument
  std::string hustlerMusic;       // the busk music type
  std::string hustlerSong;        // the song of the year, when one is played
  std::string hustlerSign;        // the panhandler's sign
  std::string hustlerStrategy;    // how the hero works the street
  std::string hustlerProp;        // what the hero carries
  std::string hustlerBox;         // the collection box
  std::string hustlerStreet;      // where the year's card happens
  std::string hustlerPerson;      // the passer-by of the year's card
  bool hustlerPersonFemale = false;
  std::string hustlerVictim;      // the mark of a scam
  bool hustlerVictimFemale = false;
  int hustlerSkill = 40;          // 0-100, the hero's way with a crowd
  int hustlerHeat = 0;            // police attention
  long long hustlerAmount = 0;    // the take of the year's card, for '<amount>'
  long long hustlerEarned = 0;

  // The space program (REPORT.en.md 73): a national agency, a rank ladder and
  // a mission record. Ranks and their pay come from a16_astronaut-career-rank.
  bool astronaut = false;
  std::string astroAgency, astroRankTitle, astroShuttle, astroCrewmate, astroColleague;
  std::string astroDirector, astroMission, astroDestination, astroDiscovery;
  std::string astroBody, astroPublication, astroArtifact;
  // The publication's peer review score and its Noble Prize latch (REPORT.en.md 372).
  // The native card keeps a list of publications; the port keeps one.
  int astroPubScore = -1;
  bool astroPubPrize = false;
  // A bare result card waiting to be shown: 'You lost' (REPORT.en.md 372).
  std::string astroFeedbackTitle, astroFeedbackBody;
  bool astroCrewmateFemale = false, astroColleagueFemale = false, astroBeenToSpace = false;
  int astroRank = 1;              // 1 Cadet, 2 Captain, 3 Major, 4 General
  int astroRespect = 40, astroFitness = 40, astroTechnical = 40, astroFlight = 40;
  int astroMissions = 0, astroYearsInAgency = 0, astroStress = 0, astroAnxiety = 0;
  long long astroSalary = 0, astroEarned = 0;

  // Recording artist, part two (REPORT.en.md 68): the studio. The genre is
  // a16_music-genres-enc's, the label a16_music-record-label-company-name-enc's,
  // the album a16_music-album-title-enc's, the cover a16_music-album-covers-enc's
  // and the tracks a16_music-track-title-enc's.
  bool musicSigned = false;   // true once a label has taken the artist on
  std::string musicLabel;     // a16_music-record-label-company-name-enc 'Name'
  std::string musicRole;      // a16_music-roles-enc 'Role' (singer, guitarist ...)
  std::string musicSubgenre;  // a16_music-subgenres-enc 'Subgenre'
  std::string musicAlbum;     // the record in the works or on the shelf
  std::string musicCover;     // a16_music-album-covers-enc 'Cover'
  std::string musicCoverArt;  // a16_music-cover-art-string-enc 'String'
  std::string musicSigSound;  // a16_music-signature-sound-enc 'Sound'
  std::vector<std::string> musicTrackList;   // the album's track titles
  int musicFame = 20;         // 0-100, the door to every venue and fan row
  int musicRespect = 40;      // 0-100, what the fans and the label think
  int musicAlbums = 0;
  int musicCompilations = 0;
  long long musicEarned = 0;
  // The studio trades (REPORT.en.md 69): the five music jobs that are not the
  // artist line. Each flag switches one line on; tradeYear walks whichever is
  // set and every card names the act the trade is working with.
  bool tradeProducer = false;    // a producer with a roster of artists
  bool tradeSoundcloud = false;  // an independent uploading to SoundCloud
  bool tradeScout = false;       // a scout looking for acts for a label
  bool tradeComposer = false;    // a composer for film, TV and games
  bool tradeStunts = false;      // an act whose club nights turn into stunts
  std::string tradeStage;        // the producer stage name / screen name
  std::string tradeArtist;       // the act on the books right now
  bool tradeArtistFemale = false;
  int tradeArtistPop = 0;        // that act's popularity 0-100
  int tradeArtistVocal = 50;     // their vocal quality, for the roster rows
  std::string tradeGenre;        // the genre the trade works in
  std::string tradeTalent;       // the SoundCloud collaborator / scouted act
  std::string tradeTrack;        // the SoundCloud track on the card
  std::string tradeInstrument;   // the instrument the hero plays
  std::string tradeProject;      // the score production project
  std::string tradeProjectType;  // film / tv / video game
  std::string tradeTone;         // the tone of that score
  int tradeBpm = 0;
  int tradeRep = 20;             // producer reputation 0-100
  int tradeFollowers = 0;        // SoundCloud followers
  int tradeClients = 0;          // artists on the producer's books
  int tradeHits = 0;             // hits produced / scores accepted / acts signed
  long long tradeEarned = 0;
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
  // The lowest the score ever got (REPORT.en.md 192): the break-up gate is
  // 'relationship < 30' and §191 found it almost never fires, so the question is whether
  // the gate or the score is at fault.
  int relationshipLow = 100;
  // Years together (REPORT.en.md 303): the proposal card prints 'Years Together', so
  // the pair's start age has to be kept beside the score. Both cells are stamped
  // lazily -- in relationshipYear and in the proposal card itself -- because a
  // partner is created and replaced from a dozen places in the family files and
  // none of them would remember to reset a counter.
  int loverSinceAge = -1;
  std::string loverSinceName;
  // The hero has questioned or changed their identity (REPORT.en.md 195). The table's
  // 'Transgender Identity' and 'Other Identity' columns are **flags**, not names, so the
  // port stores the fact rather than a string it would have had to invent.
  bool identityQuestioned = false;
  // The hero's sexuality (REPORT.en.md 217). a16_sexuality-scenarios-enc gates every one
  // of its 102 rows on this column, so the card needs the value before it can fire;
  // 'straight' is the default the script can override.
  std::string sexuality = "straight";
  // The hero's own illnesses (REPORT.en.md 247). The played-life trace diagnoses
  // anxiety at 36 and depression at 37 -- two at once -- and cures both in the same
  // year at 41, so the hero carries a list, not a single slot.
  struct Ailment {
    std::string name;   // display form with its article: "the flu", "acne"
    std::string key;    // the row's Disease cell
    int health = 0;     // Health Impact Score of the row
    int cures = 0;      // Cures Organically Score (0 = never on its own)
    int years = 0;      // how many age-ups it has lasted
    bool noCure = false;
    bool fatal = false;
    bool diagnosed = true;  // native IsUndiagnosed is false for the existing diagnosis lane
  };
  std::vector<Ailment> ailments;
  // Ordinary witch-doctor visits (REPORT.en.md 308): opening fixes this age's offer.
  int witchRemedyAge = -1, witchRemedyRow = -1, witchCostFactor = 0;
  bool curedByWitchDoctor = false, ateSnake = false;
  std::string witchDeathRemedy;
  std::vector<Ailment> pastAilments;
  std::vector<std::string> obituaryColors;
  // The house the hero lives in (REPORT.en.md 218).
  std::string home;
  // The aircraft the hero owns (REPORT.en.md 222), 'Make Model' from a16_aircraft-enc.
  std::string aircraft;
  // SimAircraft's own state (REPORT.en.md 334): the a16_aircraft-enc row, Age, Condition,
  // AgeAtLastRepair, the open repair (ActiveRepair) and NeedsToBeScrapped.
  int aircraftRow = -1;
  int aircraftAge = 0;
  double aircraftCondition = 100.0;
  int aircraftAgeAtLastRepair = 0;
  int aircraftTickAge = -1;
  bool aircraftRepairPending = false;
  bool aircraftScrapPending = false;
  long long aircraftRepairCost = 0;
  std::string aircraftPart, aircraftBrokenTerm;
  int aircraftPartRow = -1;
  // An OnlyFans channel (REPORT.en.md 225): the subscribers it has, and whether the
  // channel exists at all. The tip-response table is the only card the channel owns.
  bool onlyfans = false;
  int onlyfansFans = 0;
  int loverCraziness = 0;
  // The gift (REPORT.en.md 89) needs the partner's pride as well as her
  // craziness: every refusal row in the two property pools is flagged one or
  // the other, so the two scores are the traits the pools are read with.
  int loverPride = 0;
  // The celebrity dating app (REPORT.en.md 94) pairs the hero with a lover the
  // category tables describe, so the pairing carries the trade, the money and
  // the fame the game's own SimCelebrityLover keeps beside the relationship.
  bool loverCelebrity = false;
  std::string loverFameType;   // actor / musician / athlete / influencer / ceo / model
  // The magazine photo shoot (REPORT.en.md 335): the open offer, the month counter the
  // card draws its issue from, and the payment the offer quotes.
  int magPose = 0;
  int magRow = -1;
  long long magPayment = 0;
  std::string magTitle;
  int magMonthNI = 1;
  // The book deal (REPORT.en.md 336): the open offer, the five title candidates the
  // card offers, and the two counters the free offers are paced by.
  bool bookPending = false;
  int bookCount = 0;
  int lastBookAge = -100;
  long long bookCost = 0;
  long long bookProceeds = 0;
  std::string bookPublisher;
  std::vector<std::string> bookTitles;
  // The talk show and the commercial (REPORT.en.md 365): the open offers, and
  // for the commercial the reach word its body quotes and the payment it quoted.
  std::string talkShowName;
  std::string commCompany;
  std::string commReach;
  long long commPayment = 0;
  int talkShowAge = -1;    // the year the show invitation was answered (365)
  int commAge = -1;        // the year the commercial offer was answered (365)
  std::string loverGenre;      // the career row's Genre, Platform, Sport or Model Type
  std::string loverSkin;       // the names row's Skin, 1-6
  int loverFameScore = 0;      // the career row's Fame Score
  int loverRetirementAge = 0;  // the career row's Retirement Age
  long long loverSalary = 0;   // drawn between Minimum Salary and Maximum Salary
  int celebDates = 0;          // how many times the app has paired the hero
  // The breakup announcement (REPORT.en.md 125). LoverBreakUp is built with the
  // partner who has just walked, so the card is a report and the three strings it
  // needs are carried until the port offers it at the head of the next age-up.
  bool breakUpPending = false;
  std::string breakUpWord;     // RelationshipStatusString lowered: girlfriend / wife / ...
  std::string breakUpName;     // the body's {1}, the partner's first name
  std::string breakUpFull;     // the narrative's {0}, the partner's FullName
  // The movie theater (REPORT.en.md 96). The game caches four movies on the
  // hero's own SimPlace -- AvailableMovie, AvailableChildrensMovie,
  // AvailableComedyMovie, AvailableDocumentaryMovie -- and RefreshAvailableMovies()
  // refills them for the place the hero stands in. The port keeps the same four
  // slots with the row each was drawn from, so a card can name the film and the
  // log line that follows it names the same one.
  struct PlaceMovie { std::string title; int row = 0; };
  PlaceMovie movies[4];       // 0 non-children 1 children 2 comedy 3 documentary
  std::string movieCity;      // the city the four slots were drawn for
  int moviesWatched = 0;      // the game's own 'MoviesWatched' stat (#30523)
  bool virgin = true;
  // The ex-lovers (REPORT.en.md 134). The game keeps them as a SimLover array
  // (`Life.ExLoverArray`), and the eighteen ExLover* cards read the entry they were
  // constructed with: who it is, when it ended, and what the pair had been. The
  // port used to carry only the most recent ex; the vector is what the family
  // needs, and `exName`/`exFemale` are now views of its last entry.
  struct ExLover {
    std::string name;
    bool female = false;
    int age = 0;
    std::string status;    // what the pair were when it ended: dating / engaged / married
    int relationship = 0;
  // The lowest the score ever got (REPORT.en.md 192). The break-up gate is
  // 'relationship < 30', and §191 found it almost never fires; this is the measurement
  // that says whether the gate or the score is the problem.

    int year = 0;          // the year it ended, counted from the hero's birth
    // Where the ex lives (REPORT.en.md 163). The game's ex cards ask
    // SimPerson::LivesInSamePlaceAsHero before they push, and the port had no place
    // for anybody but the hero. Empty means "same place as the hero was then".
    std::string city;
  };
  std::vector<ExLover> exLovers;  // oldest first; the most recent is back()
  const ExLover* latestEx() const { return exLovers.empty() ? nullptr : &exLovers.back(); }
  int exes = 0;
  bool musician = false;
  std::string musicGenre;  // a16_music-genres-enc 'Genre'
  std::string bandName;    // a16_music-names-bands-enc 'Title'
  bool ownsBusiness = false;
  std::string industry;  // a16_business-industries-enc 'Industry'
  // The company itself (REPORT.en.md 74): the product line, its brand name, the
  // supplier, the staff, and the figures the books run on.
  std::string bizProduct;      // a16_business-products-enc 'Product', one entry
  std::string bizProductName;  // a16_business-product-names-enc 'Format', filled in
  std::string bizSupplier;     // a16_business-supplier-name-structure-enc
  std::string bizUnit;         // a16_business-products-enc 'Unit'
  int bizDemand = 50;          // 0..100, the market's appetite for the product
  int bizCompetition = 50;     // 0..100, how crowded the market is
  int bizHealth = 60;          // 0..100, what the consultant has to say
  int bizAttitude = 60;        // 0..100, what the staff think of the boss
  int bizEmployees = 0;
  int bizFacilities = 0;
  long long bizPrice = 0;         // 'Avg Unit Price'
  long long bizUnitCost = 0;      // 'Min'/'Max Unit Cost'
  long long bizFacilityCost = 0;  // 'Facility Annual Cost'
  long long bizSalary = 0;        // 'Average Employee Salary'
  long long bizCash = 0;
  long long bizRevenue = 0;
  long long bizCosts = 0;
  long long bizProfit = 0;

  // The car and the races (REPORT.en.md 75). The hero owns one car, drawn from
  // a16_car-models-enc, and races it in its own category.
  bool racing = false;
  std::string carMake;
  std::string carModel;
  std::string carType;   // a16_car-models-enc 'Type', also <car-type>
  std::string carName;   // "Make Model", the <car> token
  std::string carClass;  // 'Racing Class': C/B/A/S
  std::string racerCategory;  // the car's first Race Type Whitelist entry
  std::string racerRival;     // the rival racer's name
  std::string racerSponsor;
  std::string racerCup;
  std::string racerTrack;
  std::string racerGarage;   // the shop the car lives at
  std::string racerPitCrew;
  std::string racerMechanic;
  std::string racerJacket;
  int carCondition = 100;    // 0..100: repairs and races move it
  int carHandling = 40;
  int carSpeed = 40;
  int carAcceleration = 40;
  int carDurability = 40;
  int carMods = 0;           // how many race-car mods are bolted on
  int racerRank = 0;         // 0 Bronze, 1 Silver, 2 Gold, 3 Platinum
  int racerWins = 0;
  int racerLosses = 0;
  int racerRivalCraziness = 40;
  int racerProfessionalism = 50;
  int racerFans = 0;
  long long carValue = 0;
  long long racerWinnings = 0;
  long long racerSpend = 0;

  // The screen (REPORT.en.md 76).
  bool actor = false;
  std::string actorAgent;      // the agent repping the hero
  std::string actorAgency;
  std::string actorProduction;  // the film or show the hero is in
  std::string actorGenre;
  std::string actorRole;       // the character
  std::string actorType;       // "film" or "show"
  std::string actorCostar;
  std::string actorLead;
  std::string actorExtra;      // the bit part from a16_actor-extra-roles-enc
  bool actorCostarFemale = false;
  bool actorWorking = false;
  int actorReputation = 20;
  int actorAgentReputation = 40;
  // The agent's own gender and opinion of the hero (REPORT.en.md 327). The hire card
  // never stored a gender, so -1 means "not read yet"; the opinion is the card's
  // relationship slot and starts neutral.
  int actorAgentFemale = -1;
  int actorAgentOpinion = 50;
  int actorPerformance = 40;
  int actorImpression = 40;
  int actorSmugness = 40;
  int actorPropensity = 30;
  int actorFilms = 0;
  int actorEpisodes = 0;
  int actorAwards = 0;
  int actorDrama = 0;
  long long actorFee = 0;
  long long actorEarned = 0;

  // --- The market (REPORT.en.md 77) ---------------------------------------
  bool investing = false;                    // financeYear is live
  struct FinHolding {
    std::string symbol;                      // <asset-symbol>
    std::string name;                        // <company>
    std::string sector;                      // <sector>
    bool crypto = false;
    long long shares = 0;
    double price = 0;                        // per unit, moved every year
    double cost = 0;                         // what the hero paid per unit
  };
  std::vector<FinHolding> finHoldings;       // the universe the hero trades
  std::string finAdvisor, finFirm, finSector, finCompany, finSymbol;
  bool finCrypto = false, finAdvisorFemale = false, finPrivate = false;
  bool finStarted = false;   // the inherited first position is a one-off
  int finAdvisorRep = 0, finSkill = 40, finAppetite = 40, finTips = 0;
  long long finInvested = 0, finEarned = 0, finCash = 0;
  // --- The catwalk (REPORT.en.md 78) --------------------------------------
  bool model = false;
  std::string modelAgency, modelType, modelCategory, modelIndustry, modelCompany;
  std::string modelProduct, modelGift, modelVenue, modelPublication, modelOutfit;
  std::string modelExpression, modelBackground, modelRival, modelRivalGender;
  bool modelElite = false, modelFamous = false, modelRivalFemale = false;
  int modelRank = 0, modelProfile = 20, modelSkill = 40, modelAttitude = 50;
  int modelPoise = 50, modelQuirkiness = 50, modelJobs = 0, modelShows = 0, modelStress = 0;
  long long modelSalary = 0, modelEarned = 0;
  // The luxuries (REPORT.en.md 79).
  bool luxury = false;
  std::string luxWatch, luxIsland, luxIslandType, luxSociety, luxSocietyRankName;
  std::string luxFeature, luxChef, luxCuisine, luxEvent, luxCharity, luxMovie;
  bool luxIslandOwned = false, luxSocietyMember = false;
  bool luxAircraft = true, luxCar = true, luxBoat = true;
  int luxIslandSize = 0, luxSocietyRank = 0, luxEvents = 0, luxSpaVisits = 0;
  long long luxValue = 0, luxSpent = 0, luxWindfall = 0;

  // The season (REPORT.en.md 80): the pro-sports career behind the athlete
  // switch of 52. One season a year, with a stat sheet and an event.
  bool sportPro = false;
  int sportSkill = 20, sportGreatness = 10, sportRespect = 20;
  int sportFitness = 40, sportBalance = 40, sportEndurance = 40;
  int sportFlexibility = 40, sportStrength = 40;
  int sportSeasons = 0, sportGames = 0, sportWins = 0, sportAwards = 0;
  int sportFame = 0;
  bool sportChampion = false, sportTop5 = false, sportHall = false;
  bool sportRookie = false;            // true only for the first season
  bool sportInjured = false;
  std::string sportInjury;
  int sportInjuryYears = 0, sportInjurySeverity = 0, sportInjuryPain = 0;
  bool sportDebilitating = false, sportCareerEnding = false;
  std::vector<std::string> sportAwardNames;
  long long sportSalary = 0, sportEarned = 0;

  // The boat (REPORT.en.md 81).
  bool boating = false;
  std::string boatType, boatCategory, boatName, boatPart, boatDanger;
  bool boatLicensed = false, boatWrecked = false;
  int boatCondition = 100, boatPassengers = 1, boatFeet = 0;
  int boatAge = 0, boatParts = 0, boatTrips = 0;
  long long boatValue = 0, boatSpent = 0, boatEarned = 0;


  bool athlete = false;
  std::string sport;     // a16_sports-enc 'Sport'
  std::string league;    // a16_sports-leagues-enc 'Name'
  std::string team;      // a16_sports-teams-enc 'Name', alternates resolved
  std::string position;  // a16_sports-positions-enc 'Position'
  std::string careerTrack;  // the raw Seniority Track cell
  int careerStep = 0;       // 0-based index into that track
  int careerYears = 0;

  // The coworkers (REPORT.en.md 105). The office family of 83 is the hero as
  // somebody's manager; this is the peer next to him, the one the four
  // ScenarioCoworker* cards talk about. The game keeps a SimWorkplace roster
  // (SimWorkplace.RefreshCoworkers, which GetCoworker() walks) and draws the
  // card's person out of it; the port keeps one slot instead, drawn when the
  // hero takes a job, and the four cards share it for the whole career.
  std::string cwName;    // the coworker a card is about
  bool cwFemale = false;
  // ScenarioCoworkerCompliment (REPORT.en.md 331): the coworker's personality and the
  // relationship the card moves, drawn once on the first card about him.
  int cwProfessionalism = -1;
  int cwCoolness = -1;
  int cwRelation = 50;
  int cwLastComplimentAge = -1;
  std::string supName;   // the workplace supervisor the unprofessional card is about
  bool supFemale = false;
  int supPresented = 0;  // ScenarioSupervisorUnprofessional's own presented cap
  // ScenarioWorkplaceDisciplineSupervisor (REPORT.en.md 325). The supervisor's two
  // personality values are -1 until the discipline card first draws them (ceiling: the
  // native game generates them with the coworker), and the relationship strength the
  // warning-or-termination decision reads is kept here because the port has no
  // SimPerson relationship model for a supervisor.
  int supStrictness = -1;
  int supProfessionalism = -1;
  int supRelation = -1;
  int supInsulted = 0;      // times the discipline card's insult lane ran
  int troublePoints = 0;    // SimWorkplace's own counter, fed by the card's seriousness
  int disciplineAge = -1;
  // The insult's own comeback card (REPORT.en.md 343): the sentence the button wrote, the
  // comeback line that titles the card, and the insult word its journal repeats. The
  // discipline card fills them and the pending card renders and clears them.
  std::string insultSentence;
  std::string insultComeback;
  std::string insultWord;
  // The HR investigation (REPORT.en.md 339): the reason literal its caller passed,
  // the seriousness that came with it, the workplace's management strictness its
  // termination score reads (drawn once, since the port keeps no workplace object
  // -- a shortcut), and the tracker counter every path bumps.
  std::string hrReason;
  int hrSeriousness = 0;
  int mgmtStrictness = -1;
  int timesReportedToHR = 0;
  int attackAge = -1;   // the last year the hero attacked somebody (REPORT 337)   // the last year the discipline card fired
  // The walk's animal encounter (REPORT.en.md 326). The kill is handed to
  // checkDeath as a cause rather than written here, the way the other card-owned
  // deaths are.
  int animalEncounterAge = -1;  // the last year the encounter card fired
  std::string animalKill;       // the animal whose attack killed the hero
  std::string animalKillVerb;   // that row's Attack String
  // ScenarioSupervisorTeamBuilding (REPORT.en.md 117) keeps a supervisor slot of
  // its own, so its lazy name draw never moves the unprofessional card's stream,
  // and the workplace flag both of its lanes set, which the port keeps per life
  // (the game keeps it on the workplace object the port does not model).
  std::string tbSupName;
  bool tbDone = false;
  // The after-hours invitation (REPORT.en.md 118) brings the workplace family's
  // second supervisor card: it reuses the same lazy supervisor slot above and
  // keeps both of the game's own counters -- its presented cap of five and the
  // employee byte 0x39 that decides which of the two sentences the card shows.
  int ahPresented = 0;
  bool ahFirst = true;
  // The lightning strike (REPORT.en.md 119) is the first wired card that can
  // kill the hero: its DismissSelected lane hands the death system a cause of
  // its own, and the life byte 0x5e0 its refresh reads is a once-per-life flag
  // the port keeps here (the game re-offers the card every eligible year).
  bool lightDone = false;
  bool lightWillDie = false;
  // The supervisor hookup card (REPORT.en.md 120) is the workplace family's
  // fifth push: the boss propositions the hero or the hero propositions the
  // boss, and the direction byte 0x181 is drawn per year rather than kept, so
  // the game's own PresentedCount cap of five is the only counter the port
  // needs.
  int sedPresented = 0;
  // The witness card (REPORT.en.md 121) is the citizen family's first push
  // and the only street card in the port. ScenarioEthicsWitnessCrime asks
  // Life::PresentedCountForScenario < 4 (@0x73f3bcf6c0), so the cap is four
  // and the life needs no other record of it.
  int witPresented = 0;
  // REPORT.en.md 122: ScenarioEthicsFindDrugs counts its own showings (the gate
  // wants PresentedCountForScenario < 3) and its consume lane can hand the death
  // system a drug name of its own, which checkDeath honours.
  int findPresented = 0;
  std::string drugDeath;  // the drug that kills the hero, when one does
  // REPORT.en.md 124: ScenarioJobReferral counts its own showings the same way
  // (the gate wants PresentedCountForScenario < 3).
  int referralPresented = 0;

  // ScenarioSupervisorSeduceSuccess (REPORT.en.md 123) is not a card the year can
  // roll: the game pushes it the moment a hookup choice resolves, so it waits here
  // with the name of the boss it happened with until the port presents it.
  std::string morningWith;
  bool morningByHero = false;
  bool morningPrevLover = false;
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
