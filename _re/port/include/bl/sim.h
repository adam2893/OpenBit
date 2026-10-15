// bl/sim.h -- life-sim core loop over the real content tables.
#pragma once

#include "bl/game.h"
#include "bl/table.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace bl {

// One of the 102 countries in _re/gamedata_json/a16_countries-enc.json
// (BinaryFormatter dump; keys are the C# backing fields "<Name>k__BackingField").
struct Country {
  std::string name;
  std::string continent;
  std::string code;
  std::string currency;
  std::string language;
  std::string demonym;
  std::vector<std::string> cities;
};

// One resolved age-up event, taken from a real table row.
struct AgeUp {
  std::string table;      // the real table the row came from
  std::string row_id;     // row number in that table (1-based, data rows)
  std::string title;      // 'Title' / '' for narrative-only tables
  std::string body;       // 'Body' or the narrative string, placeholders resolved
  std::string cta;        // 'CTA'
  std::string choice;     // 'Choice N'
  std::string narrative;  // 'Narrative N'
  // The family lifecycle lines of that same year, written to the log before the
  // scenario's own line ("My mother has been promoted to Tour Operator.").
  std::vector<std::string> extra;
  int karma = 0;          // 'Karma Impact N'
  int discipline = 0;     // 'Discipline Impact N'
  int happiness = 0;      // 'Happiness Impact N'
  int smarts = 0;         // 'Intelligence Impact N'
  int looks = 0;          // 'Appearance Impact N'
};

// One non-hero family member. The yearly lifecycle lines name these people
// ("My little brother, Jimbo, has been diagnosed with mad cow disease."), so
// each one carries the state those lines read: a relation word, a name, an age,
// a current illness and a job. Report section 37.
struct Person {
  std::string relation;  // after "my ": "mother", "father", "big brother", "half sister"
  std::string name;      // first name; empty for parents, which the game never names
  std::string lastName;
  bool female = false;
  int age = 0;
  bool alive = true;
  std::string disease;   // current illness, empty when healthy
  std::string job;       // current job title, empty when none
  std::string company;
};

struct Life {
  Hero hero;
  int year = 0;
  bool alive = true;
  std::vector<Person> family;
  // The friend-advice card's PresentedCountForScenario counter (REPORT.en.md
  // 100): the game shows that card at most twice in one life.
  int advicePresented = 0;
  // The friend-lover card's own PresentedCountForScenario counter (REPORT.en.md
  // 104): its gate reads the count and pushes only at zero, so that card is the
  // family's one-shot.
  int loverPresented = 0;
  // The four coworker cards' own PresentedCountForScenario counters (REPORT.en.md
  // 105). Their gates read the count and withhold the card once it has been
  // shown N+1 times, with N = 4 (annoyance), 3 (inappropriate behaviour),
  // 2 (prank) and 2 (rumor).
  int cwAnnoyPresented = 0;
  int cwInappPresented = 0;
  int cwPrankPresented = 0;
  int cwRumorPresented = 0;
  // The friend-rumor outbound card's queued follow-up (REPORT.en.md 107): the
  // inbound card's "Start a rumor about ..." choice hands the friend over and
  // the next age-up renders the friend's answer, because the port models no
  // scenario push. The card is dismiss-only, so the flag clears on the next
  // age-up whether or not the card is shown.
  bool rumorOutPending = false;
  std::string rumorOutFriend;
  bool rumorOutFemale = false;
  // The best-friend request's own clock (REPORT.en.md 109): the game tests the
  // hero's age minus the friend's HeroAgeAtLastBestFriendRequest (>= 11) and
  // stamps that field with the hero's age when the card is pushed, so the port
  // keeps the hero's age at the last such request.
  int bestFriendAge = 0;
};

class Sim {
 public:
  // reDir is /Users/Adam/Documents/bitlife/_re (the directory holding
  // gamedata_plain/ and gamedata_json/).
  Sim(std::string reDir, uint64_t seed);

  bool load(std::string* error);       // loads the wired-up real tables
  const std::vector<Country>& countries() const { return countries_; }
  const Table* table(const std::string& name) const;
  std::vector<std::string> loadedTableNames() const { return loaded_; }

