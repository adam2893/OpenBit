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
  // real country, so createLife()'s stream stays untouched. Used by --script.
  bool pinCountry(Life& life, const std::string& countryName);
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

  // Turns a life into a vampire and gives it a familiar. runScript calls it when
  // the script says "vampire": true (REPORT.en.md 48).
  void makeVampire(Life& life);
  void makeMilitary(Life& life);
  void makeAthlete(Life& life);
  void makeBusiness(Life& life);
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
  std::string vampireToken(const std::string& token);
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
  void ensureZooFields(Life& life);
  void zooDrawAnimal(Life& life);
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
  bool relEligible(const Table& t, size_t row, const Life& life) const;
  void ensureOfficeFields(Life& life);
  std::string musicToken(const std::string& token);
  std::string musicTitle(const char* table, const std::string& genre);
  std::string businessToken(const std::string& token);
  std::string businessName(const std::string& industry);
  std::string sportsToken(const std::string& token);
  std::string warToken(const std::string& token);
  // One prison year: a job line or a row of the prison dumb-scenario pool, then
  // the countdown and the release line. REPORT.en.md 45.
  bool prisonYear(Life& life, AgeUp& out, int forcedChoice);
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