  Life createLife(bool female);
  // SimHero.SetupEverythingElseForNewHero; the game runs it on the first age
  // increment, so ageUp() calls it and createLife() does not.
  void applyNewbornSetup(Hero& h);
  // Pins the hero's birth country to a name from a16_countries-enc.json (exact
  // match, case-insensitive) and draws the birth city from that country's own
  // Cities array -- one RNG draw when it succeeds, none when the name is not a
  // real country, so createLife()'s stream stays untouched. The birth region
  // comes from placeRng_ for the same reason. Used by --script.
  bool pinCountry(Life& life, const std::string& countryName);
  // The region a city sits in, from a16_city-regions-enc (REPORT.en.md 95).
  std::string regionForCity(const std::string& code, const std::string& city);
  // false when no real row matched. forcedChoice >= 0 takes that 0-based index
  // out of the choices the drawn scenario row fills in instead of letting the
  // RNG pick one; out of range falls back to the RNG. -1 is the plain age-up.
  bool ageUp(Life& life, AgeUp& out, int forcedChoice = -1);
  // Replays one exact real table row instead of drawing one. The Phase 1 bench
  // uses it to feed the port the pick the game actually made, so the comparison
  // isolates mechanics from selection. row is 1-based; row <= 0 clears the
  // override. A table that is not loaded yet is loaded on demand (the wired-up
  // loop only loads five), so a script may name any real gamedata_plain table.
  bool forceScenario(const std::string& table, long long row);
  // Replays one scripted-event scenario by its real il2cpp class name, rendered
  // from the registry in sim.cpp (the game builds these from code plus il2cpp
  // string literals, so there is no table row to name). choice is the 0-based
  // button index; -1 lets the RNG pick one. A class the registry does not know
  // reports on stderr and returns false, so the bench sees the gap instead of a
  // silent fallback.
  bool forceScenarioClass(const std::string& cls, int choice);
  // The age-0 log lines, in the order the game writes them: birth place and
  // circumstance, birthday and sign, name, mother, father, an optional sibling
  // and an optional family pet. Every sentence skeleton is a real table
  // ('a16_birth-circumstances-enc', 'a16_careers-enc', 'a16_pets-enc') and the
  // connective words are the fixed English the game keeps as il2cpp literals.
  // Draws from the same RNG stream as the rest of the life, so it must be called
  // once, before the first ageUp().
  std::vector<std::string> birthLines(Life& life);

  // Turns a life into one born into royalty: reads the country's seats out of
  // a16_country-royal-systems-enc and one house out of
  // a16_royal-house-names-enc. runScript calls it before birthLines, so the
  // birth line and the hero snapshot both carry the state (REPORT.en.md 47).
  void makeRoyal(Life& life);
  // The royal year (REPORT.en.md 87): the twelve a16_royal-* pools.
  bool royalYear(Life& life, AgeUp& out, int forcedChoice);
  std::string royalPaper();

  // Turns a life into a vampire and gives it a familiar. runScript calls it when
  // the script says "vampire": true (REPORT.en.md 48).
  void makeVampire(Life& life);
  void makeMilitary(Life& life);
  void makeAthlete(Life& life);
  void makeBusiness(Life& life);
  void makeRacing(Life& life);
  void makeActor(Life& life);
  void makeFinance(Life& life);
  void makeModel(Life& life);
  void makeLuxury(Life& life);
  void makeBoating(Life& life);
  void makePrisoner(Life& life);
  void makeOffice(Life& life);
  void makeSchool(Life& life);
  void makePodcast(Life& life);
  void makeMusician(Life& life);
  void makePolitician(Life& life);
  void makeMisc(Life& life, unsigned mask);
  void makeSecretAgent(Life& life);
  void makeFighter(Life& life);
  void makeBlackMarketeer(Life& life);
  void makeCasino(Life& life);
  void makeCult(Life& life);
  void makeDealer(Life& life);
  void makeMafia(Life& life);
  void makeZoo(Life& life);
  void makeLandlord(Life& life);
  void makePet(Life& life);
  void makeOutdoor(Life& life);
  void makeHustler(Life& life);
  void makeAstronaut(Life& life);
  void ensureStudioFields(Life& life);
  std::string musicAlbumDescription();
  // The five studio trades (REPORT.en.md 69). kind: 1 producer, 2 SoundCloud,
  // 3 talent scout, 4 composer, 5 club stunts.
  void makeTrade(Life& life, int kind);
  void makeInstrument(Life& life);
  void makeRelationships(Life& life);
  // The per-person lifecycle lines of one year: illness, school milestones, the
  // relations' jobs and new siblings. Each line is one of the 58 '<person>'
  // templates in global-metadata.dat (#9372-#9429, section 36) rendered in the
  // log's first person. Ages the family, so it runs once per year, from ageUp().
  std::vector<std::string> lifecycleLines(Life& life);
  // Real-world date gate for the calendar/holiday dumb-scenario sets. The
  // game consults them through MinutesSinceLastCalendarDumbScenario()
  // (DumbScenarioInterface.GetARandomPushDumbScenario, il2cpp_dump.txt:56500)
  // and their tables carry a real date, not a hero attribute: holiday rows name
  // Halloween/Christmas/Thanksgiving/New Years, calendar rows a Month + Date.
  // 0,0 (the default) leaves both sets out of the yearly pool, which is what a
  // seed-only run can honestly model. Month is 1-12.
  void setDate(int month, int day);

  // Death roll over a16_death-types-enc ('Type','Commonness Score','Min Age','Sentence').
  bool checkDeath(const Life& life, std::string& type, std::string& sentence);

  // Resolves the real placeholder syntax used inside scenario strings:
  // "<a/b/c>" picks one alternative, "<num2-5>" / "<number:natural10-20>" a
  // number, "<br>" a newline, "<grammar:a/an>" the article for the next word,
  // "<pick:a|b>" one alternative, "<rb>" a draw made earlier in the same
  // scenario, "<hero-name>" / "<city>" the hero's own data, and the bare word
  // tokens ("<friend>", "<insult>", "<random-person>") their pools.
  std::string resolve(const std::string& text);

 private:
  const Table& tbl(const char* name) const;
  bool ensureTable(const std::string& name);
  // Renders one real row into out: Title/Body/CTA, the filled-in Choice N
  // columns and the branch's impacts. Falls back to a narrative-only table
  // (a 'Scenario' or 'String' column) so a forced filler row still renders.
  bool renderRow(const Table& t, size_t row, const Life& life, AgeUp& out, int forcedChoice);
  // Renders one registry entry (a scripted-event scenario) into out. The body
  // and the chosen branch's log line both come from the registry's literals,
  // with "{parent}" / "{name}" filled the way the game fills its own {0}/{1}.
  bool renderScenarioClass(const std::string& cls, const Life& life, AgeUp& out, int choice);
  bool scenarioEligible(const Table& t, size_t row, const Life& life) const;
  // Eligible 0-based rows of one dumb-scenario set. The set tables and their
  // gates: REPORT.en.md sections 25 and 27.
  std::vector<size_t> dumbSetRows(const char* name, const Life& life);
  // The game's yearly pick (DumbScenarioInterface.GetARandomPushDumbScenario):
  // a 6-way category roulette, re-rolled while the drawn category's gate fails
  // or its set is empty, 100 attempts, then nothing. Report section 27.
  bool pickDumbScenario(const Life& life, std::string* table, size_t* row);
  // The game never adds an 'X Impact N' cell to an attribute directly: every
  // Impact* setter runs the score through AttributeAdjustmentPointsForImpactScore
  // (@0x73f32b140c, read out of libil2cpp.so) and then clamps the attribute to
  // 0..100. Scores are the -6..6 the dumb tables actually carry; 0 is the one
  // case that draws RNG and lands on +1 or -1.
  int pointsForImpact(int score);
  bool fillerEligible(const Table& t, size_t row, const Life& life) const;
  std::string personName(const std::string& token);
  // Same walk, but reusing one token->value memo, so a nested resolve (the
  // '<grammar:a/an>' look-ahead) cannot draw a second name for one token.
  std::string resolveInto(const std::string& text, std::map<std::string, std::string>& memo);
  // The bare word tokens are entries of the game's own word pool table
  // genesis_master_tags_reference_2 ('<insult>', '<drug>', '<musician>',
  // '<celebrity>', '<random-person>', '<random-first-name-male>', ...).
  std::string referenceToken(const std::string& token);
  std::string circumstancePublic();
  std::string circumstanceHome();
  std::string vacationDestination();
  std::string vacationCruise();
  // '<hero-name>', '<hero:age>', '<city>', '<country-other>', ... read the live
  // hero, so the hero of the run being rendered has to be in scope.
  std::string heroToken(const std::string& key, bool possessive);
  // '<num2-5>' / '<number:natural10-20>' / '<currency5000-550000>' /
  // '<decnum100-105>' and their '_roundup_10|100|1000' suffixes.
  bool numberToken(const std::string& token, std::string& out);
  // '<pronoun>' and the '<candidate.he|she>' style pairs: the alternative
  // follows the hero's own gender.
  bool genderToken(const std::string& token, std::string& out);
  std::string money(long long v) const;
  // One family-member first name from a16_casino-casino-celebrity-name-enc, the
  // pool createLife() and birthLines() already draw names from.
  std::string drawFirstName(bool female);
  // A company name built the way the game builds it: a16_company-name-structure-enc
  // row for the career's own 'Company Type' ('<color> <suffix>' /
  // '<city> <suffix>', Predefined wins when set), filled from the color and
  // suffix pools. Table found by the trace line "<job> for Helsinki Travel".
  std::string companyName(const std::string& companyType, const Life& life);
  // One surname from the same celebrity-name pool, for the local physicians the
  // illness chain offers ("Dr. Seaman" in the capture).
  std::string drawSurname();
  // '<highest-royal-name>' / '<other-royal-name>' (a seat plus a first name) and
  // '<circumstance-royal>' (the royal twin of the circumstance pools, built in
  // code by DataHub.GetRandomCircumstanceRoyalWithCastleName, 0x73f2ce4050).
  std::string royalToken(const std::string& token);
  std::string royalCircumstance();
  // '<vampire>' / '<vampire-possessive>' (the year's vampire NPC) and
  // '<familiar>' / '<familiar-possessive>' (the hero's own companion).
  std::string vampToken(const std::string& key);
  // One vampire NPC name from a16_vampire-name-enc (Type = female / male /
  // surname, 170 rows).
  std::string vampireNpcName();
  // The vampire's own yearly pool, served instead of the ordinary dumb-scenario
  // draw while the hero is a vampire.
  bool vampireYear(Life& life, AgeUp& out, int forcedChoice);
  bool militaryYear(Life& life, AgeUp& out, int forcedChoice);
  bool careerYear(Life& life, AgeUp& out, int forcedChoice);
  bool sportsYear(Life& life, AgeUp& out, int forcedChoice);
  bool businessYear(Life& life, AgeUp& out, int forcedChoice);
  bool musicYear(Life& life, AgeUp& out, int forcedChoice);
  bool politicalYear(Life& life, AgeUp& out, int forcedChoice);
  bool politicalCareerYear(Life& life, AgeUp& out, int forcedChoice);
  bool politicalOfficeCard(Life& life, AgeUp& out, const Table& t, size_t row, int forcedChoice);
  std::string politicalCareerToken(const std::string& token);
  std::string politicalToken(const std::string& token);
  std::string miscToken(const std::string& key);
  bool miscPoolYear(Life& life, AgeUp& out, int forcedChoice);
  std::string relationshipToken(const std::string& key);
  // The secret-agent mission pools (report 58): one mission per year, its
  // briefing built from the type's own tables and its outcome from
  // a16_secret-agent-mission-summary-feedback-enc.
  bool spyYear(Life& life, AgeUp& out, int forcedChoice);
  // The fight career (report 59): one card per year, its opponent banded by the
  // hero's league position and its outcome fed back through the coach tables.
  bool fightYear(Life& life, AgeUp& out, int forcedChoice);
  std::string spyToken(const std::string& key);
  std::string spyFrontName(const std::string& businessType);
  std::string fightToken(const std::string& key);
  bool bmYear(Life& life, AgeUp& out, int forcedChoice);
  std::string bmToken(const std::string& key);
  void ensureBmFields(Life& life);
  std::string bmStructureName(const char* tableName);
  std::string bmDrawItem(Life& life);
  bool casinoYear(Life& life, AgeUp& out, int forcedChoice);
  bool cultYear(Life& life, AgeUp& out, int forcedChoice);
  std::string casinoToken(const std::string& key);
  std::string cultToken(const std::string& key);
  std::string cultHonorificPick();
  void ensureCasinoFields(Life& life);
  void ensureCultFields(Life& life);
  bool dealerYear(Life& life, AgeUp& out, int forcedChoice);
  std::string dealerToken(const std::string& key);
  void ensureDealerFields(Life& life);
  bool mafiaYear(Life& life, AgeUp& out, int forcedChoice);
  std::string mafiaToken(const std::string& key);
  std::string mafiaRankTitle(const Hero& h, int rank);
  void ensureMafiaFields(Life& life);
  bool zooYear(Life& life, AgeUp& out, int forcedChoice);
  std::string zooToken(const std::string& key);
  std::string movieToken(const std::string& key);
  // The four place slots of a16_movies-enc, 0 non-children 1 children 2 comedy
  // 3 documentary (REPORT.en.md 96).
  std::string placeMovie(int slot);
  void ensureZooFields(Life& life);
  void zooDrawAnimal(Life& life);
  bool landlordYear(Life& life, AgeUp& out, int forcedChoice);
  std::string landlordToken(const std::string& key);
  void ensureLandlordFields(Life& life);
  void landlordDrawTenant(Life& life);
  bool petYear(Life& life, AgeUp& out, int forcedChoice);
  std::string petToken(const std::string& key);
  void petDraw(Life& life);
  bool outdoorYear(Life& life, AgeUp& out, int forcedChoice);
  bool hustlerYear(Life& life, AgeUp& out, int forcedChoice);
  bool astroYear(Life& life, AgeUp& out, int forcedChoice);
  std::string outdoorToken(const std::string& key);
  std::string hustlerToken(const std::string& key);
  std::string astroToken(const std::string& key);
  bool studioYear(Life& life, AgeUp& out, int forcedChoice);
  bool tradeYear(Life& life, AgeUp& out, int forcedChoice);
  std::string tradeToken(const std::string& key);
  std::string tradeGenre(Life& life);
  void tradeDrawArtist(Life& life);
  size_t pickWindow(const std::string& table, const std::string& weightColumn,
                    const std::string& filterColumn, const std::string& filterValue,
                    const std::string& loColumn, const std::string& hiColumn,
                    double value, Rng& rng);
  size_t spyRow(const std::string& table, const std::string& weightColumn,
                const std::string& filterColumn, const std::string& filterValue,
                const std::string& flagColumn);
  std::string spySnippet(const std::string& table, const std::string& column,
                         const std::string& weightColumn, const std::string& filterColumn,
                         const std::string& filterValue, const std::string& flagColumn);
  // The same two draws, but over a caller-supplied stream: every career keeps
  // its own Rng so one family's draws never shift another's sequence.
  size_t pickRow(const std::string& table, const std::string& weightColumn,
                 const std::string& filterColumn, const std::string& filterValue,
                 const std::string& flagColumn, Rng& rng);
  std::string pickSnippet(const std::string& table, const std::string& column,
                          const std::string& weightColumn, const std::string& filterColumn,
                          const std::string& filterValue, const std::string& flagColumn, Rng& rng);
  bool relationshipYear(Life& life, AgeUp& out, int forcedChoice);
  bool celebAppYear(Life& life, AgeUp& out, int forcedChoice);
  bool movieYear(Life& life, AgeUp& out, int forcedChoice);
  // The generic pastime pool with a person (REPORT.en.md 97).
  bool spendTimeYear(Life& life, AgeUp& out, int forcedChoice);
  // The skip-school card (REPORT.en.md 98): the second consumer of the same pool.
  bool skipSchoolYear(Life& life, AgeUp& out, int forcedChoice);
  // The mobster's spend-time card (REPORT.en.md 99): the mafia's own push.
  bool mobsterSpendYear(Life& life, AgeUp& out, int forcedChoice);
  // The friend-advice card (REPORT.en.md 100): a friend's question, capped at
  // two showings per life. It draws on its own stream (advRng_).
  bool friendAdviceYear(Life& life, AgeUp& out, int forcedChoice);
  // The friend-prank card (REPORT.en.md 101): a friend pranks the hero out of
  // a16_friend-pranks-enc and the hero picks one of the card's four reactions.
  // It carries no presented cap and draws on its own stream (prankRng_).
  bool friendPrankYear(Life& life, AgeUp& out, int forcedChoice);
  // The friend-rumor card (REPORT.en.md 102): a friend starts a rumor about the
  // hero out of a16_friend-rumors-enc and the hero picks one of the card's four
  // reactions. It carries no presented cap either and draws on its own stream
  // (rumorRng_).
  bool friendRumorYear(Life& life, AgeUp& out, int forcedChoice);
  // The friend-rumor outbound card (REPORT.en.md 107): the answer to the inbound
  // card's "Start a rumor about ..." choice, drawn from the Outbound Scenario
  // column of a16_friend-rumors-enc. It is queued by that choice and rendered on
  // the next age-up -- the port has no scenario push -- and it is dismiss-only,
  // so it applies no stat changes and carries no choices. It draws on its own
  // stream (rumOutRng_).
  bool friendRumorOutYear(Life& life, AgeUp& out, int forcedChoice);
  // The friend-party card (REPORT.en.md 103): a friend invites the hero out of
  // a16_friend-party-scenarios-enc and the hero accepts, declines or makes up
  // an excuse. Its floor is five rather than eighteen and it carries no
  // presented cap either; it draws on its own stream (partyRng_).
  bool friendPartyYear(Life& life, AgeUp& out, int forcedChoice);
  // The friend-lover card (REPORT.en.md 104): the hero meets a lover of a
  // friend's -- a person the card generates at push time -- out of
  // a16_friend-lover-scenarios-enc and hooks up with them, ignores them, reports
  // them to the friend, or (as a vampire) hypnotizes them. It is the family's
  // only one-shot card and it draws on its own stream (flRng_).
  bool friendLoverYear(Life& life, AgeUp& out, int forcedChoice);
  // The friend spend-time card (REPORT.en.md 106): the hero and a friend go out
  // for a pastime, or the friend turns the invitation down. It is the family's
  // only card without buttons (DismissSelected writes the line it built) and it
  // draws on its own stream (fstRng_).
  bool friendSpendTimeYear(Life& life, AgeUp& out, int forcedChoice);
  // The unfriend card (REPORT.en.md 108): a friend gives the hero up, and the
  // hero can try to salvage the friendship, let the friend go or insult them one
  // last time. It is the family's only card with three buttons and the only one
  // the game pushes out of Life.IncrementAge's friend loop rather than a table;
  // it draws on its own stream (unfrRng_).
  bool friendUnfriendYear(Life& life, AgeUp& out, int forcedChoice);
  // The friend-status card (REPORT.en.md 109): the yearly friend loop's own
  // decision about what the friendship is -- a best-friend request with two
  // buttons, or a friend who turns into an enemy. It carries an eleven-year clock
  // of its own (life.bestFriendAge) and draws on its own stream (fsRng_).
  bool friendStatusYear(Life& life, AgeUp& out, int forcedChoice);
  // The coworker family (REPORT.en.md 105): four cards about the person the
  // hero works next to -- an annoyance, an inappropriate behaviour, a prank and
  // a rumor -- out of a16_coworker-annoy-scenarios-enc,
  // a16_coworker-behavior-scenarios-enc, a16_coworker-pranks-enc and
  // a16_coworker-rumors-enc. Each carries its own presented cap and none of
  // them draws a random number, so the coin is the port's; they share one
  // coworker and draw on their own stream (cwRng_).
  bool coworkerYear(Life& life, AgeUp& out, int forcedChoice);
  // The row draw and the one-of pick come out of the caller's stream, so every
  // feature keeps its own draw sequence (REPORT.en.md 97, 98).
  int pickPastime(Rng& rng, std::string& tail);
  // The mob table's own pastime draw, country keyed (the card's
  // GetRandomMafiaPastimeForCountryCode @0x73f2cf3534, REPORT.en.md 99).
  int mafiaPastimeRow(Rng& rng, std::string& tail);
  bool anniversaryYear(Life& life, AgeUp& out, int forcedChoice);
  // The gift (REPORT.en.md 89): the shop and the two property pools.
  bool giftYear(Life& life, AgeUp& out, int forcedChoice);
  // The conversations family (REPORT.en.md 90): one topic, one listener, and
  // the row's own Controversy Score for how it lands.
  bool conversationYear(Life& life, AgeUp& out, int forcedChoice);
  // The social media year, and the handle generator a stranger's name needs.
  bool socialYear(Life& life, AgeUp& out, int forcedChoice);
// The school year (REPORT.en.md 92): a trip, a dance, the prom or a party.
bool backToSchoolYear(Life& life, AgeUp& out, int forcedChoice);
  std::string makeUsername(int age, bool female, Rng& rng);
  // A troll card names the same stranger in its body and in its log, so the
  // handle is pinned while that one card renders.
  std::string usernameOverride_;
  // The badge table of the anniversary family: the row's five captions and the
  // situation description the feed shows (REPORT.en.md 88).
  std::string annBadgeOption(const std::string& tag, std::string& situation, int* rowOut = nullptr);
  bool relEligible(const Table& t, size_t row, const Life& life) const;
  void ensureOfficeFields(Life& life);
  std::string musicToken(const std::string& token);
  std::string musicTitle(const char* table, const std::string& genre);
  std::string businessToken(const std::string& token);
  std::string businessName(const std::string& industry);
  std::string businessFormat(const char* table, const std::string& industry);
  std::string businessSupplier(const std::string& industry);
  std::string businessProductName(const std::string& format);
  bool racingYear(Life& life, AgeUp& out, int forcedChoice);
  std::string racingToken(const std::string& token);
  bool racingCardRow(Life& life, AgeUp& out, const std::string& table, size_t row,
                     int forcedChoice, bool moment, int* success);
  bool actorYear(Life& life, AgeUp& out, int forcedChoice);
  std::string actorToken(const std::string& token);
  bool financeYear(Life& life, AgeUp& out, int forcedChoice);
  std::string financeToken(const std::string& token);
  std::string financeCompany(const std::string& sector);
  bool modelYear(Life& life, AgeUp& out, int forcedChoice);
  std::string modelToken(const std::string& token);
  std::string modelName(const char* table);
  int modelRankIndex() const;
  // The luxuries (REPORT.en.md 79).
  bool luxuryYear(Life& life, AgeUp& out, int forcedChoice);
  std::string luxuryToken(const std::string& token);
  std::string luxIslandName(const std::string& islandType);
  // The season (REPORT.en.md 80).
  bool sportsSeason(Life& life, AgeUp& out, int forcedChoice);
  // The boat (REPORT.en.md 81).
  bool boatYear(Life& life, AgeUp& out, int forcedChoice);
  std::string boatToken(const std::string& token);

  void racingBuyCar(long long budget);
  std::string sportsToken(const std::string& token);
  std::string warToken(const std::string& token);
  // One prison year: a job line or a row of the prison dumb-scenario pool, then
  // the countdown and the release line. REPORT.en.md 45.
  bool prisonYear(Life& life, AgeUp& out, int forcedChoice);
  bool officeYear(Life& life, AgeUp& out, int forcedChoice);
  std::string officeToken(const std::string& token);
  bool schoolYear(Life& life, AgeUp& out, int forcedChoice);
  std::string schoolToken(const std::string& token);
  // One podcast year: the books, then a card from the show's own pool.
  // REPORT.en.md 85.
  bool podcastYear(Life& life, AgeUp& out, int forcedChoice);
  bool podcastCard(Life& life, AgeUp& out, const Table& t, size_t row, int forcedChoice);
  std::string podcastToken(const std::string& token);
  std::string podRefBind(const std::string& cell);
  std::string cardRefBind(const std::string& cell, std::map<std::string, std::string>& cache, Rng& rng);
  // The yearly conviction draw that puts the hero in prison.
  bool maybeConvicted(Life& life, AgeUp& out);
  // The symptom card is the one scenario whose title, body and log line all come
  // from a table row (a16_disease-symptom-scenarios-enc); the registry entry for
  // ScenarioDiseaseContract only carries the four hard-coded option labels.
  bool renderSymptomCard(const Life& life, AgeUp& out, int choice);
  // The game caches every '<a/b/c>' alternative it draws while parsing one
  // scenario, in draw order, and lets the log line point back at them with
  // '<rb>' / '<rb1>' (the 1st draw), '<rb2>' (the 2nd) up to '<rb4>'. The cache
  // lives for one scenario render: the body and the branch's narrative share it.
  // BracketParser.SelectedRandomBracketList / CacheRandomBrackets, report 33.
  std::vector<std::string> rndCache_;
  std::string rbLast_;      // the last bracket value resolved, for "<rb>" when no alternative was drawn
  // The hero of the life currently being rendered, set by the four resolve
  // callers (birthLines / ageUp / renderRow / checkDeath): resolve() itself
  // takes no Life, and the '<hero:...>' / '<city>' tokens need one.
  const Hero* hero_ = nullptr;
  const Life* life_ = nullptr;

  // The physician the last ScenarioDoctors card picked, read back by the
  // treatment card that follows it in the same illness chain.
  std::string lastDoctor_;
  std::string reDir_;
  Rng rng_;
  // The prison subsystem's own stream, so the yearly conviction draw does not
  // shift the draws every other subsystem makes from rng_ for a given seed
  // (the port's earlier features kept the main stream intact on purpose).
  Rng prisonRng_;
  // Family mortality and any other event that must not shift the main stream.
  Rng familyRng_;
  // The royal birth roll's own stream (the probability itself is unread; see
  // REPORT.en.md 47), so the switch never shifts the main stream.
  Rng royalRng_;
  // The vampire branch's own stream.
  Rng vampireRng_;
  Rng militaryRng_;
  Rng careerRng_;
  Rng sportsRng_;
  Rng bizRng_;
  std::map<std::string, std::string> bizCache_;  // one product / rival / supplier per card
  Rng racingRng_;
  Rng actorRng_;
  Rng financeRng_;
  Rng modelRng_;
  Rng luxuryRng_;
  Rng sportProRng_;
  Rng boatRng_;
  Rng officeRng_;
  Rng schoolRng_;
  Rng podRng_;
  // The place pool's own stream (REPORT.en.md 95): the birth region is drawn
  // while the life is being set up, and createLife's attribute draws are
  // matched value-for-value against the game's own snapshots, so the region
  // must not come out of rng_.
  Rng placeRng_;
  Rng spendRng_;
  Rng skipRng_;
  Rng mobRng_;
  // The friend-advice card's own stream (REPORT.en.md 100).
  Rng advRng_;
  // The friend-prank card's own stream (REPORT.en.md 101).
  Rng prankRng_;
  // The friend-rumor card's own stream (REPORT.en.md 102).
  Rng rumorRng_;
  // The friend-party card's own stream (REPORT.en.md 103).
  Rng partyRng_;
  // The friend-lover card's own stream (REPORT.en.md 104).
  Rng flRng_;
  // The coworker family's own stream (REPORT.en.md 105).
  Rng cwRng_;
  // The friend spend-time card's own stream (REPORT.en.md 106).
  Rng fstRng_;
  // The friend-rumor outbound card's own stream (REPORT.en.md 107).
  Rng rumOutRng_;
  // The unfriend card's own stream (REPORT.en.md 108).
  Rng unfrRng_;
  // The friend-status card's own stream (REPORT.en.md 109).
  Rng fsRng_;
  std::map<std::string, std::string> racingCache_;  // one rival / mechanic / passenger per car card
  bool racingLive_ = false;
  std::map<std::string, std::string> actorCache_;  // one costar / production / role per screen card
  bool actorLive_ = false;
  std::map<std::string, std::string> financeCache_;  // one advisor / ticket per market card
  bool financeLive_ = false;
  std::map<std::string, std::string> modelCache_;  // one rival / product / look per catwalk card
  std::map<std::string, std::string> luxuryCache_;  // one island / chef / watch per luxury card
  bool modelLive_ = false;
  std::map<std::string, std::string> sportProCache_;  // one opponent team per season card
  std::map<std::string, std::string> boatCache_;      // one craft name / part / mishap per boat card
  std::map<std::string, std::string> prisonCache_;    // one inmate / nickname / gift per prison card
  bool prisonLive_ = false;
  bool prisonForced_ = false;  // the script asked for a sentence (REPORT.en.md 82)
  std::map<std::string, std::string> officeCache_;  // one employee / company per office card
  bool officeLive_ = false;
  std::map<std::string, std::string> schoolCache_;  // one teacher / classmate / principal per school card
  bool schoolLive_ = false;
  std::map<std::string, std::string> polCache_;  // one party / rival / bribe per political card
  std::map<std::string, std::string> royalCache_;  // one paper / subject / celebrity per royal card
  bool polLive_ = false;
  bool royalLive_ = false;
  std::map<std::string, std::string> podCache_;  // one guest / episode title per podcast card
  bool podLive_ = false;
  bool boatLive_ = false;
  bool luxuryLive_ = false;
  Rng musicRng_;
  Rng politicsRng_;
  Rng miscRng_;
  Rng relRng_;
  Rng spyRng_;
  Rng fightRng_;
  Rng bmRng_;
  Rng casinoRng_;
  Rng cultRng_;
  Rng dealerRng_;
  Rng mafiaRng_;
  Rng zooRng_;
  Rng landlordRng_;
  Rng petRng_;
  Rng outdoorRng_;
  Rng hustlerRng_;
  Rng astroRng_;
  std::map<std::string, std::string> outdoorCache_;  // one draw per token per card
  std::map<std::string, std::string> hustlerCache_;  // one draw per token per card
  std::map<std::string, std::string> astroCache_;    // one crewmate / body / discovery per card
  // While a hustle card renders, '<victim>', '<amount>' and '<insult>' belong to
  // the family rather than to the vampire one, which claims them first otherwise.
  bool hustlerLive_ = false;
  // The space family owns a few words the vampire one also claims.
  bool astroLive_ = false;
  Rng studioRng_;
  Rng tradeRng_;
  std::string spyTarget_;      // '<target>' / '<target-assassination>' of this card
  std::string spyColleague_;   // the other agent a train/warning row names
  bool spyAgentIsOther_ = false;
  std::map<std::string, std::string> spyCache_;  // one name per token per card
  std::string fightOpponent_;       // '<fighter>' in the fight-ing tables
  std::string fightOpponentType_;   // '<fight-opponent-type>'
  std::string fightArena_;          // '<fighting-arena>' of this card
  bool fightSelf_ = false;          // the fight-er persona tables mean the hero
  std::map<std::string, std::string> fightCache_;  // one circumstance per card
  std::string bmVendor_;   // '<vendor>' in the vendor pool
  std::string bmChemist_;  // '<drug-chemist>' in the police pool
  std::map<std::string, std::string> bmCache_;  // one item / vendor / auctioneer per card
  std::string casinoGambler_;  // one punter per casino card
  bool casinoGamblerFemale_ = false;  // and the punter's own gender
  std::string casinoRoom_;     // one room / game per casino card
  std::string cultCommunard_;  // one follower per cult card
  bool cultCommunardFemale_ = false;
  std::string cultExCommunard_;   // one ex-follower per cult card
  std::map<std::string, std::string> cultCache_;
  std::string dealerMember_;       // one cartel member per trade card
  bool dealerMemberFemale_ = false;
  std::string dealerVictim_;       // one target per trade card
  std::map<std::string, std::string> dealerCache_;
  std::string mafiaAmount_;        // one <amount> per family card
  std::map<std::string, std::string> mafiaCache_;  // one name per token per card
  std::map<std::string, std::string> zooCache_;    // one animal / keeper / park name per card
  std::string zooOther_;           // another animal in the park
  bool zooOtherFemale_ = false;
  std::vector<std::string> zooBuilt_;   // the attractions already installed
  std::string llSquatter_;      // the tenant who stopped paying
  bool llSquatterFemale_ = false;
  bool vampVictimFemale_ = false;                     // one victim per vampire card
  std::map<std::string, std::string> vampCache_;      // one council member per card
  // The lover pools write about the partner, not the hero, so the pronoun
  // brackets have to follow the partner while one of those rows renders.
  std::string relNameCache_;  // one <name> per card
  bool pronounOtherSet_ = false;
  bool pronounOtherFemale_ = false;
  std::vector<std::string> warCountries_;  // the three countries one war row names
  std::vector<std::pair<std::string, Table>> tables_;
  std::vector<std::string> loaded_;
  // Next age-up's forced row, set by forceScenario() and cleared when it fires.
  std::string forcedTable_;
  long long forcedRow_ = 0;
  // Scripted-event override: the registry class and its 0-based button index.
  std::string forcedClass_;
  int forcedClassChoice_ = -1;
  int dateMonth_ = 0;  // 1-12, 0 = no real-world date fixed
  int dateDay_ = 0;    // 1-31, 0 = no real-world date fixed
  std::vector<Country> countries_;
};

}  // namespace bl
