#include "bl/sim.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace bl {
namespace {

std::string slurp(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

std::string lower(std::string s) {
  for (char& c : s) c = char(std::tolower((unsigned char)c));
  return s;
}

std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (unsigned char)s[a] <= ' ') ++a;
  while (b > a && (unsigned char)s[b - 1] <= ' ') --b;
  return s.substr(a, b - a);
}

// ---- minimal JSON reader ------------------------------------------------
// gamedata_json/a16_countries-enc.json is a flat array of flat objects, so the
// object splitter plus a string / string-array reader is all that is needed.

void encodeUtf8(unsigned cp, std::string& out) {
  if (cp < 0x80) {
    out += char(cp);
  } else if (cp < 0x800) {
    out += char(0xC0 | (cp >> 6));
    out += char(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += char(0xE0 | (cp >> 12));
    out += char(0x80 | ((cp >> 6) & 0x3F));
    out += char(0x80 | (cp & 0x3F));
  } else {
    out += char(0xF0 | (cp >> 18));
    out += char(0x80 | ((cp >> 12) & 0x3F));
    out += char(0x80 | ((cp >> 6) & 0x3F));
    out += char(0x80 | (cp & 0x3F));
  }
}

std::vector<std::pair<size_t, size_t>> topLevelObjects(const std::string& s) {
  std::vector<std::pair<size_t, size_t>> out;
  int depth = 0;
  bool in_string = false, escape = false;
  size_t begin = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if (in_string) {
      if (escape) escape = false;
      else if (c == '\\') escape = true;
      else if (c == '"') in_string = false;
      continue;
    }
    if (c == '"') { in_string = true; continue; }
    if (c == '{') { if (depth++ == 0) begin = i; }
    else if (c == '}') { if (--depth == 0) out.emplace_back(begin, i); }
  }
  return out;
}

// Index just past the ':' of "key":, or npos.
size_t findValue(const std::string& s, const std::string& key, size_t from, size_t end) {
  const std::string needle = "\"" + key + "\"";
  size_t p = s.find(needle, from);
  if (p == std::string::npos || p > end) return std::string::npos;
  p = s.find(':', p + needle.size());
  if (p == std::string::npos || p > end) return std::string::npos;
  return p + 1;
}

std::string readString(const std::string& s, size_t& i, size_t end) {
  while (i < end && s[i] != '"') ++i;
  if (i >= end) return std::string();
  ++i;
  std::string out;
  while (i < end && s[i] != '"') {
    if (s[i] == '\\' && i + 1 < end) {
      const char e = s[++i];
      switch (e) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': break;
        case 'u': {
          if (i + 4 < end) {
            unsigned cp = unsigned(std::strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16));
            i += 4;
            encodeUtf8(cp, out);
          }
          break;
        }
        default: out += e;
      }
      ++i;
      continue;
    }
    out += s[i++];
  }
  if (i < end) ++i;
  return out;
}

std::string stringField(const std::string& s, const std::string& key, size_t from, size_t end) {
  size_t i = findValue(s, key, from, end);
  if (i == std::string::npos) return std::string();
  return readString(s, i, end);
}

std::vector<std::string> stringArrayField(const std::string& s, const std::string& key,
                                          size_t from, size_t end) {
  std::vector<std::string> out;
  size_t i = findValue(s, key, from, end);
  if (i == std::string::npos) return out;
  while (i < end && s[i] != '[') ++i;
  if (i >= end) return out;
  ++i;
  while (i < end && s[i] != ']') {
    if (s[i] == '"') out.push_back(readString(s, i, end));
    else ++i;
  }
  return out;
}

std::vector<Country> loadCountries(const std::string& path) {
  const std::string text = slurp(path);
  std::vector<Country> out;
  for (const auto& obj : topLevelObjects(text)) {
    Country c;
    c.name = stringField(text, "<Name>k__BackingField", obj.first, obj.second);
    c.continent = stringField(text, "<Continent>k__BackingField", obj.first, obj.second);
    c.code = stringField(text, "<Code>k__BackingField", obj.first, obj.second);
    c.currency = stringField(text, "<Currency>k__BackingField", obj.first, obj.second);
    c.language = stringField(text, "<Language>k__BackingField", obj.first, obj.second);
    c.demonym = stringField(text, "<Demonym>k__BackingField", obj.first, obj.second);
    c.cities = stringArrayField(text, "Cities", obj.first, obj.second);
    if (!c.name.empty()) out.push_back(std::move(c));
  }
  return out;
}

// Commonness-Score weighted pick over an explicit candidate pool (weightedPick
// above picks over a whole table; the birth rows need the age gate applied
// first).

// Scripted-event scenarios: the game builds these in code from il2cpp string
// literals (metadata layout in METADATA_STRINGS.en.md), so there is no table row
// to replay. Every string below is a literal captured from the running game
// (blh.js Life.LogScenarioPresentation probe, campaign 3) or read out of
// global-metadata.dat; the brace tokens are the game's own {0}/{1} fill points.
struct RegChoice {
  const char* label;  // ScenarioChoice.<Title>
  const char* log;    // the sentence the game logs for this branch, '' = none
};
struct RegScenario {
  const char* cls;       // real il2cpp class name
  const char* ext;       // Scenario.ExteriorTitle (card header), '' = none
  const char* title;     // Scenario.Title
  const char* body;      // Scenario.Body
  int n;
  RegChoice choices[4];  // empty for the scenes the game auto-advances
  bool silent = false;   // modal card the game keeps out of its LifeLog
};
const RegScenario kScenarios[] = {
    {"ScenarioChildhoodVaccination", "", "Vaccination",
     "Your {parent} is taking you to the doctor's office to get vaccinated.\n\nHow will you behave?",
     3,
     {{"Throw a tantrum",
       "I threw a tantrum when my {parent} took me to the doctor's office to get vaccinated."},
      {"Bite her", "I bit my {parent} when {parent} took me to the doctor's office to get vaccinated."},
      {"Try to stay calm",
       "I remained calm when my {parent} took me to the doctor's office to get vaccinated."}}},
    {"ScenarioSchoolEnroll", "Education", "Elementary School",
     "You are starting elementary school.", 0, {}},
    {"ScenarioFriendMeet", "Friend", "New Friend",
     "Your classmate, {name}, wants to become your friend.", 2,
     {{"Become friends with him", "I became friends with {name}."}, {"Reject him", ""}}},
    {"ScenarioRetentionCrime", "Crime", "Mischief Maker",
     "You're wondering how much trouble you'll get into if you do something a little bit naughty.\n\nWhat will you do?",
     2, {{"Ignore the thought", ""}, {"Cause some trouble", ""}}},
    // Auto-advancing scenes (no buttons). An empty branch log means the game's own
    // LifeLog sentence for that scene has not been captured yet, so the port logs
    // the card body; replace it once a capture pairs the two.
    // LifeLog words the sibling birth in first person about the parents, while the
    // card talks about "your parents"; the species/gender noun is drawn.
    {"ScenarioScriptEventSiblingBorn", "", "Baby",
     "Your parents just had a baby boy named {name}, your new brother.", 1,
     {{"", "My mother and my father had a baby boy named {name}, my new brother."}}},
    {"ScenarioScriptEventParentsBreakUp", "", "Divorce", "Your parents have divorced.", 0, {}},
    {"ScenarioScriptEventParentMarriesStepparent", "", "Wedding",
     "Your mother has married your new stepfather, {name}, a 42-year old grocer.\n\nThey went to "
     "Hollywood for their honeymoon.", 0, {}},
    // Captured LifeLog: "My cat, Ramona, died while sleeping peacefully." The card names
    // the animal, age and cause; the log is a first-person rewrite, so only the shape is
    // matched here.
    {"ScenarioPetDeath", "", "Death",
     "The family mutt, {name}, died at the age of 12.\n\nHe lived a great life, but sadly passed "
     "peacefully in the night.", 1,
     {{"", "My cat, {name}, died while sleeping peacefully."}}},
    // Captured LifeLog: "I am suffering from tonsillitis." The card and the log each name
    // a different ailment, both drawn at runtime.
    // The symptom card is rendered by renderSymptomCard() straight out of the
    // 432-row symptom table; this entry only carries the four option labels the
    // game hard-codes (metadata 27502 / 21936 / 15622 / 40895 / #46595) and is
    // never used for text. The third label is a home remedy that differs per
    // symptom ("Chug some water" for a fever, "Take some aspirin" for difficulty
    // urinating) and is built in code, not in the CSV. REPORT.en.md 43.
    {"ScenarioDiseaseContract", "", "Fever", "", 4,
     {{"It doesn't bother me", ""}, {"Go to the doctor", ""},
      {"Chug some water", ""}, {"Search the web", ""}}},
    {"ScenarioFriendNew", "", "My ride or die", "You are now friends with {name}.", 0, {}},
    {"ScenarioEthicsBreakSomething", "", "Integrity",
     "You broke your parents's {item} while you were playing.\n\nWhat will you do?", 2,
     {{"Admit that I broke it", "I admitted to my parents that I broke their {item}."},
      {"It wasn't me", "I broke my parents's {item} and then lied about it."}}},
    {"ScenarioSchoolClassmateActUp", "", "Troublemaker",
     "Your classmate, {name}, is marching around your music teacher, Mrs. {teacher}'s, classroom and pretending "
     "to play the trombone while smacking other students with the trombone slide.\n\nWhat will you do?",
     4,
     {{"Ignore her antics", ""}, {"Report her to the principal", ""}, {"Attack her", ""}, {"Laugh at her", ""}}},
    // Captured card: "You signed up for an OnlyFans account." (metadata literal
    // #49322 "You signed up for {0} account."); the matching LifeLog literal is
    // #25101, the first-person form "I signed up for {0} account.". The channel is
    // drawn from a16_social-media-channels-enc (7 rows, Facebook..OnlyFans).
    {"ScenarioSocialMediaJoinComplete", "", "Social Media",
     "You signed up for {channel} account.", 1,
     {{"", "I signed up for {channel} account."}}},
    // The scripted-event twin of ScenarioDiseaseContract: this class names another
    // person, and the game writes the sentence in the third person on the card
    // ("Your mother is suffering from itchy bottom.", captured) and in the first
    // person into the LifeLog (#9389/#9394 templates name <disease-new>/<disease-old>).
    {"ScenarioScriptEventDiseaseContract", "", "Diagnosis",
     "Your {parent} is suffering from {disease}.", 1,
     {{"", "My {parent} is suffering from {disease}."}}},
    // Menu scenes: card, option labels and outcome lines are C# literals with no
    // CSV row. Each was recovered by pairing the captured card with the Scenario
    // record that follows it in the trace (REPORT.en.md 41).
    {"ScenarioLoveFirstKiss", "Love", "First Kiss",
     "You have an opportunity to have your first kiss with a {partner} from your class named {name}.\n\nWhat will you do?",
     3,
     {{"Kiss {name} but definitely no tongue",
       "I had my first kiss with {name}. I made sure to not slip {partnerobj} any tongue."},
      {"Kiss {name} with tongue", "I tongue-kissed {name}. It was my first kiss."},
      {"Run away from {name}",
       "I could have had my first kiss with {name} but instead I ran away from {partnerobj}."}}},
    {"ScenarioOfferFood", "Friends", "Food",
     "Some of your friends are eating {food}.\n\nWill you join them?", 2,
     {{"Join them", "I started eating {food}."},
      {"No thanks", ""}}},
    {"ScenarioOpportunityCasino", "Friends", "Casino",
     "Some of your friends are going to a local casino called {casino}.\n\nWill you join them?", 2,
     {{"Join them", "I went with some friends to {casino}."},
      {"No thanks", ""}}},
    {"ScenarioCustomAttack", "", "Attack", "How will you attack your {victim}?", 1,
     {{"", "I {attackverb} my {victim}'s {attackpart}!"}}},
    // The casino minigames: three modal cards per visit (bets, play, outcome).
    // Only the outcome writes a LifeLog line, and its wording comes from the room
    // table's Game column (REPORT.en.md 42).
    {"ScenarioCasinoBet", "Casino", "", "Place your {casinogame} bets!", 1, {{"", ""}}, true},
    {"ScenarioGameBlackjack", "", "", "Get as close as possible to 21 without going over!\n\nYou have {casinoamount} riding on this bet.",
     1, {{"", ""}}, true},
    {"ScenarioFailingEyesight", "", "Eyesight",
     "Your {parent} is concerned with your eyesight and is taking you for an eye exam at the optometrist.",
     1, {{"", ""}}, true},
    {"ScenarioGameEyeExam", "", "Eye Exam",
     "Start the test and then tap the letter or number that's different than the others before time runs out!",
     1, {{"", ""}}, true},
    {"ScenarioProfileHero", "", "Profile", "", 0, {}, true},
    {"ScenarioEyeExamOutcome", "", "Eye Exam",
     "You {eyeexamverb} your eye exam and {eyeexamclause}", 1,
     {{"", "I {eyeexamverb} my eye exam!"}}},
    {"ScenarioCasinoOutcome", "", "Casino", "You {casinoverb} {casinoamount} {casinotail}.", 1,
     {{"", "I {casinoverb} {casinoamount} {casinotail} at {casino}."}}},
    {"ScenarioEthicsFindObject", "Ethics", "Found Item",
     "While on your walk, you find a {find}.\n\nWhat will you do?", 2,
     {{"Turn it in", "I turned in {findarticle} that I found."},
      {"Keep it", ""}}},
    // ---- The illness chain (REPORT.en.md 43) -------------------------------
    // The symptom card opens it, ScenarioDoctors picks a physician, and
    // ScenarioDoctorTreatments names the diagnosis. Every string here is a C#
    // literal (metadata indices in the comments); the drawn parts are the
    // physician surnames and the diagnosis.
    {"ScenarioDoctors", "Health", "Doctor",
     "Which local doctor will you schedule an appointment to see?", 3,
     {{"Dr. {doctor}", "I made a doctor's appointment."},
      {"Dr. {doctor2}", "I made a doctor's appointment."},
      {"Actually, never mind", ""}}},
    // Captured card and log: "Dr. Seaman has diagnosed you with kidney stones."
    // / "You continue to suffer from hay fever." / "Dr. Seaman treated my kidney
    // stones." (metadata 69923, 70362, 48211). The second sentence names an
    // ailment the hero already carries.
    {"ScenarioDoctorTreatments", "Health", "Diagnosis",
     "Dr. {doctor} has diagnosed you with {diagnosis}.\n\nYou continue to suffer from "
     "{olddisease}.\n\nSelect a treatment today!",
     2,
     {{"Treat me",
       "Dr. {doctor} diagnosed me with {diagnosis}.\nDr. {doctor} treated my {diagnosis}.\n"
       "I continue to suffer from {diagnosis}."},
      {"I'll pass for now",
       "Dr. {doctor} diagnosed me with {diagnosis}.\nI decided not to get treated."}}},
    // Captured card, three labels and the insult branch's line ("I called my
    // supervisor a chode mongrel on my way out.", insult drawn from
    // a16_insults-enc). The other two branches are first-person rewrites of the
    // card sentence.
    // shortcut: the dismissal reason is code-built; only the captured one is
    // modelled.
    {"ScenarioJobTermination", "", "Termination",
     "You have been let go from your position as {job} for {company}.\n\nThey tell you "
     "it's because {reason}.",
     3,
     {{"Insult my supervisor",
       "I was let go from my job as {job} for {company}.\nI called my supervisor a {insult} "
       "on my way out."},
      {"Wish my supervisor well",
       "I was let go from my job as {job} for {company}.\nI wished my supervisor well on my "
       "way out."},
      {"Beg my supervisor to stay",
       "I was let go from my job as {job} for {company}.\nI begged my supervisor to let me "
       "stay."}}},
    // Captured card: "You just observed a firearms activist call your mother
    // foolhardy!" The bystander is a random person with a definite article, the
    // relative is the port's fixed parent word. metadata 69566 is the captured
    // log shape "{0} I forgave {1}." whose first fill is unmeasured, so the
    // branches below are first-person rewrites.
    {"ScenarioInsultRelative", "", "Insult",
     "You just observed {stranger} call your {parent} {insult}!\n\nWhat will you do?", 3,
     {{"Forgive him", "I forgave {stranger}."},
      {"Insult him back", "I insulted {stranger} right back."},
      {"Attack him", "I attacked {stranger}."}}},
    // Fully captured card and both branch lines (metadata 23807 / 24465).
    {"ScenarioCasinoGoldenNekoAd", "Casino", "Golden Neko Statue",
     "Just as you're leaving the casino, you see a little old lady counting the biggest pile "
     "of coins you've ever seen.\n\nShe tells you that ever since she bought a Golden Neko "
     "Statue, her casino winnings have increased by 20%.",
     2,
     {{"Get one of my own",
       "I considered purchasing a Golden Neko Statue to increase my casino winnings."},
      {"Leave the casino",
       "I ignored an opportunity to purchase a Golden Neko Statue to increase my casino "
       "winnings."}}},
    // ---- Menu classes (REPORT.en.md 44) ------------------------------------
    // Cards captured by drive10 (trace_game10.json) and the c5-c10 corpus. A
    // branch label is the option literal, or the outcome card's Title when that
    // is all the capture has (drive10 shows the outcome card title IS the tapped
    // label). A branch the capture never exercised keeps an empty log, which is
    // the port's "no line" convention. Branch order matches tools/row_recover.py
    // REGISTRY so a push's choice index means the same thing on both sides.
    {"ScenarioStepparentCruelty", "Family", "Evil Stepfather",
     "Your {stepparent} pranked you by convincing you that you're invisible.\n\nWhat will you do?",
     2,
     {{"I won't play those games",
       "My {stepparent} convinced me that I'm invisible. I told my mother about his behavior."},
      {"Let it slide", "My {stepparent} convinced me that I'm invisible."}}},
    {"ScenarioSchoolBully", "School", "Bully",
     "Your classmate, {name}, has been telling everyone on social media that you are a "
     "teacher's pet.\n\nWhat will you do?",
     4,
     {{"Report the bully to my teacher",
       "I was bullied at school by my classmate, {name}. I reported the bully to my teacher."},
      {"Do nothing",
       "I was bullied at school by my classmate, {name}. I decided not to do anything about it."},
      {"Fight with a bully",
       "I was bullied at school by my classmate, {name}. I fought the bully and was suspended "
       "from school for four days."},
      {"Find the bully and go at him",
       "I was bullied at school by my classmate, {name}. I found the bully and went at him."}}},
    {"ScenarioSchoolActivityJoin", "School", "{club}",
     "Some friends that you are starting high school with are encouraging you to join the "
     "{club} with them.\n\nWhat will you do?",
     4,
     {{"Try out for the {club}", "I tried out for my {club} but did not make the cut."},
      {"", "I turned down the opportunity to join the {club} at my new high school."},
      {"", "I thought about joining an extracurricular activity at my new high school but "
           "decided against it."},
      {"Join the {club}", "I joined the {club} at my high school."}}},
    {"ScenarioEncounterProstitute", "", "Working Girl",
     "While on your walk, you encounter a working girl sitting on the curb.\n\nWhat will you do?",
     3,
     {{"Solicit {name}", "I solicited {name} that I met on the street."},
      {"", "I ignored a working girl who was sitting on the curb."},
      {"", "I verbally abused a hooker who was winking at me and she asked for forgiveness."}}},
    {"ScenarioFriendUnfriendInbound", "Friends", "Unfriended",
     "Your friend, {name}, has unfriended you.\n\nWhat will you do?",
     3,
     {{"It's not me it's you",
       "{name} unfriended me.\nI tried to salvage our relationship but was unsuccessful."},
      {"Done for good", "{name} unfriended me.\nI called him mentally defective."},
      {"Unfriend {name}", "I unfriended {name}."}}},
    {"ScenarioLoverLeaves", "Love", "Break Up",
     "Your girlfriend, {name}, wants to break up with you.\n\nShe says the alcohol has become "
     "a problem.\n\nWhat will you do?",
     2,
     {{"No shame", "{name} broke up with me. I begged her to stay and she changed her mind."},
      {"", "{name} broke up with me."}}},
    {"ScenarioLoveDate", "Love", "Love Interest",
     "While at the {venue}, you meet a {lady} from {countryother} named {cfirst}.",
     1,
     {{"True love", "I am now dating {cfirst} {clast}."}}},
    {"ScenarioSocialMediaJoinPush", "Social", "{channelbare}",
     "Your friends are all joining {channelbare}.\n\nWhat will you do?",
     2,
     {{"Sign up", "I signed up for {channel} account."}, {"", ""}}},
    {"ScenarioCarRepair", "", "{car}",
     "Your car has a worn ball joint.\n\nWhat will you do?",
     1,
     {{"Pick a mechanic",
       "My {car} has a worn ball joint. I paid a shifty mechanic to fix it."}}},
    {"ScenarioPetDiscipline", "Pets", "Behavior",
     "Your {pet}, {petname}, just ate your {food}.\n\nWhat will you do?",
     2,
     {{"Isn't she cute?",
       "My {pet}, {petname}, ate my {food}. I rewarded her with a Porterhouse steak."},
      {"", "My {pet}, {petname}, ate my {food}. I scolded her."}}},
    {"ScenarioPushSpendTime", "Family", "Quality Time",
     "Your mother wants to take you to play bingo at the community center.\n\nWill you go with "
     "her?",
     2,
     {{"Good times", "I went to play bingo at the community center with my mother."}, {"", ""}}},
    {"ScenarioIAPConfiguredPush", "", "Lights, Camera, Action",
     "While trimming the bush, you stumble upon a film set, where Greta Gerwig is directing "
     "Elton John and Emma Stone in a new movie, and you start dreaming of becoming a great "
     "movie director yourself.\n\nWhat will you do?",
     2,
     {{"Not interested", "I chose not to learn about becoming a movie director."}, {"", ""}}},
    {"ScenarioRetentionFreelanceGig", "", "Freelance Gig",
     "You're looking at your bank account and are thinking of picking up a freelance gig to get "
     "some extra cash.\n\nWhat will you do?",
     2,
     {{"", "I decided against picking up a freelance gig."},
      {"Okay, moneybags",
       "Any time your pockets are feeling empty, you can pick up a freelance gig by tapping the "
       "left-most button on the home screen, then the \"Freelance Gigs\" menu item."}}},
    {"ScenarioRetentionLover", "", "Love in the Air",
     "You're noticing that everyone you know is coupled up with someone, but you're not.",
     1,
     {{"", "Any time you're feeling lonely, you can find love by tapping the \"Activities\" "
           "button on the home screen, then the \"Love\" menu item."}}},
    {"ScenarioMediaOnlyFansArguePush", "", "Argument",
     "Your girlfriend is arguing with you because she discovered your OnlyFans page and is "
     "begging you to take it down.\n\nWhat will you do?",
     1,
     {{"Apologize", "My girlfriend argued with me because I create content for OnlyFans.\n"
                    "I apologized to her."}}},
    {"ScenarioBase", "", "Evaders of the Lost Ark",
     "You received a script for a movie called 'Evaders of the Lost Ark,' and must decide who "
     "should play the lead.\n\nIt's a film about an archeology professor who is hired to find "
     "the Ark of the Cavemen before an evil organization retrieves what's inside it: an artifact "
     "that can alter the course of history.\n\nWhat will you do?",
     1,
     {{"Make an action movie",
       "I got my shot to direct a film and chose to make an action movie."}}},
    {"ScenarioSchoolEnrollActivity", "School", "Extracurricular Activity",
     "Now that you're in high school, you are thinking about participating in an extracurricular "
     "activity.\n\nWhat will you do?",
     2,
     {{"", "I thought about joining an extracurricular activity at my new high school but "
           "decided against it."},
      {"Join the {club}", "I joined the {club} at my high school."}}},
};
// Real-world date gates for the two dated dumb-scenario sets. Holiday rows name
// the holiday; calendar rows carry 'Month' + 'Date' (one real day each).
int monthNumber(const std::string& name) {
  static const char* kNames[12] = {"january", "february", "march",     "april",
                                   "may",     "june",     "july",      "august",
                                   "september", "october", "november", "december"};
  const std::string m = lower(trim(name));
  if (m.empty()) return 0;
  for (int i = 0; i < 12; ++i)
    if (m.rfind(kNames[i], 0) == 0) return i + 1;
  return 0;
}

bool dumbDateMatches(const Table& t, size_t row, int month, int day) {
  if (t.has("Type")) {  // holiday set
    static const struct { const char* name; int month; } kHoliday[] = {
        {"Halloween", 10}, {"Thanksgiving", 11}, {"Christmas", 12}, {"New Years", 1}};
    const std::string type = trim(t.at(row, "Type"));
    for (const auto& h : kHoliday)
      if (type == h.name) return h.month == month;
    return false;
  }
  if (t.has("Month"))  // calendar set: one row per real day of the year
    return monthNumber(t.at(row, "Month")) == month && int(t.num(row, "Date")) == day;
  return true;
}

const RegScenario* findScenario(const std::string& cls) {
  for (const RegScenario& s : kScenarios)
    if (cls == s.cls) return &s;
  return nullptr;
}
// The registry covers only the scenes whose text has been captured, and none of
// their stat impacts are measured yet, so every branch lands with zero deltas.
// shortcut: registry deltas are 0 and the parent word is fixed to "mother"
// (every capture so far), upgrade when a parent object exists in the port.
void substAll(std::string& text, const std::string& token, const std::string& value) {
  for (size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + value.size()))
    text.replace(at, token.size(), value);
}

// The scripted-scenario table (gamedata_plain/scenario1) writes the hero's
// family as the game's own object tokens. The port keeps no family graph yet, so
// <parent> reads as the mother and <him/her> keeps her gender.
// shortcut: single-parent stand-in; upgrade when Life.Father/Mother exist.
std::string scriptedFamilyTokens(std::string text) {
  substAll(text, "<parent>", "mother");
  substAll(text, "<him/her>", "her");
  substAll(text, "<his/her>", "her");
  return text;
}

size_t weightedPickOf(const Table& t, const std::string& col,
                      const std::vector<size_t>& pool, Rng& rng) {
  if (pool.empty()) return size_t(-1);
  double total = 0;
  for (size_t r : pool) total += t.num(r, col) > 0 ? t.num(r, col) : 0;
  if (total <= 0) return pool[rng.below(uint32_t(pool.size()))];
  double x = rng.unit() * total;
  for (size_t r : pool) {
    x -= t.num(r, col) > 0 ? t.num(r, col) : 0;
    if (x <= 0) return r;
  }
  return pool.back();
}

// The star sign for a date. The game has no zodiac table for this: the sign is
// derived from the birthday, so the port derives it too.
const char* zodiacOf(int month, int day) {
  static const int kFirstDay[12] = {20, 19, 21, 20, 21, 21, 23, 23, 23, 23, 22, 22};
  static const char* kSign[12] = {"Aquarius", "Pisces",  "Aries",      "Taurus",
                                  "Gemini",   "Cancer",  "Leo",        "Virgo",
                                  "Libra",    "Scorpio", "Sagittarius", "Capricorn"};
  const int m = month < 1 ? 1 : (month > 12 ? 12 : month);
  return day < kFirstDay[m - 1] ? kSign[(m + 10) % 12] : kSign[m - 1];
}

void splitOn(const std::string& s, char sep, std::vector<std::string>& out) {
  std::string cur;
  for (char c : s) {
    if (c == sep) { out.push_back(cur); cur.clear(); }
    else cur += c;
  }
  out.push_back(cur);
}

}  // namespace

Sim::Sim(std::string reDir, uint64_t seed)
    : reDir_(std::move(reDir)),
      rng_(seed),
      prisonRng_(seed ^ 0x9e3779b97f4a7c15ULL),
      familyRng_(seed ^ 0x51ed270b2f5a1c31ULL),
      royalRng_(seed ^ 0x2545f4914f6cdd1dULL),
      vampireRng_(seed ^ 0x27d4eb2f165667c5ULL),
      militaryRng_(seed ^ 0x3b9aca07c2b2ae35ULL),
      careerRng_(seed ^ 0x27d4eb2f165667c5ULL ^ 0x9e3779b97f4a7c15ULL),
      sportsRng_(seed ^ 0x94d049bb133111ebULL),
      bizRng_(seed ^ 0xbf58476d1ce4e5b9ULL),
      musicRng_(seed ^ 0x2545f4914f6cdd1dULL ^ 0xa0761d6478bd642fULL),
      politicsRng_(seed ^ 0x9e3779b97f4a7c15ULL ^ 0xc2b2ae3d27d4eb4fULL),
      miscRng_(seed ^ 0xd6e8feb86659fd93ULL),
      relRng_(seed ^ 0x1b873593e6546b64ULL),
      spyRng_(seed ^ 0x7f4a7c15d1b54a3dULL),
      fightRng_(seed ^ 0x2545f4914f6cdd1dULL),
      bmRng_(seed ^ 0x8f1bbcdc9e3779b1ULL),
      casinoRng_(seed ^ 0xd1b54a32d192ed03ULL),
      cultRng_(seed ^ 0xa24baed4963ee407ULL),
      dealerRng_(seed ^ 0x9ddfea08eb382d69ULL),
      mafiaRng_(seed ^ 0x27d4eb2f165667c5ULL),
      zooRng_(seed ^ 0x3c79ac492ba7b653ULL),
      landlordRng_(seed ^ 0x6a09e667f3bcc909ULL),
      petRng_(seed ^ 0xbb67ae8584caa73bULL),
      studioRng_(seed ^ 0x3c6ef372fe94f82bULL),
      tradeRng_(seed ^ 0xd6e8feb86659fd93ULL),
      outdoorRng_(seed ^ 0xa0761d6478bd642fULL),
      hustlerRng_(seed ^ 0x9e3779b97f4a7c15ULL),
      astroRng_(seed ^ 0xbf58476d1ce4e5b9ULL) {}

const Table& Sim::tbl(const char* name) const {
  for (const auto& entry : tables_) if (entry.first == name) return entry.second;
  throw std::out_of_range(std::string("table not loaded: ") + name);
}

const Table* Sim::table(const std::string& name) const {
  for (const auto& entry : tables_) if (entry.first == name) return &entry.second;
  return nullptr;
}

bool Sim::load(std::string* error) {
  // The five real tables the loop is wired to. Names are verbatim file names
  // from _re/gamedata_plain/ (indexed in _re/gamedata_plain_index.json).
  static const char* kTables[] = {
    "a16_dumb-scenarios-mafia-enc",
    "a16_dumb-scenarios-dealer-enc",
    "a16_dumb-scenarios-hustler-enc",
    "a16_dumb-scenarios-landlord-enc",
    "a16_dumb-scenarios-zoo-enc",
    "a16_dumb-scenarios-model-enc",
    "a16_dumb-scenarios-actor-enc",
    "a16_dumb-scenarios-secret-agent-enc",
    "a16_dumb-scenarios-casino-enc",
    "a16_mafia-mobs-enc",
    "a16_mafia-nicknames-enc",
    "a16_mafia-calling-cards-enc",
    "a16_mafia-excuses-enc",
    "a16_mafia-executions-enc",
    "a16_mafia-extortion-targets-enc",
    "a16_mafia-feedback-earnings-enc",
    "a16_mafia-feedback-negative-enc",
    "a16_mafia-feedback-positive-enc",
    "a16_mafia-join-methods-enc",
    "a16_mafia-join-rejections-enc",
    "a16_mafia-pastimes-enc",
    "a16_mafia-push-crimes-enc",
    "a16_mafia-push-targets-enc",
    "a16_mafia-scare-methods-enc",
    "a16_mafia-shakedown-methods-enc",
    "a16_mafia-temptation-scenarios-enc",
    "a16_dealer-cartel-name-enc",
    "a16_hustler-collection-boxes-enc",
    "a16_hustler-busk-music-types-enc",
    "a16_hustler-panhandle-strategies-enc",
    "a16_landlord-home-default-features-enc",
    "a16_house-types-enc",
  "a16_landlord-amenity-feedback-scenario-enc",
  "a16_landlord-background-check-color-enc",
  "a16_landlord-home-remodel-feature-enc",
  "a16_landlord-maintenance-refusal-scenario-enc",
  "a16_landlord-maintenance-request-scenario-enc",
  "a16_landlord-natural-disaster-scenario-enc",
  "a16_landlord-real-estate-portfolio-enc",
  "a16_landlord-spooked-contractor-quit-color-enc",
  "a16_landlord-squatter-push-scenario-enc",
  "a16_landlord-squatter-reason-color-enc",
  "a16_landlord-tenant-beg-lower-rent-scenario-enc",
  "a16_landlord-tenant-beg-to-stay-scenario-enc",
  "a16_landlord-tenant-damaged-property-scenario-enc",
  "a16_landlord-tenant-employment-check-enc",
  "a16_landlord-tenant-inspection-color-enc",
  "a16_landlord-tenant-late-rent-color-enc",
  "a16_landlord-tenant-late-rent-insist-enc",
  "a16_landlord-tenant-leave-color-enc",
  "a16_landlord-tenant-lover-nags-hero-enc",
  "a16_landlord-tenant-warn-scenario-enc",
    "a16_street-names-enc",
    "a16_zoo-animals-enc",
    "a16_zoo-habitat-names-enc",
    "a16_zoo-animal-attack-actions-enc",
    "a16_zoo-animal-death-scenarios-enc",
    "a16_zoo-animal-diseases-enc",
    "a16_zoo-animal-grooming-methods-enc",
    "a16_zoo-animal-names-enc",
    "a16_zoo-animal-show-enc",
    "a16_zoo-animal-show-feedback-enc",
    "a16_zoo-animal-sold-color-enc",
    "a16_zoo-animal-spend-time-color-enc",
    "a16_zoo-animal-trafficker-persons-enc",
    "a16_zoo-animal-treat-color-enc",
    "a16_zoo-attraction-installation-color-enc",
    "a16_zoo-attraction-removal-color-enc",
    "a16_zoo-attractions-enc",
    "a16_zoo-buyer-person-types-enc",
    "a16_zoo-comment-card-color-enc",
    "a16_zoo-employee-application-color-enc",
    "a16_zoo-employee-background-check-color-enc",
    "a16_zoo-employee-resignation-color-enc",
    "a16_zoo-fundraiser-color-enc",
    "a16_zoo-fundraiser-enc",
    "a16_zoo-gift-shop-item-enc",
    "a16_zoo-habitat-clean-enc",
    "a16_zoo-habitat-features-enc",
    "a16_zoo-habitat-feedback-enc",
    "a16_zoo-habitat-move-in-feedback-enc",
    "a16_zoo-habitat-type-enc",
    "a16_zoo-random-zoo-names-enc",
    "a16_zoo-street-names-enc",
    "a16_zoo-supplier-names-enc",
    "a16_zoo-trade-accept-color-enc",
    "a16_zoo-trade-animal-sold-color-enc",
    "a16_zoo-trade-rejection-reasons-enc",
    "a16_model-agency-names-enc",
    "a16_actor-films-enc",
    "a16_actor-shows-enc",
    "a16_actors-real-enc",
    "a16_secret-agent-agency-fronts-enc",
    "a16_casino-casino-game-room-enc",
    "a16_casino-random-hero-casino-name-enc",
    "a16_dumb-scenarios-political-campaign-enc",
    "a16_dumb-scenarios-political-office-enc",
    "a16_country-political-systems-enc",
    "a16_dumb-scenarios-album-enc",
    "a16_dumb-scenarios-music-enc",
    "a16_music-names-bands-enc",
    "a16_music-names-songs-enc",
    "a16_music-names-albums-enc",
    "a16_music-genres-enc",
    "a16_music-album-covers-enc",              // Cover + one x flag column per genre
    "a16_music-album-rejections-enc",          // Reason,Solo,Band,Album Type
    "a16_music-album-title-enc",               // Title,Genre,Commonness Score
    "a16_music-artistic-flair-string-enc",     // String,Log Version,Genre,Reception Impact Score
    "a16_music-audition-rejections-enc",       // Reason,Solo,Band,Role
    "a16_music-band-breaks-up-scenarios-enc",  // Reason,Min/Max Popularity
    "a16_music-band-kicks-out-scenarios-enc",  // Reason,Role,Performance Based,Hero Min/Max Age
    "a16_music-concert-venues-enc",            // Venue,Min/Max Capacity,Min/Max Popularity,Money Score
    "a16_music-cover-art-string-enc",          // String,Genre (a comma list on the row)
    "a16_music-fan-interaction-scenarios-enc", // Positive,Negative,Scenario,Feedback Snippet,...
    "a16_music-names-compilations-enc",        // Title,Commonness Score,Accommodates Definite Article
    "a16_music-practice-scenarios-enc",        // Scenario,Log Version,Role
    "a16_music-record-label-company-name-enc", // Name,Genre,Commonness Score
    "a16_music-roles-enc",                     // Role,Solo,Band,Solo/Band Fame Score,...
    "a16_music-sample-track-enc",              // Song,Genre,Commonness Score
    "a16_music-signature-sound-enc",           // Sound,Use Indefinite Article,Reception Impact Score,Commonness Score
    "a16_music-subgenres-enc",                 // Subgenre + one x flag column per genre
    "a16_music-track-beat-enc",                // Beat,Genre,Commonness Score
    "a16_music-track-lyrical-hook-enc",        // Hook,Genre,Commonness Score
    "a16_music-track-rejection-string-enc",    // String,Log Version,Reason,Commonness Score
    "a16_music-track-subject-enc",             // Subject,Genre,Commonness Score
    "a16_music-track-title-enc",               // Track Title,Genre,Commonness Score
    "a16_music-track-vibe-enc",                // Vibe,Genre,Commonness Score
    // The studio trades (REPORT.en.md 69).
    "a16_music-artist-stage-name-enc",         // Stage Name,Gender,Skin,Genre,Commonness Score
    "a16_music-names-stage-enc",               // Title + one x flag column per genre
    "a16_music-producer-concert-act-enc",      // Name
    "a16_music-producer-stage-name-enc",       // Name,Hero Gender,Commonness Score
    "a16_music-producer-genre-enc",            // Genre,Compatible Genre 1-3,Predominant Country
    "a16_music-producer-titles-enc",           // Scenario Tag,Option 1-5,Situation Description
    "a16_music-producer-dumb-scenario-enc",    // a year-up prompt card with 4 choices
    "a16_music-producer-aspiring-artist-string-enc",  // String,Artist Genre,Min/Max Vocal Quality
    "a16_music-producer-artist-leave-string-enc",     // String,Min/Max Artist Popularity
    "a16_music-producer-termination-string-enc",      // String,Commonness Score
    "a16_music-soundcloud-screen-name-enc",    // Name,Hero Only,Gender,Commonness Score
    "a16_music-soundcloud-comment-enc",        // String,Log Version,Track Genre,Min/Max Popularity
    "a16_music-soundcloud-insult-enc",         // Insult,Track Genre,Commonness Score
    "a16_music-soundcloud-collab-refusal-string-enc", // String,Reason,Commonness Score
    "a16_music-soundcloud-track-instrument-enc",      // Instrument,Genre,Project Tones
    "a16_music-soundcloud-track-reception-string-enc",// String,Collab,Genre,Min/Max Reception
    "a16_music-soundcloud-dumb-scenario-enc",  // a year-up prompt card with 4 choices
    "a16_music-talent-scout-venue-name-enc",   // Venue,Event Type,Commonness Score
    "a16_music-talent-scout-potential-talent-string-enc", // String,Artist Genre,Min/Max Vocal Quality
    "a16_music-talent-scout-artist-rejection-string-enc", // String,Reason,Commonness Score
    "a16_music-score-production-project-title-enc", // Name,Project Type,Commonness Score
    "a16_music-score-production-project-tone-enc",  // Tone,Min BPM,Max BPM
    "a16_music-score-production-acceptance-string-enc", // String,Project Type
    "a16_music-score-production-rejection-string-enc",  // String,Reason,Project Type
    "a16_music-club-takeover-stunt-enc",       // Scenario,Log Version,Artist Genre,Popularity Impact
    "a16_music-club-takeover-stunt-outcome-enc", // Scenario,Log Version,Positive
    "a16_music-club-takeover-string-enc",      // Scenario,Log Version,Positive,Popularity Impact
    "a16_music-club-takeover-second-thought-enc", // String,Artist Gender,Artist Genre
    "a16_music-instruments-enc",               // Instrument,Singular Name,Category,Min/Max Cost,...
    "a16_music-dinner-location-enc",           // String,Gratitude
    "a16_dumb-scenarios-business-enc",
    "a16_business-name-structure-enc",
    "a16_business-products-enc",
    "a16_business-industries-enc",
    "a16_sports-enc",
    "a16_sports-leagues-enc",
    "a16_sports-teams-enc",
    "a16_sports-positions-enc",
    "a16_dumb-scenarios-sports-enc",
    "a16_military-ranks-enc",
    "a16_war-scenarios-enc",
      "a16_casino-casino-celebrity-name-enc",        // First Name,Last Name,Gender,Skin,Commonness Score
      "a16_dumb-scenarios-universal-enc",            // Index,Hero Min Age,Hero Max Age,...,Choice N,Narrative N,*Impact N
      // The rest of the yearly dumb-scenario pool: the career set applies to an
      // employed hero, the holiday and calendar sets are real-date gated
      // (Sim::setDate). Same column layout as the universal table.
      "a16_dumb-scenarios-career-enc",
      "a16_dumb-scenarios-holiday-enc",
      "a16_dumb-scenarios-calendar-enc",
      "a16_hero-filler-enc",                         // Condition,String,Hero Min Age,Hero Max Age,Hero Gender,...
      "a16_employee-recognition-scenarios-enc",      // Scenario,Commonness Score
      "a16_death-types-enc",                         // Type,Commonness Score,Min Age,Sentence
      // The age-0 birth lines (Sim::birthLines).
      "a16_birth-circumstances-enc",                 // Circumstance,Commonness Score,Min/Max Mother Age
      "a16_careers-enc",                             // Career,Commonness Score,...
      "a16_pets-enc",                                // Animal,Breed,Wild,Exotic,Black Market,Commonness Score
      "a16_pet-names-enc",                           // Animal,Name,Color,Gender
  "a16_pet-death-types-enc",
  "a16_pet-pastimes-enc",
  "a16_pet-treats-enc",
  "a16_pet-releases-enc",
  "a16_pet-release-filler-enc",
  "a16_pet-disease-types-enc",
  "a16_pet-found-dead-scenarios-enc",
      "a16_car-models-enc",                          // Make,Model,Type,Cost,... (ScenarioCarRepair)
      "a16_dumb-scenarios-vacation-enc",             // same layout; a coverage push targets it
      "a16_prisoner-crimes-enc",                     // Crime,Seriousness Score,Is Violent,Commonness Score
      "a16_prisoner-juvenile-crimes-enc",            // same columns, juvenile court
      "a16_prison-jobs-enc",                         // Job,Description,Daily Rate,Commonness Score
      "a16_dumb-scenarios-prison-enc",               // the prison year's own pool
      "a16_dumb-scenarios-orphan-enc",               // the orphanage's own pool
      "a16_dumb-scenarios-royal-enc",                // the royal family's own pool (REPORT.en.md 47)
      "a16_country-royal-systems-enc",               // Country,Rank 1 Male/Female,...,Seats
      "a16_royal-house-names-enc",                   // Name,Estate,Palace,Castle,Country Code,Commonness Score
      "a16_vampire-dumb-scenario-enc",               // the vampire's own pool (REPORT.en.md 48)
      "a16_vampire-name-enc",                        // Type,Name,Skin: female / male / surname
      "a16_vampire-assault-method-enc",
      "a16_vampire-blood-bath-scenario-enc",
      "a16_vampire-blood-offering-string-enc",
      "a16_vampire-blood-pact-rejection-string-enc",
      "a16_vampire-blood-taste-color-enc",
      "a16_vampire-body-dump-color-enc",
      "a16_vampire-clubbing-approache-enc",
      "a16_vampire-code-data-enc",
      "a16_vampire-coffin-string-enc",
      "a16_vampire-council-discipline-scenario-enc",
      "a16_vampire-council-party-string-enc",
      "a16_vampire-council-party-type-enc",
      "a16_vampire-council-punishment-enc",
      "a16_vampire-council-reward-enc",
      "a16_vampire-disease-enc",
      "a16_vampire-entice-method-enc",
      "a16_vampire-familiar-color-enc",
      "a16_vampire-familiar-command-enc",
      "a16_vampire-familiar-confused-string-enc",
      "a16_vampire-familiar-dump-enc",
      "a16_vampire-familiar-threat-enc",
      "a16_vampire-feeding-location-color-enc",
      "a16_vampire-feeding-location-enc",
      "a16_vampire-hunt-encounter-color-enc",
      "a16_vampire-hunt-string-enc",
      "a16_vampire-hunter-appreciation-string-enc",
      "a16_vampire-hunter-encounter-scenario-enc",
      "a16_vampire-hunter-name-enc",
      "a16_vampire-hypnosis-animal-scenario-enc",
      "a16_vampire-hypnosis-breakup-string-enc",
      "a16_vampire-hypnosis-love-string-enc",
      "a16_vampire-hypnosis-money-string-enc",
      "a16_vampire-insult-enc",
      "a16_vampire-lover-convert-nag-enc",
      "a16_vampire-solo-hunt-location-enc",
      "a16_vampire-spend-time-enc",
      "a16_vampire-spend-time-hunger-color-enc",
      "a16_vampire-street-name-enc",
      "a16_vampire-titles-enc",
      "a16_vampire-vcoli-string-enc",
      "a16_vampire-victim-status-color-enc",
      "a16_vampire-witch-doctor-remedie-enc",
      "a16_outdoor-animal-enc",
      "a16_outdoor-animal-treat-enc",
      "a16_outdoor-bike-ride-dumb-enc",
      "a16_outdoor-bike-ride-location-enc",
      "a16_outdoor-bird-enc",
      "a16_outdoor-bird-behavior-enc",
      "a16_outdoor-bird-discovery-scenario-enc",
      "a16_outdoor-bird-treat-enc",
      "a16_outdoor-camping-dog-treat-enc",
      "a16_outdoor-camping-dumb-scenario-enc",
      "a16_outdoor-catchable-fish-enc",
      "a16_outdoor-cave-enc",
      "a16_outdoor-cave-discoverie-enc",
      "a16_outdoor-destination-enc",
      "a16_outdoor-dog-behavior-enc",
      "a16_outdoor-fishing-treasure-enc",
      "a16_outdoor-ghost-encounter-enc",
      "a16_outdoor-hallucination-enc",
      "a16_outdoor-hallucination-symptom-scenario-enc",
      "a16_outdoor-hiking-dog-discovery-scenario-enc",
      "a16_outdoor-hiking-dog-fetch-item-enc",
      "a16_outdoor-hiking-drink-enc",
      "a16_outdoor-hiking-dumb-scenario-enc",
      "a16_outdoor-hiking-item-discoverie-enc",
      "a16_outdoor-hooked-fish-color-string-enc",
      "a16_outdoor-landmark-memento-scenario-enc",
      "a16_outdoor-landmark-selfie-color-enc",
      "a16_outdoor-landmark-view-reflection-enc",
      "a16_outdoor-lure-fish-color-string-enc",
      "a16_outdoor-marine-behavior-enc",
      "a16_outdoor-marine-life-enc",
      "a16_outdoor-marine-life-play-string-enc",
      "a16_outdoor-mounted-fish-name-enc",
      "a16_outdoor-offroading-encounter-enc",
      "a16_outdoor-offroading-location-color-string-enc",
      "a16_outdoor-offroading-make-love-string-enc",
      "a16_outdoor-offroading-reward-enc",
      "a16_outdoor-offroading-trick-enc",
      "a16_outdoor-plant-enc",
      "a16_outdoor-snorkeling-discovery-scenario-enc",
      "a16_outdoor-titles-enc",
      "a16_outdoor-vehicle-enc",
      // The hustle (REPORT.en.md 72): 22 tables.
      "a16_hustler-attire-enc",
      "a16_hustler-busk-music-types-enc",
      "a16_hustler-busk-negative-scenarios-enc",
      "a16_hustler-busk-positive-scenarios-enc",
      "a16_hustler-busk-requests-enc",
      "a16_hustler-collection-boxes-enc",
      "a16_hustler-insults-enc",
      "a16_hustler-panhandle-confrontation-scenarios-enc",
      "a16_hustler-panhandle-excuses-enc",
      "a16_hustler-panhandle-food-enc",
      "a16_hustler-panhandle-items-enc",
      "a16_hustler-panhandle-props-enc",
      "a16_hustler-panhandle-signs-enc",
      "a16_hustler-panhandle-strategies-enc",
      "a16_hustler-perform-acts-enc",
      "a16_hustler-perform-disrespect-scenarios-enc",
      "a16_hustler-perform-respect-scenarios-enc",
      "a16_hustler-police-scenarios-enc",
      "a16_hustler-scam-items-enc",
      "a16_hustler-scam-types-enc",
      "a16_hustler-scam-victim-scenarios-enc",
      "a16_hustler-serenade-songs-enc",
      // The space program (REPORT.en.md 73): 35 tables.
      "a16_astronaut-agency-director-warning-enc",
      "a16_astronaut-anxiety-scenarios-enc",
      "a16_astronaut-artifacts-enc",
      "a16_astronaut-asteroid-data-enc",
      "a16_astronaut-black-hole-data-enc",
      "a16_astronaut-career-rank-enc",
      "a16_astronaut-celestial-body-names-enc",
      "a16_astronaut-comet-data-enc",
      "a16_astronaut-conversations-enc",
      "a16_astronaut-countries-enc",
      "a16_astronaut-dumb-scenarios-enc",
      "a16_astronaut-experiment-scenarios-enc",
      "a16_astronaut-fitness-training-scenario-enc",
      "a16_astronaut-flight-scenario-enc",
      "a16_astronaut-galaxy-data-enc",
      "a16_astronaut-inbound-messages-enc",
      "a16_astronaut-lunar-missions-enc",
      "a16_astronaut-mars-destinations-enc",
      "a16_astronaut-martian-missions-enc",
      "a16_astronaut-mission-dumb-scenarios-enc",
      "a16_astronaut-mission-goof-off-enc",
      "a16_astronaut-moon-destinations-enc",
      "a16_astronaut-nebula-shapes-enc",
      "a16_astronaut-outbound-messages-enc",
      "a16_astronaut-peer-review-color-enc",
      "a16_astronaut-planet-data-enc",
      "a16_astronaut-pranks-enc",
      "a16_astronaut-probe-names-enc",
      "a16_astronaut-publication-titles-enc",
      "a16_astronaut-satellite-names-enc",
      "a16_astronaut-satellite-parts-enc",
      "a16_astronaut-scanner-observations-enc",
      "a16_astronaut-shuttle-names-enc",
      "a16_astronaut-star-data-enc",
      "a16_astronaut-training-questions-enc",
      "a16_vacation-destinations-enc",               // Destination,Context,Country,...
      "a16_cruises-enc",                             // Type,Continent,Cost Score,Appeal Score
      // The lover family (REPORT.en.md 57): one pool per stage of a relationship.
      "a16_date-rejections-enc",                     // Condition,Scenario,..,Hero Min Smarts/Looks
      "a16_hookup-scenarios-enc",                    // Condition,Scenario,Gender,Hero Gender,Type
      "a16_hookup-feedback-enc",                     // Condition,Feedback,Partner Gender,Min/Max Regret
      "a16_make-love-rejections-enc",                // Condition,Scenario,..,Lover Min/Max Age
      "a16_lover-suspicious-scenarios-enc",          // Scenario,..,Has Job,Has Kids
      "a16_lover-cheat-scenarios-enc",               // Scenario,Log Version,Hero/Lover Min/Max Age
      "a16_lover-make-love-nags-enc",                // Scenario,..,Married,Min/Max Relationship
      "a16_lover-marriage-nags-enc",                 // Scenario,..,Min/Max Relationship
      "a16_anniversary-date-enc",                    // Activity,Feedback String,Log String,Cost
      "a16_breakup-scenarios-enc",                   // Condition,Crazy,Scenario,Gender,Straight Only
      "a16_ex-lover-rekindle-scenarios-enc",         // Scenario,..,Ex Gender,Min/Max Age
      "a16_ex-lover-hate-scenarios-enc",             // Scenario,Hero Gender,Ex Gender
      "a16_ex-lover-booty-call-scenarios-enc",       // Scenario,..,Min/Max Age,Clean
      "a16_ex-lover-stalking-scenarios-enc",         // Outbound/Inbound Scenario,Log Versions
      "a16_relationship-counseling-scenarios-enc",   // Feedback,Log Version,Max Relationship
      "a16_parent-lover-disapprovals-enc",           // Scenario,Log Version,Min/Max Smarts,..
      "a16_wedding-venues-enc",                      // Venue,Uses Definite Article,Cost Score
      "a16_school-classmate-flirt-scenarios-enc",     // Feedback Scenario,Log Version
      "a16_friend-lover-scenarios-enc",              // Scenario,Log Version,Hero/Friend Min/Max Age
      "a16_coworker-hookup-scenarios-enc",           // Inbound/Outbound Scenario,Sender Gender
      "a16_disease-types-enc",                       // Disease,..: '<disease>' in the lover pools
      "a16_secret-agent-random-hero-agency-names-enc",  // Name,..: the hero's agency
      "a16_secret-agent-code-names-enc",             // Code Name,Gender,..: '<code-name>'
      "a16_secret-agent-potential-agent-color-enc",  // String,Specialization,..: '<specialization>'
      "a16_secret-agent-mission-target-people-enc",  // Target,..,Stealth/Assassination/Honeypot,..
      "a16_secret-agent-mission-summary-feedback-enc",  // String,Mission Type,Success/Failure Result,..
      "a16_secret-agent-mission-code-names-enc",     // Adjectives,Nouns: 'Operation <adj> <noun>'
      "a16_secret-agent-assassination-methods-enc",  // Method,Success String,Failure String,..
      "a16_secret-agent-stealth-item-targets-enc",   // Target,Plural,Verbs,Risk,..
      "a16_secret-agent-stealth-mission-locations-enc",  // Location,Uses Definite/Indefinite Article,..
      "a16_secret-agent-hack-mission-file-name-enc",  // Name,File Type,..
      "a16_secret-agent-hack-drive-names-enc",       // Name,..: the drive the file sits on
      "a16_secret-agent-honeypot-target-preferences-enc",  // Preference,Persona,Fetish,..
      "a16_secret-agent-honeypot-agent-disguises-enc",  // Disguise,Gender,Persona Tags
      "a16_secret-agent-honeypot-meeting-locations-enc",  // Location,..
      "a16_secret-agent-assassination-locations-enc",  // Location,..
      "a16_secret-agent-agency-gadgets-enc",         // Name,Specialization,..: '<gadget>'
      "a16_secret-agent-infiltration-passwords-enc",  // Password,Hints,..: '<password>'
      "a16_secret-agent-agency-front-names-enc",     // Business Type,Format,Prefix,Suffix
      "a16_secret-agent-agency-fronts-enc",          // Type,..,Starter Front: the front list
      "a16_secret-agent-train-scenarios-enc",        // Specialization,Scenario,Log Version,..
      "a16_secret-agent-resignation-reasons-enc",    // Snippet,Gender,Min/Max Proficiency,..: the agent quits
      "a16_secret-agent-assassination-client-reasons-enc",  // Snippet,Commonness Score: why the client pays
      "a16_secret-agent-assassination-mission-methods-enc",  // Method,Effectiveness,Risk,Success/Failure Snippet
      "a16_secret-agent-honeypot-agent-approach-style-enc",  // Approach,Commonness Score
      "a16_secret-agent-honeypot-sting-locations-enc",  // String,Commonness Score: where the sting lands
      "a16_secret-agent-stealth-obstacle-stories-enc",  // Snippet,Effectiveness,Commonness: the cover story
      "a16_secret-agent-stealth-obstacle-assassinations-enc",  // Fail Verb,Success Verb,..: a guard who wakes up
      "a16_secret-agent-warnings-enc",               // Scenario,Risk Score,Commonness Score: telling a colleague
      "a16_secret-agent-warn-agent-retorts-enc",     // Scenario,Min/Max Proficiency: the answer
      // The fight career (report 59): arenas, opponents banded by fighter level,
      // coaches, injuries, fan interaction, interviews, sponsorships, press
      // conferences, street fights and PEDs. The colour pools
      // (fight-coach-quality-color-string, fight-doping-challenge-color-string,
      // fight-ing-style-identity-color, fight-ing-wildounter-color,
      // fight-outcome-color, fight-wildounter-color) are stored for the level and
      // style values, not for a colour channel the port does not have.
      "a16_fight-ing-style-identity-color-enc",
      "a16_fight-ing-arena-name-enc",
      "a16_fight-ing-opponent-type-enc",
      "a16_fight-ing-win-reason-enc",
      "a16_fight-ing-loss-reason-enc",
      "a16_fight-outcome-color-enc",
      "a16_fight-ing-coach-training-string-enc",
      "a16_fight-coach-generic-feedback-string-enc",
      "a16_fight-coach-loss-feedback-string-enc",
      "a16_fight-ing-move-enc",
      "a16_fight-hero-training-injury-string-enc",
      "a16_fight-recharge-method-enc",
      "a16_fight-recharge-method-scenario-enc",
      "a16_fight-ing-dive-scenario-enc",
      "a16_fight-ing-fan-interaction-enc",
      "a16_fight-ing-interview-fan-reaction-enc",
      "a16_fight-press-conference-question-enc",
      "a16_fight-press-conference-media-outlet-enc",
      "a16_fight-ing-injurie-enc",
      "a16_fight-ing-sponsorship-company-enc",
      "a16_fight-ing-product-enc",
      "a16_fight-ing-wild-encounter-color-enc",
      "a16_fight-street-ing-location-enc",
      "a16_fight-er-animal-enc",
      "a16_fight-sportsmanship-scenario-enc",
      "a16_fight-roid-rage-scenario-enc",
      "a16_fight-roid-rage-retort-enc",
      "a16_fight-er-trash-talk-enc",
      "a16_fight-er-compliment-enc",
      "a16_fight-er-admiration-scenario-enc",
      "a16_fight-er-concern-scenario-enc",
      "a16_fight-er-training-string-enc",
      "a16_fight-er-circumstance-enc",
      "a16_fighting-products",
      "a16_fight-ufl-dumb-scenario-enc",
      "a16_fight-coach-quality-color-string-enc",
      "a16_fight-doping-challenge-color-string-enc",
      "a16_fight-wild-encounter-color",
      // The black market (report 60): eight item pools, the auction house that
      // moves them, the museum that shows them off, the street chemist and the
      // taxidermist, plus the three dumb pools that ride the shared runner.
      "a16_black-market-antiques-enc",
      "a16_black-market-art-enc",
      "a16_black-market-automobiles-enc",
      "a16_black-market-fashion-enc",
      "a16_black-market-gemstones-enc",
      "a16_black-market-historical-artifacts-enc",
      "a16_black-market-sports-memorabilia-enc",
      "a16_black-market-weapons-enc",
      "a16_black-market-auction-house-names-enc",
      "a16_black-market-auctioneer-color-enc",
      "a16_black-market-chemist-feedback-color-enc",
      "a16_black-market-museum-name-enc",
      "a16_black-market-museum-report-color-enc",
      "a16_black-market-police-intoxicated-hero-color-enc",
      "a16_black-market-police-intoxicated-warning-enc",
      "a16_black-market-purchase-requests-enc",
      "a16_black-market-street-chemist-cut-ingredients-enc",
      "a16_black-market-street-chemist-products-enc",
      "a16_black-market-taxidermist-business-names-enc",
      "a16_black-market-taxidermy-discard-locations-enc",
      "a16_black-market-exhibit-dumb-scenarios-enc",
      "a16_black-market-museum-dumb-scenarios-enc",
      "a16_black-market-vendor-dumb-scenarios-enc",
      // The casino (report 61): the house the hero owns, its amenities and game
      // rooms, the acts it books, the gamblers it serves and the celebrities it
      // hosts.
      "a16_casino-casino-accessory-acquisition-method-enc",
      "a16_casino-casino-amenity-enc",
      "a16_casino-casino-amenity-feedback-scenario-enc",
      "a16_casino-casino-audition-string-enc",
      "a16_casino-casino-celeb-party-death-string-enc",
      "a16_casino-casino-celebrity-party-scenario-enc",
      "a16_casino-casino-collectible-enc",
      "a16_casino-casino-drink-enc",
      "a16_casino-casino-dumb-scenario-enc",
      "a16_casino-casino-gambler-conversation-enc",
      "a16_casino-casino-game-room-complaint-enc",
      "a16_casino-casino-gift-enc",
      "a16_casino-casino-ledger-feedback-enc",
      "a16_casino-casino-offer-artist-refusal-enc",
      "a16_casino-casino-performance-act-enc",
      "a16_casino-casino-performance-act-name-enc",
      "a16_casino-casino-performance-act-string-enc",
      "a16_casino-casino-singer-genre-enc",
      "a16_casino-casino-singer-song-enc",
      "a16_casino-casino-street-name-enc",
      "a16_casino-casino-structure-enc",
      "a16_casino-casino-theme-enc",
      "a16_casino-banned-gambler-snippet-enc",
      "a16_casino-celebrity-demand-enc",
      "a16_casino-drink-refusal-string-enc",
      "a16_casino-gambler-compensation-method-enc",
      "a16_casino-gambler-encouragement-method-enc",
      "a16_casino-gambler-game-room-compliment-enc",
      "a16_casino-gambler-joins-casino-color-enc",
      "a16_casino-mafia-member-activity-string-enc",
      "a16_casino-meet-greet-feedback-enc",
      "a16_casino-negative-gambler-interaction-scenario-enc",
      "a16_casino-positive-gambler-interaction-scenario-enc",
      "a16_casino-random-game-room-name-enc",
      "a16_car-models-enc",
      "a16_cult-affirmation-film-titles-enc",
      "a16_cult-affirmation-receptions-enc",
      "a16_cult-anti-commune-nag-scenario-enc",
      "a16_cult-body-discard-locations-enc",
      "a16_cult-communard-apply-color-enc",
      "a16_cult-communard-conversation-enc",
      "a16_cult-communard-doubts-enc",
      "a16_cult-communard-educate-enc",
      "a16_cult-communard-leave-scenario-enc",
      "a16_cult-communard-punishment-enc",
      "a16_cult-communard-rewards-enc",
      "a16_cult-communard-rumor-enc",
      "a16_cult-communard-spend-time-enc",
      "a16_cult-communard-warning-enc",
      "a16_cult-commune-dumb-scenarios-enc",
      "a16_cult-commune-ledger-color-enc",
      "a16_cult-commune-property-types-enc",
      "a16_cult-commune-street-names-enc",
      "a16_cult-commune-structure-enc",
      "a16_cult-commune-structure-feedback-enc",
      "a16_cult-commune-teaching-enc",
      "a16_cult-default-totem-enc",
      "a16_cult-devotion-demonstration-enc",
      "a16_cult-doctrine-rules-enc",
      "a16_cult-drug-shared-experience-enc",
      "a16_cult-drug-solo-experience-enc",
      "a16_cult-drum-circle-artists-enc",
      "a16_cult-empowerments-enc",
      "a16_cult-ex-communard-dissent-scenario-enc",
      "a16_cult-expel-beg-scenario-enc",
      "a16_cult-fast-hunger-foods-enc",
      "a16_cult-fast-hunger-reasons-enc",
      "a16_cult-fast-reasons-enc",
      "a16_cult-gratitude-scenarios-enc",
      "a16_cult-healing-ceremony-enc",
      "a16_cult-honorific-name-structure-enc",
      "a16_cult-outreach-mission-heckles-enc",
      "a16_cult-random-commune-names-enc",
      "a16_cult-recruitment-location-enc",
      "a16_cult-recruitment-message-enc",
      "a16_cult-spiritual-journey-drugs-enc",
      "a16_cult-totem-disrespect-enc",
      "a16_cult-totem-inspiration-enc",
      "a16_cult-totem-lecture-enc",
      "a16_cult-workshop-products-enc",
      "a16_dealer-batch-tester-feedback-enc",
      "a16_dealer-cartel-drug-enc",
      "a16_dealer-cartel-give-money-feedback-enc",
      "a16_dealer-cartel-member-leave-scenario-enc",
      "a16_dealer-cartel-member-murder-scenario-enc",
      "a16_dealer-cartel-member-nickname-enc",
      "a16_dealer-cartel-name-enc",
      "a16_dealer-cartel-party-refusal-enc",
      "a16_dealer-cartel-street-name-enc",
      "a16_dealer-cartel-takeover-method-enc",
      "a16_dealer-cartel-threat-enc",
      "a16_dealer-cut-drug-name-enc",
      "a16_dealer-cut-drug-substance-enc",
      "a16_dealer-deal-activity-item-enc",
      "a16_dealer-deal-person-drug-experience-enc",
      "a16_dealer-drug-barter-item-enc",
      "a16_dealer-drug-drop-off-location-enc",
      "a16_dealer-drug-effect-enc",
      "a16_dealer-drug-package-type-enc",
      "a16_dealer-drug-pastime-enc",
      "a16_dealer-drug-purchase-reason-enc",
      "a16_dealer-drug-run-dumb-scenario-enc",
      "a16_dealer-drug-sale-refusal-snippet-enc",
      "a16_dealer-drug-sale-rejection-reason-enc",
      "a16_dealer-drug-trip-experience-enc",
      "a16_dealer-drug-worker-action-enc",
      "a16_dealer-drug-worker-murder-scenario-enc",
      "a16_dealer-extortion-color-snippet-enc",
      "a16_dealer-extortion-method-enc",
      "a16_dealer-hero-threatens-rival-cartel-member-enc",
      "a16_dealer-kingpin-mutiny-success-snippets-enc",
      "a16_dealer-mutiny-methods-enc",
      "a16_dealer-overdose-snippet-enc",
      "a16_dealer-police-warning-enc",
      "a16_dealer-prison-cartel-packages-enc",
      "a16_dealer-prison-drug-trips-enc",
      "a16_dealer-run-outcome-flavor-enc",
      "a16_insults-enc",
  };
  try {
    for (const char* name : kTables) {
      Table t = Table::load(reDir_ + "/gamedata_plain/" + name);
      loaded_.push_back(name);
      tables_.emplace_back(name, std::move(t));
    }
    countries_ = loadCountries(reDir_ + "/gamedata_json/a16_countries-enc.json");
  } catch (const std::exception& e) {
    if (error) *error = e.what();
    return false;
  }
  if (countries_.empty()) {
    if (error) *error = "no countries parsed";
    return false;
  }
  return true;
}

std::string Sim::personName(const std::string& token) {
  std::string key = token;
  bool possessive = false;
  const std::string suffix = "-possessive";
  if (key.size() > suffix.size() && key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0) {
    possessive = true;
    key.resize(key.size() - suffix.size());
  }
  static const char* kPeople[] = {"friend", "teacher", "classmate", "coworker", "co-worker",
                                  "parent", "lover", "employee", "boss", "spouse", "sibling",
                                  "neighbour", "neighbor", "stranger", "child", "bully",
                                  "crewmate", "colleague", "space-director", "commander"};
  bool person = false;
  for (const char* p : kPeople) if (key == p) person = true;
  if (key == "company") return "the company";
  if (!person) return std::string();

  const Table& names = tbl("a16_casino-casino-celebrity-name-enc");
  const size_t row = weightedPick(names, "Commonness Score", rng_);
  const std::string name = names.at(row, "First Name") + " " + names.at(row, "Last Name");
  // a16_bully-scenarios-enc uses a bare <bully> slot. The captured card renders it
  // as a role plus a name ("Your classmate, Moe, has been telling everyone on
  // social media that you are a teacher's pet."), so the slot keeps the role word.
  if (key == "bully") return possessive ? name + "'s" : "classmate, " + name;
  return possessive ? name + "'s" : name;
}

std::string Sim::money(long long v) const {
  std::string sym = "$";
  if (hero_) {
    for (const Country& c : countries_) {
      if (c.name == hero_->country && !c.currency.empty()) { sym = c.currency; break; }
    }
  }
  std::string digits = std::to_string(v < 0 ? -v : v);
  for (size_t i = digits.size(); i > 3; i -= 3) digits.insert(i - 3, ",");
  return (v < 0 ? "-" : "") + sym + digits;
}

// '<number:natural10-12000>', '<number:currency120-5000>', the legacy
// '<num2-5>' / '<currency5000-550000>' spellings and the '<decnum100-105>'
// tenths form. The '_roundup_10|100|1000' suffix draws first and rounds the
// drawn value up (gamedata_plain/genesis_master_tags_number).
bool Sim::numberToken(const std::string& token, std::string& out) {
  std::string body = token;
  long long round = 0;
  const size_t rp = body.find("_roundup_");
  if (rp != std::string::npos) { round = std::atoll(body.c_str() + rp + 9); body.resize(rp); }
  bool currency = false, decimal = false;
  if (body.compare(0, 15, "number:currency") == 0) { currency = true; body = body.substr(15); }
  else if (body.compare(0, 14, "number:natural") == 0) body = body.substr(14);
  else if (body.compare(0, 8, "currency") == 0) { currency = true; body = body.substr(8); }
  else if (body.compare(0, 6, "decnum") == 0) { decimal = true; body = body.substr(6); }
  else if (body.compare(0, 3, "num") == 0) body = body.substr(3);
  else return false;
  const size_t dash = body.find('-');
  if (dash == std::string::npos) return false;
  const long long lo = std::atoll(body.substr(0, dash).c_str());
  const long long hi = std::atoll(body.substr(dash + 1).c_str());
  if (hi < lo) return false;
  if (decimal) {
    // '<decnum100-105>' -> one decimal place, e.g. 102.4.
    const long long tenths = (hi - lo) * 10;
    const long long v = lo * 10 + (tenths > 0 ? static_cast<long long>(rng_.below(uint32_t(tenths + 1))) : 0);
    out += std::to_string(v / 10) + "." + std::to_string(v % 10);
    return true;
  }
  const long long span = hi - lo + 1;
  long long v = lo + (span > 1 ? static_cast<long long>(rng_.below(uint32_t(span))) : 0);
  if (round > 1) v = ((v + round - 1) / round) * round;
  out += currency ? money(v) : std::to_string(v);
  return true;
}

// The bare word tokens are entries of the game's own word pool table
// genesis_master_tags_reference_2: '<insult>', '<drug>', '<drug-illegal>',
// '<alcohol>', '<musician>', '<celebrity>', '<celebrity-male|female>',
// '<random-person>', '<random-person-indefinite>', '<random-first-name-male>',
// '<random-last-name>', '<body part>' ... Its tag_id column carries the
// '<reference:name_with_underscores>' spelling and its real 'weight' column
// decides the draw. A drawn value can itself be another pool (the
// '<reference:random_first_name>' entry only holds the male and female pools),
// so the draw repeats. Captured live: '<random-first-name-male>' -> 'Brad',
// '<random-first-name>' -> 'Buster', '<body part>' -> 'fist'.
std::string Sim::referenceToken(const std::string& token) {
  if (!ensureTable("genesis_master_tags_reference_2")) return std::string();
  const Table& t = tbl("genesis_master_tags_reference_2");
  std::string key = token;
  for (char& c : key) if (c == '-') c = '_';
  if (key == "body_part" || key == "body part") key = "body_part_gender_neutral_can_be_injured";
  for (int depth = 0; depth < 4; ++depth) {
    const std::string tag = "<reference:" + key + ">";
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) if (t.at(r, "tag_id") == tag) pool.push_back(r);
    if (pool.empty()) return std::string();
    const std::string value = t.at(weightedPickOf(t, "weight", pool, rng_), "tag_value");
    if (value.compare(0, 11, "<reference:") != 0) return value;
    key = value.substr(11, value.size() - 12);
  }
  return std::string();
}

// The game does not keep the circumstance pools in a CSV: DataHub
// 'GetRandomCircumstance()' @0x73f2ce2d08 is a switch over
// 'Random.Range(0,43)' (cmp w0,#0x2a) whose arms return string literals that
// only exist in the binary's metadata (report 38). Sampling the function live
// gives 41 fixed scenes plus two '<musician> concert' slots, so the article
// there follows the first letter of the drawn name ('an Usher concert').
// 'GetRandomCircumstanceAtHome()' @0x73f2ce322c and
// 'GetRandomCircumstanceComingBackFrom()' @0x73f2ce3888 are the same shape
// (56 and 53 slots); their scenes are captured in report 38 but no wired table
// uses '<circumstance-home>' or '<circumstance-return>' yet.
static const char* const kCircumstancePublicScenes[] = {
    "During a baseball game",
    "During your lunch break",
    "On your way to 7-Eleven",
    "On your way to the airport",
    "On your way to the dentist",
    "On your way to the movie theater",
    "On your way to the shooting range",
    "While at a cricket match",
    "While at a festival",
    "While at a funeral",
    "While at a gay pride festival",
    "While at a golf tournament",
    "While at a hockey game",
    "While at a party",
    "While getting a manicure",
    "While having a picnic",
    "While on a hike in the mountains",
    "While on a trip to the mall",
    "While out fishing",
    "While out for a walk",
    "While out getting a coffee",
    "While out grocery shopping",
    "While out jogging",
    "While out riding your bike",
    "While out running errands",
    "While playing chess at the park with an old man",
    "While returning a book to the library",
    "While returning a movie to a Redbox",
    "While riding a Segway",
    "While running the bases in a kickball game",
    "While shopping at the sex toy store",
    "While sitting at the train station",
    "While sitting on a park bench",
    "While strolling through the park",
    "While taking out the trash",
    "While trimming the bush",
    "While waiting at the bus stop",
    "While waiting for food at the drive-thru",
    "While waiting for your Uber driver",
    "While walking downtown",
    "While walking through the cemetery",
};

// The home pool (section 38): 56 slots, 55 fixed scenes plus one
// '<musician>' slot, so the artist is drawn from a16_musicians-enc and the
// article follows the name ('listening to an Arctic Monkeys'). The return pool
// (53 slots) still has no wired table, so it stays unwired.
static const char* const kCircumstanceHomeScenes[] = {
    "While at home watching scary airplane landing videos on YouTube",
    "While at home having a conversation with your imaginary friend",
    "While at home massaging your feet",
    "While taking a nap on your couch",
    "While checking yourself out in your full size mirror",
    "While relaxing on the couch Googling yourself",
    "While meditating in your living room",
    "While at home recording YouTube videos",
    "While putting away dishes in the kitchen",
    "While reorganizing the silverware drawer",
    "While scrubbing the toilet in your bathroom",
    "While cleaning some moldy food out of the refrigerator",
    "While stepping out of the shower fully nude",
    "While taking selfies in your bed",
    "While at home watching street fight videos on YouTube",
    "While in your bedroom pretending to be in a music video",
    "While reading in bed at night",
    "While planting geraniums in the garden",
    "While in the bathroom plucking your eyebrows",
    "While taking out the stinky trash at home",
    "While folding the laundry",
    "While watching the news in your living room",
    "While at home experimenting with how many grapes you can fit in your mouth",
    "While in bed fantasizing about your celebrity crush",
    "While curled up on the couch playing BitLife",
    "While mopping the floor at home",
    "While changing the air filter in your air conditioner",
    "While at home fantasizing about your dream vacation",
    "While vacuuming the house",
    "While at home plunging a clogged toilet",
    "While making your bed in the morning",
    "While at home stalking people on the internet",
    "While at home cathartically shouting out obscenities",
    "While making an egg salad in your kitchen",
    "While at home wrapping a gift for a friend",
    "While tossing a salad in your kitchen",
    "While cooking a healthy, yet savory, dinner",
    "While sitting on the toilet trying to work out your constipation issues",
    "While exfoliating the dead skin on your face",
    "While up late watching the Home Shopping Network",
    "While washing the dishes",
    "While practicing yoga in your yard",
    "While dancing alone around your bedroom",
    "While walking around your house naked",
    "While kicking back watching sports from your couch",
    "While in your bathroom clipping your overgrown toenails",
    "While walking around the house talking to yourself",
    "While using the bathroom late at night",
    "While lying on the couch at night",
    "While watering the plants at home",
    "While relaxing and listening to music at home",
    "While at home working on your twerking skills",
    "While at home creepily watching the neighbors from a window",
    "While laying in bed at night experiencing insomnia",
    "While at home practicing your foreign accents",
};

static std::string articleFor(const std::string& word) {
  const bool vowel = !word.empty() && std::string("aeiouAEIOU").find(word[0]) != std::string::npos;
  return vowel ? "an " : "a ";
}

// A resolved name that carries its own quantifier ("a pack of cigarettes", "two
// bottles of wine", "the house special") must not get a second article in front
// of it, and the same cell can resolve to either shape.
static std::string withArticle(const std::string& text, bool wantsArticle) {
  if (!wantsArticle || text.empty()) return std::string();
  const std::string first = lower(text.substr(0, text.find(' ')));
  static const char* kOwn[] = {"a", "an", "the", "some", "one", "two", "three", "four", "five",
                               "six", "seven", "eight", "nine", "ten", "no"};
  for (const char* w : kOwn)
    if (first == w) return std::string();
  if (first.find_first_of("0123456789") != std::string::npos) return std::string();
  return articleFor(text);
}

std::string Sim::circumstancePublic() {
  const size_t n = sizeof(kCircumstancePublicScenes) / sizeof(*kCircumstancePublicScenes);
  const size_t slot = rng_.below(uint32_t(n + 2));  // two of the 43 slots draw a musician
  if (slot < n) return kCircumstancePublicScenes[slot];
  const std::string who = referenceToken("musician");
  if (who.empty()) return kCircumstancePublicScenes[rng_.below(uint32_t(n))];
  return std::string(slot == n ? "While at " : "On your way to ") + articleFor(who) + who + " concert";
}

std::string Sim::circumstanceHome() {
  const size_t n = sizeof(kCircumstanceHomeScenes) / sizeof(*kCircumstanceHomeScenes);
  const size_t slot = rng_.below(uint32_t(n + 1));  // the last slot draws a musician
  if (slot < n) return kCircumstanceHomeScenes[slot];
  const std::string who = referenceToken("musician");
  if (who.empty()) return kCircumstanceHomeScenes[rng_.below(uint32_t(n))];
  return "While chilling in bed listening to " + articleFor(who) + who;
}

// '<vacation-destination>' and '<vacation-cruise>' only occur in
// a16_dumb-scenarios-vacation-enc (173 and 89 uses). The first is a row of
// a16_vacation-destinations-enc ('Abu Dhabi', 'Alaska'), the second a row of
// a16_cruises-enc ('Caribbean'), which is what the game's own strings call a
// cruise (literal #49729 'You went on {0} cruise.'). The port has no Travel
// menu, so both draw uniformly from their table.
std::string Sim::vacationDestination() {
  if (!ensureTable("a16_vacation-destinations-enc")) return std::string();
  const Table& t = tbl("a16_vacation-destinations-enc");
  if (t.size() == 0) return std::string();
  return t.at(rng_.below(uint32_t(t.size())), "Destination");
}

std::string Sim::vacationCruise() {
  if (!ensureTable("a16_cruises-enc")) return std::string();
  const Table& t = tbl("a16_cruises-enc");
  if (t.size() == 0) return std::string();
  return t.at(rng_.below(uint32_t(t.size())), "Type");
}

// The hero-facing tokens. genesis_master_tags_hero documents the '<hero:...>'
// spelling; the tables also carry the older '<hero-name>' / '<hero-first-name>'
// / '<city>' / '<country>' / '<country-other>' forms. The game itself fills a
// per-life dictionary with '<country>', '<country possessive>', '<city>',
// '<first name>', '<male name>' and '<body part>' (captured live, report 33).
std::string Sim::heroToken(const std::string& key, bool possessive) {
  if (!hero_) return std::string();
  const Hero& h = *hero_;
  const std::string own = possessive ? "'s" : "";
  if (key == "hero-name" || key == "hero:full_name") return h.firstName + " " + h.lastName + own;
  if (key == "hero-first-name" || key == "hero:first_name" || key == "first name") return h.firstName + own;
  if (key == "hero-last-name" || key == "hero:last_name" || key == "hero:last-name") return h.lastName + own;
  if (key == "hero:age") return std::to_string(h.age);
  if (key == "hero:bank_balance") return money(h.money);
  if (key == "hero:money_currency_symbol") { const std::string m = money(0); return m.substr(0, m.size() - 1); }
  if (key == "city" || key == "hero:current_city") return h.city;
  if (key == "country" || key == "country possessive" || key == "hero:current_country") return h.country + own;
  if (key == "country-other" || key == "country2" || key == "hero:country_other") {
    if (countries_.empty()) return std::string();
    for (int tries = 0; tries < 12; ++tries) {
      const std::string& c = countries_[rng_.below(uint32_t(countries_.size()))].name;
      if (c != h.country) return c;
    }
    return std::string();
  }
  return std::string();
}

// '<pronoun>' (a16_death-types-enc writes the hero's own death sentence with it)
// and the '<candidate.he|she>' pairs of the genesis-md tables, where the
// candidate being written about is the hero: the alternative follows the hero's
// gender.
bool Sim::genderToken(const std::string& token, std::string& out) {
  if (token == "pronoun") {
    out += (hero_ && hero_->female) ? "she" : "he";
    return true;
  }
  // Male/female pronoun brackets. BracketParser holds these in
  // 'static Dictionary<String,Dictionary<GENDER,String>> MaleFemalePronounBracketsForGenderStrings'
  // (il2cpp_dump.txt:1067) and applies them in
  // 'ReplaceMaleFemalePronounBracketsInStringForGender(String,Int32)' @0x73f337ce5c;
  // PRONOUN_TYPE (il2cpp_dump.txt:22199) names HE_SHE and HIS_HER. They are not
  // random draws: the '/' form would otherwise be picked at random by the bracket
  // branch below. The game passes the gender of the person the line is about; the
  // port keys on the hero, which is the same person for every wired table.
  static const struct { const char* token; const char* male; const char* female; } kPairs[] = {
    {"he/she", "he", "she"},             {"He/She", "He", "She"},
    {"him/her", "him", "her"},           {"Him/Her", "Him", "Her"},
    {"his/her", "his", "her"},           {"His/Her", "His", "Her"},
    {"his/hers", "his", "hers"},         {"His/Hers", "His", "Hers"},
    {"himself/herself", "himself", "herself"},
    // The hustle logs are written about the hero ('<you/I> sold <victim> ...'),
    // so the pair is fixed rather than a coin toss (REPORT.en.md 72).
    {"you/I", "I", "I"},                {"your/my", "my", "my"},
    {"you/me", "me", "me"},             {"son/daughter", "son", "daughter"},
    {"Himself/Herself", "Himself", "Herself"},
  };
  // The lover pools write about the partner ('<he/she> says it's not you, it's
  // <him/her>'), so the runner flips the subject while one of those rows renders.
  const bool subjectFemale = pronounOtherSet_ ? pronounOtherFemale_ : (hero_ && hero_->female);
  for (const auto& p : kPairs) {
    if (token == p.token) {
      out += subjectFemale ? p.female : p.male;
      return true;
    }
  }
  if (token.compare(0, 10, "candidate.") != 0 || token.find('|') == std::string::npos) return false;
  std::vector<std::string> options;
  splitOn(token, '|', options);
  if (options.size() != 2) return false;
  options[0] = options[0].substr(10);  // drop the 'candidate.' scope prefix
  out += (hero_ && hero_->female) ? options[1] : options[0];
  return true;
}

namespace {

// Tokens the port has no source for yet. The inline option-list selector below
// skips an alternative that contains one, so a branch the port cannot fill is
// never chosen over one it can (REPORT.en.md 49).
const char* const kUnresolvedTokens[] = {"<action>"};

// Pool-specific nouns for the misc dumb-scenario pools (REPORT.en.md 56). Each
// entry names the table and column the game fills the token from; the value is
// resolved again after the draw because some columns carry tokens of their own
// (a16_mafia-nicknames-enc stores '<first-name> Meatballs').
struct MiscSrc { const char* key; const char* table; const char* col; };

const MiscSrc kMiscSrc[] = {
  // The mafia names moved to their own token handler (REPORT.en.md 64).
  {"cartel", "a16_dealer-cartel-name-enc", "Name"},
  {"drug-cartel", "a16_dealer-cartel-name-enc", "Name"},
  {"rival-cartel", "a16_dealer-cartel-name-enc", "Name"},
  {"hustle-box", "a16_hustler-collection-boxes-enc", "Item"},
  {"hustle-act", "a16_hustler-panhandle-strategies-enc", "Strategy"},
  {"hustle-street", "a16_street-names-enc", "Street Name"},
  {"amenity", "a16_landlord-home-default-features-enc", "Feature String"},
  {"car-type", "a16_car-models-enc", "Type"},
  // The zoo names moved to zooToken (REPORT.en.md 65).
  {"front", "a16_secret-agent-agency-fronts-enc", "Type"},
  {"production", "a16_actor-films-enc", "Title"},
  {"talk-show", "a16_actor-shows-enc", "Title"},
  {"real-actor", "a16_actors-real-enc", "Full Name"},
  // <hero-casino> and <casino-gambler> moved to casinoToken (report 61)

};

// The same family spelled as a person: the pools ask for a name, the tables
// above do not hold one.
const char* const kMiscPeople[] = {
  "cartel-member", "cartel-kingpin", "booking-agent", "director", "costar",
  "agent", "agency-rival", "model", "product-model",
};

const char* const kMiscLiterals[] = {
  "hustle-instrument",
};

// The tokens the lover family owns, possessive spellings included. This is
// checked before the generic person/pool fallback so that '<lover>' is the
// partner the hero is actually with, not a fresh draw.
bool relIsToken(const std::string& token) {
  std::string key = token;
  const std::string suffix = "-possessive";
  if (key.size() > suffix.size() && key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0)
    key.resize(key.size() - suffix.size());
  static const char* kTokens[] = {"lover", "ex", "friend-lover", "house", "addiction",
                                  "disease", "gender", "ex-lover", "name"};
  for (const char* t : kTokens) if (key == t) return true;
  return false;
}

bool miscIsToken(const std::string& key) {
  const std::string psuffix = "-possessive";
  std::string base = key;
  if (key.size() > psuffix.size() &&
      key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    base = key.substr(0, key.size() - psuffix.size());
  for (const MiscSrc& m : kMiscSrc) if (base == m.key) return true;
  for (const char* k : kMiscPeople) if (base == k) return true;
  for (const char* k : kMiscLiterals) if (base == k) return true;
  return false;
}

// The secret-agent pools address the agency, the target and the client by
// bracket (report 58). '-possessive' is stripped the same way the other
// families do it.
// The family's snippets are authored for the hero's own log, so the first
// person form has to be substituted before they are resolved; the guard list
// above drops the tokens the port already knows, and these two keep the
// substitution honest.
std::string spySwap(std::string text, const std::string& from, const std::string& to) {
  if (from.empty()) return text;
  for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
    text.replace(at, from.size(), to);
  return text;
}

std::string spySentence(std::string text) {
  text = trim(text);
  if (!text.empty() && text.back() != '.' && text.back() != '!' && text.back() != '?') text += ".";
  return text;
}

std::string spyLowerFirst(std::string text) {
  if (!text.empty()) text[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[0])));
  return text;
}

// Capitalises the first letter: several pool cells are authored lowercase
// because the shipped log line embeds them mid-sentence.
std::string spyUpperFirst(std::string text) {
  for (size_t i = 0; i < text.size(); ++i) {
    const unsigned char c = (unsigned char)text[i];
    if (c == ' ') continue;
    if (c >= 'a' && c <= 'z') text[i] = char(c - 32);
    break;
  }
  return text;
}

// The casino family (report 61). '<hero-casino>' and '<casino-room>' used to be
// served by the misc map with a random row each time; now the hero's own casino
// answers them when the hero has one.
// The mafia family's own tokens (report 64). The mob tables key their rows on
// ISO country codes, so the names come from the table for the hero's country.
// The studio's own token names (REPORT.en.md 68.4). 'song-title' is a family of
// spellings, so it is matched by prefix.
// The five studio trades (REPORT.en.md 69) own '<artist>' and the name
// fragments the stage-name tables are built from.
bool tradeIsToken(const std::string& token) {
  const std::string psuffix = "-possessive";
  std::string base = token;
  if (base.size() > psuffix.size() &&
      base.compare(base.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    base.resize(base.size() - psuffix.size());
  }
  return base == "artist" || base == "track" || base == "musician-first-name" ||
         base == "musician-first-name-letter" || base == "musician-last-name" ||
         base == "musician-name" || base == "instrument" || base == "project" ||
         base == "project-tone" || base == "trade-stage";
}
bool musicIsToken(const std::string& token) {
  std::string key = token;
  const std::string psuffix = "-possessive";
  if (key.size() > psuffix.size() && key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    key.resize(key.size() - psuffix.size());
  }
  if (key == "album-title" || key == "band-member" || key == "music-genre" ||
      key == "music-role" || key == "artist-name" || key == "album-description" ||
      key == "musician-soundcloud") return true;
  return key.compare(0, 10, "song-title") == 0;
}

// The vampire family's own keys (REPORT.en.md 70). '<victim>' is shared with
// the drug trade and the service, so vampToken prefers theirs when a card from
// those families set one, and the guard runs first so the token never leaks.
bool vampIsToken(const std::string& token) {
  std::string k = token;
  const std::string psuffix = "-possessive";
  if (k.size() > psuffix.size() && k.compare(k.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    k = k.substr(0, k.size() - psuffix.size());
  else if (k == "victim-possesive") k = "victim";   // the data ships this spelling
  return k == "victim" || k == "familiar" || k == "vampire" || k == "vampire-name" ||
         k == "maker" || k == "vampire-council-member" || k == "amount" || k == "insult";
}

bool petIsToken(const std::string& token) {
  std::string key = token;
  const std::string psuffix = "-possessive";
  if (key.size() > psuffix.size() &&
      key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    key.resize(key.size() - psuffix.size());
  static const char* kTokens[] = {"pet", "pet-name", "breed", "animal", "animal-type",
                                  "animal-type-indefinite"};
  for (const char* t : kTokens) if (key == t) return true;
  return false;
}

// The outdoor tokens (REPORT.en.md 71). The family also writes '< pet>' with a
// leading space once, so the key is trimmed before it is compared.
bool outdoorIsToken(const std::string& token) {
  std::string key = trim(token);
  const std::string psuffix = "-possessive";
  if (key.size() > psuffix.size() &&
      key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    key.resize(key.size() - psuffix.size());
  static const char* kTokens[] = {"outdoor-location", "outdoor-companion", "outdoor-cave",
                                  "bicycle", "outdoor-landmark", "outdoor-plant",
                                  "outdoor-marine-life", "outdoor-bird", "pet"};
  for (const char* t : kTokens) if (key == t) return true;
  return false;
}

// The hustle (REPORT.en.md 72). '<victim>', '<amount>' and '<insult>' are the
// vampire pools' own tokens, so a hustle card only takes them while it renders.
// The space program (REPORT.en.md 73). '<crewmate>', '<colleague>' and
// '<space-director>' name a person; the rest are draws from the family's own
// tables. Only a space card asks for them, so no live flag is needed.
bool astroIsToken(const std::string& token) {
  std::string key = token;
  while (!key.empty() && (key[0] == ' ' || key[0] == '	')) key.erase(0, 1);
  while (!key.empty() && (key[key.size() - 1] == ' ' || key[key.size() - 1] == '	')) key.resize(key.size() - 1);
  static const std::string suffix = "-possessive";
  if (key.size() > suffix.size() && key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0)
    key.resize(key.size() - suffix.size());
  static const char* kKeys[] = {"crewmate", "colleague", "space-director", "shuttle", "space-agency",
                                "destination-moon", "destination-mars", "discovery-name"};
  for (const char* k : kKeys) if (key == k) return true;
  return false;
}

bool hustlerIsToken(const std::string& token, bool live) {
  std::string key = trim(token);
  const std::string psuffix = "-possessive";
  if (key.size() > psuffix.size() &&
      key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    key.resize(key.size() - psuffix.size());
  static const char* kAlways[] = {"hustle-act", "hustle-street", "hustle-instrument",
                                  "hustle-box"};
  for (const char* t : kAlways) if (key == t) return true;
  if (!live) return false;
  static const char* kLive[] = {"victim", "victim-possesive", "amount", "insult", "item"};
  for (const char* t : kLive) if (key == t) return true;
  return false;
}

bool landlordIsToken(const std::string& token) {
  std::string key = token;
  const std::string psuffix = "-possessive";
  if (key.size() > psuffix.size() &&
      key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    key.resize(key.size() - psuffix.size());
  static const char* kTokens[] = {"tenant", "squatter", "address", "tenant-address",
                                  "squatter-address", "property-city", "house-type"};
  for (const char* t : kTokens) if (key == t) return true;
  return false;
}

bool zooIsToken(const std::string& token) {
  std::string base = token;
  const std::string psuffix = "-possessive";
  if (base.size() > psuffix.size() && base.compare(base.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    base = base.substr(0, base.size() - psuffix.size());
  static const char* kKeys[] = {"animal-zoo", "zoo-animal", "zoo-animal-other", "zoo-name",
                                "habitat-zoo", "habitat-animal", "animal-handler",
                                "zoo-veterinarian", "zoo-operations-manager", "zoo-other",
                                "rival-zoo-animal-breed"};
  for (const char* k : kKeys) if (base == k) return true;
  return false;
}

bool mafiaIsToken(const std::string& token) {
  std::string key = token;
  const std::string psuffix = "-possessive";
  if (key.size() > psuffix.size() && key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    key = key.substr(0, key.size() - psuffix.size());
  static const char* kKeys[] = {"mafia-boss", "mafia-godfather", "mafia-family", "mafia-rival-family",
                                "mafia-rival-family-member", "mafia-colleague", "mafia-nickname",
                                "mafia-calling-card", "first-name", "last-name", "other-person",
                                "amount", "city-other"};
  for (const char* k : kKeys) if (key == k) return true;
  return false;
}

bool dealerIsToken(const std::string& token) {
  std::string k = token;
  const std::string psuffix = "-possessive";
  if (k.size() > psuffix.size() && k.compare(k.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    k = k.substr(0, k.size() - psuffix.size());
  else if (k == "victim-possesive")
    k = "victim";
  return k == "cartel-member" || k == "member" || k == "partner" ||
         k == "cartel-rival-member" || k == "rival-member" || k == "rival-cartel-member" ||
         k == "cartel-kingpin" || k == "kingpin" || k == "victim" ||
         k == "other-person" || k == "random-person" || k == "cartel-remember" ||
         k == "insult" || k == "random-first-name-male" || k == "random-first-name-female" ||
         k == "drug-illegal" || k == "dealer-drug" || k == "dealer-package" ||
         k == "cartel" || k == "drug-cartel" || k == "rival-cartel";
}

bool cultIsToken(const std::string& token) {
  std::string t = token;
  const std::string psuffix = "-possessive";
  if (t.size() > psuffix.size() && t.compare(t.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    t.resize(t.size() - psuffix.size());
  return t == "communard" || t == "ex-communard" || t == "totem" || t == "honorific" ||
         t == "commune-other" || t == "cult" || t == "cult-name";
}

bool casinoIsToken(const std::string& token) {
  std::string key = token;
  const std::string psuffix = "-possessive";
  if (key.size() > psuffix.size() && key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    key.resize(key.size() - psuffix.size());
  static const char* kKeys[] = {"casino-gambler", "casino-celebrity", "casino-game",
                                "casino-name", "casino", "casino-room", "hero-casino"};
  for (const char* k : kKeys)
    if (key == k) return true;
  return false;
}

bool bmIsToken(const std::string& token) {
  std::string key = token;
  const std::string psuffix = "-possessive";
  if (key.size() > psuffix.size() && key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    key.resize(key.size() - psuffix.size());
  static const char* kKeys[] = {"item", "item-short", "exhibit", "auction-house", "museum",
                                "vendor", "drug-chemist", "auctioneer", "taxidermist"};
  for (const char* k : kKeys)
    if (key == k) return true;
  return false;
}

bool fightIsToken(const std::string& token) {
  std::string key = token;
  const std::string psuffix = "-possessive";
  if (key.size() > psuffix.size() && key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    key.resize(key.size() - psuffix.size());
  static const char* kKeys[] = {"fighter", "coach-fighting", "fight-opponent-type", "opponent-name",
                                "fighting-arena", "fighting-style", "fight-style",
                                "circumstance-fighter", "fighter-circumstance"};
  for (const char* k : kKeys)
    if (key == k) return true;
  return false;
}

bool spyIsToken(const std::string& token) {
  const std::string psuffix = "-possessive";
  std::string base = token;
  if (token.size() > psuffix.size() &&
      token.compare(token.size() - psuffix.size(), psuffix.size(), psuffix) == 0)
    base = token.substr(0, token.size() - psuffix.size());
  static const char* kTokens[] = {
      "agent-spy", "client-spy", "client", "target", "target-assassination",
      "victim", "friend", "agency", "code-name", "specialization", "gadget",
      "password", "disguise", "pronoun", "color", "random-surname", "random-last-name",
      "random-first-name", "front", "operation", "agency-rival", "cover"};
  for (const char* t : kTokens) if (base == t) return true;
  return false;
}

bool hasUnresolvedToken(const std::string& s) {
  for (const char* t : kUnresolvedTokens)
    if (s.find(t) != std::string::npos) return true;
  return false;
}

}  // namespace

std::string Sim::resolve(const std::string& text) {
  std::map<std::string, std::string> memo;  // one name per token inside one line
  return resolveInto(text, memo);
}

std::string Sim::resolveInto(const std::string& text, std::map<std::string, std::string>& memo) {
  std::string out;
  for (size_t i = 0; i < text.size();) {
    if (text[i] != '<') { out += text[i++]; continue; }
    // Brackets nest: '<your server is <action>/they messed up both of your
    // orders>' is one group whose first alternative contains a token of its own,
    // so the closing bracket has to be matched with a depth count rather than
    // the next '>' (REPORT.en.md 49).
    size_t close = std::string::npos;
    {
      int depth = 0;
      for (size_t k = i; k < text.size(); ++k) {
        if (text[k] == '<') ++depth;
        else if (text[k] == '>') { if (--depth == 0) { close = k; break; } }
      }
    }
    if (close == std::string::npos) { out += text[i++]; continue; }
    const std::string token = text.substr(i + 1, close - i - 1);
    if (token.empty()) { out += "<>"; i = close + 1; continue; }

    if (token.compare(0, 8, "grammar:") == 0) {
      // "<grammar:a/an>" / "<grammar:A/An>" -> the article that fits the word
      // that follows it, so that word is resolved here and the walk ends.
      const std::string tail = resolveInto(text.substr(close + 1), memo);
      const size_t w = tail.find_first_not_of(' ');
      const bool vowel = w != std::string::npos &&
                         std::string("aeiouAEIOU").find(tail[w]) != std::string::npos;
      const bool upper = token.size() > 8 && std::isupper(static_cast<unsigned char>(token[8]));
      out += upper ? (vowel ? "An" : "A") : (vowel ? "an" : "a");
      out += tail;
      return out;
    }
    std::string genderValue;
    if (genderToken(token, genderValue)) {
      // handled: a pronoun bracket keyed on the hero's gender. A pair written
      // as a slash list ('<himself/herself>') is also an alternative, so a
      // later '<rb>' can point back at it (REPORT.en.md 70).
      out += genderValue;
      // A pronoun pair is not the alternative a '<rb>' points back at. The
      // astronaut pools write '<he/she> ... to be <captured/probed/studied> by
      // aliens' and then '...afraid I'd be <rb> by aliens', where '<rb>' is the
      // list and not the pronoun, so a pair only stands in as the last drawn
      // bracket value (REPORT.en.md 73).
      if (token.find('/') != std::string::npos && !genderValue.empty()) rbLast_ = genderValue;
    } else if (token.find('/') != std::string::npos) {
      // "<I got a flat tire/I ran out of gas/my battery died>" -> pick one, and
      // cache the draw for "<rb>" / "<rb1>" / "<rb2>" (report 33).
      std::vector<std::string> options;
      splitOn(token, '/', options);
      std::string drawn;
      // "</b>" is a closing tag rather than an alternative list, and it is the
      // only shape where the token itself starts with the separator. An empty
      // alternative in the middle ('cyclist//lifeguard') is a real one.
      if (!options.empty() && options[0].empty()) out += text.substr(i, close - i + 1);
      else {
        std::vector<size_t> usable;
        for (size_t k = 0; k < options.size(); ++k)
          if (!options[k].empty() && !hasUnresolvedToken(options[k])) usable.push_back(k);
        if (usable.empty())
          for (size_t k = 0; k < options.size(); ++k)
            if (!options[k].empty()) usable.push_back(k);
        if (usable.empty()) for (size_t k = 0; k < options.size(); ++k) usable.push_back(k);
        drawn = options[usable[rng_.below(uint32_t(usable.size()))]];
        // An alternative can hold tokens of its own, so the drawn text goes back
        // through the resolver before it is cached for '<rb>' (report 33) and
        // appended.
        const std::string picked = resolveInto(drawn, memo);
        if (rndCache_.size() < 8) rndCache_.push_back(picked);
        rbLast_ = picked;
        out += picked;
      }
    } else if (token.size() >= 2 && token[0] == 'r' && token[1] == 'b' &&
               token.find_first_not_of("0123456789", 2) == std::string::npos) {
      // "<rb>" / "<rb1>" = the first alternative drawn for this scenario, "<rb2>"
      // the second, up to "<rb4>". Out of range means the game never drew that
      // many, so the token stays verbatim.
      const int n = token.size() == 2 ? 1 : std::atoi(token.substr(2).c_str());
      // Out of range means the row asked for more alternatives than it drew; the
      // port reuses the last one instead of leaking the token (REPORT.en.md 56).
      if (n >= 1 && size_t(n) <= rndCache_.size()) out += rndCache_[size_t(n) - 1];
      else if (!rndCache_.empty()) out += rndCache_.back();
      else if (!rbLast_.empty()) out += rbLast_;   // the card drew no alternative, so the last bracket value stands in (REPORT 63)
      else out += text.substr(i, close - i + 1);
    } else if (token == "br") {
      out += '\n';
    } else if (token.compare(0, 5, "pick:") == 0) {
      // "<pick:Option 1|Option 2|Option 3>" -> one alternative.
      std::vector<std::string> options;
      splitOn(token.substr(5), '|', options);
      out += resolveInto(options[rng_.below(uint32_t(options.size()))], memo);
    } else if (token == "employee" || token == "employee-possessive" ||
               token == "competitor" || token == "competitor-possessive" ||
               token == "facility-singular" || token == "facility-multiple" ||
               token == "product") {
      const std::string v = businessToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (token == "jurisdiction" || token == "jurisdiction-possessive" ||
               token == "political-office" || token == "political-office-possessive" ||
               token == "opponent" || token == "opponent-possessive" ||
               token == "country-other-possessive" ||
               token == "jurisdiction adjective" || token == "jurisdiction-adjective") {
      const std::string v = politicalToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (hustlerIsToken(token, hustlerLive_)) {
      const std::string v = hustlerToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);  // unresolved: verbatim
      else { out += v; rbLast_ = v; }
    } else if (astroIsToken(token)) {
      const std::string v = astroToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);  // unresolved: verbatim
      else { out += v; rbLast_ = v; }
    } else if (vampIsToken(token)) {
      const std::string v = vampToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (petIsToken(token)) {
      const std::string v = petToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (outdoorIsToken(token)) {
      const std::string v = outdoorToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (landlordIsToken(token)) {
      const std::string v = landlordToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (zooIsToken(token)) {
      const std::string v = zooToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (mafiaIsToken(token)) {
      const std::string v = mafiaToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (dealerIsToken(token)) {
      const std::string v = dealerToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (cultIsToken(token)) {
      const std::string v = cultToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (casinoIsToken(token)) {
      const std::string v = casinoToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);  // unresolved: verbatim
      else { out += v; rbLast_ = v; }
    } else if (miscIsToken(token)) {
      const std::string v = miscToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (tradeIsToken(token)) {
      const std::string v = tradeToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);  // unresolved: keep verbatim
      else { out += v; rbLast_ = v; }
    } else if (musicIsToken(token)) {
      const std::string v = musicToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (token == "sport" || token == "league" || token == "team" ||
               token == "teammate" || token == "coach" || token == "ball" ||
               token == "referee" || token == "game" || token == "field" ||
               token == "stadium") {
      const std::string v = sportsToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (token.compare(0, 4, "war-") == 0) {
      const std::string v = warToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (token == "military-branch" || token == "military-rank") {
      if (!hero_) out += text.substr(i, close - i + 1);
      else {
        const std::string v = token == "military-branch" ? hero_->militaryBranch : hero_->militaryRank;
        if (v.empty()) out += text.substr(i, close - i + 1);
        else { out += v; rbLast_ = v; }
      }
    } else if (token == "restaurant") {
      // <restaurant> is the Restaurant row of a16_company-name-structure-enc
      // ('<color> <suffix>, <prefix> <color>, <random-first-name-possessive>
      // <suffix>'), the same generator the port already uses for company names.
      const std::string name = life_ ? companyName("Restaurant", *life_) : std::string();
      if (name.empty()) out += text.substr(i, close - i + 1);
      else out += name;
    } else if (token == "vacation-destination") {
      const std::string place = vacationDestination();
      if (place.empty()) out += text.substr(i, close - i + 1);
      else out += place;
    } else if (token == "vacation-cruise") {
      const std::string line = vacationCruise();
      if (line.empty()) out += text.substr(i, close - i + 1);
      else out += line;
    } else if (token == "highest-royal-name" || token == "other-royal-name" ||
               token == "circumstance-royal") {
      const std::string value = royalToken(token);
      if (value.empty()) out += text.substr(i, close - i + 1);  // unresolved: verbatim
      else out += value;
    } else if (token == "circumstance-public") {
      out += circumstancePublic();
    } else if (token == "circumstance-home") {
      out += circumstanceHome();
    } else if (token == "musician-indefinite") {
      // The indefinite twin of '<musician>': the reference pool holds the bare
      // name, the article is added here (report 38).
      const std::string who = referenceToken("musician");
      if (who.empty()) out += text.substr(i, close - i + 1);
      else out += articleFor(who) + who;
    } else if (token.compare(0, 5, "hero-") == 0 || token.compare(0, 5, "hero:") == 0 ||
               token == "city" || token == "country" || token == "country2" ||
               token == "country-other" || token == "country possessive" || token == "first name") {
      std::string key = token;
      bool possessive = false;
      const std::string suffix = "-possessive";
      if (key.size() > suffix.size() && key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0) {
        possessive = true;
        key.resize(key.size() - suffix.size());
      }
      const std::string value = heroToken(key, possessive);
      if (value.empty()) out += text.substr(i, close - i + 1);  // unresolved: keep verbatim
      else out += value;
    } else if (relIsToken(token)) {
      const std::string v = relationshipToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);
      else { out += v; rbLast_ = v; }
    } else if (spyIsToken(token)) {
      const std::string v = spyToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);  // unresolved: verbatim
      else { out += v; rbLast_ = v; }
    } else if (fightIsToken(token)) {
      const std::string v = fightToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);  // unresolved: verbatim
      else { out += v; rbLast_ = v; }
    } else if (bmIsToken(token)) {
      const std::string v = bmToken(token);
      if (v.empty()) out += text.substr(i, close - i + 1);  // unresolved: verbatim
      else { out += v; rbLast_ = v; }
    } else if (token == "a" || token == "a/an") {
      // '<a>' and '<a/an>' are indefinite articles, so the choice between a and
      // an comes from the word that follows in the same sentence.
      size_t w = close + 1;
      while (w < text.size() && text[w] == ' ') ++w;
      size_t we = w;
      while (we < text.size() && std::isalpha(static_cast<unsigned char>(text[we]))) ++we;
      std::string art = we > w ? articleFor(text.substr(w, we - w)) : std::string("a ");
      while (!art.empty() && art[art.size() - 1] == ' ') art.resize(art.size() - 1);
      out += art;
    } else if (token == "person" || token == "person-possessive") {
      std::string v = referenceToken("random_person_indefinite");
      if (v.empty()) v = referenceToken("random_person");
      if (v.empty()) out += text.substr(i, close - i + 1);  // unresolved: verbatim
      else out += (token == "person-possessive" ? v + "'s" : v);
    } else if (!numberToken(token, out)) {
      // The royal pool addresses the reference pools by slot number
      // ('<celebrity-female1>', '<celebrity-male2>', '<celebrity1>'): the
      // trailing digits are the slot, not part of the pool name.
      std::string slot = token;
      if (slot.compare(0, 9, "celebrity") == 0) {
        size_t cut = slot.size();
        while (cut > 0 && std::isdigit(static_cast<unsigned char>(slot[cut - 1]))) --cut;
        if (cut < slot.size()) slot.resize(cut);
        if (slot == "celebrity" || slot == "celebrity-")
          slot = (hero_ != nullptr && hero_->female) ? "celebrity-female" : "celebrity-male";
      }
      auto it = memo.find(token);
      if (it == memo.end()) {
        const std::string pool = referenceToken(slot);
        it = memo.emplace(token, pool.empty() ? personName(token) : pool).first;
      }
      if (it->second.empty()) out += text.substr(i, close - i + 1);  // unresolved: keep verbatim
      else out += it->second;
    }
    i = close + 1;
  }
  return out;
}

Life Sim::createLife(bool female) {
  Life life;
  Hero& h = life.hero;
  h.female = female;
  h.age = 0;

  const Table& names = tbl("a16_casino-casino-celebrity-name-enc");
  const std::string want = female ? "female" : "male";
  std::vector<size_t> pool;
  for (size_t r = 0; r < names.size(); ++r) {
    if (lower(trim(names.at(r, "Gender"))) == want) pool.push_back(r);
  }
  const size_t person = pickFrom(pool, rng_);
  if (person != size_t(-1)) {
    h.firstName = names.at(person, "First Name");
    h.lastName = names.at(person, "Last Name");
  }

  const Country& c = countries_[rng_.below(uint32_t(countries_.size()))];
  h.country = c.name;
  h.continent = c.continent;
  h.money = 0;

  // SimPerson..ctor(bool) draws the person's attributes, then
  // SimHero.SetupEverythingElseForNewHero rewrites four of them. Both were read
  // out of the shipping libil2cpp.so: the setup at file offset 0x2a74048 (it
  // branches on GameVars.LivesStarted <= 1) and the ctor's draw order from a
  // hooked System.Random stream matched value-for-value against the 22-attribute
  // hero snapshots of 14 captures.
  //
  // The ctor makes nineteen 0..100 draws in its own order, then six draws whose
  // target attribute is not attributed yet (four small ranges and two
  // Next(0,100)) -- consumed here so the stream keeps the ctor's shape. homo and
  // fertility are random in the game as well but are not drawn in this stream,
  // and fame stayed 0 in all 14 captures.
  // shortcut: only the six attributes the port models are kept, the other draws
  // are made and dropped.
  h.happiness = int(rng_.below(101));   // <Att_happiness>
  h.health = int(rng_.below(101));      // <Att_health>
  h.smarts = int(rng_.below(101));      // <Att_intelligence>
  h.looks = int(rng_.below(101));       // <Att_appearance>
  h.discipline = int(rng_.below(101));  // <Att_discipline>
  h.karma = int(rng_.below(101));       // <Att_karma>
  rng_.below(5);
  rng_.below(10);
  rng_.below(5);
  rng_.below(11);
  for (int i = 0; i < 13; ++i) rng_.below(101);  // willpower .. dealing
  rng_.below(100);
  rng_.below(100);

  h.fame = 0;  // never drawn for a newborn in any of the 14 captures
  // The four rewritten attributes are not written here: the game runs
  // SimHero.SetupEverythingElseForNewHero on the newborn's first age increment,
  // not at birth. Its age-0 snapshot still carries the ctor values
  // (happiness=0, health=100 in the 35-year capture) while age 1 carries
  // 81 + Next(0,20) for both. See applyNewbornSetup(), called from the first
  // ageUp().
  return life;
}

// SimHero.SetupEverythingElseForNewHero, first-life branch (LivesStarted <= 1):
// happiness and health land in 81..100, then one coin decides whether the
// newborn is book-smart or good-looking. A later life (LivesStarted > 1) draws
// happiness = 51 + Next(0,50) and health = 81 + Next(0,20) instead and leaves
// intelligence and appearance at their ctor values; the port plays one life per
// run, so only the first-life branch applies.
//
// The game runs this on the first age increment, so the hero's age-0 record
// keeps the ctor values. Moving it to the first ageUp() was measured, not
// guessed: the 35-year capture has game age 0 = {happiness 0, health 100,
// smarts 63, looks 18, karma 50, discipline 74} and game age 1 = {83, 84, 68,
// 14, ...}, i.e. the setup values.
// shortcut: health only ever changes here. In the game every health change goes
// through SimPerson.ImpactHealth(System.Int32) @0x73f30e487c, which is the sole
// writer of the Att_health store (@0x73f30e4944): a full instruction scan of
// libil2cpp.so finds only that store and set_Att_health, which the game never
// calls. It converts the score with AttributeAdjustmentPointsForImpactScore and
// clamps to 0..100 exactly like pointsForImpact() does, and the yearly driver is
// SimHero.ImpactHealth @0x73f2f21f0c, reached virtually from the per-item
// foreach in SimHero.IncrementAge. No dumb-scenario table carries a Health
// Impact column, so the yearly +-1 drift and the larger jumps in the 35-year
// capture (100 84 83 87 88 92) come from systems the port does not model yet:
// addictions (SimAddiction.AnnualHealthImpact @0x73f2fa8a3c), diseases
// (UpdateWithFatalDisease @0x73f2fb0d30) and the lifecycle events. Upgrade when
// those are ported.
void Sim::applyNewbornSetup(Hero& h) {
  h.happiness = 81 + int(rng_.below(20));
  h.health = 81 + int(rng_.below(20));
  if (rng_.below(2) != 0) {
    h.smarts = 10 + int(rng_.below(10));
    h.looks = 61 + int(rng_.below(10));
  } else {
    h.happiness = 81 + int(rng_.below(10));
    h.health = 81 + int(rng_.below(10));
    h.smarts = 61 + int(rng_.below(10));
    h.looks = 10 + int(rng_.below(10));
  }
}

std::vector<std::string> Sim::birthLines(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;  // '<hero-name>' / '<city>' read the live hero
  life_ = &life;
  std::vector<std::string> out;
  const std::string gender = h.female ? "female" : "male";

  // Parent names come from the same celebrity-name table createLife() uses, one
  // draw per parent, matched to that parent's gender like the real draw does.
  const auto drawFirst = [this](bool female) { return drawFirstName(female); };
  const auto career = [this]() {
    const Table& c = tbl("a16_careers-enc");
    const size_t row = weightedPick(c, "Commonness Score", rng_);
    return row == size_t(-1) ? std::string() : lower(trim(c.at(row, "Career")));
  };

  // Parent ages are drawn first: the circumstance row is gated on the mother's.
  const int motherAge = 24 + int(rng_.below(22));
  const int fatherAge = motherAge + int(rng_.below(7)) - 3;

  // 1) Place, gender and the conception circumstance.
  std::string circumstance;
  {
    const Table& t = tbl("a16_birth-circumstances-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (trim(t.at(r, "Circumstance")).empty()) continue;
      if (!trim(t.at(r, "Min Mother Age")).empty() && motherAge < int(t.num(r, "Min Mother Age"))) continue;
      if (!trim(t.at(r, "Max Mother Age")).empty() && motherAge > int(t.num(r, "Max Mother Age"))) continue;
      // The table's Royal column marks the 37 conception lines the game draws
      // from once royalty is already decided (the separate entry point is
      // GetRandomBirthCircumstanceForHeroRoyalWithLooksAndSmarts @0x73f2cb1b14,
      // reached only when SimHero.Royal is set). REPORT.en.md 47.
      const bool royalRow = t.has("Royal") && t.flag(r, "Royal");
      if (royalRow != h.royal) continue;
      pool.push_back(r);
    }
    const size_t row = weightedPickOf(t, "Commonness Score", pool, rng_);
    if (row != size_t(-1)) circumstance = resolve(t.at(row, "Circumstance"));
  }
  out.push_back("I was born a " + gender + " in " + h.city + ", " + h.country + "." +
                (circumstance.empty() ? "" : " " + circumstance));
  // Literal #70877 '{0}{1} was born into {2} royalty. ' -- {0} is the prefix that
  // stays empty for the hero's own life, so this is the first-person line.
  if (h.royal) out.push_back("I was born into " + h.country + " royalty.");

  // 2) Birthday and sign; the sign is derived from the date, not drawn.
  static const char* kMonths[12] = {"January", "February", "March",     "April",
                                    "May",     "June",     "July",      "August",
                                    "September", "October", "November", "December"};
  const int month = 1 + int(rng_.below(12));
  const int day = 1 + int(rng_.below(28));
  out.push_back(std::string("My birthday is ") + kMonths[month - 1] + " " +
                std::to_string(day) + ". I am a " + zodiacOf(month, day) + ".");

  // 3) The hero's own name, already drawn by createLife().
  out.push_back("My name is " + h.firstName + " " + h.lastName + ".");

  // 4/5) Both parents: a real career row (weighted by how common it is) and an age.
  // The two parents are also the first entries of the family model: the yearly
  // lifecycle lines talk about them by relation and need their ages and jobs
  // (section 37). Parents are never named inside those lines, so name stays empty.
  {
    // One draw per parent first, then that parent's career: the original order,
    // so every seed keeps its stream.
    const std::string motherName = drawFirst(true);
    Person mother;
    mother.relation = "mother";
    mother.female = true;
    mother.age = motherAge;
    mother.lastName = h.lastName;
    mother.job = career();
    life.family.push_back(mother);
    out.push_back("My mother is " + motherName + " " + h.lastName + ", a " + mother.job +
                  " (age " + std::to_string(motherAge) + ").");
    const std::string fatherName = drawFirst(false);
    Person father;
    father.relation = "father";
    father.age = fatherAge;
    father.lastName = h.lastName;
    father.job = career();
    life.family.push_back(father);
    out.push_back("My father is " + fatherName + " " + h.lastName + ", a " + father.job +
                  " (age " + std::to_string(fatherAge) + ").");
  }

  // 6) An older or younger sibling, about half the time.
  if (rng_.chance(0.55)) {
    const bool older = rng_.chance(0.5);
    const bool sister = rng_.chance(0.5);
    const int age = older ? 1 + int(rng_.below(6)) : 1 + int(rng_.below(3));
    Person sib;
    // The lifecycle lines use "big"/"little", the same words the real log does
    // ("My little brother, Jimbo, graduated from high school.").
    sib.relation = std::string(older ? "big " : "little ") + (sister ? "sister" : "brother");
    sib.name = drawFirst(sister);
    sib.lastName = h.lastName;
    sib.female = sister;
    sib.age = age;
    life.family.push_back(sib);
    out.push_back("I have " + std::string(older ? "an older " : "a younger ") +
                  std::string(sister ? "sister" : "brother") + " named " + sib.name + " (age " +
                  std::to_string(age) + ").");
  }

  // 7) A family pet, about two thirds of the time: a domestic row from the pet
  // table plus a name from the pet-name table for that same animal.
  if (rng_.chance(0.6)) {
    const Table& pets = tbl("a16_pets-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < pets.size(); ++r) {
      if (!trim(pets.at(r, "Wild")).empty() || !trim(pets.at(r, "Exotic")).empty() ||
          !trim(pets.at(r, "Black Market")).empty()) continue;
      pool.push_back(r);
    }
    const size_t row = weightedPickOf(pets, "Commonness Score", pool, rng_);
    if (row != size_t(-1)) {
      const std::string animal = lower(trim(pets.at(row, "Animal")));
      const Table& pn = tbl("a16_pet-names-enc");
      std::vector<size_t> names;
      for (size_t r = 0; r < pn.size(); ++r) {
        if (lower(trim(pn.at(r, "Animal"))) == animal) names.push_back(r);
      }
      const size_t pick = pickFrom(names, rng_);
      if (pick != size_t(-1)) {
        out.push_back("We have a family " + animal + " named " + pn.at(pick, "Name") + ".");
      }
    }
  }
  return out;
}


// The name pool every family member and every unnamed extra in a scenario draws
// from: a16_casino-casino-celebrity-name-enc, matched to the wanted gender.
// Extracted from birthLines' original lambda so the lifecycle lines can draw a
// name for a new sibling too (report section 37).
std::string Sim::drawFirstName(bool female) {
  ensureTable("a16_casino-casino-celebrity-name-enc");
  const Table& names = tbl("a16_casino-casino-celebrity-name-enc");
  const std::string want = female ? "female" : "male";
  std::vector<size_t> pool;
  for (size_t r = 0; r < names.size(); ++r) {
    if (lower(trim(names.at(r, "Gender"))) == want) pool.push_back(r);
  }
  const size_t p = pickFrom(pool, rng_);
  return p == size_t(-1) ? std::string() : names.at(p, "First Name");
}

// The local physicians of the illness chain carry surnames only ("Dr. Seaman",
// "Dr. Coleman" in the capture). The game fills its own pool from the same
// celebrity name table the port draws personal names from, so the surname column
// is the right source.
std::string Sim::drawSurname() {
  ensureTable("a16_casino-casino-celebrity-name-enc");
  const Table& names = tbl("a16_casino-casino-celebrity-name-enc");
  if (names.size() == 0) return std::string();
  return names.at(rng_.below(uint32_t(names.size())), "Last Name");
}

// A company name built the way the game builds one. a16_company-name-structure-enc
// holds the per-industry format ('<color> <suffix>', '<color> <suffix>, <city>
// <suffix>') and, for the industries that ship fixed names, a 'Predefined' name
// that wins over the format. The trace line "My mother started a new position
// as Travel Associate for Helsinki Travel." is the '<city> <suffix>' case.
std::string Sim::companyName(const std::string& companyType, const Life& life) {
  ensureTable("a16_company-name-structure-enc");
  const Table& st = tbl("a16_company-name-structure-enc");
  const std::string want = lower(trim(companyType));
  std::vector<size_t> pool;
  for (size_t r = 0; r < st.size(); ++r) {
    if (lower(trim(st.at(r, "Company Type"))) == want) pool.push_back(r);
  }
  const size_t row = pickFrom(pool, rng_);
  if (row == size_t(-1)) return std::string();
  const std::string pre = trim(st.at(row, "Predefined"));
  const auto replaceAll = [](std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
      s.replace(at, from.size(), to);
    return s;
  };
  // The Format cell and the Suffix / Prefix cells hold comma-separated
  // alternatives, not one string: 'Aircraft Dealer' is '<color> <suffix>,
  // <city> <suffix>', i.e. two different formats for one industry.
  const auto pickList = [this](const std::string& list) {
    std::vector<std::string> parts;
    splitOn(list, ',', parts);
    return parts.empty() ? std::string() : trim(parts[rng_.below(uint32_t(parts.size()))]);
  };
  std::string fmt = pre;
  if (fmt.empty()) fmt = pickList(st.at(row, "Format"));
  if (fmt.empty()) return std::string();
  if (fmt.find("<color>") != std::string::npos) {
    ensureTable("a16_company-name-color-enc");
    const Table& colors = tbl("a16_company-name-color-enc");
    if (colors.size()) fmt = replaceAll(fmt, "<color>", trim(colors.at(rng_.below(uint32_t(colors.size())), "Color")));
  }
  if (fmt.find("<suffix>") != std::string::npos) fmt = replaceAll(fmt, "<suffix>", pickList(st.at(row, "Suffix")));
  if (fmt.find("<prefix>") != std::string::npos) fmt = replaceAll(fmt, "<prefix>", pickList(st.at(row, "Prefix")));
  if (fmt.find("<city>") != std::string::npos) fmt = replaceAll(fmt, "<city>", life.hero.city);
  if (fmt.find("<random-first-name>") != std::string::npos)
    fmt = replaceAll(fmt, "<random-first-name>", drawFirstName(rng_.chance(0.5)));
  if (fmt.find("<random-first-name-possessive>") != std::string::npos)
    fmt = replaceAll(fmt, "<random-first-name-possessive>", drawFirstName(rng_.chance(0.5)) + "'s");
  if (fmt.find("<random-surname>") != std::string::npos || fmt.find("<surname2>") != std::string::npos) {
    ensureTable("a16_casino-casino-celebrity-name-enc");
    const Table& names = tbl("a16_casino-casino-celebrity-name-enc");
    const std::string sur =
        names.size() ? names.at(rng_.below(uint32_t(names.size())), "Last Name") : std::string();
    fmt = replaceAll(fmt, "<random-surname>", sur);
    fmt = replaceAll(fmt, "<surname2>", sur);
  }
  // Anything a format still asks for is a pool the port does not have: drop the
  // token rather than leak it into the line.
  while (true) {
    const size_t at = fmt.find('<');
    if (at == std::string::npos) break;
    const size_t close = fmt.find('>', at);
    if (close == std::string::npos) break;
    fmt.erase(at, close - at + 1);
  }
  while (fmt.find("  ") != std::string::npos) fmt = replaceAll(fmt, "  ", " ");
  return trim(fmt);
}

// The per-person lifecycle lines of one year. Every sentence is one of the real
// templates in global-metadata.dat (#9372-#9429, report section 36): the game
// renders them in the second person as a scenario body ("Your little brother,
// Jimbo, has been diagnosed with mad cow disease.") and in the first person in
// the log ("My little brother, Jimbo, has been diagnosed with mad cow
// disease."). This emits the log form; the hero's own sentences are separate.
// Ages the family, so it runs once per year.
std::vector<std::string> Sim::lifecycleLines(Life& life) {
  hero_ = &life.hero;
  life_ = &life;
  std::vector<std::string> out;
  ensureTable("a16_careers-enc");
  ensureTable("a16_disease-types-enc");
  const Table& careers = tbl("a16_careers-enc");
  const Table& diseases = tbl("a16_disease-types-enc");
  // The stage the hero's own country starts children in: trace_fresh (US) has
  // "started elementary school", trace_game3 (Finland) "started primary school".
  const bool usa = lower(trim(life.hero.country)) == "united states";
  std::vector<Person> newborns;
  for (size_t i = 0; i < life.family.size(); ++i) {
    Person& p = life.family[i];
    if (!p.alive) continue;
    ++p.age;
    // Parents are never named in these lines, so the comma pair only appears
    // when there is a name to set off ("My little brother, Jimbo, ...").
    const std::string who =
        "My " + p.relation + (p.name.empty() ? std::string() : ", " + p.name + ",");

    // Death comes before everything else the relative might do this year. The
    // game's own person template is 'Your {0}, {1}, passed away at the age of
    // {2}.' (literal #50746) or, without a name, 'Your {0} passed away at the
    // age of {1}.' (#50594); the hero's family lines are first person in the
    // port, the shape '#30974 My {0}, {1}, died of starvation.' already uses.
    // shortcut: the mortality curve (0.5%/year from 30, 2%/year from 45,
    // +0.4%/year after) is port-side; the game's per-relative life table has not
    // been read. The young tail is what makes a childhood orphan possible at
    // all -- without it the hero's parents never die before he turns 18.
    const double deathChance =
        p.age < 30 ? 0.0 : (p.age < 45 ? 0.005 : 0.02 + 0.004 * double(p.age - 45));
    if (p.age >= 30 && familyRng_.chance(deathChance)) {
      out.push_back(who + " passed away at the age of " + std::to_string(p.age) + ".");
      p.alive = false;
      continue;
    }

    // A mother can have another child. The real line names both parents, the
    // baby, the new relation and whether it was planned: "My mother and my
    // stepfather had a baby girl named Inka Tuominen, my new half sister. She
    // was an accidental pregnancy." The port's two parents are the biological
    // ones, so the new sibling is a full one.
    if ((p.relation == "mother" || p.relation == "stepmother") && life.hero.age >= 1 &&
        p.age >= 20 && p.age <= 45 && rng_.chance(0.05)) {
      const bool sister = rng_.chance(0.5);
      const std::string baby = drawFirstName(sister);
      const std::string other = p.relation == "mother" ? "father" : "stepfather";
      const bool planned = rng_.chance(0.5);
      out.push_back(who + " and my " + other + " had a baby " + (sister ? "girl" : "boy") +
                    " named " + baby + " " + life.hero.lastName + ", my new " +
                    (sister ? "sister" : "brother") + ". " + (sister ? "She" : "He") + " was " +
                    (planned ? "a planned" : "an accidental") + " pregnancy.");
      Person nb;
      nb.relation = sister ? "little sister" : "little brother";
      nb.name = baby;
      nb.lastName = life.hero.lastName;
      nb.female = sister;
      newborns.push_back(nb);
      continue;
    }

    // School milestones first: they are pinned to one age, so a chance-based
    // illness must not steal the year they happen in. The templates are the
    // '#9405 started <high-school>' and '#9385 graduated from <high-school>'
    // ones; the stage the hero's own country uses comes first ("elementary
    // school" in the US, "primary school" elsewhere: trace_fresh vs trace_game3).
    if (p.age == 6) {
      out.push_back(who + " started " + (usa ? "elementary school" : "primary school") + ".");
      continue;
    }
    if (p.age == 18) {
      out.push_back(who + " graduated from high school.");
      continue;
    }

    // Illness: the '#9394 is no longer suffering from <disease-old>' recovery
    // first, then a fresh '#9389 has been diagnosed with <disease-new>'.
    if (!p.disease.empty()) {
      if (rng_.chance(0.5)) {
        out.push_back(who + " is no longer suffering from " + p.disease + ".");
        p.disease.clear();
      }
      continue;
    }
    if (p.age >= 1 && rng_.chance(0.07)) {
      std::vector<size_t> pool;
      for (size_t r = 0; r < diseases.size(); ++r) {
        if (diseases.num(r, "Contracts Organically Score") <= 0) continue;
        const double minA = diseases.num(r, "Minimum Age");
        const double maxA = diseases.num(r, "Maximum Age");
        if (minA > 0 && p.age < int(minA)) continue;
        if (maxA > 0 && p.age > int(maxA)) continue;
        pool.push_back(r);
      }
      const size_t row = weightedPickOf(diseases, "Contracts Organically Score", pool, rng_);
      if (row != size_t(-1)) {
        std::string d = lower(trim(diseases.at(row, "Disease")));
        if (!trim(diseases.at(row, "Uses Indefinite Article")).empty() && !d.empty()) {
          d = (std::strchr("aeiou", d[0]) ? "an " : "a ") + d;
        }
        p.disease = d;
        out.push_back(who + " has been diagnosed with " + d + ".");
        continue;
      }
    }

    // Work: '#9408 started a new position as <job-title> for <company>' and, on
    // a later draw, '#9390 has been promoted to <job-title>'. The title ladder is
    // the career's own Seniority Track ("Apprentice Cameraman, Cameraman, ...").
    if (p.age < 18) continue;
    if (p.job.empty()) {
      const size_t row = weightedPick(careers, "Commonness Score", rng_);
      if (row == size_t(-1)) continue;
      std::vector<std::string> track;
      splitOn(careers.at(row, "Seniority Track"), ',', track);
      // 'Seniority Track' carries a real ladder for 5 careers and a class word
      // for the rest ('None' 57 rows, 'Abbreviated' 29, 'Working Class' 24,
      // 'Corporate' 10). The port has no evidence of the per-class ladders, so
      // those rows use their own career name.
      // shortcut: class-word tracks ('None'/'Abbreviated'/'Working Class'/
      // 'Corporate') do not produce a ladder; upgrade when that ladder table is
      // located.
      const std::string first = track.empty() ? std::string() : trim(track.front());
      const bool ladder = !first.empty() && first != "None" && first != "Abbreviated" &&
                          first != "Working Class" && first != "Corporate";
      const std::string title = ladder ? first : trim(careers.at(row, "Career"));
      const std::string comp = companyName(careers.at(row, "Company Type"), life);
      p.job = title;
      p.company = comp;
      out.push_back(who + " started a new position as " + title +
                    (comp.empty() ? std::string() : " for " + comp) + ".");
      continue;
    }
    if (!rng_.chance(0.05)) continue;
    bool promoted = false;
    for (size_t r = 0; r < careers.size() && !promoted; ++r) {
      std::vector<std::string> track;
      splitOn(careers.at(r, "Seniority Track"), ',', track);
      for (size_t k = 0; k + 1 < track.size(); ++k) {
        if (trim(track[k]) != p.job) continue;  // class words carry no ladder
        p.job = trim(track[k + 1]);
        out.push_back(who + " has been promoted to " + p.job + ".");
        promoted = true;
        break;
      }
    }
  }
  for (Person& nb : newborns) life.family.push_back(nb);
  // With no surviving parent the hero is sent to an orphanage: literal #70722
  // '{0}I have been sent to an orphanage for the remainder of {1} childhood.'.
  if (!life.hero.orphan && life.hero.age < 18) {
    bool mother = false, father = false;
    for (const Person& p : life.family) {
      // A surviving parent is one that is still in the list and alive; the
      // relation is what the line calls them, the flag is what matters here.
      if (p.relation == "mother" && p.alive) mother = true;
      if (p.relation == "father" && p.alive) father = true;
    }
    if (!mother && !father) {
      life.hero.orphan = true;
      out.push_back("I have been sent to an orphanage for the remainder of my childhood.");
    }
  } else if (life.hero.orphan && life.hero.age >= 18) {
    // shortcut: the game has no literal for leaving the orphanage (its
    // orphanage has its own menu), so the state simply ends at 18.
    life.hero.orphan = false;
  }
  return out;
}

bool Sim::pinCountry(Life& life, const std::string& countryName) {
  const std::string want = lower(trim(countryName));
  const Country* found = nullptr;
  for (const Country& c : countries_) {
    if (lower(trim(c.name)) == want) { found = &c; break; }
  }
  if (!found) return false;
  Hero& h = life.hero;
  h.country = found->name;
  h.continent = found->continent;
  // The country's own city list; a country with none leaves the city unset
  // rather than inventing one.
  h.city = found->cities.empty() ? std::string()
                                 : found->cities[rng_.below(uint32_t(found->cities.size()))];
  return true;
}

bool Sim::scenarioEligible(const Table& t, size_t row, const Life& life) const {
  const Hero& h = life.hero;
  if (t.has("Hero Min Age") && h.age < int(t.num(row, "Hero Min Age"))) return false;
  // An empty Hero Max Age means 'no upper bound', not 'zero': 151 of the
  // universal table's 258 rows and every row of the vampire pool leave it empty,
  // and reading those as 0 excluded them from every natural life.
  if (t.has("Hero Max Age")) {
    const double maxAge = t.num(row, "Hero Max Age");
    if (maxAge > 0 && h.age > int(maxAge)) return false;
  }

  const std::string gender = lower(trim(t.has("Hero Gender") ? t.at(row, "Hero Gender") : ""));
  if (gender == "male" && h.female) return false;
  if (gender == "female" && !h.female) return false;

  const bool student = h.age < 18;
  if (t.flag(row, "Student") && !student) return false;
  // 'Job' is a requirement flag like 'Student' ('x' = row needs the condition),
  // not an age proxy: an unemployed adult must not take a job-only row.
  if (t.flag(row, "Job") && !h.employed) return false;
  // 'Vampire' gates the DLC state: with the vampire branch (REPORT.en.md 48) the
  // port can now honour it instead of skipping every row that asks for it.
  if (t.flag(row, "Vampire") && !h.vampire) return false;
  // 'Hero Has Familiar' is the vampire pool's own companion gate.
  if (t.has("Hero Has Familiar") && t.flag(row, "Hero Has Familiar") && h.familiar.empty()) return false;
  return true;
}

bool Sim::fillerEligible(const Table& t, size_t row, const Life& life) const {
  const Hero& h = life.hero;
  const bool student = h.age < 18;
  const std::string condition = trim(t.at(row, "Condition"));
  if (!condition.empty() && condition != "Student" && condition != "Job") return false;
  if (condition == "Student" && !student) return false;
  if (condition == "Job" && student) return false;
  if (t.has("Hero Min Age") && h.age < int(t.num(row, "Hero Min Age"))) return false;
  if (t.has("Hero Max Age") && t.num(row, "Hero Max Age") > 0 &&
      h.age > int(t.num(row, "Hero Max Age"))) return false;
  const std::string gender = lower(trim(t.at(row, "Hero Gender")));
  if (gender == "male" && h.female) return false;
  if (gender == "female" && !h.female) return false;
  return true;
}

void Sim::setDate(int month, int day) {
  dateMonth_ = (month >= 1 && month <= 12) ? month : 0;
  dateDay_ = (dateMonth_ && day >= 1 && day <= 31) ? day : 0;
}

// The game's yearly "push" dumb scenario comes out of more than one set:
// DumbScenarioInterface.GetARandomPushDumbScenario (il2cpp_dump.txt:56500,
// @0x73f2eb3238) builds the per-activity dictionaries, then gates on
// CurrentlyInPrison / IsARoyal / CurrentlyHasAJob / get_TeacherArray /
// get_StepchildArray / MinutesSinceLastCalendarDumbScenario before falling back
// to the universal set. Evidence and the disassembly walk-through: REPORT.en.md
// section 25.
//
// shortcut: prison, royal, school (teacher), family (stepchild), friend and
// vacation rows are real sets the game consults but the port models none of the
// hero state they gate on, so they are left out rather than approximated. The
// calendar and holiday sets are real-world-date gated (one date per real day)
// and join the pool only when setDate() fixed a date. Every in-pool pick stays
// uniform: no dumb-scenarios table has a Commonness Score column.
// The dumb-scenario set tables, by their real gamedata_plain file names.
const char* const kDumbUniversal = "a16_dumb-scenarios-universal-enc";
const char* const kDumbCareer = "a16_dumb-scenarios-career-enc";
const char* const kDumbHoliday = "a16_dumb-scenarios-holiday-enc";
const char* const kDumbCalendar = "a16_dumb-scenarios-calendar-enc";
const char* const kDumbFriend = "a16_dumb-scenarios-friend-enc";
const char* const kDumbPrison = "a16_dumb-scenarios-prison-enc";
const char* const kDumbOrphan = "a16_dumb-scenarios-orphan-enc";
const char* const kDumbRoyal = "a16_dumb-scenarios-royal-enc";

std::vector<size_t> Sim::dumbSetRows(const char* name, const Life& life) {
  std::vector<size_t> rows;
  // Only the holiday, calendar and career sets carry a gate beyond the table's
  // own columns. The career set has no gate columns at all: its 'Type' column IS
  // the career name the game matches against the hero's job
  // (GetRandomCareerDumbScenarioForCareer, 0x73f2ebec08).
  const bool dated = (std::string(name) == kDumbHoliday || std::string(name) == kDumbCalendar);
  const bool career = (std::string(name) == kDumbCareer);
  if (career && !life.hero.employed) return rows;
  if (dated && dateMonth_ == 0) return rows;
  if (!ensureTable(name)) return rows;
  const Table* t = table(name);
  if (t == nullptr) return rows;
  for (size_t r = 0; r < t->size(); ++r) {
    if (!scenarioEligible(*t, r, life)) continue;
    if (career && trim(t->at(r, "Type")) != life.hero.job) continue;
    if (dated && !dumbDateMatches(*t, r, dateMonth_, dateDay_)) continue;
    rows.push_back(r);
  }
  return rows;
}

bool Sim::pickDumbScenario(const Life& life, std::string* table, size_t* row) {
  // DumbScenarioInterface.GetARandomPushDumbScenario (il2cpp_dump.txt:56500,
  // 0x73f2eb3238) is not one uniform pick over a merged pool: every attempt
  // draws GetRandomNumber(0,6) and consults that category's set
  // (0x73f2eb3430). A category whose gate fails, or whose set yields nothing,
  // falls to 0x73f2eb4ce0 -- 'add w26,w26,#1; cmp w26,#0x64; b.ne 0x73f2eb3414'
  // -- i.e. it re-rolls the category, up to 100 attempts, and the function then
  // returns null (0x73f2eb5074). Within one set the row pick is uniform: the
  // set tables carry no Commonness Score.
  // shortcut: orphan (k=0), royal and military are categories the game gated on
  // hero state the port does not model, so they read as "gate failed" and
  // re-roll; k=1 is the prison set and is now gated on Hero::inPrison -- which is exactly what the game does for a hero that
  // is none of them. The k=4..5 tail is the orphan/military/career tail; the
  // port routes k=4 to the career set when the hero has a job and k=5 to the
  // universal set, so every set stays reachable until that tail is mapped.
  // The friend set is likewise treated as non-empty: the game's
  // GetRandomFriendDumbScenario yields nothing for a hero with no friends and
  // then re-rolls, and the port keeps no friend list yet.
  for (int attempt = 0; attempt < 100; ++attempt) {
    const int k = int(rng_.below(6));
    // shortcut: the real gates of the remaining categories during a prison
    // sentence are not read, so a prison year only accepts the prison set and
    // re-rolls the rest. Until they are read the port cannot tell whether the
    // game also serves calendar or universal rows to an inmate.
    if (life.hero.inPrison && k != 1) continue;
    const char* set = nullptr;
    if (k == 0) {
      // Orphan set (0x73f2eb3574): accepted only once both parents are dead.
      if (!life.hero.orphan) continue;
      set = kDumbOrphan;
    } else if (k == 1) {
      // In prison the category is the prison set itself. Not in prison -> the
      // holiday/universal roulette at 0x73f2eb3734:
      // GetRandomNumber(0,4); > 2, or no holiday season, lands on the universal
      // set, otherwise the season's holiday rows.
      if (life.hero.inPrison) { set = kDumbPrison; }
      else {
      const int j = int(rng_.below(4));
      set = (j > 2 || dateMonth_ == 0) ? kDumbUniversal : kDumbHoliday;
      }
    } else if (k == 2) {
      set = kDumbFriend;  // 0x73f2eb369c, gated on !prison && !royal
    } else if (k == 3) {
      set = kDumbCalendar;  // 0x73f2eb34a0 -> GetTodaysCalendarDumbScenario
    } else if (k == 4) {
      set = kDumbCareer;
    } else if (life.hero.royal) {
      // The royal set, one of the game's own dumb-scenario categories
      // (DUMB_SCENARIO_TYPE_ROYAL, literal #17018). shortcut: the k -> set
      // mapping for this bucket was not read out of
      // GetARandomPushDumbScenario, and the birth roll behind the state sits
      // behind an indirect IL2CPP call (report 47), so the port accepts the
      // royal set whenever the script asked for a royal life and otherwise
      // leaves the universal tail as it was.
      set = kDumbRoyal;
    } else {
      set = kDumbUniversal;
    }
    const std::vector<size_t> rows = dumbSetRows(set, life);
    const size_t r = pickFrom(rows, rng_);  // empty set -> -1 -> re-roll
    if (r == size_t(-1)) continue;
    *table = set;
    *row = r;
    return true;
  }
  return false;
}

bool Sim::ageUp(Life& life, AgeUp& out, int forcedChoice) {
  hero_ = &life.hero;
  life_ = &life;
  rndCache_.clear(); rbLast_.clear();  // one scenario per year: the '<rbN>' cache starts empty
  ++life.year;
  Hero& h = life.hero;
  ++h.age;
  if (h.age == 1) applyNewbornSetup(h);
  // The family's own lines for the year, written before the scenario's
  // (trace_game9 age 7: the mother's diagnosis, then the scenario line).
  out.extra = lifecycleLines(life);
  if (getenv("BL_DBG")) fprintf(stderr, "[dbg] age %d family %zu extra %zu\n", h.age, life.family.size(), out.extra.size());
  const bool student = h.age < 18;
  h.employed = !student;
  // A hero who leaves school takes a real job, drawn once from a16_careers-enc
  // ('Career' + 'Commonness Score' + 'Min Age'/'Max Age'). The career name is
  // what the career dumb-scenario set matches its own 'Type' column against
  // (DumbScenarioInterface.GetARandomPushDumbScenario -> get_Career()).
  // shortcut: the table's gates the port cannot evaluate yet (Required
  // Education, Required Major, licences, criminal record, per-career attribute
  // minimums) are ignored, and there is no job change, promotion or firing.
  if (!h.employed) {
    h.job = "Student";
  } else if (h.job.empty() || h.job == "Student") {
    const Table& t = tbl("a16_careers-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Min Age") && h.age < int(t.num(r, "Min Age"))) continue;
      if (t.has("Max Age") && t.num(r, "Max Age") > 0 && h.age > int(t.num(r, "Max Age"))) continue;
      const std::string gender = lower(trim(t.has("Gender") ? t.at(r, "Gender") : ""));
      if (gender == "male" && h.female) continue;
      if (gender == "female" && !h.female) continue;
      if (t.has("Unavailable") && t.flag(r, "Unavailable")) continue;
      pool.push_back(r);
    }
    const size_t row = weightedPickOf(t, "Commonness Score", pool, rng_);
    h.job = row == size_t(-1) ? "Worker" : trim(t.at(row, "Career"));
  }

  // (0) A forced scenario class (the Phase 1 bench replaying a scripted event
  //     the game presented) wins over every draw below and consumes no RNG
  //     beyond the name draws its own text needs.
  if (!forcedClass_.empty()) {
    const std::string cls = forcedClass_;
    const int c = forcedClassChoice_;
    forcedClass_.clear();
    forcedClassChoice_ = -1;
    if (renderScenarioClass(cls, life, out, c)) return true;
  }

  // (0b) A forced row (the Phase 1 bench replaying the game's own pick) wins over
  //     every draw below and consumes no RNG, so the rest of the stream is
  //     unaffected by the override.
  if (forcedRow_ > 0) {
    const std::string name = forcedTable_;
    const size_t row = size_t(forcedRow_ - 1);
    forcedTable_.clear();
    forcedRow_ = 0;
    if (ensureTable(name)) {
      const Table* ft = table(name);
      if (ft && renderRow(*ft, row, life, out, forcedChoice)) {
        return true;
      }
    }
    // An unusable override falls through to the normal draw instead of losing
    // the year: the bench wants a diff, not a silent hole.
  }

  // (0b) Prison: the yearly conviction draw, then the prison years themselves.
  //      The conviction draw runs on its own stream (Sim::prisonRng_), so the
  //      main stream -- and therefore every earlier subsystem's draws for a
  //      given seed -- is untouched (REPORT.en.md 45).
  if (life.hero.vampire && vampireYear(life, out, forcedChoice)) return true;
  if ((life.hero.military || life.hero.militaryPending) && militaryYear(life, out, forcedChoice)) return true;
  if (careerYear(life, out, forcedChoice)) return true;
  if (sportsYear(life, out, forcedChoice)) return true;
  if (businessYear(life, out, forcedChoice)) return true;
  if (musicYear(life, out, forcedChoice)) return true;
  if (studioYear(life, out, forcedChoice)) return true;
  if (tradeYear(life, out, forcedChoice)) return true;
  if (politicalYear(life, out, forcedChoice)) return true;
  if (miscPoolYear(life, out, forcedChoice)) return true;
  if (relationshipYear(life, out, forcedChoice)) return true;
  if (spyYear(life, out, forcedChoice)) return true;
  if (fightYear(life, out, forcedChoice)) return true;
  if (bmYear(life, out, forcedChoice)) return true;
  if (casinoYear(life, out, forcedChoice)) return true;
  if (cultYear(life, out, forcedChoice)) return true;
  if (dealerYear(life, out, forcedChoice)) return true;
  if (mafiaYear(life, out, forcedChoice)) return true;
  if (zooYear(life, out, forcedChoice)) return true;
  if (landlordYear(life, out, forcedChoice)) return true;
  if (petYear(life, out, forcedChoice)) return true;
  if (life.hero.outdoors && outdoorYear(life, out, forcedChoice)) return true;
  if (life.hero.hustle && hustlerYear(life, out, forcedChoice)) return true;
  if (life.hero.astronaut && astroYear(life, out, forcedChoice)) return true;
  if (maybeConvicted(life, out)) return true;
  if (life.hero.inPrison) return prisonYear(life, out, forcedChoice);

  // (1) Job-flavoured line from the only real core-loop table that carries a
  //     Commonness Score: a16_employee-recognition-scenarios-enc.
  if (h.employed && rng_.chance(0.25)) {
    const Table& t = tbl("a16_employee-recognition-scenarios-enc");
    const size_t row = weightedPick(t, "Commonness Score", rng_);
    if (row != size_t(-1)) {
      // The year's family lifecycle lines were written before the scenario was
      // picked and belong to no row, so they survive this reset.
      const std::vector<std::string> extra = out.extra;
      out = AgeUp();
      out.extra = extra;
      out.table = t.name();
      out.row_id = std::to_string(row + 1);
      out.narrative = resolve(t.at(row, "Scenario"));
      return true;
    }
  }

  // (2) Age- and gender-gated choice scenario: the dumb-scenario category
  //     roulette the game runs each year (see pickDumbScenario).
  {
    std::string table;
    size_t row = 0;
    if (pickDumbScenario(life, &table, &row)) {
      const Table& t = tbl(table.c_str());
      return renderRow(t, row, life, out, forcedChoice);
    }
  }

  // (3) Plain narration for a year that no choice scenario covers:
  //     a16_hero-filler-enc.
  {
    const Table& t = tbl("a16_hero-filler-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) if (fillerEligible(t, r, life)) pool.push_back(r);
    const size_t row = pickFrom(pool, rng_);
    if (row != size_t(-1)) {
      // The year's family lifecycle lines were written before the scenario was
      // picked and belong to no row, so they survive this reset.
      const std::vector<std::string> extra = out.extra;
      out = AgeUp();
      out.extra = extra;
      out.table = t.name();
      out.row_id = std::to_string(row + 1);
      out.narrative = resolve(t.at(row, "String"));
      return true;
    }
  }
  return false;
}

bool Sim::renderRow(const Table& t, size_t row, const Life& life, AgeUp& out, int forcedChoice) {
  hero_ = &life.hero;
  life_ = &life;
  (void)life;  // the row's text is already gender-gated by scenarioEligible()
  rndCache_.clear(); rbLast_.clear();  // the body is parsed first, its draws feed the log line
  relNameCache_.clear();  // one <name> per card
  spyCache_.clear();      // one target / client / colleague name per spy card
  fightCache_.clear();    // one circumstance phrase per fight card
  vampCache_.clear();     // one victim / council member per vampire card
  outdoorCache_.clear();  // one cave / plant / bird per outdoor card
  hustlerCache_.clear();  // one person / victim / insult per hustle card
  astroCache_.clear();    // one crewmate / body / discovery per space card
  if (hero_ != nullptr) const_cast<Hero*>(hero_)->vampireVictim.clear();
  bmCache_.clear();       // one item / vendor / auctioneer per black-market card
  casinoGambler_.clear(); // one punter per casino card
  casinoRoom_.clear();
  spyAgentIsOther_ = false;
  if (row >= t.size()) return false;
  // The year's family lifecycle lines were written before the scenario was
  // picked and belong to no row, so they survive this reset.
  const std::vector<std::string> extra = out.extra;
  out = AgeUp();
  out.extra = extra;
  out.table = t.name();
  out.row_id = std::to_string(row + 1);

  // Narrative-only shapes: the filler table has 'String', the employee one
  // 'Scenario'. They carry no choice columns, so there is nothing to branch on.
  // A table with a CTA or numbered choices is a card even when it names neither
  // a Title nor a Body (a16_outdoor-bike-ride-dumb-enc, whose opening paragraph
  // is "Scenario" and whose buttons are still 'Choice N' / 'Narrative N').
  const bool hasButtons = t.has("CTA") || t.has("ChoiceList") || t.has("Choice 1") ||
                          t.has("Choice 2") || t.has("Choice 3") || t.has("Choice 4");
  if (!hasButtons && !t.has("Title") && !t.has("Body")) {
    // Narrative-only tables keep the card's wording in "Scenario" and, in the
    // menu families (a16_stepparent-cruelty-scenarios-enc, a16_pet-discipline-enc),
    // the first-person sentence the game writes into the LifeLog in "Log Version"
    // ("Your stepfather pranked you..." vs "My stepfather pranked me..."). The log
    // is what this port records, so the log column wins; a16_bully-scenarios-enc
    // and a16_first-kiss-reactions-enc spell it "Log".
    static const char* kNarrativeCols[] = {"Log Version", "Log", "Narrative", "Scenario", "String"};
    for (const char* c : kNarrativeCols) {
      if (t.has(c)) {
        // The log line refers back to the alternative the card drew ('I was not
        // raised to mate with <rb>'), and the alternative lives in the Scenario
        // column, so that column is resolved first even though only the log is
        // recorded (REPORT.en.md 57).
        // Every other column that holds a bracket is the card, whatever its
        // header is called ("Feedback" for a16_relationship-counseling-scenarios-enc,
        // "Feedback Scenario" for a16_school-classmate-flirt-scenarios-enc,
        // "Scenario" for the rest). They are resolved in column order, which is
        // the order the row's alternatives were drawn in, so '<rb>' picks the
        // right one (REPORT.en.md 57).
        for (const std::string& cc : t.columns()) {
          if (cc == c) continue;
          const std::string& cell = t.at(row, cc);
          if (cell.find('<') == std::string::npos) continue;
          const std::string card = resolve(cell);
          if (!card.empty()) out.body = card;
        }
        out.narrative = resolve(t.at(row, c));
        return true;
      }
    }
    return false;
  }

  // The dumb scenario tables (a16_outdoor-hiking-dumb-scenario-enc and its
  // camping / bike-ride siblings, a16_outdoor-cave-discoverie-enc) call the
  // card's opening paragraph "Scenario" instead of "Body". It has to be read
  // here, before the narrative, because the narrative refers back to the
  // alternative the paragraph drew (REPORT.en.md 71).
  out.title = resolve(scriptedFamilyTokens(t.at(row, "Title")));
  out.body = resolve(scriptedFamilyTokens(t.has("Body") ? t.at(row, "Body") : t.at(row, "Scenario")));
  out.cta = resolve(t.at(row, "CTA"));

  std::vector<int> choices;  // 1-based choice numbers that are filled in
  for (int n = 1; n <= 4; ++n) {
    const std::string col = "Choice " + std::to_string(n);
    if (t.has(col) && !trim(t.at(row, col)).empty()) choices.push_back(n);
  }

  // The scripted-scenario table packs its buttons into one '[Yes/No/Maybe]' cell
  // instead of the dumb tables' 'Choice N' columns.
  // shortcut: only the label list is read -- Filter, HeroAge, ActionList,
  // Impacts and ResultNarratives of that table are not evaluated yet, so the log
  // line falls back to the body text.
  if (choices.empty() && t.has("ChoiceList")) {
    // The family tokens are substituted first: the list's own separators are
    // '/', and so is the one inside '<him/her>', so splitting the raw cell would
    // cut every button in half.
    const std::string list = trim(scriptedFamilyTokens(t.at(row, "ChoiceList")));
    if (list.size() > 2 && list.front() == '[' && list.back() == ']') {
      std::vector<std::string> labels;
      std::string cur;
      int angle = 0;  // any other <a/b> token stays one label
      for (size_t i = 1; i + 1 < list.size(); ++i) {
        if (list[i] == '<') ++angle;
        if (list[i] == '>') angle = angle > 0 ? angle - 1 : 0;
        if (list[i] == '/' && angle == 0) { labels.push_back(trim(cur)); cur.clear(); }
        else cur += list[i];
      }
      labels.push_back(trim(cur));
      std::vector<std::string> filled;
      for (const std::string& l : labels) if (!l.empty()) filled.push_back(l);
      if (!filled.empty()) {
        const size_t k = forcedChoice >= 0 && size_t(forcedChoice) < filled.size()
                             ? size_t(forcedChoice)
                             : rng_.below(uint32_t(filled.size()));
        out.choice = resolve(filled[k]);
      }
    }
  }
  if (!choices.empty()) {
    // A scripted run can name the branch it wants; anything else is the seeded
    // pick, so both paths stay reproducible from the seed.
    const int n = forcedChoice >= 0 && size_t(forcedChoice) < choices.size()
                      ? choices[size_t(forcedChoice)]
                      : choices[rng_.below(uint32_t(choices.size()))];
    const std::string suffix = " " + std::to_string(n);
    out.choice = resolve(t.at(row, "Choice" + suffix));
    out.narrative = resolve(t.at(row, "Narrative" + suffix));
    // The cells hold impact scores (-6..6), not points: the game maps each one
    // through AttributeAdjustmentPointsForImpactScore before touching the
    // attribute. The draw for a score of 0 is the only RNG here, and the game
    // makes it in this same order (Karma .. Appearance).
    out.karma = pointsForImpact(int(t.num(row, "Karma Impact" + suffix)));
    out.discipline = pointsForImpact(int(t.num(row, "Discipline Impact" + suffix)));
    out.happiness = pointsForImpact(int(t.num(row, "Happiness Impact" + suffix)));
    out.smarts = pointsForImpact(int(t.num(row, "Intelligence Impact" + suffix)));
    out.looks = pointsForImpact(int(t.num(row, "Appearance Impact" + suffix)));
  }
  return true;
}

// AttributeAdjustmentPointsForImpactScore (@0x73f32b140c): a 13-entry jump
// table indexed by the score clamped to -6..6. Read as a static table, not
// guessed: bytes at file offset 0xfc5e9c are
// [34, 0, 10, 12, 4, 16, 18, 14, 33, 8, 31, 2, 6] and the case bodies hold the
// point values below. The score <= -6 case falls through to the epilogue with
// w0 = -100. Score 0 draws one value: System.Random.Next(0,2) != 0 -> -1, else +1.
int Sim::pointsForImpact(int score) {
  if (score > 6) score = 6;
  if (score < -6) score = -6;
  switch (score) {
    case -6: return -100;
    case -5: return -50;
    case -4: return -30;
    case -3: return -16;
    case -2: return -8;
    case -1: return -4;
    case 0: return rng_.below(2) != 0 ? -1 : 1;
    case 1: return 4;
    case 2: return 8;
    case 3: return 16;
    case 4: return 30;
    case 5: return 50;
    default: return 100;  // 6
  }
}

bool Sim::ensureTable(const std::string& name) {
  if (table(name)) return true;
  try {
    Table t = Table::load(reDir_ + "/gamedata_plain/" + name);
    loaded_.push_back(name);
    tables_.emplace_back(name, std::move(t));
  } catch (const std::exception&) {
    return false;  // unreadable or missing: the caller falls back to the draw
  }
  return true;
}

bool Sim::forceScenario(const std::string& table, long long row) {
  if (row <= 0) {  // clear: the next age-up draws as usual
    forcedTable_.clear();
    forcedRow_ = 0;
    return true;
  }
  if (!ensureTable(table)) {
    std::fprintf(stderr, "forceScenario: table not loaded: %s\n", table.c_str());
    return false;
  }
  forcedTable_ = table;
  forcedRow_ = row;
  return true;
}

// The symptom card is the only scenario whose title, body and log line all come
// out of one table row: a16_disease-symptom-scenarios-enc (432 rows). The game
// shows the abbreviated symptom as the card title (falling back to the symptom
// itself, capitalised), the row's own 'Scenario' text plus the fixed question,
// and four literal option labels. Only the first log sentence is table-driven
// (the 'Log Version' column) - the branch sentence that follows it is built in
// code, and only two of the four variants were ever captured.
// shortcut: the third label (the home remedy) is modelled for two symptoms and
// the remedy branch logs no branch sentence; the second symptom sentence the
// game appends is a coin flip, not its own gate.
bool Sim::renderSymptomCard(const Life& life, AgeUp& out, int choice) {
  (void)life;
  ensureTable("a16_disease-symptom-scenarios-enc");
  const Table& t = tbl("a16_disease-symptom-scenarios-enc");
  std::vector<size_t> pool, supp;
  for (size_t r = 0; r < t.size(); ++r) {
    // The 169 rows whose Supplementary cell is set are the extra sentences the
    // game stacks on a card; only the others carry a card of their own.
    if (!trim(t.at(r, "Supplementary")).empty()) { supp.push_back(r); continue; }
    if (!trim(t.at(r, "Scenario")).empty()) pool.push_back(r);
  }
  if (pool.empty()) return false;
  const size_t row = pool[rng_.below(uint32_t(pool.size()))];
  const std::string symptom = lower(trim(t.at(row, "Symptom")));
  std::string title = trim(t.at(row, "Abbreviated Symptom"));
  if (title.empty() && !symptom.empty()) {
    title = symptom;
    title[0] = static_cast<char>(::toupper(static_cast<unsigned char>(title[0])));
  }
  std::string body = trim(t.at(row, "Scenario"));
  if (!supp.empty() && rng_.chance(0.5))
    body += " " + trim(t.at(supp[rng_.below(uint32_t(supp.size()))], "Scenario"));
  body += "\n\nWhat will you do?";

  const std::vector<std::string> extra = out.extra;
  out = AgeUp();
  out.extra = extra;
  out.table = "a16_disease-symptom-scenarios-enc";  // the row is the provenance
  out.row_id = std::to_string(row + 1);
  out.title = title;
  out.body = resolve(body);

  const bool water = symptom.find("fever") != std::string::npos ||
                     symptom.find("vomit") != std::string::npos ||
                     symptom.find("nausea") != std::string::npos ||
                     symptom.find("dehydrat") != std::string::npos;
  static const char* kLabel[4] = {"It doesn't bother me", "Go to the doctor", nullptr,
                                  "Search the web"};
  static const char* kTail[4] = {" I didn't let it bother me.", " I made a doctor's appointment.",
                                 "", " I searched the web for my symptoms."};
  const int idx = choice >= 0 && choice < 4 ? choice : int(rng_.below(4));
  out.choice = kLabel[idx] ? kLabel[idx] : (water ? "Chug some water" : "Take some aspirin");
  out.narrative = resolve(trim(t.at(row, "Log Version")) + kTail[idx]);
  return true;
}

bool Sim::renderScenarioClass(const std::string& cls, const Life& life, AgeUp& out, int choice) {
  life_ = &life;
  const RegScenario* s = findScenario(cls);
  if (!s) return false;
  // The symptom card draws its own title, body and log line from the symptom
  // table, so it never uses the registry's literals.
  if (cls == "ScenarioDiseaseContract") return renderSymptomCard(life, out, choice);
  // The year's family lifecycle lines were written before the scenario was
  // picked and belong to no row, so they survive this reset.
  const std::vector<std::string> extra = out.extra;
  out = AgeUp();
  out.extra = extra;
  out.table = cls;  // not a table row: the class is the provenance
  if (s->silent) {
    // Modal cards the game never logs (casino bets, the eye-exam test, the
    // profile screen): the card is rendered, the year writes no line.
    out.title.clear();
    return true;
  }
  out.title = s->title;
  std::string body = s->body;
  std::string item;
  substAll(body, "{parent}", "mother");
  if (body.find("{item}") != std::string::npos) {
    // The item the hero broke is a runtime value in the game (ScenarioEthics'
    // _brokenItem); the two variants below are the ones captured so far.
    // shortcut: two-variant item list, extend as more captures land.
    static const char* kItems[] = {"mailbox", "bedroom window"};
    item = kItems[rng_.below(2)];
    substAll(body, "{item}", item);
  }
  std::string name;
  if (body.find("{name}") != std::string::npos) {
    name = personName("classmate");
    substAll(body, "{name}", name);
  }

  std::string channelBare;
  // The channel card spells the bare name ("Your friends are all joining Twitch.")
  // while the log spells it with the article ("I signed up for a Twitch account."),
  // so both forms come out of one draw.
  bool wantsChannel = body.find("{channel}") != std::string::npos ||
                      body.find("{channelbare}") != std::string::npos;
  for (int i = 0; !wantsChannel && i < s->n; ++i)
    if (std::string(s->choices[i].log).find("{channel}") != std::string::npos) wantsChannel = true;
  std::string channel;
  if (wantsChannel) {
    // The channel is a real table (a16_social-media-channels-enc, 7 rows). The
    // game's own card literal is "You signed up for {0} account." (metadata
    // #49322) with the article baked into the drawn value, so build it the same
    // way: a/an + the Channel column.
    ensureTable("a16_social-media-channels-enc");
    const Table& ct = tbl("a16_social-media-channels-enc");
    if (ct.size() > 0) {
      const std::string cname = trim(ct.at(rng_.below(uint32_t(ct.size())), "Channel"));
      channel = articleFor(cname) + cname;
      channelBare = cname;
      substAll(body, "{channel}", channel);
      substAll(body, "{channelbare}", cname);
    }
  }

  std::string disease;
  if (body.find("{disease}") != std::string::npos) {
    // Same draw the family lifecycle lines use (lifecycleLines): only rows that
    // contract organically are eligible, and the article is the row's own flag.
    ensureTable("a16_disease-types-enc");
    const Table& dt = tbl("a16_disease-types-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < dt.size(); ++r)
      if (dt.num(r, "Contracts Organically Score") > 0) pool.push_back(r);
    if (!pool.empty()) {
      const size_t row = weightedPickOf(dt, "Contracts Organically Score", pool, rng_);
      disease = lower(trim(dt.at(row, "Disease")));
      if (!trim(dt.at(row, "Uses Indefinite Article")).empty() && !disease.empty())
        disease = articleFor(disease) + disease;
      substAll(body, "{disease}", disease);
    }
  }
  // Menu scenes draw their nouns at render time. Their cards, option labels and
  // outcome lines are C# string literals, not CSV rows, so the values come from
  // the same tables the game draws from (REPORT.en.md 41).
  const auto need = [&](const char* tok) {
    if (body.find(tok) != std::string::npos) return true;
    for (int i = 0; i < s->n; ++i)
      if (std::string(s->choices[i].log).find(tok) != std::string::npos) return true;
    return false;
  };
  const auto drawFrom = [&](const char* table, const char* weightCol, const char* valueCol) {
    ensureTable(table);
    const Table& t = tbl(table);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    const size_t row = weightedPickOf(t, weightCol, pool, rng_);
    return row == size_t(-1) ? std::string() : lower(trim(t.at(row, valueCol)));
  };
  // The illness chain: ScenarioDoctors offers two local physicians by surname,
  // and the treatment card reads back the one that was picked.
  std::string doctor1, doctor2, diagnosis, lingering, heroJob, heroCompany, insultWord, strangerWord;
  if (need("{doctor}") || need("{doctor2}") || cls == "ScenarioDoctors" ||
      cls == "ScenarioDoctorTreatments") {
    doctor1 = lastDoctor_.empty() ? drawSurname() : lastDoctor_;
    doctor2 = drawSurname();
    if (doctor2 == doctor1) doctor2 = drawSurname();
    substAll(body, "{doctor2}", doctor2);
    substAll(body, "{doctor}", doctor1);
  }
  if (cls == "ScenarioDoctorTreatments") {
    // The freshly diagnosed ailment is a real disease row; the sentence that
    // follows names a second condition the hero already has (both symptom
    // names in the capture: kidney stones / hay fever).
    diagnosis = drawFrom("a16_disease-types-enc", "Contracts Organically Score", "Disease");
    ensureTable("a16_disease-symptom-scenarios-enc");
    const Table& st = tbl("a16_disease-symptom-scenarios-enc");
    if (st.size() > 0)
      lingering = lower(trim(st.at(rng_.below(uint32_t(st.size())), "Symptom")));
    substAll(body, "{diagnosis}", diagnosis);
    substAll(body, "{olddisease}", lingering);
  }
  if (need("{job}") || need("{company}")) {
    // The termination card names the hero's own position and employer; the
    // employer is built from the career's 'Company Type' row, the same way the
    // family lifecycle lines build one.
    heroJob = lower(life.hero.job.empty() ? std::string("housekeeper") : life.hero.job);
    ensureTable("a16_careers-enc");
    const Table& cr = tbl("a16_careers-enc");
    std::string ctype;
    for (size_t r = 0; r < cr.size(); ++r)
      if (lower(trim(cr.at(r, "Career"))) == heroJob) { ctype = trim(cr.at(r, "Company Type")); break; }
    if (ctype.empty() && cr.size() > 0) {
      // The card can fire while the hero holds no career at all (it did at age
      // 4 in the forced-row test), so fall back to a real career row rather than
      // leaving the position unnamed.
      const size_t r = rng_.below(uint32_t(cr.size()));
      heroJob = lower(trim(cr.at(r, "Career")));
      ctype = trim(cr.at(r, "Company Type"));
    }
    heroCompany = companyName(ctype, life);
    substAll(body, "{job}", heroJob);
    substAll(body, "{company}", heroCompany);
  }
  std::string reason;
  if (need("{reason}")) {
    // shortcut: the dismissal reason is a code-built sentence; only the captured
    // one is modelled.
    reason = "you danced too promiscuously at the holiday party";
    substAll(body, "{reason}", reason);
  }
  if (need("{insult}")) {
    insultWord = drawFrom("a16_insults-enc", "Clean", "Insult");
    // The insult table's own rows carry the article ('a douche'); the card's
    // sentence supplies its own, so strip it here.
    static const char* kArt[3] = {"a ", "an ", "the "};
    for (const char* art : kArt) {
      const size_t n = std::string(art).size();
      if (insultWord.compare(0, n, art) == 0) { insultWord = insultWord.substr(n); break; }
    }
    substAll(body, "{insult}", insultWord);
  }
  if (need("{stranger}")) {
    // Whoever insulted the relative is a random person with the indefinite
    // article the game spells ("a firearms activist" in the capture).
    const std::string who = referenceToken("random-person");
    strangerWord = who.empty() ? std::string("a stranger") : articleFor(who) + who;
    substAll(body, "{stranger}", strangerWord);
  }
  // The menu classes of REPORT.en.md 44: the pet, the club, the car, the venue
  // and the stranger on the date card are the drawn values; everything else on
  // those cards is captured text.
  std::string pet, petname, club, car, venueName, otherCountry, cfirst, clast;
  std::string ladyWord;
  if (need("{pet}")) {
    // a16_pets-enc Breed, weighted, restricted to dogs: every captured card is
    // about a dog ("Your vizsla, Hazel, just ate your fruit snacks.").
    ensureTable("a16_pets-enc");
    const Table& pt = tbl("a16_pets-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < pt.size(); ++r)
      if (lower(trim(pt.at(r, "Animal"))) == "dog") pool.push_back(r);
    const size_t row = weightedPickOf(pt, "Commonness Score", pool, rng_);
    if (row != size_t(-1)) pet = lower(trim(pt.at(row, "Breed")));
    if (!pet.empty()) substAll(body, "{pet}", pet);
  }
  if (need("{petname}")) {
    ensureTable("a16_pet-names-enc");
    const Table& nt = tbl("a16_pet-names-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < nt.size(); ++r)
      if (lower(trim(nt.at(r, "Animal"))) == "dog") pool.push_back(r);
    if (!pool.empty()) petname = trim(nt.at(pool[rng_.below(uint32_t(pool.size()))], "Name"));
    if (!petname.empty()) substAll(body, "{petname}", petname);
  }
  if (need("{club}")) {
    // shortcut: the club list is code-built in the game (metadata #27937-#27943);
    // these are the two captured names.
    static const char* kClubs[] = {"Billiards Club", "Weightlifting Club"};
    club = kClubs[rng_.below(2)];
    substAll(body, "{club}", club);
  }
  if (need("{car}")) {
    ensureTable("a16_car-models-enc");
    const Table& ct = tbl("a16_car-models-enc");
    if (ct.size() > 0) {
      const size_t row = rng_.below(uint32_t(ct.size()));
      car = trim(ct.at(row, "Make")) + " " + trim(ct.at(row, "Model"));
    }
    if (!car.empty()) substAll(body, "{car}", car);
  }
  if (need("{venue}")) {
    // shortcut: the meeting place is code-built in the game (gym and library
    // captured); this list is the stand-in.
    static const char* kVenues[] = {"gym", "library", "coffee shop", "park", "supermarket"};
    venueName = kVenues[rng_.below(5)];
    substAll(body, "{venue}", venueName);
  }
  if (need("{stepparent}")) {
    // shortcut: the port's family model has a stepfather relation but no
    // gender-neutral slot; every captured card says stepfather.
    substAll(body, "{stepparent}", "stepfather");
  }
  if (need("{lady}")) {
    static const char* kLadies[] = {"lady", "female"};
    ladyWord = kLadies[rng_.below(2)];
    substAll(body, "{lady}", ladyWord);
  }
  if (need("{countryother}")) {
    otherCountry = heroToken("country-other", false);
    if (!otherCountry.empty()) substAll(body, "{countryother}", otherCountry);
  }
  if (need("{cfirst}")) {
    cfirst = drawFirstName(rng_.chance(0.5));
    substAll(body, "{cfirst}", cfirst);
  }
  if (need("{clast}")) {
    clast = drawSurname();
    substAll(body, "{clast}", clast);
  }
  std::string partner, partnerObj;
  if (need("{partner}")) {
    // The classmate the scene offers is the hero's opposite sex, and the game
    // spells the object pronoun to match it.
    partner = life.hero.female ? "boy" : "girl";
    partnerObj = life.hero.female ? "him" : "her";
    substAll(body, "{partner}", partner);
  }
  std::string food;
  if (need("{food}")) {
    food = drawFrom("a16_food-enc", "Commonness Score", "Food");
    if (!food.empty()) substAll(body, "{food}", food);
  }
  std::string casino;
  if (need("{casino}")) {
    casino = companyName("Casino", life);  // the Casino format is '<color> Casino Resort'
    if (!casino.empty()) substAll(body, "{casino}", casino);
  }
  std::string findWord, findPhrase;
  if (need("{find}")) {
    // shortcut: the findable is a code array in the game, not a table, so only
    // the captured one (metadata #70591) is modelled; extend on more captures.
    findWord = "duffle bag";
    findPhrase = "a duffle bag";
    substAll(body, "{find}", findWord);
  }
  std::string eyeexamverb, eyeexamclause;
  if (need("{eyeexamverb}")) {
    // Only two outcomes exist for the optometrist test, and the game's own cards
    // and log lines spell both (metadata 48452 / 49098 / 24139 / 24779).
    const bool passed = rng_.chance(0.5);
    eyeexamverb = passed ? "passed" : "failed";
    eyeexamclause = passed ? "do not need eyeglasses!"
                           : "have been issued a prescription for eyeglasses!";
    substAll(body, "{eyeexamverb}", eyeexamverb);
    substAll(body, "{eyeexamclause}", eyeexamclause);
  }
  std::string casinogame, casinoamount, casinoverb, casinotail;
  if (need("{casinogame}") || need("{casinoamount}") || need("{casinoverb}")) {
    // The casino minigames read their numbers from the room table. Its bet
    // columns are chip units, and the capture ($7,200 inside a 10..100 unit
    // range) prices one unit at $100.
    // shortcut: stake drawn from Default..Max Bet * 100; the game reads the
    // player's own slider, and only the blackjack card lines are captured.
    ensureTable("a16_casino-casino-game-room-enc");
    const Table& gt = tbl("a16_casino-casino-game-room-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < gt.size(); ++r)
      if (!trim(gt.at(r, "Game")).empty()) pool.push_back(r);
    const size_t row = pool.empty() ? size_t(-1) : pool[rng_.below(uint32_t(pool.size()))];
    const std::string game = row == size_t(-1) ? std::string("Blackjack") : trim(gt.at(row, "Game"));
    const long long lo = row == size_t(-1) ? 10 : static_cast<long long>(gt.num(row, "Default Bet"));
    const long long hi = row == size_t(-1) ? 100 : static_cast<long long>(gt.num(row, "Max Bet"));
    const long long units = hi > lo ? lo + static_cast<long long>(rng_.below(uint32_t(hi - lo + 1))) : lo;
    casinoamount = money(units * 100);
    casinoverb = !rng_.chance(row == size_t(-1) ? 0.6 : gt.num(row, "Default House Win Rate") / 100.0)
                     ? "won"
                     : "lost";
    casinogame = lower(game);
    casinotail = "on this bet";
    if (casinogame == "blackjack") casinotail = "on the hand of blackjack";
    else if (casinogame == "craps") casinotail = "on this craps roll";
    else if (casinogame == "keno") casinotail = "on this game of keno";
    else if (casinogame == "three-card shuffle") casinotail = "on this game of three-card shuffle";
    else if (casinogame == "high-low") casinotail = "on this high-low draw";
    else if (casinogame == "texas hold'em") casinotail = "on this hand";
    else if (casinogame == "roulette" || casinogame == "slots") casinotail = "on this spin";
    substAll(body, "{casinogame}", casinogame);
    substAll(body, "{casinoamount}", casinoamount);
    substAll(body, "{casinoverb}", casinoverb);
    substAll(body, "{casinotail}", casinotail);
  }
  std::string victim, attackVerb, attackPart;
  if (need("{victim}")) {
    // The game attaches the scene to whoever it fired on (the capture hit the
    // stepfather); the port names the first alive relative, mother as fallback.
    // shortcut: no step-relative model yet, upgrade with the full family graph.
    for (const Person& fam : life.family)
      if (fam.alive) {
        victim = fam.relation;
        break;
      }
    if (victim.empty()) victim = "mother";
    attackVerb = drawFrom("a16_custom-attack-actions-enc", "Commonness Score", "Past Verb");
    attackPart = drawFrom("a16_custom-attack-body-parts-enc", "Commonness Score", "Body Part");
    substAll(body, "{victim}", victim);
  }
    // The drawn values have to reach the card, the button and the log alike:
  // one filler instead of one substAll per call site.
  const auto fillAll = [&](std::string& text) {
    if (!name.empty()) substAll(text, "{name}", name);
    if (!club.empty()) substAll(text, "{club}", club);
    if (!pet.empty()) substAll(text, "{pet}", pet);
    if (!petname.empty()) substAll(text, "{petname}", petname);
    if (!car.empty()) substAll(text, "{car}", car);
    if (!cfirst.empty()) substAll(text, "{cfirst}", cfirst);
    if (!clast.empty()) substAll(text, "{clast}", clast);
    if (!channelBare.empty()) substAll(text, "{channelbare}", channelBare);
    if (!otherCountry.empty()) substAll(text, "{countryother}", otherCountry);
    if (!venueName.empty()) substAll(text, "{venue}", venueName);
    if (!ladyWord.empty()) substAll(text, "{lady}", ladyWord);
    // shortcut: the port has no step-parent gender slot (see above).
    substAll(text, "{stepparent}", "stepfather");
  };
  if (s->n > 0) {
    // A single-entry list is the no-button form (label ""), so it must not consume
    // a draw: only a real multi-branch scene randomizes its pick.
    const int idx = choice >= 0 && choice < s->n
                        ? choice
                        : (s->n > 1 ? int(rng_.below(uint32_t(s->n))) : 0);
    out.choice = s->choices[idx].label;
    fillAll(out.choice);
    // The physician cards carry a drawn surname inside their option labels.
    if (!doctor2.empty()) substAll(out.choice, "{doctor2}", doctor2);
    if (!doctor1.empty()) {
      substAll(out.choice, "{doctor}", doctor1);
      if (cls == "ScenarioDoctors") lastDoctor_ = idx == 1 ? doctor2 : doctor1;
    }
    std::string log = s->choices[idx].log;
    if (log.find("{name}") != std::string::npos) {
      if (name.empty()) name = personName("classmate");
      substAll(log, "{name}", name);
    }
    substAll(log, "{parent}", "mother");
    if (!item.empty()) substAll(log, "{item}", item);
    if (!channel.empty()) substAll(log, "{channel}", channel);
    if (!disease.empty()) substAll(log, "{disease}", disease);
    if (!doctor2.empty()) substAll(log, "{doctor2}", doctor2);
    if (!doctor1.empty()) substAll(log, "{doctor}", doctor1);
    if (!diagnosis.empty()) substAll(log, "{diagnosis}", diagnosis);
    if (!lingering.empty()) substAll(log, "{olddisease}", lingering);
    if (!heroJob.empty()) substAll(log, "{job}", heroJob);
    if (!heroCompany.empty()) substAll(log, "{company}", heroCompany);
    if (!insultWord.empty()) substAll(log, "{insult}", insultWord);
    if (!strangerWord.empty()) substAll(log, "{stranger}", strangerWord);
    fillAll(log);
    if (!pet.empty()) substAll(log, "{pet}", pet);
    if (!petname.empty()) substAll(log, "{petname}", petname);
    if (!club.empty()) substAll(log, "{club}", club);
    if (!car.empty()) substAll(log, "{car}", car);
    if (!venueName.empty()) substAll(log, "{venue}", venueName);
    if (!otherCountry.empty()) substAll(log, "{countryother}", otherCountry);
    if (!cfirst.empty()) substAll(log, "{cfirst}", cfirst);
    if (!clast.empty()) substAll(log, "{clast}", clast);
    if (!partner.empty()) {
      substAll(log, "{partner}", partner);
      substAll(log, "{partnerobj}", partnerObj);
    }
    if (!food.empty()) substAll(log, "{food}", food);
    if (!casino.empty()) substAll(log, "{casino}", casino);
    if (!findWord.empty()) {
      substAll(log, "{find}", findWord);
      substAll(log, "{findarticle}", findPhrase);
    }
    if (!eyeexamverb.empty()) substAll(log, "{eyeexamverb}", eyeexamverb);
    if (!casinoamount.empty()) {
      substAll(log, "{casinogame}", casinogame);
      substAll(log, "{casinoamount}", casinoamount);
      substAll(log, "{casinoverb}", casinoverb);
      substAll(log, "{casinotail}", casinotail);
    }
    if (!victim.empty()) {
      substAll(log, "{victim}", victim);
      substAll(log, "{attackverb}", attackVerb);
      substAll(log, "{attackpart}", attackPart);
    }
    out.narrative = log;
  } else {
    // No buttons: the game writes the sentence straight into its LifeLog.
    out.narrative = body;
  }
  {
    const std::string teacher = body.find("{teacher}") != std::string::npos ? personName("teacher") : std::string();
    if (!teacher.empty()) substAll(body, "{teacher}", teacher);
  }
  fillAll(out.title);
  fillAll(body);
  if (!car.empty()) substAll(out.title, "{car}", car);
  if (!club.empty()) substAll(out.title, "{club}", club);
  if (!pet.empty()) substAll(out.title, "{pet}", pet);
  out.body = body;
  return true;
}

bool Sim::forceScenarioClass(const std::string& cls, int choice) {
  if (cls.empty()) {  // clear: the next age-up draws as usual
    forcedClass_.clear();
    forcedClassChoice_ = -1;
    return true;
  }
  if (!findScenario(cls)) {
    std::fprintf(stderr, "forceScenarioClass: unknown scenario class: %s\n", cls.c_str());
    return false;
  }
  forcedClass_ = cls;
  forcedClassChoice_ = choice;
  return true;
}

// A conviction. The real game reaches prison through the interactive Crime menu
// and its court, and the port has neither, so it draws one small yearly chance
// and takes the sentence from the crime's own seriousness. The wording is the
// game's own non-hero template -- '<pronoun> have been convicted of {0} and
// sentenced to {1} in prison!' (literal #9559) and its juvenile variant
// '... and sentenced to {1} {2} at a juvenile detention center!' (#9562) -- with
// <pronoun> resolving to 'I' for the hero, exactly as the game renders it.
// shortcut: the 2%/year conviction chance and the sentence table {1,2,3,5,10}
// years for Seriousness Score 1..5 are port-side; the real sentencing code has
// not been read.
bool Sim::maybeConvicted(Life& life, AgeUp& out) {
  Hero& h = life.hero;
  if (h.inPrison || h.age < 10) return false;
  if (!prisonRng_.chance(0.02)) return false;
  const bool juvenile = h.age < 18;
  const char* name = juvenile ? "a16_prisoner-juvenile-crimes-enc" : "a16_prisoner-crimes-enc";
  if (!ensureTable(name)) return false;
  const Table& t = tbl(name);
  const size_t row = weightedPick(t, "Commonness Score", prisonRng_);
  if (row == size_t(-1)) return false;
  static const int kSentence[] = {1, 2, 3, 5, 10};
  const int ser = std::max(1, std::min(5, int(t.num(row, "Seriousness Score"))));
  const int years = kSentence[ser - 1];
  h.inPrison = true;
  h.prisonYearsLeft = years;
  h.prisonJob.clear();
  h.employed = false;
  // The year's family lines were written before this scenario was picked and
  // belong to no row, so they survive the reset.
  const std::vector<std::string> extra = out.extra;
  out = AgeUp();
  out.extra = extra;
  out.table = t.name();
  out.row_id = std::to_string(row + 1);
  const std::string crime = resolve(t.at(row, "Crime"));
  const std::string span = (years == 1) ? "1 year" : (std::to_string(years) + " years");
  out.narrative = "I have been convicted of " + crime + " and sentenced to " + span +
                  (juvenile ? " at a juvenile detention center!" : " in prison!");
  return true;
}

// One prison year: the year's scenario comes from the prison dumb-scenario pool
// (dumbSetRows gates it), the hero may pick up a prison job, and the sentence
// counts down to the game's own release line 'I was released from prison.'
// (literal #1959).
bool Sim::prisonYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.inPrison) return false;
  h.employed = false;  // no career lines while inside
  std::string jobLine;
  if (h.prisonJob.empty() && prisonRng_.chance(0.35) && ensureTable("a16_prison-jobs-enc")) {
    const Table& jt = tbl("a16_prison-jobs-enc");
    const size_t jr = weightedPick(jt, "Commonness Score", prisonRng_);
    if (jr != size_t(-1)) {
      // a16_prison-jobs-enc spells the {0} of 'I started a prison job {0}.'
      // (literal #25148) in its Description column: 'as a laundry attendant'.
      h.prisonJob = trim(jt.at(jr, "Description"));
      jobLine = "I started a prison job " + h.prisonJob + ".";
    }
  }
  std::string table;
  size_t row = 0;
  bool rendered = false;
  if (pickDumbScenario(life, &table, &row)) {
    rendered = renderRow(tbl(table.c_str()), row, life, out, forcedChoice);
  }
  if (!jobLine.empty()) out.extra.push_back(jobLine);
  if (--h.prisonYearsLeft <= 0) {
    h.inPrison = false;
    h.prisonYearsLeft = 0;
    h.prisonJob.clear();
    // shortcut: a juvenile sentence reuses the adult release line; the literal
    // table has no juvenile-release string (#9564 is re-entry, #24122 escape).
    if (out.narrative.empty()) out.narrative = "I was released from prison.";
    else out.narrative += "\nI was released from prison.";
  }
  return rendered || !jobLine.empty() || !out.narrative.empty();
}

void Sim::makeVampire(Life& life) {
  Hero& h = life.hero;
  if (!ensureTable("a16_vampire-name-enc")) return;
  hero_ = &h;
  life_ = &life;
  h.vampire = true;
  h.vampireEssence = 60;
  h.vampireNotoriety = 20;
  h.vampireStanding = 50;
  h.vampireHunger = 40;
  h.vampireFamiliarRel = 60;
  // The familiar is a character the player meets in the vampire screens; there
  // is no familiar-name table, so the name comes from the port's own person
  // pool and the description and the numbers from a16_vampire-familiar-color-enc
  // (REPORT.en.md 70).
  if (h.familiar.empty()) {
    h.vampireFamiliarFemale = vampireRng_.chance(0.5);
    h.familiar = drawFirstName(h.vampireFamiliarFemale);
  }
  if (ensureTable("a16_vampire-familiar-color-enc")) {
    const Table& t = tbl("a16_vampire-familiar-color-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Familiar Gender")) {
        const std::string g = lower(trim(t.at(r, "Familiar Gender")));
        if (!g.empty() && g != "both" && g != "all" && g != "any" &&
            g != (h.vampireFamiliarFemale ? "female" : "male")) continue;
      }
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      h.vampireFamiliarDesc = resolve(trim(t.at(r, "Descriptive String")));
      const auto roll = [&](const char* lo, const char* hi, int fallback) -> int {
        const std::string los = t.has(lo) ? trim(t.at(r, lo)) : std::string();
        const std::string his = t.has(hi) ? trim(t.at(r, hi)) : std::string();
        const int l = los.empty() ? fallback : std::atoi(los.c_str());
        const int hh = his.empty() ? fallback : std::atoi(his.c_str());
        if (hh <= l) return l;
        return l + int(vampireRng_.below(uint32_t(hh - l + 1)));
      };
      h.vampireFamiliarLooks = roll("Min Familiar Looks", "Max Familiar Looks", 50);
      h.vampireFamiliarSmarts = roll("Min Familiar Smarts", "Max Familiar Smarts", 50);
    }
  }
  // The address off a16_vampire-street-name-enc (the street cells are templates
  // of their own, so they are resolved before the house number goes on).
  if (ensureTable("a16_vampire-street-name-enc")) {
    const size_t r = pickRow("a16_vampire-street-name-enc", "Commonness Score", "", "", "", vampireRng_);
    if (r != size_t(-1)) {
      const Table& st = tbl("a16_vampire-street-name-enc");
      std::string street = resolve(trim(st.at(r, "Street Name")));
      if (st.flag(r, "Add Random Street Type")) street += " Street";
      h.vampireHome = std::to_string(100 + int(vampireRng_.below(900))) + " " + street;
    }
  }
  h.vampireMaker = vampireNpcName();
}

std::string Sim::vampireNpcName() {
  if (!ensureTable("a16_vampire-name-enc")) return std::string();
  const Table& t = tbl("a16_vampire-name-enc");
  const std::string want = hero_ != nullptr && hero_->female ? "female" : "male";
  std::vector<size_t> firsts, surnames;
  for (size_t r = 0; r < t.size(); ++r) {
    const std::string type = lower(trim(t.at(r, "Type")));
    if (type == "surname") surnames.push_back(r);
    else if (type == want) firsts.push_back(r);
  }
  if (firsts.empty() || surnames.empty()) return std::string();
  const size_t f = firsts[vampireRng_.below(uint32_t(firsts.size()))];
  const size_t s = surnames[vampireRng_.below(uint32_t(surnames.size()))];
  return trim(t.at(f, "Name")) + " " + trim(t.at(s, "Name"));
}

// ---------------------------------------------------------------------------
// Military service (REPORT.en.md 50). a16_military-ranks-enc is the ladder:
// Rank E1..E9 (Enlisted) and O1..O10 (Officer), each row carrying the rank's
// longhand name per branch, a Salary Average and Minimum Military Years. The
// game's own lines are code-side literals (#24115 enlist, #24507 officer,
// #24373 promotion, #25005 retirement); the war pool is a16_war-scenarios-enc,
// four stages of escalating country-vs-country text.
// ---------------------------------------------------------------------------
// The hero's own job (REPORT.en.md 51). a16_careers-enc has 168 rows; the
// Seniority Track cell is either a real ladder ('Cameraman, Camera Operator,
// Director of Photography, ...') or a category word (None / Abbreviated /
// Working Class / Corporate), in which case the port keeps the Career name as
// the title. The company name comes from the existing companyName() generator.
// shortcut: the careers table's own eligibility columns (Required Education,
// Required Major, Min/Max Age, Min Intelligence/Appearance/Health/Karma, drug
// tests, clear criminal record, licences) are not evaluated; hiring is a draw.
bool Sim::careerYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (h.military || h.militaryPending) return false;
  hero_ = &h;
  life_ = &life;

  // The hero has no career row until one is drawn: 'employed' alone is set by
  // the school state at 18 (line 1762 of this file), so the career is keyed on
  // the empty track instead.
  if (h.careerTrack.empty()) {
    if (h.age < 18) return false;
    if (!careerRng_.chance(0.6)) return false;
    if (!ensureTable("a16_careers-enc")) return false;
    const Table& t = tbl("a16_careers-enc");
    if (!t.size()) return false;
    const size_t row = careerRng_.below(uint32_t(t.size()));
    const std::string track = trim(t.at(row, "Seniority Track"));
    std::vector<std::string> steps;
    if (track.find(',') != std::string::npos) splitOn(track, ',', steps);
    for (std::string& st : steps) st = trim(st);
    const std::string title = steps.empty() ? trim(t.at(row, "Career")) : steps[0];
    if (title.empty()) return false;
    const std::string company = companyName(t.at(row, "Company Type"), life);
    h.job = title;
    h.employed = true;
    h.careerTrack = track;
    h.careerStep = 0;
    h.careerYears = 0;
    out.narrative = "I started a new position as " + title +
                    (company.empty() ? "." : " for " + company + ".");
    return true;
  }

  // Promotions: one rung every four years of service, while the track has one.
  ++h.careerYears;
  if (h.careerTrack.find(',') == std::string::npos) return false;
  if (h.careerYears % 4 != 0) return false;
  std::vector<std::string> steps;
  splitOn(h.careerTrack, ',', steps);
  for (std::string& st : steps) st = trim(st);
  const size_t next = size_t(h.careerStep) + 1;
  if (next >= steps.size() || steps[next].empty()) return false;
  h.careerStep = int(next);
  h.job = steps[next];
  out.narrative = "I have been promoted to " + h.job + ".";
  return true;
}

// ---------------------------------------------------------------------------
// Professional athlete (REPORT.en.md 52). a16_sports-enc holds the seven sports
// and, per sport, its own words for the ball, the stadium, the field, the
// official and the game; a16_sports-leagues-enc and a16_sports-teams-enc give
// the league and club (team names carry <a/b/c> alternates), and
// a16_sports-positions-enc the position for that sport.
// ---------------------------------------------------------------------------
// Business owner (REPORT.en.md 53). The pool is a16_dumb-scenarios-business-enc
// (168 rows) and it gates itself with Industry Whitelist / Industry Blacklist,
// Requires Single Facility / Requires Multiple Facilities and Business Size, so
// the hero needs exactly one new state: which industry the business is in.
// ---------------------------------------------------------------------------
// Recording artist (REPORT.en.md 54). a16_music-genres-enc has 11 genres and the
// titles tables carry one flag column per genre, so the genre picks the album,
// the song and the band. Album titles and stage names carry <a/b/c> alternates
// and <hero-name> slots, both handled by re-resolving the drawn title.
// ---------------------------------------------------------------------------
// Elected office (REPORT.en.md 55). a16_country-political-systems-enc is the
// only place the game says what each country calls its offices, so the hero's
// country picks the row, its availability flags pick the level, and the level's
// Title column picks the office. The pool then gates itself on US Only /
// UK Only / National / Regional / City / Local, which the port can honour
// exactly because it knows both the country code and the level.
// The office fields are needed by the campaign pool too - a candidate is asking
// for votes for an office they do not hold yet - so they are drawn on demand.
void Sim::ensureOfficeFields(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  const Table& t = tbl("a16_country-political-systems-enc");
  if (!t.size()) return;
  size_t row = size_t(-1);
  for (size_t r = 0; r < t.size(); ++r) {
    if (lower(trim(t.at(r, "Country"))) == lower(h.country)) { row = r; break; }
  }
  if (row == size_t(-1)) return;
  struct Level { const char* key; const char* flag; const char* title; };
  const Level levels[4] = {{"national", "National Office Available", "Title National"},
                           {"regional", "Regional Office Available", "Title Regional"},
                           {"city", "City Office Available", "Title City"},
                           {"local", "Local Office Available", "Title Local"}};
  std::vector<Level> open;
  for (const Level& lv : levels) {
    if (t.has(lv.flag) && t.flag(row, lv.flag) && t.has(lv.title) &&
        !trim(t.at(row, lv.title)).empty())
      open.push_back(lv);
  }
  if (open.empty()) return;
  const Level pick = open[politicsRng_.below(uint32_t(open.size()))];
  h.officeLevel = pick.key;
  // '<jurisdiction>' is the place the office covers: the country itself at the
  // national level, the country's own word for its regions at the regional one
  // ('state', 'province'), the hero's city, or the neighbourhood.
  // The templates say 'your <jurisdiction>', so the slot is the level's noun
  // rather than a place name: 'your country', 'your state', 'your city'.
  // The regional noun is the country's own word for its regions.
  if (h.officeLevel == "national") h.jurisdiction = "country";
  else if (h.officeLevel == "regional")
    h.jurisdiction = t.has("Region Jurisdiction") ? trim(t.at(row, "Region Jurisdiction")) : std::string("state");
  else if (h.officeLevel == "city") h.jurisdiction = "city";
  else h.jurisdiction = "neighborhood";
  if (h.jurisdiction.empty()) h.jurisdiction = "country";
  // '<jurisdiction adjective>' is the same level as an adjective: the country
  // table carries the region's own word ('provincial' for a province).
  if (h.officeLevel == "national") h.jurisdictionAdj = "national";
  else if (h.officeLevel == "regional")
    h.jurisdictionAdj = t.has("Region Jurisdiction Adjective")
                            ? trim(t.at(row, "Region Jurisdiction Adjective"))
                            : std::string("regional");
  else if (h.officeLevel == "city") h.jurisdictionAdj = "city";
  else h.jurisdictionAdj = "local";
  if (h.jurisdictionAdj.empty()) h.jurisdictionAdj = h.officeLevel;
  // The title column itself carries the token: the region's office is written
  // '<jurisdiction adjective> governor' (or 'mayor', 'premier'), so it is
  // resolved once the level's adjective is known. The guard in politicalToken
  // is ordered so that this lookup does not need the office to exist yet.
  h.office = resolve(trim(t.at(row, pick.title)));
}

void Sim::makePolitician(Life& life) {
  ensureOfficeFields(life);
  if (life.hero.office.empty()) return;
  life.hero.politician = true;
}

std::string Sim::miscToken(const std::string& key) {
  const std::string psuffix = "-possessive";
  const bool possessive =
      key.size() > psuffix.size() &&
      key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0;
  const std::string base = possessive ? key.substr(0, key.size() - psuffix.size()) : key;
  std::string value;
  // Table cells carry tokens of their own: the nickname list stores
  // '<first-name> Meatballs' and the street list '<random-surname> Garden'.
  const auto sub = [](std::string text, const std::string& from, const std::string& to) {
    for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
      text = text.substr(0, at) + to + text.substr(at + from.size());
    return text;
  };
  for (const MiscSrc& m : kMiscSrc) {
    if (base != m.key) continue;
    if (!ensureTable(m.table)) return std::string();
    const Table& t = tbl(m.table);
    if (t.size() == 0 || !t.has(m.col)) return std::string();
    value = trim(t.at(miscRng_.below(uint32_t(t.size())), m.col));
    break;
  }
  if (value.empty()) {
    bool person = false;
    for (const char* k : kMiscPeople) if (base == k) person = true;
    if (person) {
      value = drawFirstName(miscRng_.chance(0.5)) + " " + drawSurname();
    } else if (base == "animal-handler") {
      value = "animal handler";
    } else if (base == "zoo-veterinarian") {
      value = "zoo veterinarian";
    } else if (base == "zoo-operations-manager") {
      value = "zoo operations manager";
    } else if (base == "hustle-instrument") {
      // 'Possible Instruments' is a comma list ('harp, cello, triangle'); the
      // game picks one of them.
      if (!ensureTable("a16_hustler-busk-music-types-enc")) return std::string();
      const Table& t = tbl("a16_hustler-busk-music-types-enc");
      if (t.size() == 0 || !t.has("Possible Instruments")) return std::string();
      const std::string list = t.at(miscRng_.below(uint32_t(t.size())), "Possible Instruments");
      std::vector<std::string> items;
      splitOn(list, ',', items);
      if (items.empty()) return std::string();
      value = trim(items[miscRng_.below(uint32_t(items.size()))]);
    } else if (base == "tenant-address") {
      if (!ensureTable("a16_street-names-enc")) return std::string();
      const Table& t = tbl("a16_street-names-enc");
      if (t.size() == 0 || !t.has("Street Name")) return std::string();
      std::string street = trim(t.at(miscRng_.below(uint32_t(t.size())), "Street Name"));
      if (street.find("<random-surname>") != std::string::npos)
        street = sub(street, "<random-surname>", drawSurname());
      street = resolve(street);
      value = std::to_string(100 + int(miscRng_.below(9800))) + " " + street;
    } else {
      return std::string();
    }
  } else {
    // Columns such as the nickname list carry tokens of their own: the nickname
    // table stores '<first-name> Meatballs', and street names carry
    // '<random-surname> Garden'. They are filled after the row is picked and
    // before the value goes back through the resolver.
    if (value.find("<first-name>") != std::string::npos)
      value = sub(value, "<first-name>", drawFirstName(miscRng_.chance(0.5)));
    if (value.find("<last-name>") != std::string::npos)
      value = sub(value, "<last-name>", drawSurname());
    if (value.find("<random-surname>") != std::string::npos)
      value = sub(value, "<random-surname>", drawSurname());
    value = resolve(value);
  }
  if (value.empty()) return std::string();
  return possessive ? value + "'s" : value;
}

// The nine remaining dumb-scenario pools share one runner: each names its table,
// the age it can start and how often it fires, and the script's switch selects
// which of them are live. The pools have no career model behind them, so there is
// no make* step beyond recording the mask (REPORT.en.md 56).

// ── Secret agent (report 58) ────────────────────────────────────────────────
// 'makeSecretAgent' is the whole setup: an agency, a code name, a specialization
// and a starting proficiency. The career's dumb-scenario pool (misc bit 7) is
// switched on here as well, so the script only has to say "spy".
// One draw over a secret-agent table. The weight column may be empty (then the
// pick is uniform), a blank cell in the filter column matches anything, which is
// how this family authors its gender columns, and the flag column is optional.
size_t Sim::pickWindow(const std::string& table, const std::string& weightColumn,
                       const std::string& filterColumn, const std::string& filterValue,
                       const std::string& loColumn, const std::string& hiColumn,
                       double value, Rng& rng) {
  for (int attempt = 0; attempt < 3; ++attempt) {
    const size_t r = pickRow(table, weightColumn, filterColumn, filterValue, "", rng);
    if (r == size_t(-1)) return size_t(-1);
    const Table& t = tbl(table.c_str());
    if (!loColumn.empty() && t.has(loColumn)) {
      const double lo = t.num(r, loColumn);
      if (lo > 0 && value < lo) continue;
    }
    if (!hiColumn.empty() && t.has(hiColumn)) {
      const double hi = t.num(r, hiColumn);
      if (hi > 0 && value > hi) continue;
    }
    return r;
  }
  return pickRow(table, weightColumn, filterColumn, filterValue, "", rng);
}

size_t Sim::pickRow(const std::string& table, const std::string& weightColumn,
                    const std::string& filterColumn, const std::string& filterValue,
                    const std::string& flagColumn, Rng& rng) {
  if (!ensureTable(table)) return size_t(-1);
  const Table& t = tbl(table.c_str());
  std::vector<size_t> pool;
  for (size_t r = 0; r < t.size(); ++r) {
    if (!flagColumn.empty() && !t.flag(r, flagColumn.c_str())) continue;
    if (!filterColumn.empty() && !filterValue.empty() && t.has(filterColumn)) {
      const std::string cell = lower(trim(t.at(r, filterColumn)));
      if (!cell.empty() && cell != filterValue) continue;
    }
    pool.push_back(r);
  }
  if (pool.empty()) return size_t(-1);
  if (weightColumn.empty() || !t.has(weightColumn))
    return pool[rng.below(uint32_t(pool.size()))];
  const size_t row = weightedPickOf(t, weightColumn, pool, rng);
  return row == size_t(-1) ? pool[rng.below(uint32_t(pool.size()))] : row;
}

std::string Sim::pickSnippet(const std::string& table, const std::string& column,
                             const std::string& weightColumn, const std::string& filterColumn,
                             const std::string& filterValue, const std::string& flagColumn, Rng& rng) {
  const size_t row = pickRow(table, weightColumn, filterColumn, filterValue, flagColumn, rng);
  if (row == size_t(-1)) return std::string();
  const Table& t = tbl(table.c_str());
  if (!t.has(column)) return std::string();
  return resolve(trim(t.at(row, column)));
}

// The secret-agent call sites keep their own names, so they all draw on the
// spy stream exactly as before the two draws were generalised.
size_t Sim::spyRow(const std::string& table, const std::string& weightColumn,
                   const std::string& filterColumn, const std::string& filterValue,
                   const std::string& flagColumn) {
  return pickRow(table, weightColumn, filterColumn, filterValue, flagColumn, spyRng_);
}

std::string Sim::spySnippet(const std::string& table, const std::string& column,
                            const std::string& weightColumn, const std::string& filterColumn,
                            const std::string& filterValue, const std::string& flagColumn) {
  return pickSnippet(table, column, weightColumn, filterColumn, filterValue, flagColumn, spyRng_);
}

void Sim::makeSecretAgent(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (ensureTable("a16_secret-agent-random-hero-agency-names-enc")) {
    const Table& agencies = tbl("a16_secret-agent-random-hero-agency-names-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < agencies.size(); ++r) pool.push_back(r);
    const size_t row = weightedPickOf(agencies, "Commonness Score", pool, spyRng_);
    if (row != size_t(-1)) h.spyAgency = resolve(trim(agencies.at(row, "Name")));
  }
  if (ensureTable("a16_secret-agent-code-names-enc")) {
    const Table& names = tbl("a16_secret-agent-code-names-enc");
    std::vector<size_t> pool;
    const std::string want = h.female ? "female" : "male";
    for (size_t r = 0; r < names.size(); ++r) {
      const std::string g = lower(trim(names.at(r, "Gender")));
      if (!g.empty() && g != want) continue;  // blank Gender fits anyone
      pool.push_back(r);
    }
    const size_t row = weightedPickOf(names, "Commonness Score", pool, spyRng_);
    if (row != size_t(-1)) h.spyCodeName = resolve(trim(names.at(row, "Code Name")));
  }
  if (ensureTable("a16_secret-agent-potential-agent-color-enc")) {
    const Table& specs = tbl("a16_secret-agent-potential-agent-color-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < specs.size(); ++r)
      if (!trim(specs.at(r, "Specialization")).empty()) pool.push_back(r);
    if (!pool.empty())
      h.spySpec = lower(trim(specs.at(pool[spyRng_.below(uint32_t(pool.size()))], "Specialization")));
  }
  if (h.spySpec.empty()) h.spySpec = "stealth";
  // The feedback table's windows start at 0 and top out at 49, so a fresh agent
  // sits in the low half and works up with every mission.
  if (ensureTable("a16_secret-agent-potential-agent-careers-enc")) {
    const Table& jobs = tbl("a16_secret-agent-potential-agent-careers-enc");
    std::vector<size_t> pool;
    const std::string want = h.female ? "female" : "male";
    for (size_t r = 0; r < jobs.size(); ++r) {
      const std::string gender = lower(trim(jobs.at(r, "Gender")));
      if (!gender.empty() && gender != want) continue;
      pool.push_back(r);
    }
    const size_t row = weightedPickOf(jobs, "Commonness Score", pool, spyRng_);
    if (row != size_t(-1)) h.spyCover = lower(trim(jobs.at(row, "Occupation")));
  }
  {
    const size_t street = spyRow("a16_secret-agent-agency-street-names-enc", "Commonness Score", "", "", "");
    if (street != size_t(-1)) {
      const Table& sn = tbl("a16_secret-agent-agency-street-names-enc");
      h.spyAddress = std::to_string(100 + int(spyRng_.below(900))) + " " + trim(sn.at(street, "Street Name"));
      if (sn.flag(street, "Add Random Street Type")) h.spyAddress += " Street";
    }
  }
  h.spyProficiency = 20 + int(spyRng_.below(21));
  h.secretAgent = true;
  h.miscMask |= 1u << 7;
}

// A front's name: a16_secret-agent-agency-front-names-enc is the same shape as
// the company-name table (report 53), keyed by 'Business Type' instead of
// 'Industry', so the same format/pool walk builds it.
std::string Sim::spyFrontName(const std::string& businessType) {
  if (!ensureTable("a16_secret-agent-agency-front-names-enc")) return std::string();
  const Table& st = tbl("a16_secret-agent-agency-front-names-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < st.size(); ++r)
    if (businessType.empty() || lower(trim(st.at(r, "Business Type"))) == lower(businessType))
      pool.push_back(r);
  if (pool.empty())
    for (size_t r = 0; r < st.size(); ++r) pool.push_back(r);
  if (pool.empty()) return std::string();
  const size_t row = pool[spyRng_.below(uint32_t(pool.size()))];
  const auto pickList = [this](const std::string& list) {
    std::vector<std::string> parts;
    splitOn(list, ',', parts);
    return parts.empty() ? std::string() : trim(parts[spyRng_.below(uint32_t(parts.size()))]);
  };
  const auto replaceAll = [](std::string s, const std::string& from, const std::string& to) {
    if (from.empty() || to.empty()) return s;
    for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
      s.replace(at, from.size(), to);
    return s;
  };
  std::string fmt = pickList(st.at(row, "Format"));
  if (fmt.empty()) return std::string();
  if (fmt.find("<color>") != std::string::npos && ensureTable("a16_company-name-color-enc")) {
    const Table& colors = tbl("a16_company-name-color-enc");
    if (colors.size())
      fmt = replaceAll(fmt, "<color>", trim(colors.at(spyRng_.below(uint32_t(colors.size())), "Color")));
  }
  if (fmt.find("<suffix>") != std::string::npos) fmt = replaceAll(fmt, "<suffix>", pickList(st.at(row, "Suffix")));
  if (fmt.find("<prefix>") != std::string::npos) fmt = replaceAll(fmt, "<prefix>", pickList(st.at(row, "Prefix")));
  if (fmt.find("<city>") != std::string::npos) fmt = replaceAll(fmt, "<city>", hero_ ? hero_->city : std::string());
  if (fmt.find("<random-first-name-possessive>") != std::string::npos)
    fmt = replaceAll(fmt, "<random-first-name-possessive>", drawFirstName(spyRng_.chance(0.5)) + "'s");
  if (fmt.find("<random-first-name>") != std::string::npos)
    fmt = replaceAll(fmt, "<random-first-name>", drawFirstName(spyRng_.chance(0.5)));
  if (fmt.find("<random-surname>") != std::string::npos) fmt = replaceAll(fmt, "<random-surname>", drawSurname());
  if (fmt.find("<random-last-name>") != std::string::npos) fmt = replaceAll(fmt, "<random-last-name>", drawSurname());
  std::string clean;  // anything still unfilled is dropped, never printed
  for (size_t i = 0; i < fmt.size(); ++i) {
    if (fmt[i] == '<') {
      const size_t close = fmt.find('>', i);
      if (close != std::string::npos) { i = close; continue; }
    }
    clean += fmt[i];
  }
  return trim(clean);
}

// The brackets the secret-agent pools address (report 58). Pronouns here always
// speak as the hero: the mission strings are the hero's own reports, and the
// targets of a16_secret-agent-mission-target-people-enc are described relative
// to the hero ('<his/her> receptionist' is the hero's).
std::string Sim::spyToken(const std::string& token) {
  if (!hero_) return std::string();
  const Hero& h = *hero_;
  const std::string psuffix = "-possessive";
  const bool possessive =
      token.size() > psuffix.size() &&
      token.compare(token.size() - psuffix.size(), psuffix.size(), psuffix) == 0;
  const std::string key = possessive ? token.substr(0, token.size() - psuffix.size()) : token;
  const auto withOwnership = [&](std::string v) {
    return (possessive && !v.empty()) ? v + "'s" : v;
  };
  const auto drawName = [&](const std::string& slot) {
    auto it = spyCache_.find(slot);
    if (it != spyCache_.end()) return it->second;
    const std::string who = drawFirstName(spyRng_.chance(0.5)) + " " + drawSurname();
    spyCache_[slot] = who;
    return who;
  };
  if (key == "agent-spy") {
    if (spyAgentIsOther_) return withOwnership(spyColleague_.empty() ? drawName("agent-spy") : spyColleague_);
    if (h.spyCodeName.empty()) return withOwnership(drawName("agent-spy"));
    return withOwnership(h.spyCodeName);
  }
  if (key == "client-spy" || key == "client") return withOwnership(drawName("client-spy"));
  if (key == "target" || key == "target-assassination") {
    if (!spyTarget_.empty()) return withOwnership(spyTarget_);
    return withOwnership(drawName("target"));
  }
  if (key == "victim") {
    // The disposal snippets talk about the body, and the body is the target.
    if (!spyTarget_.empty()) return withOwnership(spyTarget_);
    return withOwnership(drawName("victim"));
  }
  if (key == "cover") return h.spyCover;
  if (key == "agency-rival") {
    auto it = spyCache_.find("agency-rival");
    if (it == spyCache_.end()) {
      std::string rival = "the rival agency";
      const size_t row = spyRow("a16_secret-agent-rival-agency-names-enc", "Commonness Score", "", "", "");
      if (row != size_t(-1)) {
        const Table& ra = tbl("a16_secret-agent-rival-agency-names-enc");
        rival = trim(ra.at(row, "Abbreviation"));
      }
      it = spyCache_.emplace("agency-rival", rival).first;
    }
    return withOwnership(it->second);
  }
  if (key == "friend") {
    const std::string who = personName("friend");
    return withOwnership(who.empty() ? drawName("friend") : who);
  }
  if (key == "color") {
    // '<color>' in the mission strings: the company-name colour pool.
    if (!ensureTable("a16_company-name-color-enc")) return std::string();
    const Table& colors = tbl("a16_company-name-color-enc");
    if (colors.size() == 0) return std::string();
    return trim(colors.at(spyRng_.below(uint32_t(colors.size())), "Color"));
  }
  if (key == "agency") return withOwnership(h.spyAgency.empty() ? std::string("the agency") : h.spyAgency);
  if (key == "code-name") return withOwnership(h.spyCodeName);
  if (key == "specialization") return h.spySpec;
  if (key == "pronoun") return h.female ? "she" : "he";
  if (key == "random-surname" || key == "random-last-name") return withOwnership(drawSurname());
  if (key == "random-first-name") return withOwnership(drawFirstName(spyRng_.chance(0.5)));
  if (key == "operation") {
    if (!ensureTable("a16_secret-agent-mission-code-names-enc")) return std::string();
    const Table& mn = tbl("a16_secret-agent-mission-code-names-enc");
    if (mn.size() == 0) return std::string();
    const size_t row = mn.size() == 1 ? 0 : weightedPickOf(mn, "Adjective Commonness Score",
                                                            [&] {
                                                              std::vector<size_t> all;
                                                              for (size_t r = 0; r < mn.size(); ++r) all.push_back(r);
                                                              return all;
                                                            }(),
                                                            spyRng_);
    const std::string adj = trim(mn.at(row, "Adjectives"));
    const std::string noun = trim(mn.at(row, "Nouns"));
    if (adj.empty() || noun.empty()) return std::string();
    return "Operation " + adj + " " + noun;
  }
  if (key == "front") {
    std::string type;
    if (ensureTable("a16_secret-agent-agency-fronts-enc")) {
      const Table& fronts = tbl("a16_secret-agent-agency-fronts-enc");
      if (fronts.size()) {
        const size_t row = weightedPickOf(fronts, "Commonness Score",
                                          [&] {
                                            std::vector<size_t> all;
                                            for (size_t r = 0; r < fronts.size(); ++r) all.push_back(r);
                                            return all;
                                          }(),
                                          spyRng_);
        type = trim(fronts.at(row, "Type"));
      }
    }
    return spyFrontName(type);
  }
  if (key == "gadget") {
    if (!ensureTable("a16_secret-agent-agency-gadgets-enc")) return std::string();
    const Table& g = tbl("a16_secret-agent-agency-gadgets-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < g.size(); ++r) {
      const std::string spec = lower(trim(g.at(r, "Specialization")));
      if (!h.spySpec.empty() && !spec.empty() && spec != h.spySpec) continue;
      pool.push_back(r);
    }
    if (pool.empty())
      for (size_t r = 0; r < g.size(); ++r) pool.push_back(r);
    if (pool.empty()) return std::string();
    const size_t grow = pool[spyRng_.below(uint32_t(pool.size()))];
    const std::string name = trim(g.at(grow, "Name"));
    return g.flag(grow, "Plural") ? name + "s" : name;
  }
  if (key == "password") {
    if (!ensureTable("a16_secret-agent-infiltration-passwords-enc")) return std::string();
    const Table& pw = tbl("a16_secret-agent-infiltration-passwords-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < pw.size(); ++r) pool.push_back(r);
    const size_t row = weightedPickOf(pw, "Commonness Score", pool, spyRng_);
    return row == size_t(-1) ? std::string() : trim(pw.at(row, "Password"));
  }
  if (key == "disguise") {
    if (!ensureTable("a16_secret-agent-honeypot-agent-disguises-enc")) return std::string();
    const Table& d = tbl("a16_secret-agent-honeypot-agent-disguises-enc");
    const std::string want = h.female ? "female" : "male";
    std::vector<size_t> pool;
    for (size_t r = 0; r < d.size(); ++r) {
      const std::string g = lower(trim(d.at(r, "Gender")));
      if (!g.empty() && g != want) continue;
      pool.push_back(r);
    }
    if (pool.empty()) return std::string();
    return trim(d.at(pool[spyRng_.below(uint32_t(pool.size()))], "Disguise"));
  }
  return std::string();
}

// One mission a year. The type picks the subject table (a person for stealth,
// assassination and the honeypot; a file for a hack), the briefing is composed
// from that type's own table, and the outcome is a risk-versus-proficiency roll
// whose window selects the line of
// a16_secret-agent-mission-summary-feedback-enc.
bool Sim::spyYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.secretAgent) return false;
  if (h.age < 18) return false;
  if (!spyRng_.chance(0.35)) return false;
  // A year in the service can also be a year on the range: the training table
  // is drawn instead of a mission while the agent is still learning.
  if (h.spyProficiency < 80 && spyRng_.chance(0.25)) {
    const size_t row = spyRow("a16_secret-agent-train-scenarios-enc", "Commonness Score", "Specialization", h.spySpec, "");
    if (row != size_t(-1)) {
      const Table& tt = tbl("a16_secret-agent-train-scenarios-enc");
      spyColleague_ = drawFirstName(spyRng_.chance(0.5)) + " " + drawSurname();
      spyAgentIsOther_ = true;  // the scenario addresses the colleague, not the hero
      const std::string scenario = resolve(trim(tt.at(row, "Scenario")));
      const std::string logged = resolve(trim(tt.at(row, "Log Version")));
      spyAgentIsOther_ = false;
      const std::vector<std::string> carried = out.extra;
      out = AgeUp();
      out.extra = carried;
      out.table = tt.name();
      out.row_id = std::to_string(row + 1);
      out.title = "A day on the range";
      out.body = scenario;
      if (!logged.empty()) out.extra.push_back(logged);
      const std::string agencySecurity = spySnippet("a16_secret-agent-security-features-enc", "Descriptive Snippet", "Commonness Score", "", "", "");
      if (!agencySecurity.empty()) out.extra.push_back("Agency security: " + agencySecurity);
      h.spyProficiency += 2;
      return true;
    }
  }
  // A long-serving agent can walk away from the service instead of taking the
  // job: a16_secret-agent-resignation-reasons says why, in the agent's words.
  if (h.spyMissions >= 3 && h.spyProficiency >= 45 && spyRng_.chance(0.04)) {
    const size_t row = spyRow("a16_secret-agent-resignation-reasons-enc", "Commonness Score",
                              "Gender", h.female ? "female" : "male", "");
    if (row != size_t(-1)) {
      const Table& rr = tbl("a16_secret-agent-resignation-reasons-enc");
      std::string raw = trim(rr.at(row, "Snippet"));
      raw = spySwap(raw, "<he/she>", "I");
      raw = spySwap(raw, "<his/her>", "my");
      raw = spySwap(raw, "<him/her>", "me");
      const std::string reason = spySentence(spyLowerFirst(resolve(raw)));
      const std::vector<std::string> carried = out.extra;
      out = AgeUp();
      out.extra = carried;
      out.table = rr.name();
      out.row_id = std::to_string(row + 1);
      out.title = "I turned in my badge";
      out.body = "I resigned from " +
                 (h.spyAgency.empty() ? std::string("the agency") : h.spyAgency) + ".";
      if (!reason.empty()) out.extra.push_back(reason);
      h.secretAgent = false;
      h.spyMissions = 0;
      h.miscMask &= ~(1u << 7);
      return true;
    }
  }
  static const char* kTypes[] = {"stealth", "assassination", "honeypot", "hacking", "intel"};
  const std::string type = kTypes[spyRng_.below(5)];

  spyTarget_.clear();
  int risk = 2;
  if (type != "hacking" && ensureTable("a16_secret-agent-mission-target-people-enc")) {
    const Table& people = tbl("a16_secret-agent-mission-target-people-enc");
    // The intel missions tail any person the client names, so they take the
    // whole table rather than one of the flag columns.
    const std::string flag = type == "stealth" ? "Stealth"
                             : (type == "assassination" ? "Assassination"
                                : (type == "honeypot" ? "Honeypot" : std::string()));
    std::vector<size_t> pool;
    for (size_t r = 0; r < people.size(); ++r)
      if (flag.empty() || people.flag(r, flag.c_str())) pool.push_back(r);
    const size_t row = weightedPickOf(people, "Commonness Score", pool, spyRng_);
    if (row != size_t(-1)) {
      spyTarget_ = resolve(trim(people.at(row, "Target")));
      // The table holds either a relation ('<his/her> mistress') or a bare
      // description ('corrupt police officer'), so an article is only added
      // when the drawn text does not already carry one.
      if (!spyTarget_.empty()) {
        const std::string head = lower(spyTarget_.substr(0, spyTarget_.find(' ')));
        static const char* kArticles[] = {"his", "her", "their", "the", "a", "an"};
        bool hasArticle = false;
        for (const char* a : kArticles) if (head == a) hasArticle = true;
        if (!hasArticle) spyTarget_ = articleFor(spyTarget_) + spyTarget_;
      }
      risk = int(people.num(row, "Risk Score"));
    }
  }

  // The outcome comes first: the briefing's own strings differ per result.
  const double odds = 0.55 + h.spyProficiency / 200.0 - risk * 0.04;
  const bool success = spyRng_.chance(odds < 0.10 ? 0.10 : (odds > 0.95 ? 0.95 : odds));
  h.spyMissions++;
  h.spyProficiency += success ? 4 : 1;
  if (h.spyProficiency > 100) h.spyProficiency = 100;

  std::string briefing;
  std::string line;  // the mission log: the type's own success/failure string
  std::string stealthObstacle;  // shared by the stealth briefing and its failure log
  // Half the hits are run the way a16_secret-agent-assassination-mission-methods
  // names them, whose own success and failure strings are whole sentences.
  std::string hitMethod;
  if (type == "assassination" && spyRng_.chance(0.5)) {
    const size_t mm = spyRow("a16_secret-agent-assassination-mission-methods-enc", "", "", "", "");
    if (mm != size_t(-1)) {
      const Table& mt = tbl("a16_secret-agent-assassination-mission-methods-enc");
      hitMethod = trim(mt.at(mm, "Method"));
      std::string raw = trim(mt.at(mm, success ? "Success Snippet" : "Failure Snippet"));
      raw = spySwap(raw, "<he/she>", "I");
      raw = spySwap(raw, "<his/her>", "my");
      raw = spySwap(raw, "<him/her>", "me");
      raw = spySwap(raw, "<target>", spyTarget_.empty() ? std::string("the target") : spyTarget_);
      std::string shown = resolve(raw);
      if (!(shown.size() >= 2 && shown[0] == 'I' && shown[1] == ' '))
        shown = std::string("I ") + spyLowerFirst(shown);
      line = spySentence(shown);
    }
  }
  if (type == "assassination" && ensureTable("a16_secret-agent-assassination-methods-enc")) {
    const Table& m = tbl("a16_secret-agent-assassination-methods-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < m.size(); ++r) pool.push_back(r);
    const size_t row = pool.empty() ? size_t(-1) : pool[spyRng_.below(uint32_t(pool.size()))];
    if (row != size_t(-1)) {
      // The mission-methods row above already produced the log when it fired.
      const bool methodLineTaken = !line.empty();
      if (!methodLineTaken) line = resolve(trim(m.at(row, success ? "Success String" : "Failure String")));
      if (!methodLineTaken && !success && spyRng_.chance(0.5)) {
        // '<pronoun>' sits in object position in the retaliatory string
        // ('... electrocuted <pronoun> instead!'), so the object form goes in
        // before the row is resolved.
        std::string ret = trim(m.at(row, "Retaliatory String"));
        const std::string objForm = h.female ? "her" : "him";
        for (size_t at = ret.find("<pronoun>"); at != std::string::npos; at = ret.find("<pronoun>", at + objForm.size()))
          ret.replace(at, 9, objForm);
        ret = resolve(ret);
        if (!ret.empty()) ret[0] = static_cast<char>(toupper(static_cast<unsigned char>(ret[0])));
        // The retaliatory string repeats the target as its own subject, so it
        // reads as the next beat of the failure, not a continuation.
        line += ". " + ret;
      }
      std::string where;
      if (ensureTable("a16_secret-agent-assassination-locations-enc")) {
        const Table& loc = tbl("a16_secret-agent-assassination-locations-enc");
        std::vector<size_t> lp;
        for (size_t r = 0; r < loc.size(); ++r) lp.push_back(r);
        const size_t lr = weightedPickOf(loc, "Commonness Score", lp, spyRng_);
        if (lr != size_t(-1)) where = trim(loc.at(lr, "Location"));
      }
      briefing = "The client wants " + (spyTarget_.empty() ? std::string("the target") : spyTarget_) +
                 " dead. Suggested method: " + (hitMethod.empty() ? trim(m.at(row, "Method")) : hitMethod) + "." +
                 (where.empty() ? std::string() : " The opportunity comes at the " + where + ".");
      // The client's own reason for the contract. The table writes the client in
      // the third person, so the pronoun pair points at a freshly drawn client.
      const size_t cr = spyRow("a16_secret-agent-assassination-client-reasons-enc", "Commonness Score", "", "", "");
      if (cr != size_t(-1)) {
        const bool savedSet = pronounOtherSet_;
        const bool savedFemale = pronounOtherFemale_;
        pronounOtherSet_ = true;
        pronounOtherFemale_ = spyRng_.chance(0.5);
        const std::string why =
            resolve(trim(tbl("a16_secret-agent-assassination-client-reasons-enc").at(cr, "Snippet")));
        pronounOtherSet_ = savedSet;
        pronounOtherFemale_ = savedFemale;
        if (!why.empty()) briefing += " The client says " + spySentence(spyLowerFirst(why));
      }
    }
  } else if (type == "stealth") {
    std::string item, verb, where;
    if (ensureTable("a16_secret-agent-stealth-item-targets-enc")) {
      const Table& it = tbl("a16_secret-agent-stealth-item-targets-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < it.size(); ++r) pool.push_back(r);
      const size_t row = weightedPickOf(it, "Commonness Score", pool, spyRng_);
      if (row != size_t(-1)) {
        item = trim(it.at(row, "Target"));
        std::vector<std::string> verbs;
        splitOn(it.at(row, "Verbs"), ',', verbs);
        if (!verbs.empty()) verb = lower(trim(verbs[spyRng_.below(uint32_t(verbs.size()))]));
      }
    }
    if (ensureTable("a16_secret-agent-stealth-mission-locations-enc")) {
      const Table& loc = tbl("a16_secret-agent-stealth-mission-locations-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < loc.size(); ++r) pool.push_back(r);
      const size_t row = weightedPickOf(loc, "Commonness Score", pool, spyRng_);
      if (row != size_t(-1)) {
        std::string name = resolve(trim(loc.at(row, "Location")));
        std::string article;
        if (loc.flag(row, "Uses Definite Article")) article = "the ";
        else if (loc.flag(row, "Uses Indefinite Article")) article = articleFor(name);
        where = article + name;
      }
    }
    if (!item.empty()) {
      briefing = "Get into " + (where.empty() ? std::string("the venue") : where) + " and " +
                 (verb.empty() ? std::string("steal") : verb) + " the " + item;
      if (item.back() != '.') briefing += ".";
    } else {
      briefing = "Get into " + (where.empty() ? std::string("the venue") : where) + " undetected.";
    }
  } else if (type == "honeypot") {
    std::string preference, where;
    if (ensureTable("a16_secret-agent-honeypot-target-preferences-enc")) {
      const Table& p = tbl("a16_secret-agent-honeypot-target-preferences-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < p.size(); ++r)
        if (!trim(p.at(r, "Preference")).empty()) pool.push_back(r);
      const size_t row = weightedPickOf(p, "Turn-On Commonness", pool, spyRng_);
      if (row != size_t(-1)) preference = trim(p.at(row, "Preference"));
    }
    if (ensureTable("a16_secret-agent-honeypot-meeting-locations-enc")) {
      const Table& loc = tbl("a16_secret-agent-honeypot-meeting-locations-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < loc.size(); ++r) pool.push_back(r);
      const size_t row = weightedPickOf(loc, "Commonness Score", pool, spyRng_);
      if (row != size_t(-1)) where = trim(loc.at(row, "Location"));
    }
    if (!preference.empty()) {
      briefing = "Meet " + (spyTarget_.empty() ? std::string("the mark") : spyTarget_) + " at the " +
                 (where.empty() ? std::string("usual place") : where) + " and work the fact that " +
                 (h.female ? "he" : "she") + " cannot resist " + preference + ".";
    } else {
      briefing = "Meet " + (spyTarget_.empty() ? std::string("the mark") : spyTarget_) + " at the " +
                 (where.empty() ? std::string("usual place") : where) + ".";
    }
    const std::string approach = spySnippet("a16_secret-agent-honeypot-agent-approach-style-enc",
                                            "Approach", "Commonness Score", "", "", "");
    if (!approach.empty()) briefing += " Approach: " + spySentence(approach);
  } else if (type == "hacking") {
    std::string file, kind, drive;
    if (ensureTable("a16_secret-agent-hack-mission-file-name-enc")) {
      const Table& f = tbl("a16_secret-agent-hack-mission-file-name-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < f.size(); ++r) pool.push_back(r);
      const size_t row = weightedPickOf(f, "Commonness Score", pool, spyRng_);
      if (row != size_t(-1)) {
        file = trim(f.at(row, "Name"));
        kind = lower(trim(f.at(row, "File Type")));
      }
    }
    if (ensureTable("a16_secret-agent-hack-drive-names-enc")) {
      const Table& d = tbl("a16_secret-agent-hack-drive-names-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < d.size(); ++r) pool.push_back(r);
      const size_t row = weightedPickOf(d, "Commonness Score", pool, spyRng_);
      if (row != size_t(-1)) drive = trim(d.at(row, "Name"));
    }
    if (file.empty()) return false;
    briefing = "Pull the " + file + (kind.empty() ? std::string() : " " + kind) + " file off the " +
               (drive.empty() ? std::string("agency") : drive) + " drive and leave no trace.";
  } else {  // intel: no physical job, just the tail
    briefing = "Tail " + (spyTarget_.empty() ? std::string("the mark") : spyTarget_) + ".";
  }

  // The rest of the family rides on the same briefing: the target's routine and
  // the opening it leaves, the distraction that buys it, the venue's own
  // security, the noise cover, the gadget the agency issued, and the cover job.
  {
    if (type == "assassination") {
      const size_t ar = spyRow("a16_secret-agent-assassination-target-activities-enc", "Commonness Score", "", "", "");
      if (ar != size_t(-1)) {
        const Table& t = tbl("a16_secret-agent-assassination-target-activities-enc");
        std::string routine = resolve(trim(t.at(ar, "Snippet")));
        const std::string loc = trim(t.at(ar, "Location"));
        if (!routine.empty()) {
          if (!loc.empty()) routine += " at the " + loc;
          briefing += " The target is usually " + routine + ".";
        }
      }
      const std::string opening = spySnippet("a16_secret-agent-assassination-target-opportunity-snippets-enc",
                                             "Snippet", "Opportunity Score", "", "", "");
      if (!opening.empty()) {
        std::string shown = opening;
        shown[0] = static_cast<char>(toupper(static_cast<unsigned char>(shown[0])));
        briefing += " Best opening: " + shown + ".";
      }
      const std::string who = spySnippet("a16_secret-agent-distraction-random-people-enc", "Person", "Commonness Score", "", "", "");
      const std::string doing = spySnippet("a16_secret-agent-distraction-people-activities-enc", "Snippet", "Commonness Score", "", "", "");
      if (!who.empty()) {
        std::string distraction = articleFor(who) + who;
        if (!doing.empty()) distraction += ", " + doing;
        briefing += " Distraction: " + distraction + ".";
      }
    } else if (type == "stealth" || type == "hacking") {
      const std::string security = spySnippet("a16_secret-agent-security-features-enc", "Security Feature", "Commonness Score", "", "", "");
      if (!security.empty()) briefing += " Security on site: " + spySentence(security);
      if (type == "stealth") {
        stealthObstacle = spySnippet("a16_secret-agent-stealth-mission-obstacles-enc", "Person", "Commonness Score", "", "", "");
    const std::string& obstacle = stealthObstacle;
        const std::string encounters = spySnippet("a16_secret-agent-stealth-cover-encounters-enc", "Hostile Entity", "", "", "", "");
        const std::string hiding = spySnippet("a16_secret-agent-stealth-cover-hiding-place-enc", "Hiding Place", "", "", "", "");
        const std::string noise = spySnippet("a16_secret-agent-stealth-door-noises-enc", "Positive Outcome Snippet", "", "", "", "");
        if (!obstacle.empty()) briefing += " Watch for " + spySentence(articleFor(obstacle) + obstacle);
      // The obstacle is also who the cover story is for (…-obstacle-stories):
      // the story tells about the obstacle's own family, so its pronoun pair is
      // drawn with it.
      const size_t storyRow = spyRow("a16_secret-agent-stealth-obstacle-stories-enc", "Commonness Score", "", "", "");
      if (storyRow != size_t(-1) && !obstacle.empty()) {
        const bool guardFemale = spyRng_.chance(0.5);
        std::string raw = trim(tbl("a16_secret-agent-stealth-obstacle-stories-enc").at(storyRow, "Snippet"));
        raw = spySwap(raw, "<his/her>", guardFemale ? "her" : "his");
        raw = spySwap(raw, "<him/her>", guardFemale ? "her" : "him");
        raw = spySwap(raw, "<he/she>", guardFemale ? "she" : "he");
        const std::string story = resolve(raw);
        if (!story.empty())
          briefing += " My cover story: I tell them " + spySentence(spyLowerFirst(story));
      }
        if (!encounters.empty()) briefing += " Expect " + spySentence(encounters);
        if (!hiding.empty()) briefing += " Hiding place: " + spySentence(hiding);
        if (!noise.empty()) briefing += " Cover the noise with " + spySentence(noise);
      } else {
        const size_t row = spyRow("a16_secret-agent-infiltration-passwords-enc", "Commonness Score", "", "", "");
        if (row != size_t(-1)) {
          const Table& p = tbl("a16_secret-agent-infiltration-passwords-enc");
          std::vector<std::string> hints, wrong;
          splitOn(p.at(row, "Hints"), ',', hints);
          splitOn(p.at(row, "Incorrect Choices"), ',', wrong);
          if (!hints.empty()) {
            std::string hint = " The password hints at " + lower(trim(hints[spyRng_.below(uint32_t(hints.size()))]));
            if (!wrong.empty()) hint += ", not " + lower(trim(wrong[spyRng_.below(uint32_t(wrong.size()))]));
            briefing += hint + ".";
          }
        }
      }
    } else if (type == "honeypot") {
      const std::string hobby = spySnippet("a16_secret-agent-honeypot-target-hobbies-enc", "Hobby", "Commonness Score", "", "", "");
      const std::string place = spySnippet("a16_secret-agent-honeypot-date-locations-enc", "Date Location", "", "", "", "");
      std::string sting = spySnippet("a16_secret-agent-honeypot-sting-actions-enc", "String", "Commonness Score", "", "", "");
      if (!hobby.empty()) briefing += " Their weak spot is " + hobby + ".";
      if (!place.empty())
        briefing += " Take " + (spyTarget_.empty() ? std::string("the mark") : spyTarget_) + " to " + place + ".";
      const std::string stingWhere = spySnippet("a16_secret-agent-honeypot-sting-locations-enc",
                                                "String", "Commonness Score", "", "", "");
      if (!stingWhere.empty()) sting += (sting.empty() ? std::string() : " ") + stingWhere;
      if (!sting.empty()) {
        // The sting strings address the mark, so the object pronoun is the
        // mark's own: the opposite of the hero, as the target draw assumes.
        sting = spySwap(sting, "<him/her>", h.female ? "him" : "her");
        sting = spySwap(sting, "<his/her>", h.female ? "his" : "her");
        briefing += " Close with " + spyLowerFirst(sting) + ".";
      }
    } else {
      const std::string about = spySnippet("a16_secret-agent-spy-on-person-intel-enc", "Snippet", "", "Target Gender", h.female ? "male" : "female", "");
      const std::string doing = spySnippet("a16_secret-agent-snoop-person-activity-enc", "Snippet", "", "Gender", h.female ? "male" : "female", "");
      if (!about.empty())
        briefing += " Intel: " + (spyTarget_.empty() ? std::string("the mark") : spyTarget_) + " " + about + ".";
      if (!doing.empty()) briefing += " They have been " + doing + ".";
    }
    if (!h.spyCover.empty() && spyRng_.chance(0.35)) briefing += " Cover: " + spySentence(h.spyCover);
  }
  if (briefing.empty()) return false;

  std::string summary;
  std::string summaryTable;
  std::string summaryRow;
  if (ensureTable("a16_secret-agent-mission-summary-feedback-enc")) {
    const Table& sm = tbl("a16_secret-agent-mission-summary-feedback-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < sm.size(); ++r) {
      if (lower(trim(sm.at(r, "Mission Type"))) != type) continue;
      if (!sm.flag(r, success ? "Success Result" : "Failure Result")) continue;
      const double lo = sm.num(r, "Min Agent Proficiency");
      const double hi = sm.num(r, "Max Agent Proficiency");
      if (lo > 0 && h.spyProficiency < int(lo)) continue;
      if (hi > 0 && h.spyProficiency > int(hi)) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t row = pool[spyRng_.below(uint32_t(pool.size()))];
      summary = resolve(trim(sm.at(row, "String")));
      summaryTable = sm.name();
      summaryRow = std::to_string(row + 1);
    }
  }

  if (type == "intel" && line.empty())
    line = success ? "I got everything the client asked for and nobody knew I was there."
                   : "I came back with nothing and the client was not impressed.";
  if (!h.spySpec.empty()) {
    const std::string gadgetTable = success ? "a16_secret-agent-gadget-positive-snippet-enc"
                                            : "a16_secret-agent-gadget-negative-snippet-enc";
    const size_t row = spyRow(gadgetTable, "", success ? "Mission Type" : "Incorrect Mission Type", type, "");
    if (row != size_t(-1)) {
      const Table& gt = tbl(gadgetTable.c_str());
      const std::string name = lower(trim(gt.at(row, "Gadget")));
      std::string what = trim(gt.at(row, success ? "Snippet" : "Negative Snippet"));
      if (success) {
        std::string shown = resolve(what);
        if (!(shown.size() >= 2 && shown[0] == 'I' && shown[1] == ' ')) shown = spyLowerFirst(shown);
        line += (line.empty() ? std::string() : " ") + std::string("My ") + name + " let me " + shown + ".";
      } else {
        // The negative snippets talk about the agent in the third person, so the
        // first person goes in before the row is resolved.
        what = spySwap(what, "<he/she>", "I");
        what = spySwap(what, "<his/her>", "my");
        what = spySwap(what, "<him/her>", "me");
        std::string shown = resolve(what);
        if (!(shown.size() >= 2 && shown[0] == 'I' && shown[1] == ' ')) shown = spyLowerFirst(shown);
        line += (line.empty() ? std::string() : " ") + std::string("My ") + name + " misfired: " + shown + ".";
      }
    }
  }
  if (success && type == "assassination") {
    const std::string method = spySnippet("a16_secret-agent-assassination-body-disposal-methods-enc", "Feedback Snippet", "", "", "", "");
    const std::string where = spySnippet("a16_secret-agent-assassination-body-disposal-locations-enc", "Feedback Snippet", "", "", "", "");
    if (!method.empty() || !where.empty())
      line += (line.empty() ? std::string() : " ") + std::string("I ") + method +
              (where.empty() ? std::string() : " " + where) + ".";
  }
  if (!success && spyRng_.chance(0.35)) {
    const size_t row = spyRow("a16_secret-agent-rival-agency-names-enc", "Commonness Score", "", "", "");
    if (row != size_t(-1)) {
      const Table& ra = tbl("a16_secret-agent-rival-agency-names-enc");
      line += (line.empty() ? std::string() : " ") + trim(ra.at(row, "Abbreviation")) + " (" +
              trim(ra.at(row, "Name")) + ") was working the same job.";
    }
  }
  if (!success && h.spyProficiency < 35 && spyRng_.chance(0.30)) {
    const std::string seen = spySnippet("a16_secret-agent-missing-agent-reveal-snippet-enc", "Snippet", "Commonness Score", "Gender", h.female ? "female" : "male", "");
    if (!seen.empty())
      line += (line.empty() ? std::string() : " ") + "Someone swears they saw me " + seen + ".";
  }
  // A botched sneak job on a guard who will not stay down (…-obstacle-assassinations).
  if (type == "stealth" && !success && !stealthObstacle.empty() && spyRng_.chance(0.30)) {
    const size_t oa = spyRow("a16_secret-agent-stealth-obstacle-assassinations-enc", "Commonness Score", "", "", "");
    if (oa != size_t(-1)) {
      const std::string failVerb =
          lower(trim(tbl("a16_secret-agent-stealth-obstacle-assassinations-enc").at(oa, "Fail Verb")));
      if (!failVerb.empty())
        line += (line.empty() ? std::string() : " ") + "I tried to " + failVerb + " " +
                articleFor(stealthObstacle) + stealthObstacle + ", and it went badly.";
    }
  }
  // Warnings pass down the chain: the hero tells a colleague to shape up, and
  // the colleague answers (a16_secret-agent-warnings / …-warn-agent-retorts).
  if (h.secretAgent && h.spyProficiency >= 40 && spyRng_.chance(0.25)) {
    const size_t wr = spyRow("a16_secret-agent-warnings-enc", "Commonness Score", "", "", "");
    if (wr != size_t(-1)) {
      if (spyColleague_.empty()) spyColleague_ = drawFirstName(spyRng_.chance(0.5)) + " " + drawSurname();
      const bool savedSet = pronounOtherSet_;
      const bool savedFemale = pronounOtherFemale_;
      pronounOtherSet_ = true;
      pronounOtherFemale_ = spyRng_.chance(0.5);
      std::string raw = spySwap(trim(tbl("a16_secret-agent-warnings-enc").at(wr, "Scenario")),
                                "<agent-spy>", spyColleague_);
      const std::string warning = spySentence(resolve(raw));
      std::string retort;
      if (spyRng_.chance(0.6)) {
        const size_t rr = spyRow("a16_secret-agent-warn-agent-retorts-enc", "", "", "", "");
        if (rr != size_t(-1))
          retort = spySentence(resolve(spySwap(
              trim(tbl("a16_secret-agent-warn-agent-retorts-enc").at(rr, "Scenario")),
              "<agent-spy>", spyColleague_)));
      }
      pronounOtherSet_ = savedSet;
      pronounOtherFemale_ = savedFemale;
      if (!warning.empty()) line += (line.empty() ? std::string() : " ") + warning;
      if (!retort.empty()) line += " " + retort;
    }
  }

  const std::vector<std::string> extra = out.extra;
  out = AgeUp();
  out.extra = extra;
  out.table = summaryTable.empty() ? (type == "intel" ? "a16_secret-agent-spy-on-person-intel-enc"
                                                      : "a16_secret-agent-mission-summary-feedback-enc")
                                  : summaryTable;
  out.row_id = summaryRow;
  out.title = spyToken("operation");
  if (out.title.empty()) out.title = "Mission briefing";
  out.body = briefing;
  if (!line.empty()) out.extra.push_back(line);
  if (!summary.empty()) out.narrative = summary;
  h.happiness += success ? 2 : -2;
  return true;
}

// ---------------------------------------------------------------------------
// The fight career (REPORT.en.md 59). The a16_fight-* family is a combat line:
// arenas, opponents banded by fighter level, coaches, injuries, fan
// interaction, interviews, sponsorships, press conferences, street fights and
// PEDs. The port models it with a level (the opponent-type band), a league
// position (1 is the top of the card), a form score, reputation, street cred
// and a stamina pool; every gate column the family ships is checked against
// those numbers, so a top-of-the-card fighter never draws a gym brute's reason.
void Sim::makeFighter(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.fighter = true;
  // The fighting style is a label: the move system is not modelled, so nothing
  // is filtered on it and the style gates no row.
  const size_t styleRow = pickRow("a16_fight-ing-style-identity-color-enc", "Commonness Score", "", "", "", fightRng_);
  if (styleRow != size_t(-1))
    h.fightStyle = trim(tbl("a16_fight-ing-style-identity-color-enc").at(styleRow, "Fighting Style"));
  if (h.fightStyle.empty()) h.fightStyle = "striker";
  const std::string arena = pickSnippet("a16_fight-ing-arena-name-enc", "Arena Name", "Commonness Score", "", "", "", fightRng_);
  h.fightArena = arena.empty() ? "a local gym" : arena;
  // No coach roster ships with the family, only coach feedback, so the coach is
  // drawn from the name pools and a quality score stands in for a real ranking.
  h.fightCoach = drawFirstName(fightRng_.chance(0.5)) + " " + drawSurname();
  h.fightCoachFemale = fightRng_.chance(0.5);
  h.fightCoachQuality = 40 + int(fightRng_.below(51));
  h.fightLevel = 1;
  h.fightLeaguePos = 20;
  h.fightReputation = 10;
  h.fightStreetCred = 0;
  h.fightStrength = 40 + int(fightRng_.below(21));
  h.fightToughness = h.fightStrength;
  h.fightAgility = 40 + int(fightRng_.below(21));
  h.fightTechnique = 40 + int(fightRng_.below(21));
  h.fightStamina = 100;
}

std::string Sim::fightToken(const std::string& token) {
  if (!hero_) return std::string();
  Hero& h = *const_cast<Hero*>(hero_);
  std::string key = token;
  bool possessive = false;
  const std::string psuffix = "-possessive";
  if (key.size() > psuffix.size() && key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    possessive = true;
    key.resize(key.size() - psuffix.size());
  }
  if (key == "fighter") {
    // The fight-ing rows mean the opponent; the fight-er persona rows mean the
    // hero, which is what fightSelf_ marks.
    const std::string base = fightSelf_ || fightOpponent_.empty() ? h.firstName : fightOpponent_;
    return possessive ? base + "'s" : base;
  }
  if (key == "coach-fighting") return possessive ? h.fightCoach + "'s" : h.fightCoach;
  if (key == "fight-opponent-type") return fightOpponentType_;
  if (key == "opponent-name") return fightOpponent_;
  if (key == "fighting-arena") return fightArena_.empty() ? h.fightArena : fightArena_;
  if (key == "fighting-style") return h.fightStyle;
  if (key == "circumstance-fighter" || key == "fighter-circumstance") {
    const std::map<std::string, std::string>::const_iterator it = fightCache_.find("circumstance");
    if (it != fightCache_.end()) return it->second;
    const size_t r = pickRow("a16_fight-er-circumstance-enc", "", "", "", "", fightRng_);
    const std::string v = (r == size_t(-1)) ? std::string() : resolve(trim(tbl("a16_fight-er-circumstance-enc").at(r, "String")));
    if (!v.empty()) fightCache_["circumstance"] = v;
    return v;
  }
  return std::string();
}

bool Sim::fightYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.fighter || h.age < 18) return false;
  if (!fightRng_.chance(0.55)) return false;
  fightSelf_ = false;
  fightOpponent_.clear();
  fightOpponentType_.clear();
  fightArena_ = h.fightArena;
  std::string title, body, line, tableRef, rowRef;
  std::vector<std::string> extra;
  int happiness = 0;
  auto say = [&body](const std::string& s) {
    if (s.empty()) return;
    body += body.empty() ? s : " " + s;
  };
  auto tell = [&line](const std::string& s) {
    if (s.empty()) return;
    line += line.empty() ? s : " " + s;
  };
  // pickRow matches exact values (a blank cell fits anyone); the Min/Max windows
  // the fight tables ship are checked here, with a couple of retries so an
  // unlucky draw never empties the pool.
  auto pickWin = [&](const char* t, const char* weight, const char* fcol, const std::string& fval,
                     const char* loCol, const char* hiCol, double value) -> size_t {
    return pickWindow(t, weight, fcol, fval, loCol ? loCol : "", hiCol ? hiCol : "", value, fightRng_);
  };

  if (h.fightStamina < 35 && fightRng_.chance(0.6)) {
    // A worn-down fighter takes the year off; the recharge-scenario Method cell
    // is the link into the recharge-method table that carries the numbers.
    const size_t r = pickRow("a16_fight-recharge-method-scenario-enc", "Commonness Score", "", "", "", fightRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_fight-recharge-method-scenario-enc");
      title = "Time off";
      body = resolve(trim(t.at(r, "String")));
      line = resolve(trim(t.at(r, "Log Version")));
      tableRef = t.name();
      rowRef = std::to_string(r + 1);
      const std::string method = lower(trim(t.at(r, "Method")));
      if (ensureTable("a16_fight-recharge-method-enc")) {
        const Table& m = tbl("a16_fight-recharge-method-enc");
        for (size_t i = 0; i < m.size(); ++i) {
          if (lower(trim(m.at(i, "Method"))) != method) continue;
          h.fightStamina = std::min(100, h.fightStamina + int(m.num(i, "Stamina Gain")));
          happiness += int(m.num(i, "Happiness Impact"));
          say("It sets me back $" + std::to_string(int(m.num(i, "Cost"))) + ".");
          break;
        }
      }
      if (h.fightStamina < 20) h.fightStamina = 20;
    }
  } else if (fightRng_.chance(0.30)) {
    const size_t r = pickWin("a16_fight-ing-coach-training-string-enc", "", "Hero Gender",
                             h.female ? "female" : "male", "Min Coach Quality", "Max Coach Quality", h.fightCoachQuality);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_fight-ing-coach-training-string-enc");
      title = "A session at the gym";
      const bool savedSet = pronounOtherSet_, savedFemale = pronounOtherFemale_;
      pronounOtherSet_ = true;
      pronounOtherFemale_ = h.fightCoachFemale;
      body = resolve(trim(t.at(r, "String")));
      line = resolve(trim(t.at(r, "Log Version")));
      pronounOtherSet_ = savedSet;
      pronounOtherFemale_ = savedFemale;
      tableRef = t.name();
      rowRef = std::to_string(r + 1);
      h.fightStamina = std::min(100, h.fightStamina + 10);
      const size_t mv = pickRow("a16_fight-ing-move-enc", "", "", "", "", fightRng_);
      if (mv != size_t(-1)) {
        const std::string name = resolve(trim(tbl("a16_fight-ing-move-enc").at(mv, "Move Name")));
        if (!name.empty()) say("I drilled the " + spyLowerFirst(name) + ".");
      }
      if (fightRng_.chance(0.08)) {
        const size_t inj = pickRow("a16_fight-hero-training-injury-string-enc", "Commonness Score", "", "", "", fightRng_);
        if (inj != size_t(-1))
          extra.push_back(resolve(trim(tbl("a16_fight-hero-training-injury-string-enc").at(inj, "String"))));
      }
    }
  } else {
    const int band = h.fightLeaguePos <= 3 ? 5 : (h.fightLeaguePos <= 7 ? 4 : (h.fightLeaguePos <= 12 ? 3 : (h.fightLeaguePos <= 18 ? 2 : 1)));
    h.fightLevel = band;
    bool oppFemale = false;
    const size_t orow = pickRow("a16_fight-ing-opponent-type-enc", "Commonness Score", "Fighter Level", std::to_string(band), "", fightRng_);
    if (orow != size_t(-1)) {
      const Table& ot = tbl("a16_fight-ing-opponent-type-enc");
      fightOpponentType_ = resolve(trim(ot.at(orow, "Description")));
      double pctMale = ot.has("Percent Male") ? ot.num(orow, "Percent Male") : 100;
      if (pctMale > 1) pctMale /= 100.0;  // the column ships whole percents
      if (pctMale < 0) pctMale = 0;
      if (pctMale > 1) pctMale = 1;
      oppFemale = !fightRng_.chance(pctMale);
    }
    if (fightOpponentType_.empty()) fightOpponentType_ = "opponent";
    fightOpponent_ = drawFirstName(oppFemale) + " " + drawSurname();
    // Championship venues only headline for a fighter near the top of the card.
    if (ensureTable("a16_fight-ing-arena-name-enc")) {
      const Table& an = tbl("a16_fight-ing-arena-name-enc");
      std::vector<size_t> pool;
      for (size_t i = 0; i < an.size(); ++i) {
        const bool champ = an.flag(i, "Is Championship Match Arena");
        if (h.fightLeaguePos <= 4 ? champ : !champ) pool.push_back(i);
      }
      const size_t ar = weightedPickOf(an, "Commonness Score", pool, fightRng_);
      if (ar != size_t(-1)) {
        const std::string name = resolve(trim(an.at(ar, "Arena Name")));
        if (!name.empty()) fightArena_ = name;
      }
    }
    title = "A card at " + fightArena_;
    say("My opponent: " + fightOpponent_ + ", " + articleFor(fightOpponentType_) + fightOpponentType_ + ".");
    if (fightRng_.chance(0.35)) {
      // The coach-quality table is the coach's own visible character, so its
      // pronouns follow the coach and its quality window gates the row.
      const size_t cq = pickWin("a16_fight-coach-quality-color-string-enc", "", "Coach Gender",
                                h.fightCoachFemale ? "female" : "male", "Min Coach Quality", "Max Coach Quality", h.fightCoachQuality);
      if (cq != size_t(-1)) {
        const bool savedSet = pronounOtherSet_, savedFemale = pronounOtherFemale_;
        pronounOtherSet_ = true;
        pronounOtherFemale_ = h.fightCoachFemale;
        const std::string cl = resolve(trim(tbl("a16_fight-coach-quality-color-string-enc").at(cq, "String")));
        pronounOtherSet_ = savedSet;
        pronounOtherFemale_ = savedFemale;
        if (!cl.empty()) say(spySentence(cl));
      }
    }
    if (fightRng_.chance(0.10)) {
      // A doping challenge reads as whoever accuses the hero, which is the
      // opponent drawn above.
      const size_t dc = pickRow("a16_fight-doping-challenge-color-string-enc", "Commonness Score", "", "", "", fightRng_);
      if (dc != size_t(-1)) {
        const bool savedSet = pronounOtherSet_, savedFemale = pronounOtherFemale_;
        pronounOtherSet_ = true;
        pronounOtherFemale_ = oppFemale;
        const std::string ch = resolve(trim(tbl("a16_fight-doping-challenge-color-string-enc").at(dc, "String")));
        pronounOtherSet_ = savedSet;
        pronounOtherFemale_ = savedFemale;
        if (!ch.empty()) say(spySentence(ch));
      }
    }
    const double form = (h.fightStrength + h.fightToughness + h.fightAgility + h.fightTechnique) / 4.0;
    double odds = 0.5 + (form - 50.0) / 150.0 + (12 - h.fightLeaguePos) / 60.0 - (band - 1) * 0.05;
    if (odds < 0.15) odds = 0.15;
    if (odds > 0.9) odds = 0.9;
    bool win = fightRng_.chance(odds);
    // A thrown fight is rare, and it only ever comes out of a win.
    if (win && fightRng_.chance(0.05)) {
      const size_t dr = pickRow("a16_fight-ing-dive-scenario-enc", "Commonness Score", "Hero Gender", h.female ? "female" : "male", "", fightRng_);
      if (dr != size_t(-1)) {
        const Table& d = tbl("a16_fight-ing-dive-scenario-enc");
        title = "I went down on purpose";
        body = resolve(trim(d.at(dr, "Scenario")));
        line = resolve(trim(d.at(dr, "Log Version")));
        tableRef = d.name();
        rowRef = std::to_string(dr + 1);
        win = false;
      }
    }
    if (win) {
      ++h.fightWins;
      if (h.fightLeaguePos > 1) --h.fightLeaguePos;
      h.fightReputation += 5;
      happiness += 3;
    } else {
      ++h.fightLosses;
      if (h.fightLeaguePos < 24) ++h.fightLeaguePos;
      h.fightReputation -= 2;
      happiness -= 3;
    }
    h.fightStamina -= 20;
    if (h.fightStamina < 0) h.fightStamina = 0;
    if (tableRef.empty()) {
      const size_t oc = pickRow("a16_fight-outcome-color-enc", "Commonness Score", "", "", "", fightRng_);
      if (oc != size_t(-1)) {
        const Table& o = tbl("a16_fight-outcome-color-enc");
        line = resolve(trim(o.at(oc, win ? "Hero Win Version" : "Hero Lose Version")));
        tableRef = o.name();
        rowRef = std::to_string(oc + 1);
      }
    }
    // Why it went that way: the reason tables gate on where both fighters sit
    // on the card, so a prelim never draws a title-fight reason.
    const char* rtable = win ? "a16_fight-ing-win-reason-enc" : "a16_fight-ing-loss-reason-enc";
    const char* rweight = win ? "Coolness Score" : "Sympathy Score";
    for (int attempt = 0; attempt < 3; ++attempt) {
      const size_t rr = pickRow(rtable, rweight, "", "", "", fightRng_);
      if (rr == size_t(-1)) break;
      const Table& t = tbl(rtable);
      const double loH = t.has("Min Hero League Position") ? t.num(rr, "Min Hero League Position") : 0;
      const double hiH = t.has("Max Hero League Position") ? t.num(rr, "Max Hero League Position") : 0;
      if ((loH > 0 && h.fightLeaguePos < loH) || (hiH > 0 && h.fightLeaguePos > hiH)) continue;
      const std::string reason = resolve(trim(t.at(rr, "Reason")));
      if (!reason.empty())
        tell(std::string(win ? "I put it down to " : "The reason: ") + spySentence(spyLowerFirst(reason)));
      break;
    }
    // The corner talks: the coach feedback tables are filtered by outcome and by
    // the coach's quality window.
    if (h.fightCoachQuality > 0) {
      const size_t cf = pickWin("a16_fight-coach-generic-feedback-string-enc", "", "Match Outcome",
                                win ? "win" : "loss", "Min Coach Quality", "Max Coach Quality", h.fightCoachQuality);
      if (cf != size_t(-1)) {
        const Table& t = tbl("a16_fight-coach-generic-feedback-string-enc");
        const bool savedSet = pronounOtherSet_, savedFemale = pronounOtherFemale_;
        pronounOtherSet_ = true;
        pronounOtherFemale_ = h.fightCoachFemale;
        const std::string fb = resolve(trim(t.at(cf, "Log Version")));
        pronounOtherSet_ = savedSet;
        pronounOtherFemale_ = savedFemale;
        if (!fb.empty()) tell(spySentence(fb));
      }
      if (!win && fightRng_.chance(0.4)) {
        const size_t lf = pickWin("a16_fight-coach-loss-feedback-string-enc", "", "", "", "Min Coach Quality", "Max Coach Quality", h.fightCoachQuality);
        if (lf != size_t(-1)) {
          const Table& t = tbl("a16_fight-coach-loss-feedback-string-enc");
          const bool savedSet = pronounOtherSet_, savedFemale = pronounOtherFemale_;
          pronounOtherSet_ = true;
          pronounOtherFemale_ = h.fightCoachFemale;
          const std::string fb = resolve(trim(t.at(lf, "Log Version")));
          pronounOtherSet_ = savedSet;
          pronounOtherFemale_ = savedFemale;
          if (!fb.empty()) tell(spySentence(fb));
        }
      }
    }
    if (fightRng_.chance(0.40)) {
      const bool fanFemale = fightRng_.chance(0.5);
      const size_t fr = pickRow("a16_fight-ing-fan-interaction-enc", "", "Fan Gender", fanFemale ? "female" : "male", "", fightRng_);
      if (fr != size_t(-1)) {
        const Table& t = tbl("a16_fight-ing-fan-interaction-enc");
        const std::string scen = resolve(trim(t.at(fr, "Scenario")));
        if (!scen.empty()) say(spySentence(scen));
        const std::string flog = resolve(trim(t.at(fr, "Log Version Snippet")));
        if (!flog.empty()) extra.push_back("A fan " + spySentence(spyLowerFirst(flog)));
      }
    }
    if (fightRng_.chance(0.30)) {
      const size_t ir = pickRow("a16_fight-ing-interview-fan-reaction-enc", "Commonness Score", "Match Outcome", win ? "win" : "loss", "", fightRng_);
      if (ir != size_t(-1)) {
        const std::string reaction = resolve(trim(tbl("a16_fight-ing-interview-fan-reaction-enc").at(ir, "Log Version")));
        if (!reaction.empty()) extra.push_back(spySentence(reaction));
      }
    }
    if (fightRng_.chance(0.25)) {
      const size_t qr = pickRow("a16_fight-press-conference-question-enc", "Commonness Score", "", "", "", fightRng_);
      if (qr != size_t(-1)) {
        const Table& q = tbl("a16_fight-press-conference-question-enc");
        // The card cannot offer twenty answers, so one is picked; every effect
        // cell in this table is an x-flag rather than a stat name, so the port
        // moves reputation and stamina instead of inventing a stat map.
        std::vector<int> pool;
        for (int c = 1; c <= 20; ++c) {
          const std::string col = "Choice " + std::to_string(c);
          if (q.has(col) && !trim(q.at(qr, col)).empty()) pool.push_back(c);
        }
        if (!pool.empty()) {
          const int c = pool[fightRng_.below(uint32_t(pool.size()))];
          const std::string n = std::to_string(c);
          say("The press asked: " + spySentence(resolve(trim(q.at(qr, "Question")))) +
              " I said: " + spySentence(resolve(trim(q.at(qr, "Choice " + n)))));
          const std::string bcol = "Hero Buff " + n, dcol = "Hero Debuff " + n;
          if (q.has(bcol) && !trim(q.at(qr, bcol)).empty()) { h.fightReputation += 4; h.fightStamina = std::min(100, h.fightStamina + 5); }
          if (q.has(dcol) && !trim(q.at(qr, dcol)).empty()) { h.fightReputation -= 4; h.fightStamina = std::max(0, h.fightStamina - 5); }
          const std::string outlet = pickSnippet("a16_fight-press-conference-media-outlet-enc", "Media Outlet String", "Commonness Score", "", "", "", fightRng_);
          if (!outlet.empty()) extra.push_back("The conference ran on " + outlet + ".");
        }
      }
    }
    if (fightRng_.chance(0.20)) {
      const size_t inr = pickRow("a16_fight-ing-injurie-enc", "Commonness Score", "Gender", h.female ? "female" : "male", "", fightRng_);
      if (inr != size_t(-1)) {
        const Table& t = tbl("a16_fight-ing-injurie-enc");
        const std::string ilog = resolve(trim(t.at(inr, "Log Snippet")));
        if (!ilog.empty()) extra.push_back(spySentence(ilog));
        // Surgery and the cost columns are not modelled: the injury is the stat
        // dent and the log line.
        if (t.has("Strength Decrease")) h.fightStrength = std::max(5, h.fightStrength - int(t.num(inr, "Strength Decrease")));
        if (t.has("Toughness Decrease")) h.fightToughness = std::max(5, h.fightToughness - int(t.num(inr, "Toughness Decrease")));
        if (t.has("Agility Decrease")) h.fightAgility = std::max(5, h.fightAgility - int(t.num(inr, "Agility Decrease")));
        if (t.has("Technique Decrease")) h.fightTechnique = std::max(5, h.fightTechnique - int(t.num(inr, "Technique Decrease")));
        happiness -= 2;
      }
    }
    if (h.fightLeaguePos <= 9 && fightRng_.chance(0.30)) {
      const size_t sr = pickWin("a16_fight-ing-sponsorship-company-enc", "", "", "", "Min League Position", "Max League Position", h.fightLeaguePos);
      if (sr != size_t(-1)) {
        const std::string co = resolve(trim(tbl("a16_fight-ing-sponsorship-company-enc").at(sr, "Company Name")));
        if (!co.empty()) extra.push_back("I signed with " + co + ".");
      }
    }
    if (h.fightLeaguePos <= 12 && fightRng_.chance(0.25)) {
      const size_t pr = pickWin("a16_fight-ing-product-enc", "Commonness Score", "Hero Gender", h.female ? "female" : "male", "Min Reputation", "Max Reputation", h.fightReputation);
      if (pr != size_t(-1)) {
        const std::string pn = resolve(trim(tbl("a16_fight-ing-product-enc").at(pr, "Product Name")));
        if (!pn.empty()) extra.push_back("I started pushing " + pn + ".");
      }
      // The second product table carries no Hero Gender column at all, so it is
      // filtered on the reputation window alone.
      const size_t pr2 = pickWin("a16_fighting-products", "", "", "", "MinReputation", "MaxReputation", h.fightReputation);
      if (pr2 != size_t(-1)) {
        const std::string pn = resolve(trim(tbl("a16_fighting-products").at(pr2, "ProductName")));
        if (!pn.empty()) extra.push_back("I started pushing " + pn + ".");
      }
    }
    if (h.fightLeaguePos <= 2 && fightRng_.chance(0.20)) {
      // The UFL card set is gated on the hero being champion (league position 1
      // is the top of the card) and it offers choices; the port renders it as a
      // card whose first choice is the one taken, because a year-up card here
      // has no prompt of its own to route a choice through.
      const size_t ur = pickRow("a16_fight-ufl-dumb-scenario-enc", "", "", "", "Hero is UFL Champ", fightRng_);
      if (ur != size_t(-1)) {
        const Table& t = tbl("a16_fight-ufl-dumb-scenario-enc");
        const std::string ut = resolve(trim(t.at(ur, "Title")));
        const std::string ub = resolve(trim(t.at(ur, "Body")));
        if (!ut.empty() || !ub.empty()) {
          title = ut.empty() ? std::string("The UFL") : ut;
          body = ub;
          if (t.has("CTA")) say(resolve(trim(t.at(ur, "CTA"))));
          if (t.has("Narrative 1")) tell(spySentence(resolve(trim(t.at(ur, "Narrative 1")))));
          else if (t.has("Feedback String 1")) tell(spySentence(resolve(trim(t.at(ur, "Feedback String 1")))));
          tableRef = t.name();
          rowRef = std::to_string(ur + 1);
          if (t.has("Street Cred Base Value 1")) h.fightStreetCred += int(t.num(ur, "Street Cred Base Value 1"));
          if (t.has("Happiness Impact 1")) happiness += int(t.num(ur, "Happiness Impact 1"));
        }
      }
    }
    if (fightRng_.chance(0.15)) {
      const size_t wr = pickWin("a16_fight-ing-wild-encounter-color-enc", "Commonness Score", "", "", "Min Hero League Position", "Max Hero League Position", h.fightLeaguePos);
      if (wr != size_t(-1)) {
        const Table& t = tbl("a16_fight-ing-wild-encounter-color-enc");
        const std::string scen = resolve(trim(t.at(wr, "String")));
        if (!scen.empty()) say(spySentence(scen));
        const std::string wlog = resolve(trim(t.at(wr, "Log Version")));
        if (!wlog.empty()) extra.push_back(spySentence(wlog));
        h.fightStreetCred += 3;
      }
      if (fightRng_.chance(0.35)) {
        const std::string opener = pickSnippet("a16_fight-wild-encounter-color", "ColorString", "CommonnessScore", "", "", "", fightRng_);
        if (!opener.empty()) say(spySentence(opener));
        const std::string loc = pickSnippet("a16_fight-street-ing-location-enc", "Location", "Commonness Score", "", "", "", fightRng_);
        if (!loc.empty()) { extra.push_back("Street fight outside " + loc + "."); h.fightStreetCred += 2; }
      }
      if (fightRng_.chance(0.08)) {
        const size_t anr = pickRow("a16_fight-er-animal-enc", "Commonness Score", "", "", "", fightRng_);
        if (anr != size_t(-1)) {
          const Table& t = tbl("a16_fight-er-animal-enc");
          const std::string animal = lower(trim(t.at(anr, "Animal")));
          std::string verb = "fought";
          const std::string verbs = t.at(anr, "Attack Verb");
          for (size_t s = 0; s < verbs.size();) {
            const size_t comma = verbs.find(',', s);
            const std::string part = lower(trim(comma == std::string::npos ? verbs.substr(s) : verbs.substr(s, comma - s)));
            if (!part.empty()) { verb = part; break; }
            if (comma == std::string::npos) break;
            s = comma + 1;
          }
          if (!animal.empty()) {
            extra.push_back("I " + verb + " " + articleFor(animal) + animal + ".");
            h.fightStreetCred += int(t.num(anr, "Street Creds Gained"));
            happiness += 2;
          }
        }
      }
    }
    if (fightRng_.chance(0.15)) {
      const size_t spr = pickRow("a16_fight-sportsmanship-scenario-enc", "Commonness Score", "", "", "", fightRng_);
      if (spr != size_t(-1)) {
        const Table& t = tbl("a16_fight-sportsmanship-scenario-enc");
        const std::string slog = resolve(trim(t.at(spr, "Log Version")));
        if (!slog.empty()) extra.push_back(spySentence(slog));
        h.fightReputation += t.flag(spr, "Is Positive") ? 3 : -3;
      }
    }
    if (fightRng_.chance(0.10)) {
      const size_t rrr = pickWin("a16_fight-roid-rage-scenario-enc", "", "Audience Gender", fightRng_.chance(0.5) ? "female" : "male", "Hero Min Age", "Hero Max Age", h.age);
      if (rrr != size_t(-1)) {
        const Table& t = tbl("a16_fight-roid-rage-scenario-enc");
        const std::string scen = resolve(trim(t.at(rrr, "Log Version")));
        if (!scen.empty()) extra.push_back(spySentence(scen));
        const size_t rt = pickRow("a16_fight-roid-rage-retort-enc", "", "", "", "", fightRng_);
        if (rt != size_t(-1)) {
          // The retort is aimed at the hero, so the shipped second person has to
          // move to the first before it resolves.
          std::string raw = trim(tbl("a16_fight-roid-rage-retort-enc").at(rt, "String"));
          raw = spySwap(raw, " you ", " me ");
          raw = spySwap(raw, " your ", " my ");
          if (raw.rfind("you ", 0) == 0) raw = "me " + raw.substr(4);
          const bool savedSet = pronounOtherSet_, savedFemale = pronounOtherFemale_;
          pronounOtherSet_ = true;
          pronounOtherFemale_ = fightRng_.chance(0.5);
          const std::string said = resolve(raw);
          pronounOtherSet_ = savedSet;
          pronounOtherFemale_ = savedFemale;
          if (!said.empty()) extra.push_back(spySentence(spyLowerFirst(said)));
        }
      }
    }
    if (fightRng_.chance(0.25)) {
      // The fight-er persona tables talk about the hero as the fighter, which is
      // what fightSelf_ switches <fighter> to.
      fightSelf_ = true;
      static const char* kPersona[] = {"a16_fight-er-trash-talk-enc", "a16_fight-er-compliment-enc",
                                       "a16_fight-er-admiration-scenario-enc", "a16_fight-er-concern-scenario-enc",
                                       "a16_fight-er-training-string-enc"};
      const char* pt = kPersona[fightRng_.below(5)];
      const size_t pr = pickRow(pt, "", "", "", "", fightRng_);
      if (pr != size_t(-1)) {
        const Table& t = tbl(pt);
        std::string text;
        if (t.has("Inbound Log Version")) text = trim(t.at(pr, "Inbound Log Version"));
        else if (t.has("Log Version")) text = trim(t.at(pr, "Log Version"));
        else if (t.has("Inbound Scenario")) text = trim(t.at(pr, "Inbound Scenario"));
        const std::string said = resolve(text);
        if (!said.empty()) extra.push_back(spySentence(said));
      }
      fightSelf_ = false;
    }
  }

  if (title.empty() && body.empty() && line.empty()) return false;
  out = AgeUp();
  out.table = tableRef.empty() ? "a16_fight-outcome-color-enc" : tableRef;
  out.row_id = rowRef;
  out.title = title.empty() ? "Fight night" : title;
  out.body = body;
  out.narrative = line;
  out.extra = extra;
  out.happiness = happiness;
  return true;
}

// ---------------------------------------------------------- the black market
// REPORT.en.md 60. Four businesses in one flag: an auction house that moves the
// items, a museum that shows them off, a street chemist who cuts the product,
// and a taxidermist who needs the leftovers gone. The eight item pools share a
// column shape (name / fake name / short name / possessive / article flags /
// value band), so they are read by column name rather than by index.
void Sim::makeBlackMarketeer(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.blackMarket = true;
  // The exhibit, museum and vendor pools ride the shared dumb-scenario runner.
  h.miscMask |= (1u << 9) | (1u << 10) | (1u << 11);
  ensureBmFields(life);
  h.bmAppeal = 35 + int(bmRng_.below(31));
}

// The auction house, the museum, and the two faces of the trade are named once
// and reused for the rest of the life.
void Sim::ensureBmFields(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (h.bmAuctionHouse.empty())
    h.bmAuctionHouse = bmStructureName("a16_black-market-auction-house-names-enc");
  if (h.bmMuseum.empty()) {
    const size_t r = pickRow("a16_black-market-museum-name-enc", "Commonness Score", "", "", "", bmRng_);
    if (r != size_t(-1))
      h.bmMuseum = resolve(trim(tbl("a16_black-market-museum-name-enc").at(r, "Name")));
  }
  if (h.bmMuseum.empty()) h.bmMuseum = "Memory Lane Museum";
  if (bmVendor_.empty()) bmVendor_ = drawFirstName(bmRng_.chance(0.5)) + " " + drawSurname();
  if (bmChemist_.empty()) bmChemist_ = drawFirstName(bmRng_.chance(0.5)) + " " + drawSurname();
}

// The auction house and the taxidermist ship the same Format/Prefix/Suffix
// structure table, which is the business-name shape the port already knows.
std::string Sim::bmStructureName(const char* tableName) {
  if (!ensureTable(tableName)) return std::string();
  const Table& st = tbl(tableName);
  if (st.size() == 0) return std::string();
  const size_t row = bmRng_.below(uint32_t(st.size()));
  const auto pickList = [this](const std::string& list) {
    std::vector<std::string> parts;
    splitOn(list, ',', parts);
    return parts.empty() ? std::string() : trim(parts[bmRng_.below(uint32_t(parts.size()))]);
  };
  std::string fmt = pickList(st.at(row, "Format"));
  if (fmt.empty()) return std::string();
  if (fmt.find("<color>") != std::string::npos && ensureTable("a16_company-name-color-enc")) {
    const Table& colors = tbl("a16_company-name-color-enc");
    if (colors.size())
      fmt = spySwap(fmt, "<color>", trim(colors.at(bmRng_.below(uint32_t(colors.size())), "Color")));
  }
  if (fmt.find("<suffix>") != std::string::npos) fmt = spySwap(fmt, "<suffix>", pickList(st.at(row, "Suffix")));
  if (fmt.find("<prefix>") != std::string::npos) fmt = spySwap(fmt, "<prefix>", pickList(st.at(row, "Prefix")));
  if (fmt.find("<city>") != std::string::npos)
    fmt = spySwap(fmt, "<city>", hero_ ? hero_->city : std::string());
  if (fmt.find("<random-first-name-possessive>") != std::string::npos)
    fmt = spySwap(fmt, "<random-first-name-possessive>", drawFirstName(bmRng_.chance(0.5)) + "'s");
  if (fmt.find("<random-first-name>") != std::string::npos)
    fmt = spySwap(fmt, "<random-first-name>", drawFirstName(bmRng_.chance(0.5)));
  if (fmt.find("<random-surname>") != std::string::npos) fmt = spySwap(fmt, "<random-surname>", drawSurname());
  std::string out = resolve(fmt);
  std::string clean;
  for (size_t i = 0; i < out.size(); ++i) {
    if (out[i] == '<') {
      const size_t close = out.find('>', i);
      if (close != std::string::npos) { i = close; continue; }
    }
    clean += out[i];
  }
  while (clean.find("  ") != std::string::npos) clean.replace(clean.find("  "), 2, " ");
  return trim(clean);
}

// One item off one of the eight pools. The name, the fake name, the possessive
// and the value band all come from the same row, and the row stays on the hero
// until it is sold.
std::string Sim::bmDrawItem(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  static const struct BmPool { const char* table; const char* category; } kPools[] = {
      {"a16_black-market-antiques-enc", "antiques"},
      {"a16_black-market-art-enc", "art"},
      {"a16_black-market-automobiles-enc", "auto"},
      {"a16_black-market-fashion-enc", "fashion"},
      {"a16_black-market-gemstones-enc", "gemstone"},
      {"a16_black-market-historical-artifacts-enc", "historical artifact"},
      {"a16_black-market-sports-memorabilia-enc", "sports memorabilia"},
      {"a16_black-market-weapons-enc", "weapon"},
  };
  const BmPool& pool = kPools[bmRng_.below(uint32_t(sizeof(kPools) / sizeof(kPools[0])))];
  if (!ensureTable(pool.table)) return std::string();
  const Table& t = tbl(pool.table);
  if (t.size() == 0) return std::string();
  const size_t picked = pickRow(pool.table, "Commonness Score", "", "", "", bmRng_);
  const size_t row = picked == size_t(-1) ? bmRng_.below(uint32_t(t.size())) : picked;
  auto colOf = [&t](std::initializer_list<const char*> names) -> std::string {
    for (const char* n : names)
      if (t.has(n)) return std::string(n);
    return std::string();
  };
  const std::string nameCol = colOf({"Item Name", "Title", "Gemstone", "Make"});
  std::string name = nameCol.empty() ? std::string() : trim(t.at(row, nameCol));
  if (t.has("Model")) name = trim(t.at(row, "Make")) + " " + trim(t.at(row, "Model"));
  const std::string fake = t.has("Fake Name") ? trim(t.at(row, "Fake Name")) : std::string();
  const bool canFake = t.has("Can Be Fake") && t.flag(row, "Can Be Fake") && !fake.empty();
  const bool faking = canFake && bmRng_.chance(0.4);
  const std::string shown = faking ? fake : name;
  if (shown.empty()) return std::string();
  const bool indefinite = t.has("Uses Indefinite Article") && t.flag(row, "Uses Indefinite Article");
  const bool definite = t.has("Uses Definite Article") && t.flag(row, "Uses Definite Article");
  const std::string display =
      indefinite ? articleFor(shown) + shown : (definite ? "the " + shown : shown);
  h.bmItem = display;
  h.bmItemPossessive = t.has("Possessive Name") && !trim(t.at(row, "Possessive Name")).empty()
                           ? trim(t.at(row, "Possessive Name"))
                           : shown + "'s";
  h.bmItemShort = t.has("Short Name") && !trim(t.at(row, "Short Name")).empty()
                      ? trim(t.at(row, "Short Name"))
                      : shown;
  h.bmItemPlural = t.has("Plural") && t.flag(row, "Plural");
  h.bmFake = faking;
  h.bmCategory = pool.category;
  const std::string lo = colOf({"Min Value", "Min Cost"});
  const std::string hi = colOf({"Max Value", "Max Cost"});
  const double vlo = lo.empty() ? 0.0 : t.num(row, lo);
  const double vhi = hi.empty() ? vlo : t.num(row, hi);
  const double value = vhi > vlo ? vlo + (vhi - vlo) * bmRng_.unit() : vhi;
  h.bmValue = int(value);
  return display;
}

std::string Sim::bmToken(const std::string& key) {
  if (hero_ == nullptr) return std::string();
  std::string base = key;
  const std::string suffix = "-possessive";
  bool possessive = false;
  if (base.size() > suffix.size() && base.compare(base.size() - suffix.size(), suffix.size(), suffix) == 0) {
    possessive = true;
    base.resize(base.size() - suffix.size());
  }
  if (life_ != nullptr) ensureBmFields(*const_cast<Life*>(life_));
  const Hero& h = *hero_;
  auto cached = [&](const std::string& kind, const std::string& fallback) -> std::string {
    std::string& slot = bmCache_[kind];
    if (slot.empty()) slot = fallback;
    return slot;
  };
  auto drawn = [&]() { return drawFirstName(bmRng_.chance(0.5)) + " " + drawSurname(); };
  if (base == "item" || base == "exhibit") {
    const std::string item = h.bmItem.empty() && life_ != nullptr
                                 ? bmDrawItem(*const_cast<Life*>(life_))
                                 : cached("item", h.bmItem);
    if (item.empty()) return std::string();
    if (!possessive) return item;
    return h.bmItemPossessive.empty() ? item + "'s" : h.bmItemPossessive;
  }
  if (base == "item-short") return h.bmItemShort.empty() ? bmToken("item") : h.bmItemShort;
  if (base == "auction-house") return h.bmAuctionHouse;
  if (base == "museum") return h.bmMuseum;
  if (base == "vendor") return cached("vendor", bmVendor_.empty() ? drawn() : bmVendor_);
  if (base == "drug-chemist") return cached("chemist", bmChemist_.empty() ? drawn() : bmChemist_);
  if (base == "auctioneer") return cached("auctioneer", drawn());
  if (base == "taxidermist")
    return cached("taxidermist", bmStructureName("a16_black-market-taxidermist-business-names-enc"));
  return std::string();
}

bool Sim::bmYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.blackMarket || h.age < 18) return false;
  if (!bmRng_.chance(0.45)) return false;
  ensureBmFields(life);
  std::string title, body, line, tableRef, rowRef;
  std::vector<std::string> extra;
  int happiness = 0, smarts = 0, looks = 0, discipline = 0, karma = 0;
  auto say = [&body](const std::string& s) {
    if (!s.empty()) body += body.empty() ? s : " " + s;
  };
  auto tell = [&line](const std::string& s) {
    if (!s.empty()) line += line.empty() ? s : " " + s;
  };
  const auto listHas = [](const std::string& list, const std::string& want) {
    if (trim(list).empty()) return false;
    std::vector<std::string> parts;
    splitOn(list, ',', parts);
    for (std::string& p : parts)
      if (lower(trim(p)) == want) return true;
    return false;
  };
  // Retries for the Min/Max windows pickRow cannot express; a couple of attempts
  // and then whatever was drawn, so a pool is never empty.
  const auto window = [&](const char* t, const char* weight, const char* fcol, const std::string& fval,
                          const char* loCol, const char* hiCol, double value) -> size_t {
    for (int attempt = 0; attempt < 3; ++attempt) {
      const size_t r = pickRow(t, weight, fcol, fval, "", bmRng_);
      if (r == size_t(-1)) return size_t(-1);
      const Table& tt = tbl(t);
      if (loCol != nullptr && tt.has(loCol)) {
        const double lo = tt.num(r, loCol);
        if (lo > 0 && value < lo) continue;
      }
      if (hiCol != nullptr && tt.has(hiCol)) {
        const double hi = tt.num(r, hiCol);
        if (hi > 0 && value > hi) continue;
      }
      return r;
    }
    return pickRow(t, weight, fcol, fval, "", bmRng_);
  };

  if (bmRng_.chance(0.22)) {
    // The street chemist: a product, a cutting agent, and the year's numbers.
    const size_t p = pickRow("a16_black-market-street-chemist-products-enc", "", "", "", "", bmRng_);
    if (p != size_t(-1)) {
      const Table& t = tbl("a16_black-market-street-chemist-products-enc");
      const std::string product = trim(t.at(p, "Name"));
      const int lo = int(t.num(p, "Min Price")), hi = int(t.num(p, "Max Price"));
      const int price = lo + int(bmRng_.below(uint32_t(std::max(1, hi - lo + 1))));
      title = "Cutting the product";
      tableRef = t.name();
      rowRef = std::to_string(p + 1);
      say("I cut a batch of " + product + " and moved it for $" + std::to_string(price) + ".");
      const size_t c = pickRow("a16_black-market-street-chemist-cut-ingredients-enc", "Commonness Score",
                               "", "", "", bmRng_);
      if (c != size_t(-1)) {
        const Table& ci = tbl("a16_black-market-street-chemist-cut-ingredients-enc");
        say("The cut: " + resolve(trim(ci.at(c, "Ingredient"))) + ".");
        if (ci.has("Can Be Fatal") && ci.flag(c, "Can Be Fatal") && bmRng_.chance(0.25)) {
          tell("One of my customers ended up in hospital.");
          happiness -= 4;
        }
      }
      const size_t f = pickRow("a16_black-market-chemist-feedback-color-enc", "Commonness Score", "Hero Gender",
                               h.female ? "female" : "male", "", bmRng_);
      if (f != size_t(-1)) {
        const std::string fb =
            resolve(trim(tbl("a16_black-market-chemist-feedback-color-enc").at(f, "String")));
        if (!fb.empty()) say(spySentence(fb));
      }
      if (t.has("Happiness Impact")) happiness += int(t.num(p, "Happiness Impact"));
      if (t.has("Smarts Impact")) smarts += int(t.num(p, "Smarts Impact"));
      if (t.has("Looks Impact")) looks += int(t.num(p, "Looks Impact"));
    }
  } else if (bmRng_.chance(0.14)) {
    // A police stop: the warning is addressed to the hero, the second row is the
    // officer reading the hero's face.
    const size_t w = pickRow("a16_black-market-police-intoxicated-warning-enc", "Commonness Score",
                             "", "", "", bmRng_);
    const size_t hh = pickRow("a16_black-market-police-intoxicated-hero-color-enc", "", "Hero Gender",
                              h.female ? "female" : "male", "", bmRng_);
    if (w != size_t(-1) || hh != size_t(-1)) {
      title = "Stopped and searched";
      const bool savedSet = pronounOtherSet_, savedFemale = pronounOtherFemale_;
      pronounOtherSet_ = true;
      pronounOtherFemale_ = bmRng_.chance(0.5);
      if (w != size_t(-1)) {
        const Table& t = tbl("a16_black-market-police-intoxicated-warning-enc");
        say(spySentence(resolve(trim(t.at(w, "Scenario")))));
        tableRef = t.name();
        rowRef = std::to_string(w + 1);
      }
      if (hh != size_t(-1)) {
        const Table& t = tbl("a16_black-market-police-intoxicated-hero-color-enc");
        tell(spySentence(resolve(trim(t.at(hh, "Scenario")))));
        const std::string stat = t.has("Boosted Stat") ? trim(t.at(hh, "Boosted Stat")) : std::string();
        if (stat == "Happiness") happiness += 2;
        else if (stat == "Smarts") smarts += 2;
        else if (stat == "Looks") looks += 2;
      }
      pronounOtherSet_ = savedSet;
      pronounOtherFemale_ = savedFemale;
    }
  } else if (bmRng_.chance(0.14)) {
    // Taxidermy: a job, a place to leave what is left over, and the risk of the
    // location's own Risk Score catching up with the hero.
    const size_t d = pickRow("a16_black-market-taxidermy-discard-locations-enc", "Commonness Score",
                             "", "", "", bmRng_);
    if (d != size_t(-1)) {
      const Table& t = tbl("a16_black-market-taxidermy-discard-locations-enc");
      title = "Work for the taxidermist";
      const std::string biz = bmToken("taxidermist");
      say("I did a job for " + (biz.empty() ? std::string("the taxidermist") : biz) +
          " and dumped the leftovers " + resolve(trim(t.at(d, "String"))) + ".");
      tableRef = t.name();
      rowRef = std::to_string(d + 1);
      if (t.num(d, "Risk Score") >= 4 && bmRng_.chance(0.25)) {
        tell("A dog walker found it and called it in.");
        happiness -= 3;
      }
    }
  } else if (bmRng_.chance(0.20)) {
    // The museum report is gated on the collection's appeal, which the items the
    // hero keeps moving through raise.
    const size_t m = window("a16_black-market-museum-report-color-enc", "", "", "", "Min Appeal",
                            "Max Appeal", h.bmAppeal);
    if (m != size_t(-1)) {
      const Table& t = tbl("a16_black-market-museum-report-color-enc");
      title = h.bmMuseum;
      say(spySentence(resolve(trim(t.at(m, "String")))));
      tableRef = t.name();
      rowRef = std::to_string(m + 1);
      h.bmAppeal = std::max(0, std::min(100, h.bmAppeal + (h.bmItems > 0 ? 2 : -3)));
      happiness += h.bmAppeal > 50 ? 2 : -1;
    }
  } else {
    // Buy low, sell at auction: the item, the auctioneer's patter, a buyer with
    // a reason, and the sale -- unless the buyer's appraiser looks twice.
    const std::string item = bmDrawItem(life);
    if (!item.empty()) {
      const int price = int(double(h.bmValue) * 0.55);
      h.bmItems++;
      title = h.bmFake ? "A risky buy" : "A new find";
      tableRef = "a16_black-market-antiques-enc";
      rowRef = "1";
      say("I picked up " + item + " for $" + std::to_string(price) + ".");
      if (ensureTable("a16_black-market-auctioneer-color-enc")) {
        const Table& t = tbl("a16_black-market-auctioneer-color-enc");
        std::vector<size_t> pool;
        for (size_t r = 0; r < t.size(); ++r) {
          const std::string cat = t.at(r, "Auction Category");
          if (!trim(cat).empty() && !listHas(cat, h.bmCategory) && !listHas(cat, h.bmCategory + "s"))
            continue;
          const bool plural = t.flag(r, "Plural");
          if (plural != h.bmItemPlural) continue;
          pool.push_back(r);
        }
        if (pool.empty())
          for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
        if (!pool.empty()) {
          const size_t r = pool[bmRng_.below(uint32_t(pool.size()))];
          say("The auctioneer, " + bmToken("auctioneer") + ": \"" + resolve(trim(t.at(r, "String"))) + "\".");
          tableRef = t.name();
          rowRef = std::to_string(r + 1);
        }
      }
      if (ensureTable("a16_black-market-purchase-requests-enc")) {
        const Table& t = tbl("a16_black-market-purchase-requests-enc");
        std::vector<size_t> pool;
        for (size_t r = 0; r < t.size(); ++r) {
          const std::string cat = t.at(r, "Auction Category");
          if (trim(cat).empty()) continue;
          if (!listHas(cat, h.bmCategory) && !listHas(cat, h.bmCategory + "s")) continue;
          pool.push_back(r);
        }
        if (!pool.empty()) {
          const size_t r = pool[bmRng_.below(uint32_t(pool.size()))];
          const bool savedSet = pronounOtherSet_, savedFemale = pronounOtherFemale_;
          pronounOtherSet_ = true;
          pronounOtherFemale_ = bmRng_.chance(0.5);
          say("A buyer: " + resolve(trim(t.at(r, "String"))));
          pronounOtherSet_ = savedSet;
          pronounOtherFemale_ = savedFemale;
        }
      }
      if (h.bmFake && bmRng_.chance(0.30)) {
        tell("The buyer's appraiser called it a fake.");
        happiness -= 4;
      } else {
        tell("Sold for " + money(h.bmValue) + ".");
        happiness += 3;
      }
    }
  }

  if (title.empty() && body.empty() && line.empty()) return false;
  out = AgeUp();
  out.table = tableRef.empty() ? "a16_black-market-auctioneer-color-enc" : tableRef;
  out.row_id = rowRef;
  out.title = title.empty() ? "The black market" : title;
  out.body = body;
  out.narrative = line;
  out.extra = extra;
  out.happiness = happiness;
  out.smarts = smarts;
  out.looks = looks;
  out.discipline = discipline;
  out.karma = karma;
  return true;
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

// ---------------------------------------------------------------------------
// The casino (report 61).
//
// BitLife's casino is a business the hero owns rather than a menu the hero
// visits: a16_casino-casino-structure-enc sells the building with its amenity,
// game-room and entertainment capacities, a16_casino-casino-amenity-enc fills
// the amenity slots, a16_casino-casino-game-room-enc the game rooms, and the
// rest of the family is the life around them: the ledger, the gamblers, the
// acts and the celebrities.
// ---------------------------------------------------------------------------

void Sim::ensureCasinoFields(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  int amenityCap = 4, gameCap = 3;
  if (h.casinoTheme.empty() && ensureTable("a16_casino-casino-theme-enc")) {
    const Table& t = tbl("a16_casino-casino-theme-enc");
    const size_t r = weightedPick(t, "Popularity Score", casinoRng_);
    if (r != size_t(-1)) h.casinoTheme = trim(t.at(r, "Theme"));
  }
  if (h.casinoStructure.empty() && ensureTable("a16_casino-casino-structure-enc")) {
    const Table& t = tbl("a16_casino-casino-structure-enc");
    const size_t r = pickFrom([](size_t n) {
      std::vector<size_t> v;
      for (size_t i = 0; i < n; ++i) v.push_back(i);
      return v;
    }(t.size()), casinoRng_);
    if (r != size_t(-1)) {
      h.casinoStructure = trim(t.at(r, "Structure Type"));
      if (t.has("Amenity Capacity")) amenityCap = int(t.num(r, "Amenity Capacity"));
      if (t.has("Game Room Capacity")) gameCap = int(t.num(r, "Game Room Capacity"));
    }
    if (h.casinoStructure.empty()) h.casinoStructure = "casino";
  }
  if (h.casinoName.empty())
    h.casinoName = pickSnippet("a16_casino-random-hero-casino-name-enc", "Name",
                               "Commonness Score", "", "", "", casinoRng_);
  if (h.casinoName.empty()) h.casinoName = "The Lucky Ace";
  if (h.casinoStreet.empty() && ensureTable("a16_casino-casino-street-name-enc")) {
    const Table& t = tbl("a16_casino-casino-street-name-enc");
    const size_t r = pickRow("a16_casino-casino-street-name-enc", "Commonness Score", "", "", "", casinoRng_);
    if (r != size_t(-1)) {
      std::string street = trim(t.at(r, "Street Name"));
      if (t.flag(r, "Add Random Street Type")) street += " Street";
      if (!street.empty())
        h.casinoStreet = std::to_string(100 + int(casinoRng_.below(900))) + " " + street;
    }
  }
  if (h.casinoStreet.empty())
    h.casinoStreet = std::to_string(100 + int(casinoRng_.below(900))) + " Casino Boulevard";
  if (h.casinoAmenities.empty() && ensureTable("a16_casino-casino-amenity-enc")) {
    // One amenity per category, up to the structure's Amenity Capacity. The rows
    // that name a Theme only fit a house of that theme.
    const Table& t = tbl("a16_casino-casino-amenity-enc");
    std::vector<std::string> used;
    for (int slot = 0; slot < amenityCap; ++slot) {
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        const std::string cat = lower(trim(t.at(r, "Category")));
        bool dup = false;
        for (const std::string& u : used) if (u == cat) dup = true;
        if (dup) continue;
        const std::string theme = lower(trim(t.at(r, "Theme")));
        if (!theme.empty() && theme != lower(h.casinoTheme)) continue;
        pool.push_back(r);
      }
      if (pool.empty()) break;
      const size_t r = pool[casinoRng_.below(uint32_t(pool.size()))];
      const std::string name = resolve(trim(t.at(r, "Amenity Name")));
      if (name.empty()) break;
      h.casinoAmenities.push_back(name);
      used.push_back(lower(trim(t.at(r, "Category"))));
      if (t.has("Base Popularity Impact")) h.casinoPopularity += int(t.num(r, "Base Popularity Impact"));
    }
  }
  if (h.casinoGames.empty() && ensureTable("a16_casino-casino-game-room-enc")) {
    const Table& t = tbl("a16_casino-casino-game-room-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    for (int slot = 0; slot < gameCap && !pool.empty(); ++slot) {
      const size_t at = casinoRng_.below(uint32_t(pool.size()));
      const std::string g = trim(t.at(pool[at], "Game"));
      if (!g.empty()) h.casinoGames.push_back(g);
      pool.erase(pool.begin() + static_cast<long>(at));
    }
  }
  if (h.casinoPopularity < 1) h.casinoPopularity = 1;
  if (h.casinoPopularity > 100) h.casinoPopularity = 100;
}

void Sim::makeCasino(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.ownsCasino = true;
  ensureCasinoFields(life);
  h.casinoGamblers = 20 + int(casinoRng_.below(80));
}

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

// ---------------------------------------------------------------- report 62 ---
// The commune: the hero founds a cult, and the year is spent keeping the
// followers believing. Every table of the a16_cult family is drawn from here.
namespace {
// The four philosophies are the flag columns of the commune-name table.
const char* const kCultPhilosophyFlags[] = {"Armageddon", "New Age", "Antiestablishment", "Alien"};
const char* const kCultPhilosophyNames[] = {"armageddon", "new age", "antiestablishment", "alien"};

// A gender cell of this family is blank, "both", "male" or "female".
bool cultGenderFits(const std::string& cell, bool wantFemale) {
  const std::string g = lower(trim(cell));
  if (g.empty() || g == "both" || g == "all" || g == "any") return true;
  if (g == "male") return !wantFemale;
  if (g == "female") return wantFemale;
  return true;
}
}  // namespace

// The honorific the followers use for the hero, drawn from the rows that mark
// themselves as intended for the leader.
std::string Sim::cultHonorificPick() {
  const Table& t = tbl("a16_cult-honorific-name-structure-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < t.size(); ++r) {
    if (!t.flag(r, "Hero Is Leader")) continue;
    if (!cultGenderFits(t.at(r, "Gender"), hero_ ? hero_->female : false)) continue;
    pool.push_back(r);
  }
  if (pool.empty()) return std::string();
  const size_t pick = pool[cultRng_.below(uint32_t(pool.size()))];
  return resolve(trim(t.at(pick, "Honorific")));
}

void Sim::ensureCultFields(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;

  // The name table carries the philosophy in its flag columns, so one draw
  // gives the commune both its name and its beliefs.
  if (h.cultName.empty() || h.cultPhilosophy.empty()) {
    const size_t r = pickRow("a16_cult-random-commune-names-enc", "Commonness Score", "", "",
                             "Hero Founded Cult", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-random-commune-names-enc");
      h.cultName = resolve(trim(t.at(r, "Name")));
      h.cultPhilosophy = kCultPhilosophyNames[cultRng_.below(4)];
      for (int i = 0; i < 4; ++i) {
        if (t.flag(r, kCultPhilosophyFlags[i])) {
          h.cultPhilosophy = kCultPhilosophyNames[i];
          break;
        }
      }
    }
    if (h.cultName.empty()) h.cultName = "the commune";
    if (h.cultPhilosophy.empty()) h.cultPhilosophy = "new age";
  }

  if (h.cultTotem.empty()) {
    const std::string totem = pickSnippet("a16_cult-default-totem-enc", "Item Name", "Commonness Score",
                                          "Philosophy", h.cultPhilosophy, "", cultRng_);
    h.cultTotem = totem.empty() ? std::string("a smooth stone") : totem;
  }

  if (h.cultHonorific.empty()) {
    h.cultHonorific = cultHonorificPick();
    if (h.cultHonorific.empty()) h.cultHonorific = h.female ? "Mother" : "Father";
  }

  // The property the commune lives on, and the street it stands on.
  if (h.cultProperty.empty()) {
    const size_t pr = pickRow("a16_cult-commune-property-types-enc", "Commonness Score", "", "", "", cultRng_);
    if (pr != size_t(-1)) h.cultProperty = resolve(trim(tbl("a16_cult-commune-property-types-enc").at(pr, "Type")));
    if (h.cultProperty.empty()) h.cultProperty = "compound";
  }
  if (h.cultHome.empty()) {
    std::string street = pickSnippet("a16_cult-commune-street-names-enc", "Street Name", "Commonness Score",
                                     "Street Type", "Commune", "", cultRng_);
    if (street.empty())
      street = pickSnippet("a16_cult-commune-street-names-enc", "Street Name", "Commonness Score", "", "", "", cultRng_);
    if (street.empty()) street = "Communal Way";
    h.cultHome = std::to_string(100 + int(cultRng_.below(900))) + " " + street;
  }
}

void Sim::makeCult(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.cultLeader = true;
  ensureCultFields(life);
  h.communards = 3 + int(cultRng_.below(5));
  h.cultLoyalty = 55;
  h.cultCharisma = 45 + int(cultRng_.below(21));
}

std::string Sim::cultToken(const std::string& key) {
  std::string base = key;
  const std::string psuffix = "-possessive";
  const bool poss = base.size() > psuffix.size() &&
                    base.compare(base.size() - psuffix.size(), psuffix.size(), psuffix) == 0;
  if (poss) base.resize(base.size() - psuffix.size());
  if (!hero_) return std::string();
  const Hero& h = *hero_;
  if (h.cultName.empty() || h.cultTotem.empty())
    ensureCultFields(*const_cast<Life*>(life_));
  if (base == "communard") {
    // One follower per card, and the follower-facing pools address them.
    if (cultCommunard_.empty()) {
      cultCommunardFemale_ = cultRng_.chance(0.5);
      cultCommunard_ = drawFirstName(cultCommunardFemale_) + " " + drawSurname();
    }
    pronounOtherSet_ = true;
    pronounOtherFemale_ = cultCommunardFemale_;
    return poss ? cultCommunard_ + "'s" : cultCommunard_;
  }
  if (base == "ex-communard") {
    if (cultExCommunard_.empty())
      cultExCommunard_ = drawFirstName(cultRng_.chance(0.5)) + " " + drawSurname();
    return poss ? cultExCommunard_ + "'s" : cultExCommunard_;
  }
  if (base == "totem") return h.cultTotem;
  if (base == "honorific") return h.cultHonorific;
  if (base == "cult" || base == "cult-name") return h.cultName;
  if (base == "commune-other") {
    for (int i = 0; i < 6; ++i) {
      const std::string other = pickSnippet("a16_cult-random-commune-names-enc", "Name", "Commonness Score",
                                            "", "", "", cultRng_);
      if (!other.empty() && other != h.cultName) return other;
    }
    return "a rival commune";
  }
  return std::string();
}

bool Sim::cultYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.cultLeader || h.age < 18) return false;
  if (!cultRng_.chance(0.55)) return false;
  ensureCultFields(life);
  rndCache_.clear(); rbLast_.clear();
  cultCommunard_.clear();
  cultExCommunard_.clear();
  pronounOtherSet_ = false;
  std::string title, body, line, tableRef, rowRef;
  std::vector<std::string> extra;
  int happiness = 0;
  auto say = [&body](const std::string& s) {
    if (!s.empty()) body += body.empty() ? s : " " + s;
  };
  auto tell = [&line](const std::string& s) {
    if (!s.empty()) line += line.empty() ? s : " " + s;
  };
  auto mark = [&tableRef, &rowRef](const Table& t, size_t r) {
    tableRef = t.name();
    rowRef = std::to_string(r + 1);
  };
  auto cell = [this](const Table& t, size_t r, const char* col) -> std::string {
    if (!t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  auto bump = [&h](int d) {
    h.cultLoyalty += d;
    if (h.cultLoyalty < 0) h.cultLoyalty = 0;
    if (h.cultLoyalty > 100) h.cultLoyalty = 100;
  };
  // A row whose Philosophy cell is blank belongs to every commune; a row that
  // names one only fits that one. The filtered draw is tried first.
  auto cultPick = [&](const char* table, const char* weight, const char* phiCol) -> size_t {
    if (phiCol && *phiCol) {
      const size_t r0 = pickRow(table, weight, phiCol, h.cultPhilosophy, "", cultRng_);
      if (r0 != size_t(-1)) return r0;
    }
    return pickRow(table, weight, "", "", "", cultRng_);
  };
  // The followers' devotion drifts a little every year.
  bump(int(cultRng_.below(7)) - 3);

  // --- The commune shop: the followers sell what they make.
  if (title.empty() && cultRng_.chance(0.06)) {
    const size_t r = cultPick("a16_cult-workshop-products-enc", "Engagement Score", "Philosophy");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-workshop-products-enc");
      std::string product = cell(t, r, "Product Name Plural");
      if (product.empty()) product = cell(t, r, "Product Name");
      const std::string place = cell(t, r, "Optimal Location");
      const double cost = t.num(r, "Min Unit Cost");
      const double made = t.num(r, "Average Unit Price") * double(3 + cultRng_.below(10));
      title = "The commune shop";
      say("My followers sold " + (product.empty() ? std::string("plaques") : product) +
          (place.empty() ? std::string() : " at the " + place) + " and brought in " +
          money((long long)(made - cost)) + ".");
      mark(t, r);
      happiness += 1;
    }
  }

  // --- A fast: the reason first, then what the hunger felt like.
  if (title.empty() && cultRng_.chance(0.08)) {
    const size_t r = cultPick("a16_cult-fast-reasons-enc", "Engagement Score", "Philosophy");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-fast-reasons-enc");
      const std::string what = cell(t, r, "Scenario Snippet");
      const std::string why = cell(t, r, "Reason");
      title = "The fast";
      if (!what.empty()) say("I led the commune in fasting " + what + ".");
      const size_t f = pickRow("a16_cult-fast-hunger-foods-enc", "Commonness Score", "", "", "", cultRng_);
      if (f != size_t(-1)) {
        const Table& ft = tbl("a16_cult-fast-hunger-foods-enc");
        const std::string food = cell(ft, f, "Food");
        const std::string art = withArticle(food, ft.flag(f, "Uses Indefinite Article"));
        if (!food.empty()) say("By the end we were all dreaming about " + art + food + ".");
      }
      if (!why.empty()) tell("I called the fast " + why + ".");
      mark(t, r);
      bump(2);
    }
  }

  // --- A film about the commune, or a drum circle with a name act.
  if (title.empty() && cultRng_.chance(0.08)) {
    if (cultRng_.chance(0.5)) {
      const size_t r = cultPick("a16_cult-affirmation-film-titles-enc", "Engagement Score", "Philosophy");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-affirmation-film-titles-enc");
        const std::string film = cell(t, r, "Title");
        const double eng = t.num(r, "Engagement Score");
        title = "A film about the commune";
        say("Film crews followed us for a year and released " + (film.empty() ? std::string("a documentary") : film) + ".");
        const size_t rec = pickWindow("a16_cult-affirmation-receptions-enc", "", "", "", "Min Reception",
                                      "Max Reception", eng * 20.0, cultRng_);
        if (rec != size_t(-1)) {
          const Table& rt = tbl("a16_cult-affirmation-receptions-enc");
          const std::string s = cell(rt, rec, "String");
          if (!s.empty()) say(s);
          mark(rt, rec);
          bump(int(eng) - 1);
          happiness += 1;
        }
        if (tableRef.empty()) mark(t, r);
      }
    } else {
      const size_t ar = pickRow("a16_cult-drum-circle-artists-enc", "Engagement Score", "", "", "", cultRng_);
      if (ar != size_t(-1)) {
        const Table& at = tbl("a16_cult-drum-circle-artists-enc");
        const std::string artist = cell(at, ar, "Artist Name");
        if (!artist.empty()) {
          title = "A drum circle";
          say("We held a drum circle, and " + artist + " played until the fire went out.");
          if (lower(trim(at.at(ar, "Philosophy Engagement Bonus"))) == h.cultPhilosophy) bump(4);
          else bump(1);
          mark(at, ar);
          happiness += 2;
        }
      }
    }
  }

  // --- Someone who left: the ones who shout at the gate, and the locals who nag.
  if (title.empty() && cultRng_.chance(0.08)) {
    if (cultRng_.chance(0.5)) {
      const size_t r = pickWindow("a16_cult-ex-communard-dissent-scenario-enc", "", "", "", "Min Craziness",
                                  "Max Craziness", double(cultRng_.below(101)), cultRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-ex-communard-dissent-scenario-enc");
        const std::string scen = cell(t, r, "Scenario");
        if (!scen.empty()) {
          title = "A former follower";
          say(scen);
          const std::string l = cell(t, r, "Log Version");
          if (!l.empty()) tell(l);
          mark(t, r);
          bump(-3);
          happiness -= 1;
        }
      }
    } else {
      const size_t r = pickWindow("a16_cult-anti-commune-nag-scenario-enc", "", "", "", "Audience Min Age",
                                  "Audience Max Age", double(h.age), cultRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-anti-commune-nag-scenario-enc");
        const std::string who = lower(trim(t.at(r, "Audience")));
        const std::string s = cell(t, r, "Snippet");
        if (!s.empty()) {
          title = "The neighbours";
          pronounOtherSet_ = who.find("male") != std::string::npos && who.find("female") == std::string::npos;
          pronounOtherFemale_ = who.find("female") != std::string::npos;
          say(spyUpperFirst(s));
          const std::string l = cell(t, r, "Log Version");
          if (!l.empty()) tell(l);
          mark(t, r);
          bump(-1);
        }
      }
    }
  }

  // --- A trip: together on the commune's own supply, or alone with it.
  if (title.empty() && cultRng_.chance(0.10)) {
    const bool shared = cultRng_.chance(0.5);
    const char* tt = shared ? "a16_cult-drug-shared-experience-enc" : "a16_cult-drug-solo-experience-enc";
    const size_t r = cultPick(tt, "Commonness Score", "Philosophy");
    if (r != size_t(-1)) {
      const Table& t = tbl(tt);
      const std::string snip = cell(t, r, "String Snippet");
      const std::string logv = cell(t, r, "Log Version");
      std::string drug = cell(t, r, "Drug");
      std::string drugNote;
      if (drug.empty()) {
        const size_t dr = pickRow("a16_cult-spiritual-journey-drugs-enc", "", "", "", "", cultRng_);
        if (dr != size_t(-1)) {
          const Table& dt = tbl("a16_cult-spiritual-journey-drugs-enc");
          drug = cell(dt, dr, "Name");
          if (!drug.empty()) {
            const double risk = cultRng_.below(4) == 0 ? dt.num(dr, "Double Dose Risk Score")
                               : cultRng_.below(2) == 0 ? dt.num(dr, "Dose Risk Score")
                                                        : dt.num(dr, "Microdose Risk Score");
            drugNote = "We paid " + money((long long)dt.num(dr, "Min Price Per Dose")) + " a dose for it.";
            if (risk >= 4 && cultRng_.chance(0.35)) {
              extra.push_back("The dose went wrong: a follower was hospitalised.");
              bump(-2);
            }
          }
        }
      }
      if (!snip.empty() || !logv.empty()) {
        title = shared ? "A shared trip" : "A solo trip";
        // Shared snippets are past-tense clauses, solo snippets gerunds.
        if (!snip.empty()) say(shared ? ("My followers and I " + spySentence(spyLowerFirst(snip)))
                                      : ("I remember " + spySentence(spyLowerFirst(snip))));
        else say(logv);
        if (!drug.empty()) say("The commune supply: " + drug + ".");
        if (!drugNote.empty()) extra.push_back(drugNote);
        mark(t, r);
        if (lower(trim(t.has("Experience Type") ? t.at(r, "Experience Type") : std::string())) == "positive") bump(2);
        else bump(-2);
        happiness += 1;
      }
    }
  }

  // --- The totem: a lecture about it, a sign from it, or an insult to it.
  if (title.empty() && cultRng_.chance(0.10)) {
    const int mode = int(cultRng_.below(3));
    if (mode == 0) {
      const size_t r = pickRow("a16_cult-totem-lecture-enc", "Commonness Score", "", "", "", cultRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-totem-lecture-enc");
        const std::string s = cell(t, r, "String");
        if (!s.empty()) {
          title = "The totem";
          say("I lectured the commune " + s + ".");
          mark(t, r);
          bump(2);
        }
      }
    } else if (mode == 1) {
      const size_t r = pickRow("a16_cult-totem-inspiration-enc", "Commonness Score", "", "", "", cultRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-totem-inspiration-enc");
        const std::string s = cell(t, r, "String");
        if (!s.empty()) {
          title = "The totem speaks";
          say("I sat with " + h.cultTotem + " and " + s + ".");
          if (t.flag(r, "Humorous")) happiness += 2;
          mark(t, r);
          bump(1);
        }
      }
    } else {
      const size_t r = pickWindow("a16_cult-totem-disrespect-enc", "", "", "", "Min Loyalty", "Max Loyalty",
                                  double(h.cultLoyalty), cultRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-totem-disrespect-enc");
        const std::string act = cell(t, r, "String");
        const std::string l = cell(t, r, "Log Version");
        if (!act.empty() || !l.empty()) {
          title = "Disrespecting the totem";
          if (!l.empty()) tell(l);
          else say("I caught a follower " + act + " " + h.cultTotem + ".");
          extra.push_back("The rule was enforced: the follower was punished in front of the commune.");
          mark(t, r);
          bump(-1);
        }
      }
    }
  }

  // --- A ceremony: a healing ritual, and the hands laid on a follower.
  if (title.empty() && cultRng_.chance(0.10)) {
    const size_t r = pickRow("a16_cult-healing-ceremony-enc", "Engagement Score", "", "", "", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-healing-ceremony-enc");
      const std::string kind = cell(t, r, "Ceremony Type");
      const std::string fb = cell(t, r, "Feedback Snippet");
      title = "A ceremony";
      if (!kind.empty()) say("I called a " + kind + " ceremony.");
      if (!fb.empty()) say("A follower came forward and " + fb + ".");
      const std::string power = pickSnippet("a16_cult-empowerments-enc", "String", "Commonness Score", "", "", "", cultRng_);
      if (!power.empty()) say(spySentence(spySwap(spySwap(spySwap(spySwap(power, "You ", "I "), " your ", " my "), "your ", "my "), "you ", "I ")));
      const size_t gr = pickWindow("a16_cult-gratitude-scenarios-enc", "", "", "", "Min Loyalty", "Max Loyalty",
                                   double(h.cultLoyalty), cultRng_);
      if (gr != size_t(-1)) {
        const Table& gt = tbl("a16_cult-gratitude-scenarios-enc");
        const std::string s = cell(gt, gr, "String");
        if (!s.empty()) { say(s); mark(gt, gr); }
      }
      if (tableRef.empty()) mark(t, r);
      bump(int(t.num(r, "Effectiveness Score")) + 1);
      happiness += 2;
    }
  }

  // --- A rumour moving through the commune, in one direction or the other.
  if (title.empty() && cultRng_.chance(0.10)) {
    const bool inbound = cultRng_.chance(0.5);
    const size_t r = pickRow("a16_cult-communard-rumor-enc", "", "", "", "", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-communard-rumor-enc");
      const char* col = inbound ? "Inbound Scenario" : "Outbound Scenario";
      const std::string scen = cell(t, r, col);
      if (!scen.empty()) {
        title = inbound ? "A rumour reaches me" : "A rumour leaves the commune";
        say(scen);
        const std::string l = cell(t, r, "Inbound Scenario Log Version");
        if (inbound && !l.empty()) tell(l);
        mark(t, r);
        bump(inbound ? -2 : 2);
      }
    }
  }

  // --- Doubt, devotion, a warning, or a follower begging not to be expelled.
  if (title.empty() && cultRng_.chance(0.10)) {
    const int mode = int(cultRng_.below(3));
    const char* table = mode == 0 ? "a16_cult-communard-doubts-enc"
                       : mode == 1 ? "a16_cult-communard-apply-color-enc"
                                   : "a16_cult-communard-warning-enc";
    const size_t r = pickWindow(table, "", "", "", "Min Loyalty", "Max Loyalty", double(h.cultLoyalty), cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl(table);
      const std::string s = cell(t, r, mode == 2 ? "Scenario" : "String");
      if (!s.empty()) {
        title = mode == 2 ? "A warning" : "A follower speaks";
        say(spyUpperFirst(s));
        mark(t, r);
        bump(mode == 1 ? 2 : -1);
        if (mode == 2 && t.has("Risk Score") && t.num(r, "Risk Score") >= 4) {
          const size_t br = pickWindow("a16_cult-expel-beg-scenario-enc", "", "", "", "Min Loyalty", "Max Loyalty",
                                       double(h.cultLoyalty), cultRng_);
          if (br != size_t(-1)) {
            const Table& bt = tbl("a16_cult-expel-beg-scenario-enc");
            const std::string beg = cell(bt, br, "String");
            if (!beg.empty()) extra.push_back(beg);
            if (h.communards > 0) --h.communards;
          }
        }
      }
    }
  }

  // --- A building project the commune has not taken on yet.
  if (title.empty() && cultRng_.chance(0.10)) {
    const size_t r = pickRow("a16_cult-commune-structure-enc", "", "", "", "", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-commune-structure-enc");
      const std::string project = cell(t, r, "Project");
      bool owned = false;
      for (size_t i = 0; i < h.cultStructures.size(); ++i)
        if (lower(trim(h.cultStructures[i])) == lower(project)) owned = true;
      if (!project.empty() && !owned) {
        h.cultStructures.push_back(project);
        title = "A new building";
        say("The commune broke ground on " + articleFor(project) + project + ".");
        const std::string sub = cell(t, r, "Secondary Line Text");
        if (!sub.empty()) say(spySentence(sub));
        tell("It set the commune back " + money((long long)t.num(r, "Minimum Cost")) + ".");
        double appeal = t.num(r, "Appeal Impact");
        if (lower(trim(t.at(r, "Bonus Appeal Impact (Philosophy)"))) == h.cultPhilosophy) appeal += 3;
        bump(int(appeal));
        const size_t fr = pickRow("a16_cult-commune-structure-feedback-enc", "Commonness Score", "Structure",
                                  lower(project), "", cultRng_);
        if (fr != size_t(-1)) {
          const Table& ft = tbl("a16_cult-commune-structure-feedback-enc");
          const std::string s = cell(ft, fr, "Scenario");
          if (!s.empty()) { say(spySentence(s)); mark(ft, fr); }
          const std::string l = cell(ft, fr, "Log Version");
          if (!l.empty()) extra.push_back(l);
        }
        if (tableRef.empty()) mark(t, r);
        happiness += 2;
      }
    }
  }

  // --- A demonstration of devotion, and what it cost.
  if (title.empty() && cultRng_.chance(0.12)) {
    const size_t r = cultPick("a16_cult-devotion-demonstration-enc", "Commonness Score", "Philosophy");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-devotion-demonstration-enc");
      const std::string act = cell(t, r, "Scenario Snippet");
      const double risk = t.num(r, "Risk Score");
      if (double(cultRng_.below(100)) < risk * 10.0) {
        const std::string d = cell(t, r, "Death Snippet");
        title = "A demonstration of devotion";
        say("A follower died " + (d.empty() ? std::string("trying to prove their devotion") : d) + ".");
        const size_t bl = pickRow("a16_cult-body-discard-locations-enc", "Commonness Score", "", "", "", cultRng_);
        if (bl != size_t(-1)) {
          const Table& bt = tbl("a16_cult-body-discard-locations-enc");
          const std::string where = cell(bt, bl, "String");
          if (!where.empty()) extra.push_back("The body was left " + where + ".");
          bump(-2);
        }
        if (h.communards > 0) --h.communards;
      } else {
        const std::string s = cell(t, r, "Success Snippet");
        title = "A demonstration of devotion";
        // The Scenario Snippet is a base-form verb phrase and the Success
        // Snippet a gerund or noun phrase, so they cannot share one frame.
        say("A follower volunteered to " + spyLowerFirst(act) + " in my honour.");
        if (!s.empty()) tell("The outcome: " + spySentence(s));
        bump(4);
        happiness += 2;
      }
      mark(t, r);
    }
  }

  // --- Rewarding a follower for something small.
  if (title.empty() && cultRng_.chance(0.12)) {
    const size_t r = pickRow("a16_cult-communard-rewards-enc", "", "", "", "", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-communard-rewards-enc");
      const std::string what = cell(t, r, "Scenario");
      if (!what.empty()) {
        title = "Rewarding a follower";
        const std::string who = cultToken("communard");
        say("I rewarded " + who + " with " + what + ".");
        tell("I gave " + who + " " + what + ".");
        bump(int(t.num(r, "Loyalty Impact")));
        mark(t, r);
      }
    }
  }

  // --- The rules of the commune: one is stated, one is broken, one is punished.
  if (title.empty() && cultRng_.chance(0.15)) {
    const size_t r = pickRow("a16_cult-doctrine-rules-enc", "Commonness Score", "", "", "", cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-doctrine-rules-enc");
      const std::string rule = cell(t, r, "List String");
      if (!rule.empty()) h.cultDoctrine = rule;
      const std::string broken = cell(t, r, "Scenario Snippet");
      title = "A rule of the commune";
      if (!rule.empty()) say("The rule in force: " + rule + ".");
      const std::string who = cultToken("communard");
      if (!broken.empty()) say(who + " was caught " + broken + ".");
      const std::string logv = cell(t, r, "Log Version");
      if (!logv.empty()) tell("I caught " + who + " when they " + spyLowerFirst(logv) + ".");
      if (t.flag(r, "Disobedience Can Be Fatal") && cultRng_.chance(0.25)) {
        const std::string d = cell(t, r, "Disobedient Death Snippet");
        extra.push_back(who + " did not survive the lesson" + (d.empty() ? std::string(".") : ": " + d + "."));
        if (h.communards > 0) --h.communards;
        bump(-6);
      } else if (cultRng_.chance(0.5)) {
        const size_t pr = pickRow("a16_cult-communard-punishment-enc", "Commonness Score", "", "", "", cultRng_);
        if (pr != size_t(-1)) {
          const Table& pt = tbl("a16_cult-communard-punishment-enc");
          const std::string fb = cell(pt, pr, "Feedback Scenario");
          if (!fb.empty()) say(fb);
          const std::string pl = cell(pt, pr, "Log Version");
          if (!pl.empty()) extra.push_back(pl);
          bump(int(pt.num(pr, "Loyalty Impact")));
          mark(pt, pr);
        }
      } else {
        const std::string ob = cell(t, r, "Obedience Snippet");
        if (!ob.empty()) extra.push_back("The rest obeyed: " + ob + ".");
        bump(int(t.num(r, "Appeal Score")));
      }
      pronounOtherSet_ = false;
      if (tableRef.empty()) mark(t, r);
    }
  }

  // --- Time with the followers, and the things they want to talk about.
  if (title.empty() && cultRng_.chance(0.15)) {
    if (cultRng_.chance(0.5)) {
      const size_t r = cultPick("a16_cult-communard-spend-time-enc", "", "Philosophy");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-communard-spend-time-enc");
        const std::string pastime = cell(t, r, "Pastime");
        if (!pastime.empty()) {
          title = "Time with my followers";
          say("I took my followers " + pastime + ".");
          bump(2);
          mark(t, r);
        }
      }
    } else {
      const size_t r = cultPick("a16_cult-communard-conversation-enc", "", "Philosophy");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_cult-communard-conversation-enc");
        const std::string conv = cell(t, r, "Conversation");
        if (!conv.empty()) {
          title = "A conversation";
          say(cultToken("communard") + " wanted to talk about " + conv + ".");
          if (t.num(r, "Controversy Score") >= 3) {
            h.cultCharisma += 3;
            bump(1);
          } else {
            bump(2);
          }
          mark(t, r);
        }
      }
    }
  }

  // --- A lesson, and the followers who have heard it too many times.
  if (title.empty() && cultRng_.chance(0.18)) {
    const size_t r = cultPick("a16_cult-commune-teaching-enc", "Engagement Score", "Philosophy");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-commune-teaching-enc");
      const std::string scen = cell(t, r, "Scenario");
      if (!scen.empty()) {
        title = "A lesson";
        say("I taught my followers " + scen + ".");
        const size_t er = cultPick("a16_cult-communard-educate-enc", "", "Philosophy");
        if (er != size_t(-1)) {
          const Table& et = tbl("a16_cult-communard-educate-enc");
          const std::string topic = cell(et, er, "Topic");
          if (!topic.empty()) extra.push_back("The lesson: " + spySentence(topic));
          bump(int(et.num(er, "Loyalty Impact")));
          mark(et, er);
        }
        if (cultRng_.chance(0.30)) {
          const size_t lr = pickRow("a16_cult-communard-leave-scenario-enc", "", "Reason", "educate", "", cultRng_);
          if (lr != size_t(-1)) {
            const Table& lt = tbl("a16_cult-communard-leave-scenario-enc");
            const std::string s = cell(lt, lr, "Scenario Snippet");
            if (!s.empty()) {
              say(cultToken("communard") + " is losing patience: " + s);
              bump(-3);
            }
          }
        }
        if (tableRef.empty()) mark(t, r);
      }
    }
  }

  // --- Recruitment: the pitch, the place, and the odds of the police noticing.
  if (title.empty() && cultRng_.chance(0.20)) {
    const size_t lr = pickRow("a16_cult-recruitment-location-enc", "Commonness Score", "", "", "", cultRng_);
    const size_t mr = cultPick("a16_cult-recruitment-message-enc", "Success Score", "Philosophy");
    if (lr != size_t(-1) && mr != size_t(-1)) {
      const Table& lt = tbl("a16_cult-recruitment-location-enc");
      const Table& mt = tbl("a16_cult-recruitment-message-enc");
      const std::string place = cell(lt, lr, "Location");
      const std::string msg = cell(mt, mr, "Message");
      const double success = mt.num(mr, "Success Score");
      const double police = lt.num(lr, "Police Score");
      title = "Recruiting";
      if (!place.empty())
        say("I stood outside the " + place + " and told anyone who would listen: " + msg + ".");
      if (double(cultRng_.below(100)) < success * 12.0 - police * 4.0 + h.cultCharisma * 0.3) {
        ++h.communards;
        say("One of them followed me home.");
        bump(2);
        mark(mt, mr);
      } else {
        // One heckler, one card: the String column carries the bracket the log
        // line points at with '<rb>', so the String is resolved first.
        const size_t hr = pickRow("a16_cult-outreach-mission-heckles-enc", "Commonness Score",
                                  "", "", "", cultRng_);
        if (hr != size_t(-1)) {
          const Table& ht = tbl("a16_cult-outreach-mission-heckles-enc");
          const std::string heckle = cell(ht, hr, "String");
          if (!heckle.empty()) say(spySentence(heckle));
          const std::string hl = cell(ht, hr, "Log Version");
          if (!hl.empty()) tell(hl);
        }
        bump(-1);
        if (tableRef.empty()) mark(lt, lr);
      }
    }
  }

  // --- The commune's own prompt card: gated on age, philosophy, loyalty and
  //     charisma, and answered with its first choice.
  if (title.empty() && cultRng_.chance(0.25)) {
    const Table& t = tbl("a16_cult-commune-dumb-scenarios-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      const double lo = t.num(r, "Hero Min Age");
      const double hi = t.num(r, "Hero Max Age");
      if (lo > 0 && h.age < lo) continue;
      if (hi > 0 && h.age > hi) continue;
      if (!h.female && !cultGenderFits(t.at(r, "Hero Gender"), true)) continue;
      if (h.female && !cultGenderFits(t.at(r, "Hero Gender"), false)) continue;
      if (!h.loverName.empty() && !cultGenderFits(t.at(r, "Lover Gender"), h.loverFemale)) continue;
      if (h.loverName.empty() && t.flag(r, "Hero Has Lover")) continue;
      if (t.flag(r, "Requires Communard") && h.communards <= 0) continue;
      const std::string phil = lower(trim(t.at(r, "Commune Philosophy")));
      if (!phil.empty() && phil != h.cultPhilosophy) continue;
      const double ll = t.num(r, "Min Loyalty");
      const double hl = t.num(r, "Max Loyalty");
      if (ll > 0 && h.cultLoyalty < ll) continue;
      if (hl > 0 && h.cultLoyalty > hl) continue;
      const double lc = t.num(r, "Min Charisma");
      const double hc = t.num(r, "Max Charisma");
      if (lc > 0 && h.cultCharisma < lc) continue;
      if (hc > 0 && h.cultCharisma > hc) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[cultRng_.below(uint32_t(pool.size()))];
      title = cell(t, r, "Title");
      say(cell(t, r, "Body"));
      const std::string cta = cell(t, r, "CTA");
      if (!cta.empty()) extra.push_back(cta);
      const std::string choice = cell(t, r, "Choice 1");
      if (!choice.empty()) tell("I said: " + choice + ".");
      const std::string narr = cell(t, r, "Narrative 1");
      if (!narr.empty()) say(spySentence(narr));
      bump(int(t.num(r, "Loyalty Impact 1")));
      h.cultCharisma += int(t.num(r, "Charisma Impact 1"));
      mark(t, r);
    }
  }

  // --- The ledger: what the followers make of the year.
  if (title.empty() && cultRng_.chance(0.30)) {
    const size_t r = pickWindow("a16_cult-commune-ledger-color-enc", "", "", "", "Min Loyalty", "Max Loyalty",
                                double(h.cultLoyalty), cultRng_);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_cult-commune-ledger-color-enc");
      const std::string s = cell(t, r, "String");
      if (!s.empty()) {
        title = "The commune ledger";
        say(s);
        mark(t, r);
      }
    }
  }

  if (h.cultCharisma < 0) h.cultCharisma = 0;
  if (h.cultCharisma > 100) h.cultCharisma = 100;
  if (title.empty() && body.empty() && line.empty()) return false;
  out = AgeUp();
  out.table = tableRef.empty() ? "a16_cult-commune-ledger-color-enc" : tableRef;
  out.row_id = rowRef;
  out.title = title.empty() ? "The commune" : title;
  out.body = body;
  out.narrative = line;
  out.extra = extra;
  out.happiness = happiness;
  return true;
}

// ---------------------------------------------------------------------------
// Section 63: the drug trade.
//
// The family ships 37 tables and no career model of its own. The hero either
// moves small batches on the street or moves kilos for a cartel; both are the
// same loop, so the port keeps one item slot (dealerDrug + dealerPackage) and
// two Hero ints (dealerDemand, dealerStanding) and lets a sale in year 40 name
// the batch that was bought in year 19.

std::string Sim::dealerToken(const std::string& key) {
  Hero& h = *const_cast<Hero*>(hero_);
  std::string k = key;
  const std::string psuffix = "-possessive";
  bool poss = false;
  if (k.size() > psuffix.size() && k.compare(k.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    poss = true;
    k = k.substr(0, k.size() - psuffix.size());
  } else if (k == "victim-possesive") {          // the data ships this spelling
    poss = true;
    k = "victim";
  }
  const auto own = [&](const std::string& name) { return poss ? (name + "'s") : name; };
  const auto drawPerson = [&](bool female) { return drawFirstName(female) + " " + drawSurname(); };
  const auto speakAs = [&](bool female) {
    pronounOtherSet_ = true;
    pronounOtherFemale_ = female;
  };
  const auto cached = [&](const std::string& slot, bool female) -> std::string {
    std::map<std::string, std::string>::iterator it = dealerCache_.find(slot);
    if (it != dealerCache_.end()) return it->second;
    const std::string name = drawPerson(female);
    dealerCache_[slot] = name;
    return name;
  };

  if (k == "cartel-member" || k == "member" || k == "partner") {
    if (dealerMember_.empty()) {
      dealerMemberFemale_ = dealerRng_.chance(0.5);
      dealerMember_ = drawPerson(dealerMemberFemale_);
    }
    speakAs(dealerMemberFemale_);
    return own(dealerMember_);
  }
  if (k == "cartel-rival-member" || k == "rival-member" || k == "rival-cartel-member") {
    const bool female = dealerRng_.chance(0.5);
    speakAs(female);
    return own(cached("rival-member", female));
  }
  if (k == "cartel-kingpin" || k == "kingpin") {
    if (h.dealerKingpin) return own(h.firstName + " " + h.lastName);
    speakAs(false);
    return own(cached("kingpin", false));
  }
  if (k == "victim") {
    if (dealerVictim_.empty()) dealerVictim_ = drawPerson(dealerRng_.chance(0.5));
    return own(dealerVictim_);
  }
  if (k == "other-person" || k == "random-person") return own(cached("other", dealerRng_.chance(0.5)));
  if (k == "cartel-remember") {
    if (!h.dealerCartel.empty()) return own(h.dealerCartel);
    return own(cached("cartel", false));
  }
  if (k == "insult") {
    const Table& t = tbl("a16_insults-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Gender")) {
        const std::string g = lower(trim(t.at(r, "Gender")));
        if (!g.empty() && g != "both" && g != "any" && g != "all" &&
            g != (h.female ? "female" : "male")) continue;
      }
      pool.push_back(r);
    }
    const size_t r = pool.empty() ? size_t(-1) : pool[dealerRng_.below(uint32_t(pool.size()))];
    return r == size_t(-1) ? std::string("a fool") : trim(t.at(r, "Insult"));
  }
  if (k == "random-first-name-male") return drawFirstName(false);
  if (k == "random-first-name-female") return drawFirstName(true);
  if (k == "drug-illegal" || k == "dealer-drug") {
    if (!h.dealerDrug.empty()) return h.dealerDrug;
    const Table& t = tbl("a16_dealer-cartel-drug-enc");
    const size_t r = pickRow("a16_dealer-cartel-drug-enc", "", "", "", "", dealerRng_);
    return r == size_t(-1) ? std::string("yayo") : trim(t.at(r, "Drug"));
  }
  if (k == "dealer-package") return h.dealerPackage.empty() ? std::string("bag") : h.dealerPackage;
  if (k == "cartel" || k == "drug-cartel") {
    if (!h.dealerCartel.empty()) return own(h.dealerCartel);
    return own(cached("cartel", false));
  }
  if (k == "rival-cartel") return own(cached("rival-cartel", false));
  return std::string();
}

void Sim::ensureDealerFields(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (h.dealerCartel.empty()) {
    const size_t c = pickRow("a16_dealer-cartel-name-enc", "Commonness Score", "", "", "", dealerRng_);
    if (c != size_t(-1)) {
      const Table& t = tbl("a16_dealer-cartel-name-enc");
      h.dealerCartel = resolve(trim(t.at(c, "Name")));
    }
    if (h.dealerCartel.empty()) h.dealerCartel = drawSurname() + " Crew";
  }
  if (h.dealerNick.empty()) {
    const Table& t = tbl("a16_dealer-cartel-member-nickname-enc");
    std::vector<size_t> pool;
    double total = 0.0;
    std::vector<double> weights;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!h.dealerKingpin && t.has("Kingpin Only") && t.flag(r, "Kingpin Only")) continue;
      if (t.has("Gender")) {
        const std::string g = lower(trim(t.at(r, "Gender")));
        if (!g.empty() && g != "both" && g != "any" && g != "all" &&
            g != (h.female ? "female" : "male")) continue;
      }
      const double w = t.has("Commonness Score") ? t.num(r, "Commonness Score") : 1.0;
      pool.push_back(r);
      weights.push_back(w > 0 ? w : 1.0);
      total += weights.back();
    }
    if (!pool.empty()) {
      double roll = dealerRng_.unit() * total;
      size_t pick = pool.size() - 1;
      for (size_t i = 0; i < pool.size(); ++i) {
        roll -= weights[i];
        if (roll <= 0) { pick = i; break; }
      }
      h.dealerNick = resolve(trim(t.at(pool[pick], "Nickname")));
    }
  }
  if (h.dealerStreet.empty()) {
    const size_t s = pickRow("a16_dealer-cartel-street-name-enc", "Commonness Score", "", "", "", dealerRng_);
    if (s != size_t(-1)) {
      const Table& t = tbl("a16_dealer-cartel-street-name-enc");
      std::string street = trim(t.at(s, "Street Name"));
      if (t.has("Add Random Street Type") && t.flag(s, "Add Random Street Type")) street += " Street";
      char buf[32];
      snprintf(buf, sizeof(buf), "%d ", 100 + int(dealerRng_.below(900)));
      h.dealerStreet = std::string(buf) + street;
    }
  }
  if (h.dealerDrug.empty()) {
    const size_t d = pickRow("a16_dealer-cartel-drug-enc", "", "", "", "", dealerRng_);
    if (d != size_t(-1)) {
      const Table& t = tbl("a16_dealer-cartel-drug-enc");
      h.dealerDrug = trim(t.at(d, "Drug"));
      const double lo = t.has("Min Price Per Dose") ? t.num(d, "Min Price Per Dose") : 20.0;
      const double hi = t.has("Max Price Per Dose") ? t.num(d, "Max Price Per Dose") : lo + 40.0;
      h.dealerPrice = int(lo + dealerRng_.unit() * (hi - lo));
      const double demand = t.has("Base Demand") ? t.num(d, "Base Demand") : 3.0;
      h.dealerDemand = int(30.0 + demand * 8.0);
    }
  }
  if (h.dealerDrug.empty()) h.dealerDrug = "yayo";
  if (h.dealerPrice <= 0) h.dealerPrice = 40;
  if (h.dealerPackage.empty()) {
    const size_t p = pickRow("a16_dealer-drug-package-type-enc", "Commonness Score", "", "", "", dealerRng_);
    if (p != size_t(-1)) h.dealerPackage = lower(resolve(trim(tbl("a16_dealer-drug-package-type-enc").at(p, "Packaging Type"))));
  }
  if (h.dealerPackage.empty()) h.dealerPackage = "bag";
}

void Sim::makeDealer(Life& life) {
  Hero& h = life.hero;
  h.dealer = true;
  ensureDealerFields(life);
  if (h.dealerDemand <= 0) h.dealerDemand = 50;
  if (h.dealerStanding <= 0) h.dealerStanding = 50;
  h.dealerWillpower = 45 + int(dealerRng_.below(31));
  h.dealerBatch = 20 + int(dealerRng_.below(81));
  h.money += 500;
}
// ---------------------------------------------------------------------------
// The trade's year. Blocks are ordered so the core money loop lands first and
// the flavour lines fill whatever is left, the same way the other subsystems
// order their yearly tables.

bool Sim::dealerYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  (void)forcedChoice;
  if (!h.dealer || h.age < 16) return false;
  if (!dealerRng_.chance(0.50)) return false;
  ensureDealerFields(life);

  std::string tableName, title, body, narr;
  size_t tableRow = size_t(-1);
  std::vector<std::string> extra;
  int happy = 0;

  const auto tell = [&](const std::string& s) { if (!trim(s).empty()) extra.push_back(spySentence(s)); };
  const auto say = [&](const std::string& s) {
    if (trim(s).empty()) return;
    narr = narr.empty() ? spySentence(s) : narr + " " + spySentence(s);
  };
  const auto mark = [&](const std::string& t, size_t r) {
    if (tableName.empty() && r != size_t(-1)) {
      tableName = t;
      tableRow = r;
    }
  };
  const auto cell = [&](const Table& t, size_t r, const char* col) -> std::string {
    return t.has(col) ? resolve(trim(t.at(r, col))) : std::string();
  };
  const auto winLo = [&](const Table& t, size_t r, const char* col, double v) {
    const std::string s = t.has(col) ? trim(t.at(r, col)) : std::string();
    return s.empty() || v >= t.num(r, col);
  };
  const auto winHi = [&](const Table& t, size_t r, const char* col, double v) {
    const std::string s = t.has(col) ? trim(t.at(r, col)) : std::string();
    return s.empty() || v <= t.num(r, col);
  };
  const auto drawRow = [&](const char* table, const char* weight, const char* fcol,
                           const std::string& fval, const char* flag) {
    return pickRow(table, weight ? weight : "", fcol ? fcol : "", fval, flag ? flag : "", dealerRng_);
  };
  const auto nameOf = [&](bool female) { return drawFirstName(female) + " " + drawSurname(); };
  // The packaging cell is already plural ("unlubricated condoms"), so only a
  // singular word gets an "s".
  const auto plural = [](const std::string& w) {
    return (!w.empty() && w[w.size() - 1] == 's') ? w : w + "s";
  };
  const auto doseWord = [](int n) { return std::to_string(n) + (n == 1 ? " dose" : " doses"); };

  // 1. Shaking down a customer who is behind on the money.
  if (h.dealerStanding >= 45 && dealerRng_.chance(0.08)) {
    const size_t r = drawRow("a16_dealer-extortion-method-enc", "Commonness Score", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-extortion-method-enc");
      const bool female = dealerRng_.chance(0.5);
      dealerVictim_ = nameOf(female);
      pronounOtherSet_ = true;
      pronounOtherFemale_ = female;
      title = "Money owed";
      say("I went with the " + lower(cell(t, r, "Method")) + " approach.");
      // The Feedback Scenario carries the alternative list and the Log Version
      // refers back to it as "<rb>", so the two cells are read in that order.
      const std::string owed = cell(t, r, "Feedback Scenario");
      if (!owed.empty()) tell(spySwap(spySwap(owed, "You ", "I "), "your ", "my "));
      say(cell(t, r, "Log Version"));
      const size_t c = drawRow("a16_dealer-extortion-color-snippet-enc", "Commonness Score", "Target Gender",
                               female ? "female" : "male", h.dealerKingpin ? "Hero is Kingpin" : 0);
      if (c != size_t(-1)) tell(cell(tbl("a16_dealer-extortion-color-snippet-enc"), c, "Snippet"));
      mark("a16_dealer-extortion-method-enc", r);
      const long long take = 200 + static_cast<long long>(t.num(r, "Effectiveness Score") * 120.0) + h.dealerStanding * 10;
      h.money += take;
      tell("That was worth " + money(take) + ".");
      h.dealerStanding += 2;
      h.dealerHeat += 1;
    }
  }
  // 2. The run: the batch goes out and comes back as money, or it does not.
  else if (dealerRng_.chance(0.22)) {
    if (h.dealerBatch <= 0) h.dealerBatch = 20 + int(dealerRng_.below(61));
    const size_t r = drawRow("a16_dealer-drug-drop-off-location-enc", "Commonness Score", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-drug-drop-off-location-enc");
      const std::string where = cell(t, r, "Location");
      const double risk = t.num(r, "Risk Score");
      const int doses = h.dealerBatch;
      const bool clean = (h.dealerWillpower + int(dealerRng_.below(41))) > (int(risk * 12.0) + h.dealerHeat);
      title = "A run out to " + where;
      body = "I loaded " + doseWord(doses) + " of " + h.dealerDrug + " in " +
             plural(h.dealerPackage) + " and drove out to " + where + ".";
      if (clean) {
        const long long take = static_cast<long long>(doses) * static_cast<long long>(h.dealerPrice) * 2;
        h.money += take;
        h.dealerBatch = 0;
        h.dealerRunStatus = "complete";
        say(spySwap(cell(t, r, "Success Feedback Snippet"), "your ", "my "));
        tell(spySwap(cell(t, r, "Success Log Version"), "your ", "my "));
        tell("The run cleared " + money(take) + ".");
        const size_t f = drawRow("a16_dealer-run-outcome-flavor-enc", "Commonness Score", "Run Status", "complete", 0);
        if (f != size_t(-1)) tell(spyUpperFirst(spySwap(spySwap(cell(tbl("a16_dealer-run-outcome-flavor-enc"), f, "String"), "you ", "me "), "your ", "my ")));
        h.dealerInfamy += 2;
        if (h.dealerHeat > 2) h.dealerHeat -= 2;
        happy += 2;
      } else {
        const int lost = doses / 2;
        h.dealerBatch -= lost;
        h.dealerRunStatus = "ended";
        h.dealerHeat += 3;
        say(spySwap(cell(t, r, "Failure Feedback Snippet"), "your ", "my "));
        tell(spySwap(cell(t, r, "Failure Log Version"), "your ", "my "));
        tell("I lost half the batch on the road.");
        const size_t f = drawRow("a16_dealer-run-outcome-flavor-enc", "Commonness Score", "Run Status", "ended", 0);
        if (f != size_t(-1)) tell(spyUpperFirst(spySwap(spySwap(cell(tbl("a16_dealer-run-outcome-flavor-enc"), f, "String"), "you ", "me "), "your ", "my ")));
        happy -= 3;
      }
      mark("a16_dealer-drug-drop-off-location-enc", r);
    }
  }
  // 3. The everyday sale: a customer, a package, a reason to say no.
  else if (dealerRng_.chance(0.30)) {
    const size_t r = drawRow("a16_dealer-drug-sale-rejection-reason-enc", "", "Reason", "", 0);
    const bool female = dealerRng_.chance(0.5);
    const std::string buyer = nameOf(female);
    const int doses = 1 + int(dealerRng_.below(10));
    const long long price = static_cast<long long>(doses) * static_cast<long long>(h.dealerPrice) + 20;
    const bool refused = int(dealerRng_.below(100)) > ((h.dealerWillpower + h.dealerDemand) / 2);
    if (refused && r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-drug-sale-rejection-reason-enc");
      pronounOtherSet_ = true;
      pronounOtherFemale_ = female;
      title = "A customer walks";
      say(spySwap(cell(t, r, "Snippet"), "you ", "I "));
      tell(buyer + " said no because " + lower(trim(t.at(r, "Reason"))) + ".");
      const size_t q = drawRow("a16_dealer-drug-sale-refusal-snippet-enc", "", "Reason", trim(t.at(r, "Reason")), 0);
      if (q != size_t(-1)) say(spyUpperFirst(cell(tbl("a16_dealer-drug-sale-refusal-snippet-enc"), q, "Reason")));
      mark("a16_dealer-drug-sale-rejection-reason-enc", r);
      h.dealerDemand -= 2;
    } else {
      title = "Selling " + h.dealerDrug;
      body = "I sold " + doseWord(doses) + " to " + buyer + " for " + money(price) + ".";
      h.money += price;
      h.dealerDoses += doses;
      h.dealerBatch += 5;
      say("I moved the batch without a problem.");
      const size_t e = drawRow("a16_dealer-drug-effect-enc", "", "", "", 0);
      if (e != size_t(-1)) {
        const Table& t = tbl("a16_dealer-drug-effect-enc");
        tell(buyer + " reported " + lower(cell(t, e, "Effect")) + ".");
        const size_t pr = drawRow("a16_dealer-drug-purchase-reason-enc", "", "Target Gender", female ? "female" : "male", 0);
        if (pr != size_t(-1) && winLo(tbl("a16_dealer-drug-purchase-reason-enc"), pr, "Min Addiction", h.dealerDemand) &&
            winHi(tbl("a16_dealer-drug-purchase-reason-enc"), pr, "Max Addiction", h.dealerDemand))
          tell(buyer + " bought it because " + lower(cell(tbl("a16_dealer-drug-purchase-reason-enc"), pr, "Snippet")) + ".");
        mark("a16_dealer-drug-effect-enc", e);
      }
      h.dealerDemand += 2;
      happy += 1;
    }
  }
  // 4. A police warning scales with how much heat the trade has picked up.
  else if (h.dealerHeat >= 2 && dealerRng_.chance(0.10)) {
    const Table& t = tbl("a16_dealer-police-warning-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (winLo(t, r, "Min Willpower", h.dealerWillpower) && winHi(t, r, "Max Willpower", h.dealerWillpower)) pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[dealerRng_.below(uint32_t(pool.size()))];
      title = "A warning";
      say("A detective pulled me over and " + lower(cell(t, r, "String")) + ".");
      mark("a16_dealer-police-warning-enc", r);
      h.dealerHeat += 2;
      h.dealerWillpower += 3;
      happy -= 2;
    }
  }
  // 5. Killing a member of the cartel.
  else if (h.dealerStanding >= 50 && dealerRng_.chance(0.06)) {
    const size_t r = drawRow("a16_dealer-cartel-member-murder-scenario-enc", "", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-cartel-member-murder-scenario-enc");
      if (dealerMember_.empty()) {
        dealerMemberFemale_ = dealerRng_.chance(0.5);
        dealerMember_ = nameOf(dealerMemberFemale_);
      }
      dealerVictim_ = dealerMember_;
      pronounOtherSet_ = true;
      pronounOtherFemale_ = dealerMemberFemale_;
      title = "What happened to " + dealerMember_;
      say(spyUpperFirst(cell(t, r, "Success String")));
      mark("a16_dealer-cartel-member-murder-scenario-enc", r);
      h.dealerStanding += 4;
      h.dealerHeat += 4;
      h.dealerInfamy += 6;
      dealerMember_.clear();
      happy -= 3;
    }
  }
  // 6. A member has had enough.
  else if (h.dealerStanding <= 55 && dealerRng_.chance(0.06)) {
    const size_t r = drawRow("a16_dealer-cartel-member-leave-scenario-enc", "", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-cartel-member-leave-scenario-enc");
      if (winLo(t, r, "Min Relationship", h.dealerStanding) && winHi(t, r, "Max Relationship", h.dealerStanding)) {
        if (dealerMember_.empty()) {
          dealerMemberFemale_ = dealerRng_.chance(0.5);
          dealerMember_ = nameOf(dealerMemberFemale_);
        }
        title = "Someone wanted out";
        say(cell(t, r, "String"));
        mark("a16_dealer-cartel-member-leave-scenario-enc", r);
        h.dealerStanding -= 3;
        dealerMember_.clear();
      }
    }
  }
  // 7. The takeover: the hero moves on the rival cartel.
  else if (h.dealerStanding >= 55 && dealerRng_.chance(0.05)) {
    const size_t r = drawRow("a16_dealer-cartel-takeover-method-enc", "", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-cartel-takeover-method-enc");
      const double violence = t.num(r, "Violence Score");
      const bool won = (h.dealerStanding + int(dealerRng_.below(31))) > (int(violence * 10.0) + 40);
      title = won ? "The other side folded" : "The takeover went wrong";
      body = won ? cell(t, r, "Success Scenario") : cell(t, r, "Failure Scenario");
      say(won ? cell(t, r, "Success Log Version") : cell(t, r, "Failure Log Version"));
      mark("a16_dealer-cartel-takeover-method-enc", r);
      if (won) {
        h.dealerStanding += 5;
        h.dealerInfamy += 8;
        h.money += 2500;
        tell("The street was mine and it paid " + money(2500) + ".");
      } else {
        h.dealerStanding -= 5;
        h.dealerHeat += 3;
        happy -= 3;
      }
    }
  }
  // 8. The hero's own men move against the kingpin.
  else if (h.dealerKingpin && dealerRng_.chance(0.05)) {
    if (dealerRng_.chance(0.5)) {
      const size_t r = drawRow("a16_dealer-mutiny-methods-enc", "Commonness Score", "", "", 0);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_dealer-mutiny-methods-enc");
        title = "A quiet conversation";
        body = cell(t, r, "Log Version");
        say("I made it clear where the money comes from.");
        mark("a16_dealer-mutiny-methods-enc", r);
        h.dealerStanding += 2;
      }
    } else {
      const size_t r = drawRow("a16_dealer-kingpin-mutiny-success-snippets-enc", "Commonness Score", "Kingpin Gender", h.female ? "female" : "male", 0);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_dealer-kingpin-mutiny-success-snippets-enc");
        title = "My own people turned";
        say(cell(t, r, "String"));
        tell(cell(t, r, "Log Version Snippet"));
        mark("a16_dealer-kingpin-mutiny-success-snippets-enc", r);
        h.dealerKingpin = false;
        h.dealerStanding -= 12;
        h.dealerInfamy += 4;
        happy -= 4;
      }
    }
  }
  // 9. Warning the other side off.
  else if (h.dealerStanding >= 50 && dealerRng_.chance(0.05)) {
    const size_t r = drawRow("a16_dealer-hero-threatens-rival-cartel-member-enc", "Commonness Score", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-hero-threatens-rival-cartel-member-enc");
      title = "A word with the other side";
      say(cell(t, r, "Scenario"));
      mark("a16_dealer-hero-threatens-rival-cartel-member-enc", r);
      h.dealerStanding += 2;
      h.dealerHeat += 1;
    }
  }
  // 10. Cutting the batch to stretch it further.
  else if (dealerRng_.chance(0.06)) {
    const size_t n = drawRow("a16_dealer-cut-drug-name-enc", "Commonness Score", "Base Drug", h.dealerDrug, 0);
    const size_t s = drawRow("a16_dealer-cut-drug-substance-enc", "Commonness Score", "", "", 0);
    if (n != size_t(-1)) {
      const Table& t = tbl("a16_dealer-cut-drug-name-enc");
      const std::string cutName = cell(t, n, "Name");
      title = "Cutting the batch";
      body = h.dealerDrug + " became " + cutName + ".";
      mark("a16_dealer-cut-drug-name-enc", n);
      say("I cut the batch and started moving it as " + cutName + ".");
      h.dealerDemand -= 4;
      h.dealerBatch += 30;
      h.money += 300 + static_cast<long long>(h.dealerPrice) * 4;
      if (s != size_t(-1)) {
        const Table& st = tbl("a16_dealer-cut-drug-substance-enc");
        tell("Cut with " + lower(cell(st, s, "Substance")) + ".");
        if (st.flag(s, "Is Fatal") && dealerRng_.chance(0.4)) {
          tell("One of them never came back from it.");
          h.dealerHeat += 2;
          h.dealerStanding -= 3;
          happy -= 3;
        }
      }
    }
  }
  // 11. What the product does to the people who use it.
  else if (dealerRng_.chance(0.10)) {
    const bool shared = dealerRng_.chance(0.5);
    const bool female = dealerRng_.chance(0.5);
    const std::string user = nameOf(female);
    const char* which = shared ? "a16_dealer-drug-trip-experience-enc" : "a16_dealer-deal-person-drug-experience-enc";
    const size_t r = drawRow(which, "Commonness Score", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl(which);
      title = "The product did its job";
      pronounOtherSet_ = true;
      pronounOtherFemale_ = female;
      say(cell(t, r, "Effect"));
      tell(user + " says it " + lower(cell(t, r, "String")) + ".");
      mark(which, r);
      h.dealerDemand += 3;
      h.money += 150;
    }
  }
  // 12. Passing the time at a party the trade runs.
  else if (dealerRng_.chance(0.08)) {
    const size_t r = drawRow("a16_dealer-drug-pastime-enc", "", "Target Gender", h.female ? "male" : "female", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-drug-pastime-enc");
      if (h.dealerInfamy >= int(t.num(r, "Target Min Infamy"))) {
        title = "A night off the money";
        say(cell(t, r, "String"));
        mark("a16_dealer-drug-pastime-enc", r);
        h.dealerStanding += 1;
        happy += 2;
      }
    }
  }
  // 13. Trading dope for something that is not cash.
  else if (dealerRng_.chance(0.07)) {
    const size_t r = drawRow("a16_dealer-drug-barter-item-enc", "", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-drug-barter-item-enc");
      const std::string bare = cell(t, r, "Item");
      const std::string item = t.flag(r, "Plural") ? bare : withArticle(bare, t.flag(r, "Uses Indefinite Article")) + bare;
      title = "A trade";
      body = "I swapped " + plural(h.dealerPackage) + " of " + h.dealerDrug + " for " + item + ".";
      say("I took " + item + " for the package.");
      mark("a16_dealer-drug-barter-item-enc", r);
      h.money += static_cast<long long>(t.num(r, "Max Value"));
      h.dealerDemand += 1;
    }
  }
  // 14. One of the workers does something about the business.
  else if (dealerRng_.chance(0.06)) {
    const size_t r = drawRow("a16_dealer-drug-worker-action-enc", "Commonness Score", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-drug-worker-action-enc");
      if (dealerMember_.empty()) {
        dealerMemberFemale_ = dealerRng_.chance(0.5);
        dealerMember_ = nameOf(dealerMemberFemale_);
      }
      title = "Someone on the corner got ideas";
      say(dealerMember_ + " " + lower(cell(t, r, "Snippet")) + ".");
      mark("a16_dealer-drug-worker-action-enc", r);
      if (t.num(r, "Threat Score") >= 6) {
        tell("I had to make an example of someone.");
        const size_t m = drawRow("a16_dealer-drug-worker-murder-scenario-enc", "Commonness Score", "", "", 0);
        if (m != size_t(-1)) tell(cell(tbl("a16_dealer-drug-worker-murder-scenario-enc"), m, "String"));
        h.dealerStanding -= 2;
        h.dealerHeat += 2;
      } else {
        h.dealerStanding += 1;
      }
      dealerMember_.clear();
    }
  }
  // 15. The cartel wants its cut.
  else if (dealerRng_.chance(0.08)) {
    const size_t r = drawRow("a16_dealer-cartel-give-money-feedback-enc", "Commonness Score", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-cartel-give-money-feedback-enc");
      const bool refusal = t.flag(r, "Is Refusal");
      const long long due = 500 + h.dealerStanding * 20;
      title = refusal ? "I said no to the dues" : "Paying the dues";
      if (refusal) {
        say(spyUpperFirst(spySwap(spySwap(cell(t, r, "String"), "you ", "me "), "your ", "my ")));
        const size_t th = drawRow("a16_dealer-cartel-threat-enc", "", "", "", h.dealerKingpin ? "Kingpin Only" : 0);
        if (th != size_t(-1)) {
          const Table& tt = tbl("a16_dealer-cartel-threat-enc");
          if (winLo(tt, th, "Min Standing", h.dealerStanding) && winHi(tt, th, "Max Standing", h.dealerStanding))
            tell(spyUpperFirst(spySwap(spySwap(cell(tt, th, "Snippet"), "you ", "me "), "your ", "my ")));
        }
        h.dealerStanding -= 4;
      } else if (h.money >= due) {
        h.money -= due;
        say("I paid " + money(due) + " up the chain.");
        h.dealerStanding += 2;
      } else {
        say("I did not have the money and they knew it.");
        h.dealerStanding -= 2;
      }
      mark("a16_dealer-cartel-give-money-feedback-enc", r);
    }
  }
  // 16. Turning down the invitation.
  else if (dealerRng_.chance(0.06)) {
    const size_t r = drawRow("a16_dealer-cartel-party-refusal-enc", "", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-cartel-party-refusal-enc");
      title = "Not tonight";
      say(cell(t, r, "String"));
      const std::string why = lower(trim(t.at(r, "Reason")));
      if (!why.empty()) tell("The reason: " + why + ".");
      mark("a16_dealer-cartel-party-refusal-enc", r);
      h.dealerStanding -= 1;
    }
  }
  // 17. The trade follows the hero inside.
  else if (h.inPrison && dealerRng_.chance(0.05)) {
    const size_t r = drawRow("a16_dealer-prison-drug-trips-enc", "Commonness Score", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-prison-drug-trips-enc");
      title = "Inside";
      // The Feedback Snippet carries the alternative list ("nearly broke your
      // <foot/toe/...>") that the Log Version abbreviates as "<rb>".
      tell(spySwap(spySwap(cell(t, r, "Feedback Snippet"), "You ", "I "), "your ", "my "));
      say(cell(t, r, "Log Version"));
      mark("a16_dealer-prison-drug-trips-enc", r);
      h.dealerStanding += 1;
      h.money += 200;
    } else {
      const size_t p = drawRow("a16_dealer-prison-cartel-packages-enc", "Commonness Score", "", "", 0);
      if (p != size_t(-1)) {
        const Table& t = tbl("a16_dealer-prison-cartel-packages-enc");
        title = "A package came in";
        body = "A " + lower(cell(t, p, "Contents")) + " arrived in the yard.";
        mark("a16_dealer-prison-cartel-packages-enc", p);
        h.dealerStanding += 1;
      }
    }
  }
  // 18. Testing a batch before it goes out.
  else if (dealerRng_.chance(0.08)) {
    const Table& t = tbl("a16_dealer-batch-tester-feedback-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (winLo(t, r, "Min Demand", h.dealerDemand) && winHi(t, r, "Max Demand", h.dealerDemand)) pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[dealerRng_.below(uint32_t(pool.size()))];
      title = "Testing the batch";
      say(cell(t, r, "String"));
      mark("a16_dealer-batch-tester-feedback-enc", r);
      h.dealerDemand += 2;
      h.money += 200;
    }
  }
  // 19. Someone takes too much.
  else if (dealerRng_.chance(0.05)) {
    const size_t r = drawRow("a16_dealer-overdose-snippet-enc", "Commonness Score", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-overdose-snippet-enc");
      title = "An overdose";
      say(spyUpperFirst(spySwap(cell(t, r, "Snippet"), "your ", "my ")));
      mark("a16_dealer-overdose-snippet-enc", r);
      h.dealerHeat += 3;
      h.dealerStanding -= 3;
      h.dealerDemand -= 5;
      happy -= 3;
    }
  }

  // 19b. Moving the cheap gear that keeps the corner running.
  else if (dealerRng_.chance(0.07)) {
    const size_t r = drawRow("a16_dealer-deal-activity-item-enc", "Appeal Score", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-deal-activity-item-enc");
      const long long take = static_cast<long long>(t.num(r, "Max Sale Value"));
      title = "Selling the small stuff";
      const std::string bare = cell(t, r, "Item");
      body = "I sold " + (t.flag(r, "Plural") ? bare : withArticle(bare, t.flag(r, "Uses Indefinite Article")) + bare)
           + " to a " + lower(cell(t, r, "Person Type")) + " for " + money(take) + ".";
      say("I moved " + lower(cell(t, r, "Item")) + " on the side.");
      mark("a16_dealer-deal-activity-item-enc", r);
      h.money += take;
      h.dealerDemand += 1;
    }
  }
  // 20. The trade asks a question the hero has to answer.
  if (title.empty() && narr.empty()) {
    const size_t r = drawRow("a16_dealer-drug-run-dumb-scenario-enc", "", "", "", 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_dealer-drug-run-dumb-scenario-enc");
      title = cell(t, r, "Scenario Title");
      body = spySwap(cell(t, r, "Body"), "your ", "my ");
      const std::string cta = cell(t, r, "CTA");
      if (!cta.empty()) tell(cta);
      mark("a16_dealer-drug-run-dumb-scenario-enc", r);
      h.dealerStanding += 1;
      h.money += 400;
    }
  }

  if (title.empty() && narr.empty() && body.empty()) return false;
  out.table = tableName.empty() ? "a16_dealer-cartel-drug-enc" : tableName;
  out.row_id = std::to_string(tableRow == size_t(-1) ? size_t(0) : tableRow);
  out.title = title;
  out.body = body;
  out.narrative = narr;
  out.extra = extra;
  out.happiness += happy;
  return true;
}

// --------------------------------------------------------------- the mafia --
// Section 64: the a16_mafia-* family. The hero is made, takes his orders from
// a godfather, and works the neighbourhood the mob table assigns to his
// country. Most of the family's rows arrive in the second person, so they pass
// through a one-way swap before they are logged.

// The mob tables key their rows on ISO country codes ("it", "cn"), while the
// simulator keeps the country name, so the code is looked up for each call.
static std::string mafiaCountryCode(const std::vector<bl::Country>& countries, const std::string& name) {
  for (size_t i = 0; i < countries.size(); ++i)
    if (countries[i].name == name) return lower(trim(countries[i].code));
  return std::string();
}

std::string Sim::mafiaToken(const std::string& key) {
  Hero& h = *const_cast<Hero*>(hero_);
  std::string k = key;
  const std::string psuffix = "-possessive";
  bool poss = false;
  if (k.size() > psuffix.size() && k.compare(k.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    poss = true;
    k = k.substr(0, k.size() - psuffix.size());
  }
  const auto own = [&](const std::string& v) { return poss ? (v + "'s") : v; };
  const auto speakAs = [&](bool female) { pronounOtherSet_ = true; pronounOtherFemale_ = female; };
  const auto cached = [&](const std::string& slot, bool female) -> std::string {
    std::map<std::string, std::string>::iterator it = mafiaCache_.find(slot);
    if (it != mafiaCache_.end()) return it->second;
    const std::string name = drawFirstName(female) + " " + drawSurname();
    mafiaCache_[slot] = name;
    return name;
  };
  if (k == "mafia-boss" || k == "mafia-godfather") {
    // Once the hero holds the top rank he is the godfather himself.
    if (h.mafiaRank >= 5) { speakAs(h.female); return own(h.firstName + " " + h.lastName); }
    if (h.mafiaBoss.empty()) {
      size_t r = size_t(-1);
      if (ensureTable("a16_mafia-nicknames-enc")) {
        const Table& t = tbl("a16_mafia-nicknames-enc");
        std::vector<size_t> pool;
        for (size_t i = 0; i < t.size(); ++i) {
          const std::string c = lower(trim(t.at(i, "Mob Country")));
          if (!c.empty() && !h.mafiaCountry.empty() && c != h.mafiaCountry) continue;
          pool.push_back(i);
        }
        if (!pool.empty()) r = pool[mafiaRng_.below(uint32_t(pool.size()))];
      }
      h.mafiaBoss = r == size_t(-1)
                  ? (drawFirstName(false) + " " + drawSurname())
                  : resolve(trim(tbl("a16_mafia-nicknames-enc").at(r, "Nickname")));
    }
    speakAs(false);
    return own(h.mafiaBoss);
  }
  if (k == "mafia-family") return own(h.mafiaFamily);
  if (k == "mafia-rival-family") return own(h.mafiaRival);
  if (k == "mafia-rival-family-member") {
    const bool female = mafiaRng_.chance(0.5);
    speakAs(female);
    return own(cached("rival-member", female));
  }
  if (k == "mafia-colleague") {
    const bool female = mafiaRng_.chance(0.5);
    speakAs(female);
    return own(cached("colleague", female));
  }
  if (k == "mafia-nickname") return own(h.mafiaNick);
  if (k == "mafia-calling-card") {
    if (!ensureTable("a16_mafia-calling-cards-enc")) return std::string();
    const size_t r = pickRow("a16_mafia-calling-cards-enc", "", "", "", "", mafiaRng_);
    if (r == size_t(-1)) return std::string();
    const std::string item = resolve(trim(tbl("a16_mafia-calling-cards-enc").at(r, "Item")));
    // 'Uses Partitive Article' marks the mass nouns ("some dog droppings in a
    // paper bag"); everything else takes a plain article.
    return own((tbl("a16_mafia-calling-cards-enc").flag(r, "Uses Partitive Article") ? std::string("some ") : articleFor(item)) + item);
  }
  if (k == "first-name") return own(drawFirstName(mafiaRng_.chance(0.5)));
  if (k == "last-name") return own(drawSurname());
  if (k == "other-person") {
    const bool female = mafiaRng_.chance(0.5);
    speakAs(female);
    return own(cached("other", female));
  }
  if (k == "amount") {
    if (mafiaAmount_.empty()) mafiaAmount_ = money(1000 + static_cast<long long>(mafiaRng_.below(49000)));
    return mafiaAmount_;
  }
  if (k == "city-other") {
    // Another city of the hero's own country, the way <country-other> picks
    // another country (report 38).
    if (!hero_ || hero_->city.empty()) return std::string();
    const std::string home = hero_->country;
    for (size_t i = 0; i < countries_.size(); ++i) {
      if (countries_[i].name != home || countries_[i].cities.empty()) continue;
      for (int tries = 0; tries < 12; ++tries) {
        const std::string cand = countries_[i].cities[mafiaRng_.below(uint32_t(countries_[i].cities.size()))];
        if (cand != hero_->city) return own(cand);
      }
    }
    return std::string();
  }
  return std::string();
}

// The rank titles live in the mobs table, one column per rank, with the top
// rank split by gender.
std::string Sim::mafiaRankTitle(const Hero& h, int rank) {
  if (rank < 1) rank = 1;
  if (rank > 5) rank = 5;
  if (!ensureTable("a16_mafia-mobs-enc")) return std::string();
  const Table& t = tbl("a16_mafia-mobs-enc");
  for (size_t i = 0; i < t.size(); ++i) {
    if (trim(t.at(i, "Mob")) != h.mafiaFamily) continue;
    const std::string col = rank == 5 ? (h.female ? "Rank 5 Female" : "Rank 5 Male")
                                      : ("Rank " + std::to_string(rank));
    if (!t.has(col)) return std::string();
    return trim(t.at(i, col));
  }
  return std::string();
}

void Sim::ensureMafiaFields(Life& life) {
  (void)life;
  Hero& h = *const_cast<Hero*>(hero_);
  const std::string cc = mafiaCountryCode(countries_, h.country);
  h.mafiaCountry = cc;
  // The family owed to the country: a row whose Associated Country is the
  // hero's, or any row when that country has no mob of its own.
  if (ensureTable("a16_mafia-mobs-enc")) {
    const Table& t = tbl("a16_mafia-mobs-enc");
    std::vector<size_t> pool;
    for (size_t i = 0; i < t.size(); ++i) {
      const std::string ac = lower(trim(t.at(i, "Associated Country")));
      if (!cc.empty() && ac != cc) continue;
      pool.push_back(i);
    }
    if (pool.empty()) for (size_t i = 0; i < t.size(); ++i) pool.push_back(i);
    const size_t r = pool.empty() ? size_t(-1) : pool[mafiaRng_.below(uint32_t(pool.size()))];
    if (r == size_t(-1)) {
      h.mafiaFamily = drawSurname() + " Family";
    } else {
      h.mafiaFamily = trim(t.at(r, "Mob"));
      h.mafiaNotoriety = int(t.num(r, "Notoriety Score") * 10.0);
      h.mafiaMoney = int(t.num(r, "Money Score") * 10.0);
    }
    std::vector<size_t> rivals;
    for (size_t i = 0; i < t.size(); ++i)
      if (trim(t.at(i, "Mob")) != h.mafiaFamily) rivals.push_back(i);
    if (!rivals.empty()) h.mafiaRival = trim(t.at(rivals[mafiaRng_.below(uint32_t(rivals.size()))], "Mob"));
  }
  if (h.mafiaFamily.empty()) h.mafiaFamily = drawSurname() + " Family";
  if (h.mafiaRival.empty()) h.mafiaRival = "the other family";
  // The hero's own nickname, from the same table the godfathers come from
  // (report 56 wired it as a misc source first).
  if (ensureTable("a16_mafia-nicknames-enc")) {
    const Table& t = tbl("a16_mafia-nicknames-enc");
    const std::string want = h.female ? "female" : "male";
    std::vector<size_t> pool;
    for (size_t i = 0; i < t.size(); ++i) {
      const std::string g = lower(trim(t.at(i, "Gender")));
      if (!g.empty() && g != want) continue;
      const std::string c = lower(trim(t.at(i, "Mob Country")));
      if (!c.empty() && !cc.empty() && c != cc) continue;
      pool.push_back(i);
    }
    if (pool.empty()) for (size_t i = 0; i < t.size(); ++i) pool.push_back(i);
    if (!pool.empty()) h.mafiaNick = resolve(trim(t.at(pool[mafiaRng_.below(uint32_t(pool.size()))], "Nickname")));
  }
  if (h.mafiaNick.empty()) h.mafiaNick = drawSurname();
  h.mafiaRankName = mafiaRankTitle(h, h.mafiaRank);
}

void Sim::makeMafia(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (h.mafia) return;
  h.mafia = true;
  h.mafiaRank = 1;
  h.mafiaStanding = 35;
  h.mafiaHeat = 0;
  h.mafiaHits = 0;
  h.mafiaEarned = 0;
  ensureMafiaFields(life);
  h.money += 250;
  // How he got in, for the first line of the diary.
  const size_t r = pickRow("a16_mafia-join-methods-enc", "", "", "", "", mafiaRng_);
  if (r != size_t(-1)) {
    const Table& t = tbl("a16_mafia-join-methods-enc");
    h.mafiaJoin = "I got in by " + spySentence(spyLowerFirst(trim(t.at(r, "Feedback Snippet")))) +
                  " (" + h.mafiaRankName + " of the " + h.mafiaFamily + ")";
  }
}

// The producer titles table writes about "the hero" in the third person, and a
// card is the hero's own log line (REPORT.en.md 69).
// A row that opens on "<he/she> says <he/she>'s ..." wants the act's name as its
// subject once, not the name followed by a pronoun (REPORT.en.md 69).
static std::string nameTakesSubject(std::string row, const std::string& name) {
  const size_t at = row.find("<he/she>");
  if (at != std::string::npos && !name.empty()) row.replace(at, 8, name);
  return row;
}

static std::string heroToFirstPerson(const std::string& s) {
  std::string t = spySwap(s, "The hero's ", "My ");
  t = spySwap(t, "the hero's ", "my ");
  t = spySwap(t, "The hero ", "I ");
  t = spySwap(t, "the hero ", "I ");
  return t;
}

static std::string toFirstPerson(const std::string& s) {

  static const char* kVerb[] = {"told", "warned", "reminded", "asked", "took", "taught",
                                "made", "cost", "gave", "given", "give", "showed", "shown",
                                "saw", "heard", "called", "sent", "brought", "handed",
                                "offered", "dealt", "kept", "threw", "put", "left", "paid",
                                "hit", "bit", "kissed", "loved", "hugged", "thanked", "met",
                                "trusted", "believed", "followed", "joined", "helped", "hurt",
                                "excused", "forgave", "blamed", "arrested", "robbed", "beat",
                                "chose", "picked", "hired", "fired", "served", "replaced",
                                // the same verbs in the present: "X tells you ..."
                                "tells", "warns", "reminds", "asks", "costs", "gives", "shows",
                                "sees", "hears", "calls", "sends", "brings", "hands", "offers",
                                "deals", "keeps", "throws", "puts", "leaves", "pays", "hits",
                                "kisses", "loves", "hugs", "thanks", "meets", "trusts",
                                "believes", "follows", "joins", "helps", "hurts", "blames",
                                "takes", "took"};
  std::string t = spySwap(s, "yourself", "myself");
  t = spySwap(t, "your ", "my ");
  t = spySwap(t, "Your ", "My ");
  t = spySwap(t, "you're", "I am");
  t = spySwap(t, "you'll", "I'll");
  t = spySwap(t, "you've", "I've");
  t = spySwap(t, "you'd", "I'd");
  t = spySwap(t, "You'd", "I'd");
  t = spySwap(t, "you don't", "I don't");
  t = spySwap(t, "you better", "I better");
  // A row may start on the capital, which the walk cannot see because of the
  // apostrophe (REPORT.en.md 69: "You're being told ... you produced ...").
  t = spySwap(t, "You're", "I am");
  t = spySwap(t, "You'll", "I'll");
  t = spySwap(t, "You've", "I've");
  t = spySwap(t, "You don't", "I don't");
  t = spySwap(t, "You better", "I better");
  size_t p = 0;
  while (p < t.size()) {
    // 'you' and 'You' both count; npos is the largest size_t so min() is safe.
    const size_t at = std::min(t.find("you", p), t.find("You", p));
    if (at == std::string::npos) break;
    p = at;
    const char before = p > 0 ? t[p - 1] : ' ';
    const char after = p + 3 < t.size() ? t[p + 3] : ' ';
    const bool letter = (before >= 'a' && before <= 'z') || (before >= 'A' && before <= 'Z') || before == '-';
    const bool tail = (after >= 'a' && after <= 'z') || (after >= 'A' && after <= 'Z') || after == 39;
    if (letter || tail) { p += 3; continue; }
    size_t e = p;
    while (e > 0 && t[e - 1] == ' ') --e;
    size_t q = e;
    while (q > 0 && ((t[q - 1] >= 'a' && t[q - 1] <= 'z') || (t[q - 1] >= 'A' && t[q - 1] <= 'Z'))) --q;
    const std::string prev = lower(t.substr(q, e - q));
    bool object = false;
    for (const char* v : kVerb) if (prev == v) object = true;
    const std::string rep = object ? "me" : "I";
    t.replace(p, 3, rep);
    p += rep.size();
  }
  t = spySwap(t, "I aren't", "I am not");
  return t;
}

bool Sim::mafiaYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.mafia || h.age < 18) return false;
  if (!mafiaRng_.chance(0.40)) return false;
  if (h.mafiaRankName.empty()) ensureMafiaFields(life);

  std::string title, body, line, tableRef, rowRef;
  std::vector<std::string> extra;
  int happy = 0;
  mafiaAmount_.clear();
  mafiaCache_.clear();
  pronounOtherSet_ = false;
  auto say = [&body](const std::string& s) {
    if (!s.empty()) body += body.empty() ? s : " " + s;
  };
  auto tell = [&line](const std::string& s) {
    if (!s.empty()) line += line.empty() ? s : " " + s;
  };
  const auto cell = [this](const Table& t, size_t r, const char* col) {
    return t.has(col) ? resolve(trim(t.at(r, col))) : std::string();
  };
  // The family talks to the hero in the second person, so the log turns it
  // around. 'you' is the object after a verb of address ("told you") and the
  // subject everywhere else, which is as far as a token swap can go.
  // shortcut: a hand-kept verb list, upgrade if a table adds another verb.
  const auto toFirst = [](const std::string& s) { return toFirstPerson(s); };  // The family's log lines refer back to the alternatives drawn in another
  // column of the same row ("<rb>", "<amount>"), so every cell of that row
  // which carries brackets is resolved before the line itself is written.
  const auto warm = [this](const Table& t, size_t r, const char* keep) {
    const std::vector<std::string>& cols = t.columns();
    for (size_t c = 0; c < cols.size(); ++c) {
      if (keep && cols[c] == keep) continue;
      if (t.at(r, cols[c]).find('<') != std::string::npos) resolve(t.at(r, cols[c]));
    }
  };
  const auto drawRow = [&](const char* name, const char* weight, const char* fcol, const char* fval, const char* flag) {
    const size_t r = pickRow(name, weight ? weight : "", fcol ? fcol : "", fval ? fval : "",
                             flag ? flag : "", mafiaRng_);
    if (r != size_t(-1)) { tableRef = name; rowRef = std::to_string(r); }
    return r;
  };

  // 1. A shakedown: a business that is behind on its payments.
  if (mafiaRng_.chance(0.18)) {
    const size_t tg = drawRow("a16_mafia-extortion-targets-enc", "Commonness Score", 0, 0, 0);
    const size_t sm = drawRow("a16_mafia-shakedown-methods-enc", 0, 0, 0, 0);
    if (tg != size_t(-1) && sm != size_t(-1)) {
      const Table& tt = tbl("a16_mafia-extortion-targets-enc");
      const Table& st = tbl("a16_mafia-shakedown-methods-enc");
      title = "The shakedown";
      say("I paid a visit to the owner of a " + lower(trim(tt.at(tg, "Company Type"))) +
          " who was behind on his payments.");
      pronounOtherSet_ = true;
      pronounOtherFemale_ = mafiaRng_.chance(0.5);
      const bool worked = (st.num(sm, "Effectiveness Score") * 12.0 + h.mafiaStanding) > mafiaRng_.below(100);
      const char* sline = worked ? "Success Log Version" : "Failure Log Version";
      warm(st, sm, sline);
      tell(spyUpperFirst(cell(st, sm, sline)));
      if (worked) {
        const long long take = static_cast<long long>(tt.num(tg, "Money Score") * 450.0) + h.mafiaStanding * 8;
        h.money += take;
        h.mafiaEarned += take;
        h.mafiaStanding += 3;
        h.mafiaMoney += 3;
        extra.push_back("That was worth " + money(take) + " to the family.");
        happy += 2;
      } else {
        h.mafiaStanding -= 4;
        h.mafiaHeat += int(st.num(sm, "Get Caught Score"));
        happy -= 2;
      }
      tableRef = "a16_mafia-shakedown-methods-enc";
      rowRef = std::to_string(sm);
    }
  }
  // 2. A job for the family: push-crimes knows how the job is said both ways.
  else if (mafiaRng_.chance(0.30)) {
    const size_t cr = drawRow("a16_mafia-push-crimes-enc", 0, 0, 0, 0);
    const size_t pt = drawRow("a16_mafia-push-targets-enc", 0, 0, 0, 0);
    if (cr != size_t(-1)) {
      const Table& ct = tbl("a16_mafia-push-crimes-enc");
      title = "A job for the family";
      say("The ask: " + spySentence(spyLowerFirst(resolve(trim(ct.at(cr, "Present Snippet"))))));
      if (pt != size_t(-1)) {
        const Table& ptt = tbl("a16_mafia-push-targets-enc");
        const std::string reason = cell(ptt, pt, "Reason");
        say("The target: " + lower(trim(ptt.at(pt, "Target"))) +
            (reason.empty() ? std::string() : (", " + spySentence(spyLowerFirst(reason)))));
      }
      tell("I " + spySentence(spyLowerFirst(resolve(trim(ct.at(cr, "Past Snippet"))))));
      h.mafiaHeat += int(ct.num(cr, "Seriousness Score"));
      h.mafiaHits += 1;
      if (mafiaRng_.below(100) < int(ct.num(cr, "Get Caught Score") * 8.0)) {
        h.mafiaHeat += 4;
        h.mafiaStanding -= 3;
        extra.push_back("Someone talked to the police about it.");
        happy -= 2;
      } else {
        h.mafiaStanding += 2;
        h.mafiaMoney += 2;
        const long long take = 300 + static_cast<long long>(ct.num(cr, "Seriousness Score") * 700.0);
        h.money += take;
        h.mafiaEarned += take;
        extra.push_back("The family paid me " + money(take) + ".");
        happy += 2;
      }
      tableRef = "a16_mafia-push-crimes-enc";
      rowRef = std::to_string(cr);
    }
  }
  // 3. A contract, outbound while the hero is in good standing and inbound
  //    once the family has turned on him.
  else if (mafiaRng_.chance(0.10)) {
    const size_t ex = drawRow("a16_mafia-executions-enc", 0, 0, 0, 0);
    if (ex != size_t(-1)) {
      const Table& t = tbl("a16_mafia-executions-enc");
      const bool sentForHim = h.mafiaStanding < 20 && mafiaRng_.chance(0.35);
      pronounOtherSet_ = true;
      pronounOtherFemale_ = mafiaRng_.chance(0.5);
      if (sentForHim) {
        title = "They came for me";
        say("Two men I did not know took me for a drive.");
        warm(t, ex, "Hero Inbound Log Version");
        tell(spyUpperFirst(cell(t, ex, "Hero Inbound Log Version")));
        h.mafiaHeat += 4;
        h.mafiaStanding = 0;
        h.health -= int(t.num(ex, "Pain Score"));
        happy -= 8;
      } else {
        const std::string markName = drawFirstName(pronounOtherFemale_) + " " + drawSurname();
        title = "A contract";
        say("The family wanted " + markName + " gone.");
        warm(t, ex, "Hero Outbound Log Version");
        tell(spyUpperFirst(cell(t, ex, "Hero Outbound Log Version")));
        extra.push_back("Method: " + lower(trim(t.at(ex, "Method"))) + ".");
        const size_t card = drawRow("a16_mafia-calling-cards-enc", 0, 0, 0, 0);
        if (card != size_t(-1)) {
          const Table& ct = tbl("a16_mafia-calling-cards-enc");
          const std::string item = resolve(trim(ct.at(card, "Item")));
          extra.push_back("I left " + (ct.flag(card, "Uses Partitive Article") ? std::string("some ") : articleFor(item)) +
                          item + " where they would find it.");
        }
        h.mafiaStanding += 5;
        h.mafiaNotoriety += 4;
        h.mafiaHeat += 3;
        h.mafiaHits += 1;
        h.money += 2500;
        h.mafiaEarned += 2500;
        extra.push_back("The contract paid " + money(2500) + ".");
      }
      tableRef = "a16_mafia-executions-enc";
      rowRef = std::to_string(ex);
    }
  }
  // 4. The family says the hero is not earning enough.
  else if (mafiaRng_.chance(0.14)) {
    const size_t r = drawRow("a16_mafia-feedback-earnings-enc", 0, 0, 0, 0);
    if (r != size_t(-1)) {
      title = "A word about my numbers";
      say("The boss wants more money coming in.");
      tell(toFirst(cell(tbl("a16_mafia-feedback-earnings-enc"), r, "Feedback")));
      h.mafiaMoney -= 3;
      h.mafiaStanding -= 1;
      happy -= 2;
    }
  }
  // 5. Praise or disappointment, filtered to the mob's own country.
  else if (mafiaRng_.chance(0.14)) {
    const bool good = h.mafiaStanding >= 50;
    const char* name = good ? "a16_mafia-feedback-positive-enc" : "a16_mafia-feedback-negative-enc";
    size_t r = drawRow(name, 0, "Specific Mob Country", h.mafiaCountry.c_str(), 0);
    if (r == size_t(-1)) r = drawRow(name, 0, 0, 0, 0);
    if (r != size_t(-1)) {
      const Table& t = tbl(name);
      title = good ? "The boss noticed" : "The boss is not happy";
      warm(t, r, "Log Version");
      tell(spyUpperFirst(cell(t, r, "Log Version")));
      if (good) { h.mafiaStanding += 4; h.mafiaMoney += 2; happy += 3; }
      else { h.mafiaStanding -= 4; h.mafiaMoney -= 3; happy -= 3; }
      tableRef = name;
      rowRef = std::to_string(r);
    }
  }
  // 6. A temptation on the job: the cash is right there.
  else if (mafiaRng_.chance(0.12)) {
    const size_t r = drawRow("a16_mafia-temptation-scenarios-enc", 0, 0, 0, 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_mafia-temptation-scenarios-enc");
      const long long lo = static_cast<long long>(t.num(r, "Min Amount"));
      const long long hi = static_cast<long long>(t.num(r, "Max Amount"));
      const long long amount = lo + static_cast<long long>(mafiaRng_.unit() * double(hi > lo ? hi - lo : 0));
      mafiaAmount_ = money(amount);
      const bool take = mafiaRng_.chance(0.5);
      title = take ? "Nobody was watching" : "I did it the right way";
      say("I had the chance to pocket " + mafiaAmount_ + " off the top.");
      if (take) {
        warm(t, r, "Dishonorable Choice Narrative");
        tell(spyUpperFirst(cell(t, r, "Dishonorable Choice Narrative")));
        h.money += amount;
        h.mafiaEarned += amount;
        h.mafiaStanding -= 3;
        h.mafiaHeat += int(t.num(r, "Get Caught Score"));
        happy += 2;
      } else {
        tell("I turned it in and kept my hands clean.");
        h.mafiaStanding += 4;
        h.mafiaMoney += 2;
        extra.push_back("The family heard about it.");
        happy += 1;
      }
    }
  }
  // 7. An evening with the family.
  else if (mafiaRng_.chance(0.20)) {
    size_t r = drawRow("a16_mafia-pastimes-enc", 0, "Specific Mob Country", h.mafiaCountry.c_str(), 0);
    if (r == size_t(-1)) r = drawRow("a16_mafia-pastimes-enc", 0, 0, 0, 0);
    if (r != size_t(-1)) {
      title = "An evening with the family";
      tell("I went " + spySentence(spyLowerFirst(resolve(trim(tbl("a16_mafia-pastimes-enc").at(r, "Pastime"))))));
      h.mafiaStanding += 2;
      happy += 2;
    }
  }
  // 8. The hero talks his way out of a job.
  else if (mafiaRng_.chance(0.12)) {
    const char* want = h.female ? "female" : "male";
    size_t r = drawRow("a16_mafia-excuses-enc", 0, "Gender", want, 0);
    if (r == size_t(-1)) r = drawRow("a16_mafia-excuses-enc", 0, 0, 0, 0);
    if (r != size_t(-1)) {
      title = "I said I could not make it";
      tell("I told them " + toFirst(spyLowerFirst(cell(tbl("a16_mafia-excuses-enc"), r, "Scenario Snippet"))));
      h.mafiaStanding -= 2;
    }
  }
  // 9. Nobody is ready to hand out a promotion.
  else {
    const size_t r = drawRow("a16_mafia-join-rejections-enc", 0, 0, 0, 0);
    if (r != size_t(-1)) {
      title = "Not yet";
      tell(toFirst(cell(tbl("a16_mafia-join-rejections-enc"), r, "Feedback")));
      h.mafiaStanding -= 1;
    }
  }

  // The ranks move with the standing: 1 associate, 5 godfather.
  const int wantRank = h.mafiaStanding >= 92 ? 5 : h.mafiaStanding >= 78 ? 4 : h.mafiaStanding >= 62 ? 3
                    : h.mafiaStanding >= 45 ? 2 : 1;
  if (wantRank != h.mafiaRank) {
    const std::string was = h.mafiaRankName;
    h.mafiaRank = wantRank;
    h.mafiaRankName = mafiaRankTitle(h, wantRank);
    if (!h.mafiaRankName.empty() && h.mafiaRankName != was)
      extra.push_back(wantRank > 1 ? ("The family made me " + h.mafiaRankName + ".")
                                   : ("I was knocked back down to " + h.mafiaRankName + "."));
  }
  if (h.mafiaRank >= 5 && h.mafiaRival != "the other family" && mafiaRng_.chance(0.20)) {
    const size_t sc = drawRow("a16_mafia-scare-methods-enc", 0, 0, 0, 0);
    if (sc != size_t(-1)) {
      const Table& t = tbl("a16_mafia-scare-methods-enc");
      pronounOtherSet_ = true;
      pronounOtherFemale_ = mafiaRng_.chance(0.5);
      warm(t, sc, "Log Version");
      extra.push_back(spyUpperFirst(cell(t, sc, "Log Version")));
    }
  }
  if (extra.empty() && !h.mafiaRankName.empty())
    extra.push_back("I am " + withArticle(h.mafiaRankName, true) + h.mafiaRankName + " of the " + h.mafiaFamily + " with " +
                    std::to_string(h.mafiaHits) + (h.mafiaHits == 1 ? " job behind me." : " jobs behind me."));

  if (title.empty() && body.empty() && line.empty()) return false;
  out.table = tableRef.empty() ? "a16_mafia-pastimes-enc" : tableRef;
  out.row_id = rowRef.empty() ? std::string("0") : rowRef;
  out.title = title.empty() ? "The family" : title;
  out.body = body;
  out.narrative = line;
  out.extra = extra;
  out.happiness += happy;
  return true;
}

// ------------------------------------------------------------------ the zoo --
// Section 65: the a16_zoo-* family. The hero owns a park, keeps animals in
// habitats, pays keepers to look after them, and sells tickets at the gate.
// Every table in the family is keyed on one of three things: the species, the
// breed, or the individual animal's name, so a card keeps all three.

// A cell that lists habitats, breeds or features ("aviary, reptile room") is
// how these tables gate a row, so membership is checked one item at a time.
static bool zooInList(const std::string& list, const std::string& value) {
  if (list.empty() || value.empty()) return false;
  std::vector<std::string> items;
  splitOn(list, ',', items);
  for (const std::string& it : items)
    if (lower(trim(it)) == lower(trim(value))) return true;
  return false;
}

// The tables write a name and expect the English possessive after it.
static std::string zooPossessive(const std::string& name) {
  if (name.empty()) return name;
  const char last = name[name.size() - 1];
  return name + ((last == 's' || last == 'S') ? "'" : "'s");
}

// One individual drawn out of the bloodline table: the species, the breed, the
// name and the gender the tables ask for by turns.
void Sim::zooDrawAnimal(Life& life) {
  Hero& h = life.hero;
  if (!ensureTable("a16_zoo-animal-names-enc")) return;
  const Table& names = tbl("a16_zoo-animal-names-enc");
  if (names.size() == 0) return;
  const size_t r = pickRow("a16_zoo-animal-names-enc", "Commonness Score", "", "", "", zooRng_);
  if (r == size_t(-1)) return;
  h.zooAnimalSpecies = resolve(trim(names.at(r, "Animal")));
  h.zooAnimalBreed = resolve(trim(names.at(r, "Breed")));
  const std::string g = lower(resolve(trim(names.at(r, "Gender"))));
  h.zooAnimalFemale = g == "female" ? true : (g == "male" ? false : zooRng_.chance(0.5));
  // The name cell is a template of its own ("Ms. <random-surname>"), and a
  // blank one means the park never got round to naming the animal.
  std::string nm = resolve(trim(names.at(r, "Name")));
  if (nm.empty()) nm = h.zooAnimalBreed.empty() ? h.zooAnimalSpecies : h.zooAnimalBreed;
  h.zooAnimalName = nm;
  pronounOtherSet_ = true;
  pronounOtherFemale_ = h.zooAnimalFemale;
}

void Sim::ensureZooFields(Life& life) {
  Hero& h = life.hero;
  if (!h.zooName.empty()) return;
  const size_t n = pickRow("a16_zoo-random-zoo-names-enc", "Commonness Score", "", "", "", zooRng_);
  h.zooName = n == size_t(-1)
            ? "The City Zoo"
            : resolve(trim(tbl("a16_zoo-random-zoo-names-enc").at(n, "Name")));
  if (ensureTable("a16_zoo-street-names-enc")) {
    const Table& s = tbl("a16_zoo-street-names-enc");
    const size_t r = pickRow("a16_zoo-street-names-enc", "Commonness Score", "", "", "", zooRng_);
    if (r != size_t(-1)) {
      std::string street = resolve(trim(s.at(r, "Street Name")));
      if (s.flag(r, "Add Random Street Type")) street += " Street";
      h.zooStreet = std::to_string(100 + int(zooRng_.below(900))) + " " + street;
    }
  }
  if (ensureTable("a16_zoo-supplier-names-enc")) {
    const size_t r = pickRow("a16_zoo-supplier-names-enc", "Commonness Score", "", "", "", zooRng_);
    if (r != size_t(-1)) h.zooSupplier = resolve(trim(tbl("a16_zoo-supplier-names-enc").at(r, "Name")));
  }
  // The park opens with one habitat, bought off the type table.
  if (ensureTable("a16_zoo-habitat-type-enc")) {
    const Table& t = tbl("a16_zoo-habitat-type-enc");
    const size_t r = pickRow("a16_zoo-habitat-type-enc", "", "", "", "", zooRng_);
    if (r != size_t(-1)) h.zooHabitat = resolve(trim(t.at(r, "Type")));
  }
  if (h.zooHabitat.empty()) h.zooHabitat = "savanna";
  h.zooTicket = 15 + int(zooRng_.below(26));
  h.zooAnimals = 3 + int(zooRng_.below(4));
  h.zooStaff = 2;
  h.zooEngagement = 50 + int(zooRng_.below(21));
  h.zooEmployeeCompetence = 40 + int(zooRng_.below(41));
  h.zooEmployeeFemale = zooRng_.chance(0.5);
  h.zooEmployee = drawFirstName(h.zooEmployeeFemale) + " " + drawSurname();
  h.zooEmployeeRole = "keeper";
  zooBuilt_.clear();
}

void Sim::makeZoo(Life& life) {
  Hero& h = life.hero;
  h.ownsZoo = true;
  hero_ = &h;
  life_ = &life;
  ensureZooFields(life);
  zooDrawAnimal(life);
  h.money -= 25000;   // the park is bought, not inherited
}

std::string Sim::zooToken(const std::string& key) {
  Hero& h = *const_cast<Hero*>(hero_);
  std::string k = key;
  const std::string psuffix = "-possessive";
  bool poss = false;
  if (k.size() > psuffix.size() && k.compare(k.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    poss = true;
    k = k.substr(0, k.size() - psuffix.size());
  }
  const auto cached = [&](const std::string& slot, bool female) -> std::string {
    std::map<std::string, std::string>::iterator it = zooCache_.find(slot);
    if (it != zooCache_.end()) return it->second;
    const std::string nm = drawFirstName(female) + " " + drawSurname();
    zooCache_[slot] = nm;
    return nm;
  };
  // The animal the card is about, with the pronouns pointed at it.
  if (k == "animal-zoo" || k == "zoo-animal" || k == "zoo-animal-other") {
    if (k == "zoo-animal-other") {
      if (zooOther_.empty()) {
        zooOtherFemale_ = zooRng_.chance(0.5);
        zooOther_ = drawFirstName(zooOtherFemale_);
      }
      pronounOtherSet_ = true;
      pronounOtherFemale_ = zooOtherFemale_;
      return poss ? zooPossessive(zooOther_) : zooOther_;
    }
    if (h.zooAnimalName.empty()) zooDrawAnimal(*const_cast<Life*>(life_));
    pronounOtherSet_ = true;
    pronounOtherFemale_ = h.zooAnimalFemale;
    return poss ? zooPossessive(h.zooAnimalName) : h.zooAnimalName;
  }
  if (k == "zoo-name") return poss ? zooPossessive(h.zooName) : h.zooName;
  if (k == "habitat-zoo") {
    if (h.zooHabitat.empty()) ensureZooFields(*const_cast<Life*>(life_));
    if (h.zooHabitat.empty() && ensureTable("a16_zoo-habitat-names-enc")) {
      const size_t r = pickRow("a16_zoo-habitat-names-enc", "Commonness Score", "", "", "", zooRng_);
      if (r != size_t(-1)) h.zooHabitat = resolve(trim(tbl("a16_zoo-habitat-names-enc").at(r, "Habitat Type")));
    }
    return poss ? zooPossessive(h.zooHabitat) : h.zooHabitat;
  }
  // The habitat the current animal belongs in comes off its own bloodline row.
  if (k == "habitat-animal") {
    std::string hab;
    if (ensureTable("a16_zoo-animals-enc")) {
      const Table& t = tbl("a16_zoo-animals-enc");
      for (size_t r = 0; r < t.size(); ++r) {
        if (lower(resolve(trim(t.at(r, "Breed")))) != lower(h.zooAnimalBreed)) continue;
        std::vector<std::string> items;
        splitOn(resolve(trim(t.at(r, "Compatible Habitats"))), ',', items);
        if (!items.empty()) hab = resolve(trim(items[zooRng_.below(uint32_t(items.size()))]));
        break;
      }
    }
    if (hab.empty()) hab = h.zooHabitat;
    return poss ? zooPossessive(hab) : hab;
  }
  if (k == "animal-handler") return poss ? zooPossessive(cached("handler", zooRng_.chance(0.5)))
                                        : cached("handler", zooRng_.chance(0.5));
  if (k == "zoo-veterinarian") {
    const std::string vet = cached("vet", zooRng_.chance(0.5));
    return poss ? zooPossessive(vet) : vet;
  }
  if (k == "zoo-operations-manager") return "zoo operations manager";
  if (k == "zoo-other") {
    const std::string other = cached("other-zoo", false);
    return poss ? zooPossessive(other) : other;
  }
  if (k == "rival-zoo-animal-breed") {
    if (!ensureTable("a16_zoo-animal-names-enc")) return std::string();
    const Table& t = tbl("a16_zoo-animal-names-enc");
    for (int i = 0; i < 8; ++i) {
      const size_t r = pickRow("a16_zoo-animal-names-enc", "Commonness Score", "", "", "", zooRng_);
      if (r == size_t(-1)) break;
      const std::string breed = resolve(trim(t.at(r, "Breed")));
      if (!breed.empty() && lower(breed) != lower(h.zooAnimalBreed)) return breed;
    }
    return h.zooAnimalBreed;
  }
  return std::string();
}

bool Sim::zooYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.ownsZoo || h.age < 18) return false;
  if (!zooRng_.chance(0.50)) return false;
  if (h.zooName.empty()) ensureZooFields(life);
  if (h.zooAnimalName.empty()) zooDrawAnimal(life);

  std::string title, body, line, tableRef, rowRef;
  std::vector<std::string> extra;
  int happy = 0;
  zooCache_.clear();
  zooOther_.clear();
  pronounOtherSet_ = false;
  auto say = [&body](const std::string& s) {
    if (!s.empty()) body += body.empty() ? s : " " + s;
  };
  auto tell = [&line](const std::string& s) {
    if (!s.empty()) line += line.empty() ? s : " " + s;
  };
  const auto cell = [this](const Table& t, size_t r, const char* col) {
    return t.has(col) ? resolve(trim(t.at(r, col))) : std::string();
  };
  // A zoo log line quotes a cell of its own row ("<rb>", the treatment price),
  // so every bracketed cell is resolved before the line itself is written.
  const auto warm = [this](const Table& t, size_t r, const char* keep) {
    const std::vector<std::string>& cols = t.columns();
    for (size_t c = 0; c < cols.size(); ++c) {
      if (keep && cols[c] == keep) continue;
      if (t.at(r, cols[c]).find('<') != std::string::npos) resolve(t.at(r, cols[c]));
    }
  };
  const auto drawRow = [&](const char* name, const char* weight, const char* fcol, const char* fval, const char* flag) {
    const size_t r = pickRow(name, weight ? weight : "", fcol ? fcol : "", fval ? fval : "",
                             flag ? flag : "", zooRng_);
    if (r != size_t(-1)) { tableRef = name; rowRef = std::to_string(r); }
    return r;
  };
  const auto drawWin = [&](const char* name, const char* weight, const char* fcol, const char* fval,
                           const char* lo, const char* hi, double value) {
    const size_t r = pickWindow(name, weight ? weight : "", fcol ? fcol : "", fval ? fval : "",
                                lo ? lo : "", hi ? hi : "", value, zooRng_);
    if (r != size_t(-1)) { tableRef = name; rowRef = std::to_string(r); }
    return r;
  };
  // The park speaks about the animal on the card, so the pronouns follow it.
  const auto aboutAnimal = [&]() {
    pronounOtherSet_ = true;
    pronounOtherFemale_ = h.zooAnimalFemale;
  };
  const std::string species = lower(h.zooAnimalSpecies);

  // The year's costs come off the books before anything else happens.
  const long long upkeep = static_cast<long long>(h.zooAnimals) * 2500
                         + static_cast<long long>(h.zooAttractions) * 5000;
  h.money -= upkeep;
  h.zooEngagement = h.zooEngagement > 1 ? h.zooEngagement - 1 : 0;

  // 1. An animal dies. The table's rows are either species-bound or open.
  if (zooRng_.chance(0.05)) {
    const size_t r = drawRow("a16_zoo-animal-death-scenarios-enc", "Commonness Score", "Animal", species.c_str(), 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_zoo-animal-death-scenarios-enc");
      const std::string type = cell(t, r, "Type");
      title = "A death at the zoo";
      aboutAnimal();
      warm(t, r, "Sentence");
      say(spyUpperFirst(cell(t, r, "Sentence")));
      if (type == "Disease") {
        const size_t d = drawRow("a16_zoo-animal-diseases-enc", "Commonness Score", "Animal", species.c_str(), 0);
        if (d != size_t(-1))
          extra.push_back("The vet put it down to " + lower(resolve(trim(tbl("a16_zoo-animal-diseases-enc").at(d, "Disease")))) + ".");
      } else if (type == "Euthanized") {
        extra.push_back("There was nothing left to try, so the vet stepped in.");
      }
      if (h.zooAnimals > 0) h.zooAnimals -= 1;
      h.zooEngagement = h.zooEngagement > 3 ? h.zooEngagement - 3 : 0;
      happy -= 4;
      h.zooAnimalName.clear();     // the park draws another one next year
    }
  }
  // 2. An animal gets at somebody.
  else if (zooRng_.chance(0.06)) {
    const size_t r = drawRow("a16_zoo-animal-attack-actions-enc", "Commonness Score", "Animal", species.c_str(), 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_zoo-animal-attack-actions-enc");
      title = "An incident";
      aboutAnimal();
      say("A visitor was " + lower(resolve(trim(t.at(r, "Past Verb")))) + " by " + h.zooAnimalName +
          " in the " + lower(h.zooHabitat) + ".");
      extra.push_back("The report graded the damage " + std::to_string(int(t.num(r, "Damage Score"))) + " out of 5.");
      h.zooEngagement = h.zooEngagement > 4 ? h.zooEngagement - 4 : 0;
      happy -= 2;
    }
  }
  // 3. Something the animal caught; treatment, or the organ it destroys.
  else if (zooRng_.chance(0.07)) {
    const size_t r = drawRow("a16_zoo-animal-diseases-enc", "Commonness Score", "Animal", species.c_str(), 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_zoo-animal-diseases-enc");
      const std::string disease = lower(resolve(trim(t.at(r, "Disease"))));
      const bool fatal = t.flag(r, "Can Be Fatal");
      title = "The vet's rounds";
      aboutAnimal();
      say(h.zooAnimalName + " came down with " + withArticle(disease, t.flag(r, "Uses Indefinite Article")) + disease + ".");
      const int impact = int(t.num(r, "Health Impact Score"));
      h.zooAnimalHealth -= impact * 4;
      if (t.flag(r, "Euthanize") && fatal && zooRng_.chance(0.5)) {
        extra.push_back("It was decided that " + h.zooAnimalName + " would not recover.");
        if (h.zooAnimals > 0) h.zooAnimals -= 1;
        h.zooAnimalName.clear();
        h.zooEngagement = h.zooEngagement > 2 ? h.zooEngagement - 2 : 0;
        happy -= 3;
      } else {
        const long long cost = static_cast<long long>(t.num(r, "Treatment Cost Score")) * 1500;
        h.money -= cost;
        extra.push_back("The treatment came to " + money(cost) + ".");
        if (t.num(r, "Cures Organically Score") > 0)
          extra.push_back("It would have cleared up on its own, but the vet was not taking the chance.");
      }
    }
  }
  // 4. An offer in the car park: traffickers sell what the zoo cannot buy.
  else if (zooRng_.chance(0.07)) {
    const size_t p = drawRow("a16_zoo-animal-trafficker-persons-enc", "Commonness Score", 0, 0, 0);
    const size_t a = drawRow("a16_zoo-animals-enc", "", 0, 0, "Rare");
    if (p != size_t(-1) && a != size_t(-1)) {
      const Table& pt = tbl("a16_zoo-animal-trafficker-persons-enc");
      const Table& at = tbl("a16_zoo-animals-enc");
      const std::string who = lower(resolve(trim(pt.at(p, "Person Type"))));
      const std::string got = lower(resolve(trim(at.at(a, "Breed"))));
      const long long price = 5000 + static_cast<long long>(zooRng_.below(25000));
      title = "An offer in the car park";
      const std::string whoPhrase = spyUpperFirst(withArticle(who, pt.flag(p, "Uses Indefinite Article")) + who);
      body = whoPhrase + " offered me " + withArticle(got, true) + got + " for " + money(price) + ".";
      if (h.money >= price) {
        h.money -= price;
        h.zooAnimals += 1;
        h.zooEngagement += 3;
        say("I paid in cash and had it moved in overnight.");
        zooDrawAnimal(life);
      } else {
        extra.push_back("I did not have the money, and the offer walked.");
        happy -= 1;
      }
    }
  }
  // 5. A sale: the buyer either takes the animal or walks away from it.
  else if (zooRng_.chance(0.08)) {
    const size_t b = drawRow("a16_zoo-buyer-person-types-enc", "Commonness Score", 0, 0, 0);
    if (b != size_t(-1)) {
      const Table& bt = tbl("a16_zoo-buyer-person-types-enc");
      const std::string buyer = lower(resolve(trim(bt.at(b, "Description"))));
      const long long value = static_cast<long long>(bt.num(b, "Money Score")) * 12000
                            + static_cast<long long>(zooRng_.below(8000));
      aboutAnimal();
      if (zooRng_.chance(0.25)) {
        const size_t rr = drawRow("a16_zoo-trade-rejection-reasons-enc", "Commonness Score", "Reason", "animal", 0);
        title = "A sale falls through";
        body = "The " + buyer + " came to look at " + h.zooAnimalName + " and left without a deal.";
        if (rr != size_t(-1)) say(spyUpperFirst(cell(tbl("a16_zoo-trade-rejection-reasons-enc"), rr, "Snippet")));
        h.zooEngagement = h.zooEngagement > 2 ? h.zooEngagement - 2 : 0;
      } else {
        const size_t sold = drawWin("a16_zoo-animal-sold-color-enc", "Commonness Score", "", "",
                                    "Min Happiness", "Max Happiness", h.zooAnimalHappiness);
        const size_t ok = drawRow("a16_zoo-trade-animal-sold-color-enc", "Commonness Score", 0, 0, 0);
        title = "A sale";
        body = "A " + buyer + " paid " + money(value) + " for " + h.zooAnimalName + ".";
        if (sold != size_t(-1)) say(spyUpperFirst(cell(tbl("a16_zoo-animal-sold-color-enc"), sold, "Snippet")));
        if (ok != size_t(-1)) tell("The buyer promised " + resolve(trim(tbl("a16_zoo-trade-animal-sold-color-enc").at(ok, "Snippet"))) + ".");
        h.money += value;
        if (h.zooAnimals > 0) h.zooAnimals -= 1;
        happy += 2;
        h.zooAnimalName.clear();
      }
    }
  }
  // 6. A fundraiser: the reception follows how well the park is thought of.
  else if (zooRng_.chance(0.08)) {
    const size_t r = drawRow("a16_zoo-fundraiser-enc", "Commonness Score", 0, 0, 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_zoo-fundraiser-enc");
      const std::string ev = lower(resolve(trim(t.at(r, "Event Type"))));
      const long long cost = static_cast<long long>(t.num(r, "Cost"));
      h.money -= cost;
      title = "A fundraiser";
      body = "The zoo held " + withArticle(ev, true) + ev + ".";
      const size_t f = drawWin("a16_zoo-fundraiser-color-enc", "", "Event Type", ev.c_str(),
                               "Min Reception", "Max Reception", h.zooEngagement);
      if (f != size_t(-1))
        say(spyUpperFirst(spySwap(cell(tbl("a16_zoo-fundraiser-color-enc"), f, "Snippet"), "your ", "my ")));
      const long long raised = static_cast<long long>(t.num(r, "Money Score")) * 6000;
      h.money += raised;
      extra.push_back("The event raised " + money(raised) + " against a bill of " + money(cost) + ".");
      h.zooEngagement += int(t.num(r, "Appeal Score"));
      happy += 2;
    }
  }
  // 7. A show, unless the animal is on the show table's blacklist.
  else if (zooRng_.chance(0.09)) {
    const size_t st = drawRow("a16_zoo-animal-show-enc", "Commonness Score", 0, 0, 0);
    if (st != size_t(-1) && ensureTable("a16_zoo-animal-show-feedback-enc")) {
      const Table& ft = tbl("a16_zoo-animal-show-feedback-enc");
      const std::string show = lower(resolve(trim(tbl("a16_zoo-animal-show-enc").at(st, "Show Type"))));
      std::vector<size_t> pool;
      for (size_t i = 0; i < ft.size(); ++i) {
        if (lower(resolve(trim(ft.at(i, "Show Type")))) != show) continue;
        if (zooInList(lower(resolve(trim(ft.at(i, "Animal Blacklist")))), h.zooAnimalSpecies)) continue;
        pool.push_back(i);
      }
      if (!pool.empty()) {
        const size_t r = pool[zooRng_.below(uint32_t(pool.size()))];
        title = "A show";
        body = "The zoo put on " + withArticle(show, true) + show + " in the " + lower(h.zooHabitat) + ".";
        aboutAnimal();
        say(spyUpperFirst(spySwap(cell(ft, r, "String"), "your ", "my ")));
        if (ft.flag(r, "Positive")) { h.zooEngagement += 3; happy += 2; }
        else { h.zooEngagement = h.zooEngagement > 2 ? h.zooEngagement - 2 : 0; happy -= 1; }
        tableRef = "a16_zoo-animal-show-feedback-enc";
        rowRef = std::to_string(r);
      }
    }
  }
  // 8. Something new for the patrons to queue for, or the end of one.
  else if (zooRng_.chance(0.09)) {
    if (ensureTable("a16_zoo-attractions-enc")) {
      const Table& at = tbl("a16_zoo-attractions-enc");
      const bool tearDown = !zooBuilt_.empty() && zooRng_.chance(0.35);
      if (tearDown) {
        const std::string gone = zooBuilt_[zooRng_.below(uint32_t(zooBuilt_.size()))];
        int score = 0;
        for (size_t i = 0; i < at.size(); ++i)
          if (lower(resolve(trim(at.at(i, "Attraction Name")))) == lower(gone)) score = int(at.num(i, "Engagement Score"));
        const size_t r = drawWin("a16_zoo-attraction-removal-color-enc", "Commonness Score", "", "",
                                 "Min Engagement", "Max Engagement", h.zooEngagement);
        title = "Tearing it down";
        body = "I had the " + lower(gone) + " removed from the park.";
        if (r != size_t(-1)) say(spyUpperFirst(cell(tbl("a16_zoo-attraction-removal-color-enc"), r, "Snippet")));
        for (size_t i = 0; i < zooBuilt_.size(); ++i)
          if (zooBuilt_[i] == gone) { zooBuilt_.erase(zooBuilt_.begin() + long(i)); break; }
        if (h.zooAttractions > 0) h.zooAttractions -= 1;
        h.zooEngagement -= score / 2;
        happy -= 1;
      } else {
        std::vector<size_t> pool;
        for (size_t i = 0; i < at.size(); ++i) {
          bool have = false;
          for (const std::string& built : zooBuilt_)
            if (lower(built) == lower(resolve(trim(at.at(i, "Attraction Name"))))) have = true;
          if (!have) pool.push_back(i);
        }
        if (!pool.empty()) {
          const size_t r = pool[zooRng_.below(uint32_t(pool.size()))];
          const std::string name = lower(resolve(trim(at.at(r, "Attraction Name"))));
          const long long cost = static_cast<long long>(at.num(r, "Installation Cost"));
          title = "A new attraction";
          if (h.money >= cost) {
            h.money -= cost;
            zooBuilt_.push_back(resolve(trim(at.at(r, "Attraction Name"))));
            h.zooAttractions += 1;
            h.zooEngagement += int(at.num(r, "Engagement Score"));
            body = "I had " + withArticle(name, true) + name + " installed for " + money(cost) + ".";
            const size_t f = drawRow("a16_zoo-attraction-installation-color-enc", "Commonness Score", 0, 0, 0);
            if (f != size_t(-1))
              say(spyUpperFirst(cell(tbl("a16_zoo-attraction-installation-color-enc"), f, "Snippet")));
            happy += 2;
          } else {
            body = "The " + name + " would cost " + money(cost) + ", which the zoo does not have.";
            happy -= 1;
          }
          tableRef = "a16_zoo-attractions-enc";
          rowRef = std::to_string(r);
        }
      }
    }
  }
  // 9. The till in the gift shop.
  else if (zooRng_.chance(0.09)) {
    const size_t r = drawRow("a16_zoo-gift-shop-item-enc", "Popularity Score", 0, 0, 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_zoo-gift-shop-item-enc");
      std::string plural = lower(resolve(trim(t.at(r, "Plural"))));
      if (plural.empty()) plural = lower(resolve(trim(t.at(r, "Product")))) + "s";
      const int units = 40 + int(zooRng_.below(400));
      const long long each = static_cast<long long>(t.num(r, "Price Per Unit"));
      const long long take = each * units;
      title = "The gift shop";
      body = "The gift shop moved " + std::to_string(units) + " " + plural + " this year at " + money(each) + " each.";
      h.money += take;
      say("That is " + money(take) + " over the counter.");
      h.zooEngagement += 1;
    }
  }
  // 10. A keeper arrives, or one of them hands in their notice.
  else if (zooRng_.chance(0.10)) {
    if (h.zooStaff > 0 && zooRng_.chance(0.35)) {
      const size_t r = drawWin("a16_zoo-employee-resignation-color-enc", "Commonness Score", "", "",
                               "Min Competence", "Max Competence", double(h.zooEmployeeCompetence));
      title = "A resignation";
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_zoo-employee-resignation-color-enc");
        pronounOtherSet_ = true;
        pronounOtherFemale_ = h.zooEmployeeFemale;
        body = h.zooEmployee + ", the zoo's " + lower(h.zooEmployeeRole) + ", is leaving.";
        say(spyUpperFirst(cell(t, r, "String")));
        const std::string role = lower(resolve(trim(t.at(r, "Employee Type"))));
        if (!role.empty()) extra.push_back("The " + role + " post is open again.");
      } else {
        body = h.zooEmployee + ", the zoo's " + lower(h.zooEmployeeRole) + ", is leaving.";
      }
      h.zooStaff -= 1;
      h.zooEmployee.clear();
      happy -= 1;
    } else {
      const int want = 30 + int(zooRng_.below(61));
      const size_t r = drawWin("a16_zoo-employee-application-color-enc", "Commonness Score", "", "",
                               "Min Competence", "Max Competence", double(want));
      title = "Hiring";
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_zoo-employee-application-color-enc");
        std::string role = lower(resolve(trim(t.at(r, "Employee Type"))));
        if (role.empty()) role = "keeper";
        h.zooEmployeeRole = role;
        h.zooEmployeeCompetence = want;
        h.zooEmployeeFemale = zooRng_.chance(0.5);
        h.zooEmployee = drawFirstName(h.zooEmployeeFemale) + " " + drawSurname();
        pronounOtherSet_ = true;
        pronounOtherFemale_ = h.zooEmployeeFemale;
        body = "I hired " + h.zooEmployee + " as a " + role + ".";
        say(spyUpperFirst(cell(t, r, "String")));
        h.zooStaff += 1;
        if (zooRng_.chance(0.40)) {
          const size_t k = drawWin("a16_zoo-employee-background-check-color-enc", "Commonness Score", "", "",
                                   "Min Competence", "Max Competence", double(want));
          if (k != size_t(-1))
            tell(spyUpperFirst(cell(tbl("a16_zoo-employee-background-check-color-enc"), k, "String")));
        }
        happy += 1;
      } else {
        body = "Nobody worth the wage came in for an interview.";
      }
    }
  }
  // 11. A feature for the habitat the animal lives in.
  else if (zooRng_.chance(0.10)) {
    const size_t r = drawRow("a16_zoo-habitat-features-enc", "Commonness Score", "Compatible Habitat", lower(h.zooHabitat).c_str(), 0);
    if (r == size_t(-1)) {
      // The row's habitat cell holds a list, so a pickRow miss is expected.
      const size_t any = drawRow("a16_zoo-habitat-features-enc", "Commonness Score", 0, 0, 0);
      if (any != size_t(-1)) {
        const Table& t = tbl("a16_zoo-habitat-features-enc");
        const std::string feat = lower(resolve(trim(t.at(any, "Type"))));
        const long long cost = static_cast<long long>(t.num(any, "Cost"));
        title = "Improving the " + lower(h.zooHabitat);
        if (h.money >= cost) {
          h.money -= cost;
          body = "I had " + withArticle(feat, true) + feat + " put in for " + money(cost) + ".";
          // A feature is judged by the feedback table's own lists.
          if (ensureTable("a16_zoo-habitat-feedback-enc")) {
            const Table& ft = tbl("a16_zoo-habitat-feedback-enc");
            size_t fb = size_t(-1);
            for (size_t i = 0; i < ft.size(); ++i) {
              if (zooInList(ft.at(i, "Feature Whitelist"), feat) || zooInList(ft.at(i, "Feature Blacklist"), feat)) {
                fb = i;
                if (zooInList(ft.at(i, "Feature Whitelist"), feat)) break;
              }
            }
            if (fb != size_t(-1)) {
              aboutAnimal();
              say(spyUpperFirst(cell(ft, fb, "String")));
              if (ft.flag(fb, "Is Positive")) { h.zooAnimalHappiness += int(t.num(any, "Happiness Impact Score")); h.zooEngagement += 1; }
              else h.zooAnimalHappiness -= int(t.num(any, "Happiness Impact Score"));
              tableRef = "a16_zoo-habitat-feedback-enc";
              rowRef = std::to_string(fb);
            }
          }
          happy += 1;
        } else {
          body = withArticle(feat, true) + feat + " would cost " + money(cost) + ", which the zoo does not have.";
          happy -= 1;
        }
        tableRef = "a16_zoo-habitat-features-enc";
        rowRef = std::to_string(any);
      }
    } else {
      const Table& t = tbl("a16_zoo-habitat-features-enc");
      const std::string feat = lower(resolve(trim(t.at(r, "Type"))));
      const long long cost = static_cast<long long>(t.num(r, "Cost"));
      title = "Improving the " + lower(h.zooHabitat);
      if (h.money >= cost) {
        h.money -= cost;
        body = "I had " + withArticle(feat, true) + feat + " put in for " + money(cost) + ".";
        h.zooAnimalHappiness += int(t.num(r, "Happiness Impact Score"));
        h.zooEngagement += 1;
        happy += 1;
      } else {
        body = withArticle(feat, true) + feat + " would cost " + money(cost) + ", which the zoo does not have.";
        happy -= 1;
      }
      tableRef = "a16_zoo-habitat-features-enc";
      rowRef = std::to_string(r);
    }
  }
  // 12. A new animal moves in, and the table says how it takes to the place.
  else if (zooRng_.chance(0.10)) {
    if (ensureTable("a16_zoo-habitat-move-in-feedback-enc")) {
      const Table& t = tbl("a16_zoo-habitat-move-in-feedback-enc");
      std::vector<size_t> pool;
      for (size_t i = 0; i < t.size(); ++i) {
        const std::string types = resolve(trim(t.at(i, "Animal Type")));
        const std::string breeds = resolve(trim(t.at(i, "Breed")));
        if (!types.empty() && !zooInList(types, h.zooAnimalSpecies)) continue;
        if (!breeds.empty() && !zooInList(breeds, h.zooAnimalBreed)) continue;
        pool.push_back(i);
      }
      if (!pool.empty()) {
        const size_t r = pool[zooRng_.below(uint32_t(pool.size()))];
        title = "A new arrival";
        body = h.zooAnimalName + " moved into the " + lower(h.zooHabitat) + ".";
        aboutAnimal();
        say(spyUpperFirst(spySwap(cell(t, r, "String"), "your ", "my ")));
        h.zooAnimals += 1;
        h.zooEngagement += 2;
        happy += 1;
        tableRef = "a16_zoo-habitat-move-in-feedback-enc";
        rowRef = std::to_string(r);
      }
    }
  }
  // 13. Housekeeping, which is most of what running a zoo is.
  else if (zooRng_.chance(0.12)) {
    const size_t r = drawRow("a16_zoo-habitat-clean-enc", "Commonness Score", "Habitat Type", lower(h.zooHabitat).c_str(), 0);
    const size_t any = r != size_t(-1) ? r : drawRow("a16_zoo-habitat-clean-enc", "Commonness Score", 0, 0, 0);
    if (any != size_t(-1)) {
      const Table& t = tbl("a16_zoo-habitat-clean-enc");
      title = "Housekeeping";
      aboutAnimal();
      warm(t, any, "Log Version");
      tell(spySwap(cell(t, any, "Log Version"), "your ", "my "));
      say(spySwap(cell(t, any, "Snippet"), "You ", "I "));
      h.zooAnimalHappiness += 1;
      h.zooEngagement += 1;
      tableRef = "a16_zoo-habitat-clean-enc";
      rowRef = std::to_string(any);
    }
  }
  // 14. A treat from the treat table, scored for how much the animal enjoyed it.
  else if (zooRng_.chance(0.12)) {
    const size_t r = drawRow("a16_zoo-animal-treat-color-enc", "", "Animal", species.c_str(), 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_zoo-animal-treat-color-enc");
      const std::string treat = lower(resolve(trim(t.at(r, "Treat"))));
      title = "Treat time";
      aboutAnimal();
      body = "I brought " + h.zooAnimalName + " " + treat + ".";
      h.zooAnimalHappiness += int(t.num(r, "Enjoyment Score"));
      h.zooEngagement += 1;
      happy += 1;
      tableRef = "a16_zoo-animal-treat-color-enc";
      rowRef = std::to_string(r);
    }
  }
  // 15. An hour with the animal, said the way the pastime table says it.
  else if (zooRng_.chance(0.12)) {
    const size_t r = drawRow("a16_zoo-animal-spend-time-color-enc", "", "Animal", species.c_str(), 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_zoo-animal-spend-time-color-enc");
      title = "Time with " + h.zooAnimalName;
      aboutAnimal();
      body = "I " + resolve(trim(t.at(r, "Pastime"))) + " " + h.zooAnimalName + ".";
      h.zooAnimalHappiness += 2;
      happy += 2;
      tableRef = "a16_zoo-animal-spend-time-color-enc";
      rowRef = std::to_string(r);
    }
  }
  // 16. A grooming day, priced and scored by the table.
  else if (zooRng_.chance(0.08)) {
    const size_t r = drawRow("a16_zoo-animal-grooming-methods-enc", "Commonness Score", 0, 0, 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_zoo-animal-grooming-methods-enc");
      const long long cost = static_cast<long long>(t.num(r, "Cost"));
      title = "Grooming";
      aboutAnimal();
      body = "I " + spySentence(spyLowerFirst(cell(t, r, "Feedback Snippet"))) +
             " for " + money(cost) + ".";
      h.money -= cost;
      h.zooAnimalHappiness += 1;
      h.zooEngagement += 1;
      happy += 1;
      tableRef = "a16_zoo-animal-grooming-methods-enc";
      rowRef = std::to_string(r);
    }
  }
  // 17. Something a patron put in the suggestion box.
  else if (zooRng_.chance(0.14)) {
    const size_t r = drawRow("a16_zoo-comment-card-color-enc", "Commonness Score", 0, 0, 0);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_zoo-comment-card-color-enc");
      const std::string wanted = lower(resolve(trim(t.at(r, "Attraction Type"))));
      bool have = wanted.empty();
      for (const std::string& built : zooBuilt_)
        if (lower(built) == wanted) have = true;
      title = "In the suggestion box";
      body = "A patron wrote: " + cell(t, r, "Snippet");
      if (!have) {
        say("The park does not have " + withArticle(wanted, true) + wanted + " yet.");
        h.zooEngagement = h.zooEngagement > 1 ? h.zooEngagement - 1 : 0;
      } else {
        h.zooEngagement += 2;
        happy += 1;
      }
      tableRef = "a16_zoo-comment-card-color-enc";
      rowRef = std::to_string(r);
    }
  }

  // 18. The gate, which pays for the year when nothing else happened.
  if (title.empty() && line.empty() && body.empty()) {
    const long long gate = static_cast<long long>(h.zooTicket)
                         * (h.zooAnimals + h.zooAttractions) * 900;
    h.money += gate;
    h.zooMoney += gate;
    title = "A year at the gate";
    body = "The zoo took " + money(gate) + " with " + std::to_string(h.zooAnimals) +
           " animals on the books, against " + money(upkeep) + " of upkeep.";
    tableRef = "a16_zoo-random-zoo-names-enc";
    rowRef = "0";
  }

  if (title.empty() && line.empty() && body.empty()) return false;
  out.table = tableRef.empty() ? "a16_zoo-random-zoo-names-enc" : tableRef;
  out.row_id = rowRef.empty() ? std::string("0") : rowRef;
  out.title = title;
  out.body = body;
  out.narrative = line;
  out.extra = extra;
  out.happiness += happy;
  return true;
}

// ---------------------------------------------------------------------------
// The rental property (REPORT.en.md 66). One house, one tenant, and the tables
// the landlord family keeps for both of them.
// ---------------------------------------------------------------------------
static std::string llPossessive(const std::string& name) {
  if (name.empty()) return name;
  return name[name.size() - 1] == 's' ? name + "'" : name + "'s";
}

static bool llInList(const std::string& list, const std::string& value) {
  const std::string want = lower(trim(value));
  if (want.empty()) return false;
  std::string item;
  for (size_t i = 0; i <= list.size(); ++i) {
    if (i == list.size() || list[i] == ',') {
      if (lower(trim(item)) == want) return true;
      item.clear();
    } else {
      item += list[i];
    }
  }
  return false;
}

std::string Sim::landlordToken(const std::string& key) {
  if (!hero_) return std::string();
  Hero& h = *const_cast<Hero*>(hero_);
  const std::string psuffix = "-possessive";
  std::string base = key;
  bool own = false;
  if (key.size() > psuffix.size() &&
      key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    base = key.substr(0, key.size() - psuffix.size());
    own = true;
  }
  auto person = [&](std::string& slot, bool& female) -> std::string {
    if (slot.empty()) {
      female = landlordRng_.chance(0.5);
      slot = drawFirstName(female) + " " + drawSurname();
    }
    return own ? llPossessive(slot) : slot;
  };
  if (base == "tenant") return person(h.tenantName, h.tenantFemale);
  if (base == "squatter") return person(llSquatter_, llSquatterFemale_);
  if (base == "address" || base == "tenant-address" || base == "squatter-address") return h.propAddress;
  if (base == "property-city") return h.city;
  if (base == "house-type") {
    if (h.propType.empty()) {
      const size_t row = pickRow("a16_house-types-enc", "Commonness Score", "", "", "", landlordRng_);
      if (row != size_t(-1)) h.propType = trim(tbl("a16_house-types-enc").at(row, "Type"));
    }
    return h.propType;
  }
  return std::string();
}

void Sim::ensureLandlordFields(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (h.propType.empty() && ensureTable("a16_house-types-enc")) {
    const size_t row = pickRow("a16_house-types-enc", "Commonness Score", "", "", "", landlordRng_);
    if (row != size_t(-1)) {
      const Table& types = tbl("a16_house-types-enc");
      h.propType = trim(types.at(row, "Type"));
      h.propHaunted = int(types.num(row, "Haunted Score"));
      const double price = types.num(row, "Price Score");
      h.propValue = static_cast<long long>((price > 0 ? price : 2.0) * 60000.0) +
                    static_cast<long long>(landlordRng_.below(20000));
      const double blo = types.num(row, "Minimum Bedrooms"), bhi = types.num(row, "Maximum Bedrooms");
      h.propBeds = int(blo + (bhi > blo ? (bhi - blo) * landlordRng_.unit() : 1.0));
      const double alo = types.num(row, "Minimum Bathrooms"), ahi = types.num(row, "Maximum Bathrooms");
      h.propBaths = int(alo + (ahi > alo ? (ahi - alo) * landlordRng_.unit() : 1.0));
    }
  }
  if (h.propType.empty()) {
    h.propType = "bungalow";
    h.propHaunted = 1;
    h.propValue = 150000;
    h.propBeds = 2;
    h.propBaths = 1;
  }
  if (h.propBeds <= 0) h.propBeds = 2;
  if (h.propBaths <= 0) h.propBaths = 1;
  if (h.propCondition <= 0) h.propCondition = 70 + int(landlordRng_.below(31));
  if (h.propAddress.empty()) {
    std::string street = "Main";
    if (ensureTable("a16_street-names-enc")) {
      const size_t row = pickRow("a16_street-names-enc", "Commonness Score", "", "", "", landlordRng_);
      if (row != size_t(-1)) {
        const Table& names = tbl("a16_street-names-enc");
        street = resolve(trim(names.at(row, "Street Name")));
        if (names.has("Add Random Street Type") && names.flag(row, "Add Random Street Type"))
          street += " Street";
      }
    }
    h.propAddress = std::to_string(100 + int(landlordRng_.below(900))) + " " + street;
  }
  if (h.propRent <= 0) h.propRent = std::max(100, int(h.propValue / 1500) * 10);
}

void Sim::landlordDrawTenant(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.tenantFemale = landlordRng_.chance(0.5);
  h.tenantName = drawFirstName(h.tenantFemale) + " " + drawSurname();
  h.tenantJob.clear();
  h.tenantIncome = 0;
  const std::string want = h.tenantFemale ? "female" : "male";
  if (ensureTable("a16_landlord-tenant-employment-check-enc")) {
    const Table& jobs = tbl("a16_landlord-tenant-employment-check-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < jobs.size(); ++r) {
      const std::string g = lower(trim(jobs.at(r, "Gender")));
      if (!g.empty() && g != want) continue;  // a blank Gender fits anyone
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t row = pool[landlordRng_.below(uint32_t(pool.size()))];
      h.tenantJob = lower(trim(jobs.at(row, "Career")));
      const double lo = jobs.num(row, "Min Income"), hi = jobs.num(row, "Max Income");
      h.tenantIncome = int(lo + (hi > lo ? (hi - lo) * landlordRng_.unit() : 0.0));
      const double clo = jobs.num(row, "Min Craziness"), chi = jobs.num(row, "Max Craziness");
      h.tenantCraziness = int(clo + (chi > clo ? (chi - clo) * landlordRng_.unit() : 10.0));
    }
  }
  if (h.tenantJob.empty()) h.tenantJob = "unemployed";
  h.tenantSatisfaction = 55 + int(landlordRng_.below(21));
  h.tenantSquatting = false;
  h.tenantYears = 0;
}

void Sim::makeLandlord(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.landlord = true;
  h.propCity = h.city;
  ensureLandlordFields(life);
  landlordDrawTenant(life);
  h.money -= 20000;  // the deposit and the paperwork
  if (h.money < 0) h.money = 0;
  h.miscMask |= 1u << 3;
}

// A year of the rental business. One event fires, and the quieter branches
// fall through to the portfolio line so a landlord year is never silent.
bool Sim::landlordYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!h.landlord || h.age < 18) return false;
  if (!landlordRng_.chance(0.55)) return false;
  ensureLandlordFields(life);

  std::string line;
  std::string body;
  std::string title;
  std::vector<std::string> extra;
  std::string tableRef;
  std::string rowRef;
  int happy = 0;

  auto say = [&](const std::string& text) {
    if (text.empty()) return;
    line += (line.empty() ? std::string() : " ") + text;
  };
  auto tell = [&](const std::string& text) {
    if (text.empty()) return;
    extra.push_back(spyUpperFirst(text));
  };
  auto cell = [this](const Table& t, size_t r, const char* col) -> std::string {
    if (r == size_t(-1) || !t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  auto mark = [&](const std::string& name, const Table& t, size_t r) {
    if (!tableRef.empty() || r == size_t(-1)) return;
    tableRef = name;
    rowRef = std::to_string(r);
  };
  // A log line refers back to a bracket drawn in another column of its own row
  // ("<rb>"), so every bracketed cell of that row is resolved before it is read.
  auto warm = [&](const Table& t, size_t r, const char* skip) {
    if (r == size_t(-1)) return;
    for (const std::string& col : t.columns()) {
      if (skip && col == skip) continue;
      if (!t.has(col)) continue;
      const std::string value = t.at(r, col);
      if (value.find('<') != std::string::npos) resolve(value);
    }
  };
  auto poolPick = [&](const Table& t, const std::vector<size_t>& pool, const char* weight) -> size_t {
    if (pool.empty()) return size_t(-1);
    if (!weight || !*weight || !t.has(weight))
      return pool[landlordRng_.below(uint32_t(pool.size()))];
    const size_t row = weightedPickOf(t, weight, pool, landlordRng_);
    return row == size_t(-1) ? pool[landlordRng_.below(uint32_t(pool.size()))] : row;
  };
  auto fitsRooms = [&](const Table& t, size_t r) {
    if (t.has("Requires Single Bedroom") && t.flag(r, "Requires Single Bedroom") && h.propBeds != 1) return false;
    if (t.has("Requires Multiple Bedrooms") && t.flag(r, "Requires Multiple Bedrooms") && h.propBeds < 2) return false;
    if (t.has("Requires Single Bathroom") && t.flag(r, "Requires Single Bathroom") && h.propBaths != 1) return false;
    if (t.has("Requires Multiple Bathrooms") && t.flag(r, "Requires Multiple Bathrooms") && h.propBaths < 2) return false;
    return true;
  };
  auto fitsType = [&](const Table& t, size_t r) {
    if (t.has("House Type Whitelist") && !trim(t.at(r, "House Type Whitelist")).empty() &&
        !llInList(t.at(r, "House Type Whitelist"), h.propType)) return false;
    if (t.has("House Type Blacklist") && llInList(t.at(r, "House Type Blacklist"), h.propType)) return false;
    return true;
  };
  auto allRows = [&](const Table& t) {
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    return pool;
  };

  if (h.propCondition > 0) --h.propCondition;
  if (h.tenantSatisfaction < 50) h.tenantSatisfaction = std::min(100, h.tenantSatisfaction + 2);
  else if (h.tenantSatisfaction > 50) h.tenantSatisfaction = std::max(0, h.tenantSatisfaction - 2);
  if (!h.tenantName.empty()) ++h.tenantYears;

  if (h.tenantSquatting) {
    const size_t row = pickRow("a16_landlord-squatter-push-scenario-enc", "Commonness Score", "", "", "",
                               landlordRng_);
    if (row != size_t(-1)) {
      const Table& t = tbl("a16_landlord-squatter-push-scenario-enc");
      warm(t, row, "Log Version");
      title = "The squatter";
      say(cell(t, row, "Log Version"));
      mark("a16_landlord-squatter-push-scenario-enc", t, row);
      if (landlordRng_.chance(0.55)) {
        tell(llSquatter_ + " finally cleared out of " + h.propAddress + ".");
        h.tenantName.clear();
        h.tenantJob.clear();
        h.tenantIncome = 0;
        h.tenantCraziness = 20;
        h.tenantSatisfaction = 55;
        h.tenantYears = 0;
        h.tenantSquatting = false;
      } else {
        tell("The police said it was a civil matter.");
        h.tenantSatisfaction = std::max(0, h.tenantSatisfaction - 4);
      }
      happy -= 2;
    }
  } else if (h.tenantName.empty()) {
    landlordDrawTenant(life);
    title = "A new tenant";
    say(h.tenantName + " moved into " + h.propAddress + ".");
    tell(h.tenantName + " works as a " + h.tenantJob + " and earns $" +
         std::to_string(h.tenantIncome) + " a year.");
    if (ensureTable("a16_landlord-background-check-color-enc") && landlordRng_.chance(0.45)) {
      const size_t row = pickRow("a16_landlord-background-check-color-enc", "Commonness Score", "", "", "",
                                 landlordRng_);
      if (row != size_t(-1)) {
        const Table& t = tbl("a16_landlord-background-check-color-enc");
        tell("The background check came back: " + lower(trim(t.at(row, "Crime"))) + ".");
        h.tenantCraziness = std::min(100, h.tenantCraziness + 15);
      }
    } else {
      tell("The background check came back clean.");
    }
  } else if (landlordRng_.chance(0.05) && ensureTable("a16_landlord-natural-disaster-scenario-enc")) {
    const Table& t = tbl("a16_landlord-natural-disaster-scenario-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r)
      if (!t.has("House Type Blacklist") || !llInList(t.at(r, "House Type Blacklist"), h.propType))
        pool.push_back(r);
    const size_t row = poolPick(t, pool, "");
    if (row != size_t(-1)) {
      title = trim(t.at(row, "Type"));
      say(cell(t, row, "Scenario"));
      mark("a16_landlord-natural-disaster-scenario-enc", t, row);
      const int impact = int(t.num(row, "Impact Score"));
      h.propCondition = std::max(0, h.propCondition - impact * 8);
      h.tenantSatisfaction = std::max(0, h.tenantSatisfaction - impact * 4);
      if (t.flag(row, "Can Demolish") && landlordRng_.chance(0.4)) {
        h.propValue = 0;
        h.propCondition = 5;
        tell("The " + h.propType + " was a total loss.");
      } else {
        tell("The " + h.propType + " took $" + std::to_string(impact * 4000) + " of damage.");
      }
      happy -= 3;
    }
  } else if (h.propHaunted >= 3 && landlordRng_.chance(0.05) &&
             ensureTable("a16_landlord-spooked-contractor-quit-color-enc")) {
    const size_t row = pickWindow("a16_landlord-spooked-contractor-quit-color-enc", "Commonness Score", "",
                                  "", "Hauntedness Score", "", double(h.propHaunted), landlordRng_);
    if (row != size_t(-1)) {
      const Table& t = tbl("a16_landlord-spooked-contractor-quit-color-enc");
      warm(t, row, "Log Version");
      title = "The contractor";
      say(cell(t, row, "Log Version"));
      mark("a16_landlord-spooked-contractor-quit-color-enc", t, row);
      h.tenantSatisfaction = std::max(0, h.tenantSatisfaction - 3);
    }
  } else if (h.tenantSatisfaction < 45 && landlordRng_.chance(0.25) &&
             ensureTable("a16_landlord-squatter-reason-color-enc")) {
    const bool renty = h.propRent > 1500;
    const Table& t = tbl("a16_landlord-squatter-reason-color-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r)
      if (t.flag(r, "Rent-Related") == renty) pool.push_back(r);
    const size_t row = poolPick(t, pool, "Commonness Score");
    if (row != size_t(-1)) {
      title = "The rent stopped";
      say(cell(t, row, "Log Version"));
      mark("a16_landlord-squatter-reason-color-enc", t, row);
      llSquatter_ = h.tenantName;
      llSquatterFemale_ = h.tenantFemale;
      h.tenantSquatting = true;
      h.tenantSatisfaction = std::max(0, h.tenantSatisfaction - 6);
      tell(llSquatter_ + " stopped paying rent on the " + h.propType + ".");
      happy -= 3;
    }
  } else if (landlordRng_.chance(0.20) && ensureTable("a16_landlord-maintenance-request-scenario-enc")) {
    const Table& t = tbl("a16_landlord-maintenance-request-scenario-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r)
      if (fitsType(t, r) && fitsRooms(t, r)) pool.push_back(r);
    const size_t row = poolPick(t, pool, "Commonness Score");
    if (row != size_t(-1)) {
      warm(t, row, "Body Snippet");
      const bool agree = landlordRng_.chance(0.65);
      title = "A maintenance request";
      say(spySentence("A maintenance request came in: " + cell(t, row, "Body Snippet")));
      mark("a16_landlord-maintenance-request-scenario-enc", t, row);
      const double lo = t.num(row, "Min Repair Cost"), hi = t.num(row, "Max Repair Cost");
      const long long cost = static_cast<long long>(lo + (hi > lo ? (hi - lo) * landlordRng_.unit() : 0.0));
      if (agree) {
        h.money -= cost;
        h.propCondition = std::min(100, h.propCondition + 2);
        h.tenantSatisfaction = std::min(100, h.tenantSatisfaction + int(t.num(row, "Agree Satisfaction Impact")));
        tell(spySentence("I paid $" + std::to_string(cost) + " to " + lower(cell(t, row, "Feedback Past Snippet"))));
      } else {
        h.tenantSatisfaction = std::max(0, h.tenantSatisfaction + int(t.num(row, "Disagree Satisfaction Impact")));
        tell(h.tenantName + " was not pleased to hear it was their problem.");
        if (t.flag(row, "May Cause Move-Out") && landlordRng_.chance(0.3)) {
          tell(h.tenantName + " gave notice over it.");
          h.tenantName.clear();
          h.tenantJob.clear();
          h.tenantIncome = 0;
          h.tenantCraziness = 20;
          h.tenantSatisfaction = 55;
          h.tenantYears = 0;
        }
      }
      if (agree && !h.tenantName.empty() && landlordRng_.chance(0.25) &&
          ensureTable("a16_landlord-maintenance-refusal-scenario-enc")) {
        const size_t refused = pickWindow("a16_landlord-maintenance-refusal-scenario-enc", "Commonness Score",
                                          "", "", "Min Tenant Craziness", "", double(h.tenantCraziness),
                                          landlordRng_);
        if (refused != size_t(-1)) {
          const Table& rt = tbl("a16_landlord-maintenance-refusal-scenario-enc");
          tell(spySentence(cell(rt, refused, "Log Version")));
          h.tenantSatisfaction = std::max(0, h.tenantSatisfaction - 3);
        }
      }
      happy += agree ? 1 : -1;
    }
  } else if (landlordRng_.chance(0.18) && ensureTable("a16_landlord-tenant-damaged-property-scenario-enc")) {
    const Table& t = tbl("a16_landlord-tenant-damaged-property-scenario-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!fitsType(t, r) || !fitsRooms(t, r)) continue;
      if (t.has("Amenity") && !trim(t.at(r, "Amenity")).empty()) {
        bool have = false;
        for (const std::string& f : h.propFeatures) if (llInList(t.at(r, "Amenity"), f)) have = true;
        if (!have) continue;
      }
      if (t.has("Min Craziness") && t.num(r, "Min Craziness") > 0 &&
          h.tenantCraziness < t.num(r, "Min Craziness")) continue;
      if (t.has("Max Craziness") && t.num(r, "Max Craziness") > 0 &&
          h.tenantCraziness > t.num(r, "Max Craziness")) continue;
      pool.push_back(r);
    }
    if (pool.empty()) pool = allRows(t);
    const size_t row = poolPick(t, pool, "Commonness Score");
    if (row != size_t(-1)) {
      warm(t, row, "Log Version");
      title = "Damage to the " + h.propType;
      say(cell(t, row, "Scenario"));
      mark("a16_landlord-tenant-damaged-property-scenario-enc", t, row);
      const double lo = t.num(row, "Min Repair Cost"), hi = t.num(row, "Max Repair Cost");
      const long long cost = static_cast<long long>(lo + (hi > lo ? (hi - lo) * landlordRng_.unit() : 0.0));
      h.money -= cost;
      h.propCondition = std::max(0, h.propCondition - int(t.num(row, "Impact Score")) * 5);
      h.tenantSatisfaction = std::max(0, h.tenantSatisfaction - 4);
      tell(spySentence(cell(t, row, "Log Version")));
      tell("The repair came to $" + std::to_string(cost) + ".");
      happy -= 2;
    }
  } else if (landlordRng_.chance(0.15) && ensureTable("a16_landlord-tenant-inspection-color-enc")) {
    const bool good = h.tenantSatisfaction >= 60;
    const Table& t = tbl("a16_landlord-tenant-inspection-color-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.flag(r, "Positive") != good) continue;
      if (t.has("Lover Exclusive") && t.flag(r, "Lover Exclusive") && h.loverName.empty()) continue;
      if (t.has("Min Craziness") && t.num(r, "Min Craziness") > 0 &&
          h.tenantCraziness < t.num(r, "Min Craziness")) continue;
      if (t.has("Max Craziness") && t.num(r, "Max Craziness") > 0 &&
          h.tenantCraziness > t.num(r, "Max Craziness")) continue;
      pool.push_back(r);
    }
    if (pool.empty()) pool = allRows(t);
    const size_t row = poolPick(t, pool, "Commonness Score");
    if (row != size_t(-1)) {
      warm(t, row, "Scenario");
      title = good ? "Inspection day" : "A failed inspection";
      say("Inspection day at " + h.propAddress + ".");
      tell(spySentence("What I walked into: " + cell(t, row, "Scenario")));
      mark("a16_landlord-tenant-inspection-color-enc", t, row);
      if (good) {
        h.tenantSatisfaction = std::min(100, h.tenantSatisfaction + 5);
        h.propCondition = std::min(100, h.propCondition + 1);
        happy += 2;
      } else {
        h.tenantSatisfaction = std::max(0, h.tenantSatisfaction - 6);
        if (ensureTable("a16_landlord-tenant-warn-scenario-enc")) {
          const size_t warn = pickRow("a16_landlord-tenant-warn-scenario-enc", "Commonness Score", "", "", "",
                                      landlordRng_);
          if (warn != size_t(-1)) {
            const Table& wt = tbl("a16_landlord-tenant-warn-scenario-enc");
            extra.push_back(toFirstPerson(cell(wt, warn, "Scenario")));
            if (landlordRng_.chance(0.25) && ensureTable("a16_landlord-tenant-beg-to-stay-scenario-enc")) {
              const size_t beg = pickWindow("a16_landlord-tenant-beg-to-stay-scenario-enc", "Commonness Score",
                                            "", "", "Min Craziness", "Max Craziness",
                                            double(h.tenantCraziness), landlordRng_);
              if (beg != size_t(-1))
                extra.push_back(spyUpperFirst(cell(tbl("a16_landlord-tenant-beg-to-stay-scenario-enc"), beg,
                                                   "Log Version")));
            }
          }
        }
        happy -= 3;
      }
    }
  } else if (landlordRng_.chance(0.12) && ensureTable("a16_landlord-tenant-late-rent-color-enc")) {
    const size_t row = pickWindow("a16_landlord-tenant-late-rent-color-enc", "Commonness Score", "", "",
                                  "Min Craziness", "Max Craziness", double(h.tenantCraziness), landlordRng_);
    if (row != size_t(-1)) {
      const Table& t = tbl("a16_landlord-tenant-late-rent-color-enc");
      warm(t, row, "Log Version");
      title = "Late rent";
      say(cell(t, row, "Log Version"));
      mark("a16_landlord-tenant-late-rent-color-enc", t, row);
      if (landlordRng_.chance(0.5) && ensureTable("a16_landlord-tenant-late-rent-insist-enc")) {
        const Table& it = tbl("a16_landlord-tenant-late-rent-insist-enc");
        const size_t ins = poolPick(it, allRows(it), "Commonness Score");
        if (ins != size_t(-1)) extra.push_back(spyUpperFirst(cell(it, ins, "Log Version")));
        if (landlordRng_.chance(0.6)) {
          h.money += static_cast<long long>(h.propRent) * 3;
          tell(h.tenantName + " paid up the arrears.");
        } else {
          tell(h.tenantName + " promised to pay next month.");
        }
        h.tenantSatisfaction = std::max(0, h.tenantSatisfaction - 5);
      } else {
        tell("I let it slide this month.");
        h.tenantSatisfaction = std::min(100, h.tenantSatisfaction + 4);
      }
    }
  } else if (landlordRng_.chance(0.12) && ensureTable("a16_landlord-tenant-beg-lower-rent-scenario-enc")) {
    const size_t row = pickWindow("a16_landlord-tenant-beg-lower-rent-scenario-enc", "Commonness Score", "",
                                  "", "Min Craziness", "Max Craziness", double(h.tenantCraziness),
                                  landlordRng_);
    if (row != size_t(-1)) {
      const Table& t = tbl("a16_landlord-tenant-beg-lower-rent-scenario-enc");
      warm(t, row, "Log Version");
      title = "A discount asked for";
      say(cell(t, row, "Log Version"));
      mark("a16_landlord-tenant-beg-lower-rent-scenario-enc", t, row);
      if (landlordRng_.chance(0.4)) {
        h.propRent = std::max(100, h.propRent * 9 / 10);
        h.tenantSatisfaction = std::min(100, h.tenantSatisfaction + 7);
        tell("I dropped the rent to $" + std::to_string(h.propRent) + " a month.");
      } else {
        h.tenantSatisfaction = std::max(0, h.tenantSatisfaction - 6);
        tell("I told " + h.tenantName + " the rent stands.");
      }
    }
  } else if (landlordRng_.chance(0.10) && ensureTable("a16_landlord-tenant-leave-color-enc")) {
    const Table& t = tbl("a16_landlord-tenant-leave-color-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Requires Haunted House") && t.flag(r, "Requires Haunted House") && h.propHaunted < 3) continue;
      if (t.has("Min Satisfaction") && t.num(r, "Min Satisfaction") > 0 &&
          h.tenantSatisfaction < t.num(r, "Min Satisfaction")) continue;
      if (t.has("Max Satisfaction") && t.num(r, "Max Satisfaction") > 0 &&
          h.tenantSatisfaction > t.num(r, "Max Satisfaction")) continue;
      pool.push_back(r);
    }
    if (pool.empty()) pool = allRows(t);
    const size_t row = poolPick(t, pool, "Commonness Score");
    if (row != size_t(-1)) {
      warm(t, row, "Scenario");
      title = "A tenant leaving";
      say(cell(t, row, "Log Version"));
      mark("a16_landlord-tenant-leave-color-enc", t, row);
      tell(cell(t, row, "Scenario"));
      h.tenantName.clear();
      h.tenantJob.clear();
      h.tenantIncome = 0;
      h.tenantCraziness = 20;
      h.tenantSatisfaction = 55;
      h.tenantYears = 0;
      h.tenantSquatting = false;
    }
  } else if (!h.loverName.empty() && landlordRng_.chance(0.10) &&
             ensureTable("a16_landlord-tenant-lover-nags-hero-enc")) {
    const size_t row = pickWindow("a16_landlord-tenant-lover-nags-hero-enc", "", "", "", "Min Craziness",
                                  "Max Craziness", double(h.loverCraziness), landlordRng_);
    if (row != size_t(-1)) {
      const Table& t = tbl("a16_landlord-tenant-lover-nags-hero-enc");
      warm(t, row, "Log Version");
      title = "A word from home";
      say(cell(t, row, "Log Version"));
      mark("a16_landlord-tenant-lover-nags-hero-enc", t, row);
      tell(cell(t, row, "Scenario"));
      happy -= 1;
    }
  } else if (landlordRng_.chance(0.12) && ensureTable("a16_landlord-amenity-feedback-scenario-enc")) {
    const Table& t = tbl("a16_landlord-amenity-feedback-scenario-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (h.propFeatures.empty()) { pool.push_back(r); continue; }
      for (const std::string& f : h.propFeatures)
        if (llInList(t.at(r, "Amenity"), f)) { pool.push_back(r); break; }
    }
    if (pool.empty()) pool = allRows(t);
    const size_t row = poolPick(t, pool, "Commonness Score");
    if (row != size_t(-1)) {
      warm(t, row, "Log Version");
      title = "Time at the " + h.propType;
      say(cell(t, row, "Log Version"));
      mark("a16_landlord-amenity-feedback-scenario-enc", t, row);
      happy += 2;
    }
  } else if (landlordRng_.chance(0.10) && ensureTable("a16_landlord-home-remodel-feature-enc")) {
    const Table& t = tbl("a16_landlord-home-remodel-feature-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!fitsType(t, r)) continue;
      if (t.has("Country Blacklist") && llInList(t.at(r, "Country Blacklist"), h.country)) continue;
      bool have = false;
      for (const std::string& f : h.propFeatures)
        if (lower(trim(f)) == lower(trim(t.at(r, "Project")))) have = true;
      if (have) continue;
      pool.push_back(r);
    }
    const size_t row = poolPick(t, pool, "Commonness Score");
    if (row != size_t(-1)) {
      const double lo = t.num(row, "Minimum Cost"), hi = t.num(row, "Maximum Cost");
      const long long cost = static_cast<long long>(lo + (hi > lo ? (hi - lo) * landlordRng_.unit() : 0.0));
      const std::string project = lower(trim(t.at(row, "Project")));
      title = "The " + project;
      say("I had a " + project + " put in at " + h.propAddress + " for $" + std::to_string(cost) + ".");
      mark("a16_landlord-home-remodel-feature-enc", t, row);
      h.money -= cost;
      h.propFeatures.push_back(project);
      const double pct = t.num(row, "Value Percentage");
      h.propValue += static_cast<long long>(cost * (pct > 0 ? pct : 0.0) / 100.0);
      tell(cell(t, row, "Secondary Line Text"));
      happy += int(t.num(row, "Enjoyment Score"));
    }
  } else {
    const Table& t = ensureTable("a16_landlord-real-estate-portfolio-enc")
                         ? tbl("a16_landlord-real-estate-portfolio-enc")
                         : tbl("a16_landlord-real-estate-portfolio-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Only Squatters") && t.flag(r, "Only Squatters") && !h.tenantSquatting) continue;
      if (t.has("Single Squatter") && t.flag(r, "Single Squatter") && !h.tenantSquatting) continue;
      if (t.has("Min Satisfaction") && t.num(r, "Min Satisfaction") > 0 &&
          h.tenantSatisfaction < t.num(r, "Min Satisfaction")) continue;
      if (t.has("Max Satisfaction") && t.num(r, "Max Satisfaction") > 0 &&
          h.tenantSatisfaction > t.num(r, "Max Satisfaction")) continue;
      if (t.has("Min Property Condition") && t.num(r, "Min Property Condition") > 0 &&
          h.propCondition < t.num(r, "Min Property Condition")) continue;
      if (t.has("Max Property Condition") && t.num(r, "Max Property Condition") > 0 &&
          h.propCondition > t.num(r, "Max Property Condition")) continue;
      pool.push_back(r);
    }
    if (pool.empty()) pool = allRows(t);
    const size_t row = poolPick(t, pool, "");
    title = "The portfolio";
    if (row != size_t(-1)) {
      // The portfolio line talks about the tenant, so the pronoun pair points at
      // them for the length of the resolve.
      const bool savedSet = pronounOtherSet_;
      const bool savedFemale = pronounOtherFemale_;
      if (!h.tenantName.empty()) { pronounOtherSet_ = true; pronounOtherFemale_ = h.tenantFemale; }
      say(spyUpperFirst(resolve(trim(t.at(row, "String")))));
      pronounOtherSet_ = savedSet;
      pronounOtherFemale_ = savedFemale;
      mark("a16_landlord-real-estate-portfolio-enc", t, row);
    } else {
      say("The rental business rolls on.");
    }
    if (!h.tenantSquatting) {
      const long long collected = static_cast<long long>(h.propRent) * 12;
      h.money += collected;
      h.rentCollected += collected;
      tell("I collected $" + std::to_string(collected) + " in rent this year.");
    } else {
      tell("Nothing came in while the squatter sits there.");
    }
  }

  if (title.empty() && body.empty() && line.empty()) return false;
  out.table = tableRef.empty() ? "a16_landlord-real-estate-portfolio-enc" : tableRef;
  out.row_id = rowRef.empty() ? std::string("0") : rowRef;
  out.title = title.empty() ? ("The " + h.propType) : title;
  out.body = body;
  out.narrative = line;
  out.extra = extra;
  out.happiness += happy;
  return true;
}

// ---- the household pet (REPORT.en.md 67) --------------------------------

// Draws the animal, breed and name of a new pet. The viral, exotic and
// black-market rows of a16_pets-enc are not household pets (REPORT.en.md 67.2).
void Sim::petDraw(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!ensureTable("a16_pets-enc")) return;
  const Table& pets = tbl("a16_pets-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < pets.size(); ++r) {
    if (!trim(pets.at(r, "Wild")).empty() || !trim(pets.at(r, "Exotic")).empty() ||
        !trim(pets.at(r, "Black Market")).empty()) continue;
    pool.push_back(r);
  }
  if (pool.empty()) for (size_t r = 0; r < pets.size(); ++r) pool.push_back(r);
  const size_t row = weightedPickOf(pets, "Commonness Score", pool, petRng_);
  if (row == size_t(-1)) return;
  h.petAnimal = trim(pets.at(row, "Animal"));
  h.petBreed = lower(trim(pets.at(row, "Breed")));
  h.petFemale = petRng_.chance(0.5);
  h.petAge = 0;
  h.petHealth = 95;
  h.petHappiness = 75;
  h.petName.clear();
  h.petCause.clear();
  h.petDead = false;
  h.petVetSpend = 0;
  if (!ensureTable("a16_pet-names-enc")) return;
  const Table& names = tbl("a16_pet-names-enc");
  std::vector<size_t> fits;
  for (size_t r = 0; r < names.size(); ++r) {
    if (lower(trim(names.at(r, "Animal"))) != lower(h.petAnimal)) continue;
    if (names.has("Gender") && !trim(names.at(r, "Gender")).empty() &&
        (lower(trim(names.at(r, "Gender"))) == "female") != h.petFemale) continue;
    fits.push_back(r);
  }
  if (fits.empty()) return;
  h.petName = resolve(trim(names.at(fits[petRng_.below(uint32_t(fits.size()))], "Name")));
}

void Sim::makePet(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.hasPet = true;
  petDraw(life);
}

std::string Sim::petToken(const std::string& key) {
  if (!hero_) return std::string();
  const Hero& h = *hero_;
  std::string k = key;
  const std::string psuffix = "-possessive";
  bool own = false;
  if (k.size() > psuffix.size() &&
      k.compare(k.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    k.resize(k.size() - psuffix.size());
    own = true;
  }
  std::string v;
  if (k == "pet" || k == "pet-name") v = h.petName.empty() ? ("my " + lower(h.petAnimal)) : h.petName;
  else if (k == "breed") v = h.petBreed;
  else if (k == "animal" || k == "animal-type") v = lower(h.petAnimal);
  else if (k == "animal-type-indefinite") v = withArticle(lower(h.petAnimal), true);
  if (own && !v.empty()) v = llPossessive(v);
  return v;
}

bool Sim::petYear(Life& life, AgeUp& out, int forcedChoice) {
  (void)forcedChoice;
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (h.age < 6) return false;
  if (!petRng_.chance(0.55)) return false;
  // A house without a pet only has the adoption card in it (REPORT.en.md 67.3).
  const bool adopting = !h.hasPet;

  std::string line;
  std::string body;
  std::string title;
  std::vector<std::string> extra;
  std::string tableRef;
  std::string rowRef;
  int happy = 0;

  auto say = [&](const std::string& text) {
    if (text.empty()) return;
    line += (line.empty() ? std::string() : " ") + text;
  };
  auto tell = [&](const std::string& text) {
    if (text.empty()) return;
    extra.push_back(spyUpperFirst(text));
  };
  auto cell = [&](const Table& t, size_t r, const char* col) -> std::string {
    if (r == size_t(-1) || !t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  auto mark = [&](const std::string& table, const Table& t, size_t r) {
    (void)t;
    tableRef = table;
    rowRef = std::to_string(r + 1);
  };
  // Every pet table keys on the Animal column, and a row whose Animal is blank
  // fits any pet (REPORT.en.md 67.3).
  auto petPool = [&](const char* table) {
    std::vector<size_t> pool;
    if (!ensureTable(table)) return pool;
    const Table& t = tbl(table);
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Animal") && !trim(t.at(r, "Animal")).empty() &&
          lower(trim(t.at(r, "Animal"))) != lower(h.petAnimal)) continue;
      pool.push_back(r);
    }
    if (pool.empty()) {
      const Table& all = tbl(table);
      for (size_t r = 0; r < all.size(); ++r) pool.push_back(r);
    }
    return pool;
  };
  auto petPick = [&](const char* table, const char* weight) -> size_t {
    const std::vector<size_t> pool = petPool(table);
    if (pool.empty()) return size_t(-1);
    if (weight && *weight) {
      const size_t r = weightedPickOf(tbl(table), weight, pool, petRng_);
      if (r != size_t(-1)) return r;
    }
    return pool[petRng_.below(uint32_t(pool.size()))];
  };
  // The rows that describe the pet itself use its own pronouns, so the pair
  // points at the animal while one of them renders (REPORT.en.md 67.3).
  const bool savedSet = pronounOtherSet_;
  const bool savedFemale = pronounOtherFemale_;
  auto petOn = [&]() { pronounOtherSet_ = true; pronounOtherFemale_ = h.petFemale; };
  auto petOff = [&]() { pronounOtherSet_ = savedSet; pronounOtherFemale_ = savedFemale; };

  if (!adopting) {
    h.petAge += 1;
    h.petHealth = std::max(0, h.petHealth - (h.petAge > 8 ? 8 : 3));
    if (h.petHappiness < 70) h.petHappiness = std::min(100, h.petHappiness + 1);
    else h.petHappiness = std::max(0, h.petHappiness - 3);
  }

  if (adopting && petRng_.chance(0.08)) {
    // The pet shop: the same domestic draw the script flag uses at start-up.
    h.hasPet = true;
    petDraw(life);
    title = "A new friend";
    say("I adopted " + petToken("pet") + ", " + withArticle(lower(h.petBreed), true) + lower(h.petBreed) + ".");
    tell("Welcome to the family.");
    happy += 2;
  } else if (adopting) {
    return false;
  } else if (petRng_.chance(h.petHealth <= 15 ? 0.45 : 0.04) && ensureTable("a16_pet-death-types-enc")) {
    const size_t row = petPick("a16_pet-death-types-enc", "Commonness Score");
    if (row != size_t(-1)) {
      const Table& t = tbl("a16_pet-death-types-enc");
      const bool someoneElse = t.has("Other Person Involved") && t.flag(row, "Other Person Involved");
      petOn();
      std::string sentence = spySwap(trim(t.at(row, "Sentence")), "<pronoun>", h.petFemale ? "she" : "he");
      if (!someoneElse) sentence = spySwap(sentence, " <person>", "");
      say(resolve(sentence));
      petOff();
      title = "The end of " + petToken("pet");
      body = "Cause: " + trim(t.at(row, "Type")) + ".";
      mark("a16_pet-death-types-enc", t, row);
      h.petCause = trim(t.at(row, "Type"));
      h.hasPet = false;
      h.petDead = true;
      happy -= 6;
    }
  } else if (petRng_.chance(0.28) && ensureTable("a16_pet-treats-enc")) {
    const size_t row = petPick("a16_pet-treats-enc", "");
    if (row != size_t(-1)) {
      const Table& t = tbl("a16_pet-treats-enc");
      title = "A treat";
      say("I gave " + petToken("pet") + " " + resolve(trim(t.at(row, "Treat"))) + ".");
      body = "Enjoyment: " + std::to_string(int(t.num(row, "Enjoyment Score"))) + ".";
      mark("a16_pet-treats-enc", t, row);
      h.petHappiness = std::min(100, h.petHappiness + int(t.num(row, "Enjoyment Score")) * 2);
      happy += 1;
    }
  } else if (petRng_.chance(0.25) && ensureTable("a16_pet-pastimes-enc")) {
    const size_t row = petPick("a16_pet-pastimes-enc", "");
    if (row != size_t(-1)) {
      const Table& t = tbl("a16_pet-pastimes-enc");
      title = "Time with " + petToken("pet");
      say("I " + spyLowerFirst(resolve(trim(t.at(row, "Pastime")))) + " " + petToken("pet") + ".");
      mark("a16_pet-pastimes-enc", t, row);
      h.petHappiness = std::min(100, h.petHappiness + 3);
      happy += 1;
    }
  } else if (petRng_.chance(0.12) && ensureTable("a16_pet-disease-types-enc")) {
    const size_t row = petPick("a16_pet-disease-types-enc", "Commonness Score");
    if (row != size_t(-1)) {
      const Table& t = tbl("a16_pet-disease-types-enc");
      const std::string disease = lower(trim(t.at(row, "Disease")));
      const std::string named = t.flag(row, "Uses Indefinite Article")
                                    ? withArticle(disease, true) + disease
                                    : disease;
      title = "A trip to the vet";
      mark("a16_pet-disease-types-enc", t, row);
      h.petHealth = std::max(0, h.petHealth - int(t.num(row, "Health Impact Score")) * 4);
      const bool fatal = t.flag(row, "Can Be Fatal") && h.petHealth <= 20 && petRng_.chance(0.3);
      if (fatal) {
        say(petToken("pet") + " had " + named + " and did not pull through.");
        h.petCause = disease;
        h.hasPet = false;
        h.petDead = true;
        happy -= 5;
      } else {
        const long long cost = static_cast<long long>(t.num(row, "Treatment Cost Score")) * 400;
        h.money -= cost;
        h.petVetSpend += cost;
        say("The vet treated " + petToken("pet") + " for " + named + ".");
        tell("The bill came to $" + std::to_string(cost) + ".");
        h.petHealth = std::min(100, h.petHealth + int(t.num(row, "Treatment Effectiveness Score")) * 3);
        happy -= 1;
      }
    }
  } else if (petRng_.chance(h.petHappiness < 40 ? 0.10 : 0.03) && ensureTable("a16_pet-releases-enc")) {
    const size_t row = petPick("a16_pet-releases-enc", "");
    if (row != size_t(-1)) {
      const Table& t = tbl("a16_pet-releases-enc");
      title = "Letting go";
      say(spySentence(resolve(trim(t.at(row, "Scenario")))));
      mark("a16_pet-releases-enc", t, row);
      if (petRng_.chance(0.5) && ensureTable("a16_pet-release-filler-enc")) {
        const size_t f = petPick("a16_pet-release-filler-enc", "");
        if (f != size_t(-1)) tell(resolve(trim(tbl("a16_pet-release-filler-enc").at(f, "String"))));
      }
      h.hasPet = false;
      happy -= 2;
    }
  } else if (h.petHealth <= 30 && petRng_.chance(0.10) && ensureTable("a16_pet-found-dead-scenarios-enc")) {
    const Table& t = tbl("a16_pet-found-dead-scenarios-enc");
    const size_t row = t.size() ? petRng_.below(uint32_t(t.size())) : size_t(-1);
    if (row != size_t(-1)) {
      title = "Found";
      body = resolve(trim(t.at(row, "Scenario")));
      mark("a16_pet-found-dead-scenarios-enc", t, row);
      h.hasPet = false;
      h.petDead = true;
      h.petCause = "Went missing";
      happy -= 6;
    }
  } else {
    title = "A quiet year";
    say(petToken("pet") + " is " + std::to_string(h.petAge) + " now.");
    tell("Health " + std::to_string(h.petHealth) + ", happiness " + std::to_string(h.petHappiness) + ".");
  }

  petOff();
  if (title.empty() && line.empty() && body.empty()) return false;
  out.table = tableRef.empty() ? "a16_pet-pastimes-enc" : tableRef;
  out.row_id = rowRef.empty() ? std::string("0") : rowRef;
  out.title = title.empty() ? ("My " + lower(h.petAnimal)) : title;
  out.body = body;
  out.narrative = line;
  out.extra = extra;
  out.happiness += happy;
  return true;
}

// ---------------------------------------------------------------------------
// The studio (REPORT.en.md 68). The genre vocabularies of the music tables do
// not agree: the album, track, cover-art and sample tables say 'rock', 'K-pop'
// and 'rhythm and blues' while the record labels say 'hip-hop' where the genres
// table says 'rap', and a16_music-album-covers-enc keeps one x flag column per
// genre whose name is 'R&B' with the ampersand. Two helpers bridge them.
static std::string musicGenreColumn(const std::string& genre) {
  const std::string g = lower(trim(genre));
  if (g == "rhythm and blues" || g == "r&b" || g == "rnb") return "R&B";
  if (g == "k-pop") return "K-Pop";
  if (g == "heavy metal") return "Heavy Metal";
  if (g.empty()) return std::string();
  std::string out = g;
  if (out[0] >= 'a' && out[0] <= 'z') out[0] = char(out[0] - 32);
  return out;
}

static bool musicGenreFits(const std::string& cell, const std::string& genre) {
  const std::string c = lower(trim(cell));
  const std::string g = lower(trim(genre));
  if (c.empty() || g.empty()) return true;   // a blank cell fits any genre
  if (c == g) return true;
  if (g == "rap" && c == "hip-hop") return true;
  if (g == "hip-hop" && c == "rap") return true;
  if (g == "rhythm and blues" && (c == "r&b" || c == "rnb")) return true;
  if ((g == "r&b" || g == "rnb") && c == "rhythm and blues") return true;
  return false;
}

// The 'Solo' / 'Band' pair of flag columns several music tables carry.
static bool musicSideFits(const Table& t, size_t r, bool solo) {
  const char* col = solo ? "Solo" : "Band";
  if (!t.has(col)) return true;
  return t.flag(r, col);
}

void Sim::ensureStudioFields(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (h.musicRole.empty() && ensureTable("a16_music-roles-enc")) {
    const Table& t = tbl("a16_music-roles-enc");
    const bool solo = h.bandName.empty();
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!musicSideFits(t, r, solo)) continue;
      pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      h.musicRole = lower(trim(t.at(pool[studioRng_.below(uint32_t(pool.size()))], "Role")));
    }
  }
}

// The phrase several tables use in place of a whole album:
// "making <album-description> is the worst idea."
std::string Sim::musicAlbumDescription() {
  if (!hero_) return std::string();
  const Hero& h = *hero_;
  const std::string album = h.musicAlbum.empty() ? std::string("an untitled album") : h.musicAlbum;
  const std::string cover = h.musicCoverArt.empty()
      ? (h.musicCover.empty() ? std::string("the band on a rooftop") : h.musicCover)
      : h.musicCoverArt;
  const std::string n = std::to_string(h.musicTrackList.empty() ? size_t(10) : h.musicTrackList.size());
  return "an album called " + album + " with " + n + " tracks and " + cover + " on the cover";
}

// One year of the recording career: an audition, a practice session, an album
// with its tracks, a show, a fan, or the band coming apart (REPORT.en.md 68.3).
bool Sim::studioYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.musician) return false;
  hero_ = &h;
  life_ = &life;
  if (h.age < 16) return false;
  if (!studioRng_.chance(0.45)) return false;
  ensureStudioFields(life);
  const bool solo = h.bandName.empty();

  std::string title, body, line;
  std::vector<std::string> extra;
  std::string tableRef, rowRef;
  int happy = 0;

  const auto say = [&](const std::string& s) {
    if (s.empty()) return;
    if (!line.empty()) extra.push_back(s);
    else line = spyUpperFirst(s);
  };
  const auto tell = [&](const std::string& s) { if (!s.empty()) extra.push_back(spySentence(s)); };
  const auto cell = [&](const Table& t, size_t r, const char* col) -> std::string {
    if (!t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  const auto mark = [&](const char* tab, size_t r) {
    tableRef = tab;
    rowRef = std::to_string(r + 1);
  };
  const auto genrePool = [&](const Table& t, const char* weightColumn) -> size_t {
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Genre") && !musicGenreFits(t.at(r, "Genre"), h.musicGenre)) continue;
      pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (pool.empty()) return size_t(-1);
    if (weightColumn && t.has(weightColumn)) {
      const size_t picked = weightedPickOf(t, weightColumn, pool, studioRng_);
      if (picked != size_t(-1)) return picked;
    }
    return pool[studioRng_.below(uint32_t(pool.size()))];
  };
  const auto allOf = [&](const Table& t) {
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    return pool;
  };
  const auto uniformRow = [&](const Table& t) -> size_t {
    if (!t.size()) return size_t(-1);
    return studioRng_.below(uint32_t(t.size()));
  };
  const std::vector<std::string> keep = out.extra;
  const auto finish = [&]() -> bool {
    if (title.empty() && line.empty()) return false;
    out = AgeUp();
    out.table = tableRef.empty() ? "a16_music-practice-scenarios-enc" : tableRef;
    out.row_id = rowRef;
    out.title = title;
    out.body = body;
    out.narrative = line;
    out.extra = keep;
    for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
    out.happiness += happy;
    return true;
  };

  // Fame drifts back toward the middle of the road every year.
  if (h.musicFame < 50) h.musicFame += 1;
  else if (h.musicFame > 50) h.musicFame -= 1;

  // 1. Unsigned: the audition and the label (0.35).
  if (!h.musicSigned && studioRng_.chance(0.35)) {
    const bool gotIt = studioRng_.below(100) < uint32_t(45 + h.musicFame / 3);
    if (gotIt && ensureTable("a16_music-record-label-company-name-enc")) {
      const Table& labs = tbl("a16_music-record-label-company-name-enc");
      const size_t r = genrePool(labs, "Commonness Score");
      const std::string label = r == size_t(-1) ? std::string() : cell(labs, r, "Name");
      if (!label.empty()) {
        h.musicSigned = true;
        h.musicLabel = label;
        h.money += 2500;
        h.musicEarned += 2500;
        h.musicFame = std::min(100, h.musicFame + 3);
        if (r != size_t(-1)) mark("a16_music-record-label-company-name-enc", r);
        title = "Signed";
        body = "Contract: " + label + ".";
        say("I signed with " + label + " and took a $2,500 advance.");
        return finish();
      }
    }
    if (ensureTable("a16_music-audition-rejections-enc")) {
      const Table& t = tbl("a16_music-audition-rejections-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        if (!musicSideFits(t, r, solo)) continue;
        if (t.has("Role")) {
          const std::string role = lower(trim(t.at(r, "Role")));
          if (!role.empty() && role != h.musicRole) continue;
        }
        pool.push_back(r);
      }
      if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
      if (!pool.empty()) {
        const size_t r = pool[studioRng_.below(uint32_t(pool.size()))];
        mark("a16_music-audition-rejections-enc", r);
        title = "The audition";
        body = cell(t, r, "Reason");
        say("Nobody signed me this year.");
        return finish();
      }
    }
  }

  // 2. Practice (0.22). The Log Version says "I spent <rb> doing scale
  // exercises", so the Scenario has to be resolved first, exactly like the
  // mafia's shakedown rows.
  if (studioRng_.chance(0.22) && ensureTable("a16_music-practice-scenarios-enc")) {
    const Table& t = tbl("a16_music-practice-scenarios-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Role")) {
        const std::string role = lower(trim(t.at(r, "Role")));
        if (!role.empty() && role != h.musicRole) continue;
      }
      pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      const size_t r = pool[studioRng_.below(uint32_t(pool.size()))];
      mark("a16_music-practice-scenarios-enc", r);
      const std::string sc = cell(t, r, "Scenario");
      title = "Practice";
      body = toFirstPerson(spySentence(sc));
      say(cell(t, r, "Log Version"));
      h.musicRespect = std::min(100, h.musicRespect + 1);
      return finish();
    }
  }

  // 3. The album (0.35) once there is a label behind it.
  if ((h.musicSigned || h.musicFame >= 45) && studioRng_.chance(0.35) &&
      ensureTable("a16_music-album-title-enc")) {
    const Table& at = tbl("a16_music-album-title-enc");
    const size_t ar = genrePool(at, "Commonness Score");
    if (ar != size_t(-1)) {
      h.musicAlbum = cell(at, ar, "Title");
      mark("a16_music-album-title-enc", ar);
      // The compilation is the one album type the rejection table names.
      const bool compilation = studioRng_.chance(0.15) && ensureTable("a16_music-names-compilations-enc");
      if (compilation) {
        const Table& ct = tbl("a16_music-names-compilations-enc");
        const size_t rr = weightedPickOf(ct, "Commonness Score", allOf(ct), studioRng_);
        const std::string ct2 = cell(ct, rr, "Title");
        if (!ct2.empty()) {
          h.musicAlbum = ct2;
          h.musicCompilations += 1;
          if (ct.has("Accommodates Definite Article") && ct.flag(rr, "Accommodates Definite Article")) {
            h.musicAlbum = ct2;
          }
        }
      }
      // The subgenre is a flag column per genre in a16_music-subgenres-enc.
      if (ensureTable("a16_music-subgenres-enc")) {
        const Table& sg = tbl("a16_music-subgenres-enc");
        const std::string flag = musicGenreColumn(h.musicGenre);
        std::vector<size_t> pool;
        if (!flag.empty() && sg.has(flag.c_str())) {
          for (size_t r = 0; r < sg.size(); ++r) {
            if (sg.flag(r, flag.c_str())) pool.push_back(r);
          }
        }
        if (pool.empty()) for (size_t r = 0; r < sg.size(); ++r) pool.push_back(r);
        if (!pool.empty()) {
          h.musicSubgenre = lower(trim(sg.at(pool[studioRng_.below(uint32_t(pool.size()))], "Subgenre")));
        }
      }
      // The cover, the cover-art sentence and the tracks.
      if (ensureTable("a16_music-album-covers-enc")) {
        const Table& cv = tbl("a16_music-album-covers-enc");
        const std::string flag = musicGenreColumn(h.musicGenre);
        std::vector<size_t> pool;
        for (size_t r = 0; r < cv.size(); ++r) {
          if (!musicSideFits(cv, r, solo)) continue;
          if (!flag.empty() && cv.has(flag.c_str()) && !cv.flag(r, flag.c_str())) continue;
          pool.push_back(r);
        }
        if (pool.empty()) {
          for (size_t r = 0; r < cv.size(); ++r) if (musicSideFits(cv, r, solo)) pool.push_back(r);
        }
        if (pool.empty()) for (size_t r = 0; r < cv.size(); ++r) pool.push_back(r);
        if (!pool.empty()) h.musicCover = cell(cv, pool[studioRng_.below(uint32_t(pool.size()))], "Cover");
      }
      if (ensureTable("a16_music-cover-art-string-enc")) {
        const Table& ca = tbl("a16_music-cover-art-string-enc");
        std::vector<size_t> pool;
        for (size_t r = 0; r < ca.size(); ++r) {
          if (ca.has("Genre") && !h.musicGenre.empty() && !musicGenreFits(ca.at(r, "Genre"), h.musicGenre)) continue;
          pool.push_back(r);
        }
        if (pool.empty()) for (size_t r = 0; r < ca.size(); ++r) pool.push_back(r);
        if (!pool.empty()) {
          const size_t r = pool[studioRng_.below(uint32_t(pool.size()))];
          h.musicCoverArt = cell(ca, r, "String");
        }
      }
      h.musicTrackList.clear();
      const int want = 6 + int(studioRng_.below(9));
      std::vector<std::string> beats, subjects, vibes;
      std::string hook, sample;
      for (int i = 0; i < want; ++i) {
        if (ensureTable("a16_music-track-title-enc")) {
          const Table& tt = tbl("a16_music-track-title-enc");
          const size_t r = genrePool(tt, "Commonness Score");
          if (r != size_t(-1)) {
            const std::string t2 = cell(tt, r, "Track Title");
            if (!t2.empty()) h.musicTrackList.push_back(t2);
          }
        }
        if (ensureTable("a16_music-track-beat-enc")) {
          const Table& tb = tbl("a16_music-track-beat-enc");
          const size_t r = genrePool(tb, "Commonness Score");
          if (r != size_t(-1)) beats.push_back(lower(cell(tb, r, "Beat")));
        }
        if (ensureTable("a16_music-track-subject-enc")) {
          const Table& ts = tbl("a16_music-track-subject-enc");
          const size_t r = genrePool(ts, "Commonness Score");
          if (r != size_t(-1)) subjects.push_back(lower(cell(ts, r, "Subject")));
        }
        if (ensureTable("a16_music-track-vibe-enc")) {
          const Table& tv = tbl("a16_music-track-vibe-enc");
          const size_t r = genrePool(tv, "Commonness Score");
          if (r != size_t(-1)) vibes.push_back(lower(cell(tv, r, "Vibe")));
        }
      }
      if (studioRng_.chance(0.3) && ensureTable("a16_music-track-lyrical-hook-enc")) {
        const Table& th = tbl("a16_music-track-lyrical-hook-enc");
        const size_t r = genrePool(th, "Commonness Score");
        if (r != size_t(-1)) hook = lower(cell(th, r, "Hook"));
      }
      if (studioRng_.chance(0.3) && ensureTable("a16_music-sample-track-enc")) {
        const Table& st = tbl("a16_music-sample-track-enc");
        const size_t r = genrePool(st, "Commonness Score");
        if (r != size_t(-1)) sample = cell(st, r, "Song");
      }
      if (ensureTable("a16_music-signature-sound-enc")) {
        const Table& ss = tbl("a16_music-signature-sound-enc");
        const size_t rr = weightedPickOf(ss, "Commonness Score", allOf(ss), studioRng_);
        const size_t srow = rr == size_t(-1) ? uniformRow(ss) : rr;
        const std::string snd = srow == size_t(-1) ? std::string() : lower(cell(ss, srow, "Sound"));
        if (!snd.empty()) {
          h.musicSigSound = snd;
        }
      }
      int flairImpact = 0;
      std::string flair;
      if (ensureTable("a16_music-artistic-flair-string-enc")) {
        const Table& af = tbl("a16_music-artistic-flair-string-enc");
        const size_t r = genrePool(af, "");
        if (r != size_t(-1)) {
          flair = cell(af, r, "Log Version");
          flairImpact = int(af.num(r, "Reception Impact Score"));
          h.musicRespect = std::min(100, h.musicRespect + 1);
        }
      }
      // The label can refuse the album outright: <album-description> is the
      // phrase the rejection rows are written around.
      if (studioRng_.chance(0.15) && ensureTable("a16_music-album-rejections-enc")) {
        const Table& ar2 = tbl("a16_music-album-rejections-enc");
        std::vector<size_t> pool;
        for (size_t r = 0; r < ar2.size(); ++r) {
          if (!musicSideFits(ar2, r, solo)) continue;
          if (compilation && ar2.has("Album Type")) {
            const std::string type = lower(trim(ar2.at(r, "Album Type")));
            if (!type.empty() && type != "compilation") continue;
          }
          pool.push_back(r);
        }
        if (pool.empty()) for (size_t r = 0; r < ar2.size(); ++r) pool.push_back(r);
        const size_t r = pool[studioRng_.below(uint32_t(pool.size()))];
        mark("a16_music-album-rejections-enc", r);
        title = "The label said no";
        body = cell(ar2, r, "Reason");
        say("The label shelved " + h.musicAlbum + ".");
        h.musicRespect = std::max(0, h.musicRespect - 2);
        return finish();
      }
      // Or one track can be refused on its own.
      if (studioRng_.chance(0.2) && ensureTable("a16_music-track-rejection-string-enc")) {
        const Table& tr = tbl("a16_music-track-rejection-string-enc");
        const size_t rr = weightedPickOf(tr, "Commonness Score", allOf(tr), studioRng_);
        mark("a16_music-track-rejection-string-enc", rr);
        title = "A track was cut";
        body = cell(tr, rr, "String");
        say(cell(tr, rr, "Log Version"));
        return finish();
      }
      // Released: the reception is the fame, the flair and the length of the
      // record, in that order.
      const int reception = std::max(5, std::min(100,
          35 + h.musicFame / 2 + flairImpact * 4 + int(h.musicTrackList.size()) + (sample.empty() ? 0 : 4)));
      h.musicFame = std::min(100, h.musicFame + reception / 12);
      h.musicRespect = std::min(100, h.musicRespect + 2);
      h.musicAlbums += 1;
      const long long take = reception * 4000LL;
      h.money += take;
      h.musicEarned += take;
      title = "The album drops";
      std::string list;
      for (size_t i = 0; i < h.musicTrackList.size() && i < 4; ++i) {
        if (!list.empty()) list += "; ";
        list += h.musicTrackList[i];
      }
      body = h.musicAlbum + (compilation ? " (compilation)" : std::string());
      if (!h.musicSubgenre.empty()) body += " - " + h.musicSubgenre;
      if (!list.empty()) body += ". Tracks: " + list;
      if (!beats.empty()) tell("Beat: " + beats[0] + ".");
      if (!subjects.empty()) tell("Subject: " + subjects[0] + ".");
      if (!vibes.empty()) tell("Vibe: " + vibes[0] + ".");
      if (!hook.empty()) tell("The hook: " + hook + ".");
      if (!sample.empty()) tell("Samples " + sample + ".");
      if (!h.musicCoverArt.empty()) tell("Cover art: " + h.musicCoverArt + ".");
      if (!h.musicSigSound.empty()) tell("Signature sound: " + h.musicSigSound + ".");
      if (!flair.empty()) tell(flair + ".");
      if (h.musicLabel.empty()) say("My album " + h.musicAlbum + " dropped.");
      else say("My album " + h.musicAlbum + " dropped on " + h.musicLabel + ".");
      tell("The reception came to " + std::to_string(reception) + " out of 100 and it made $" + std::to_string(take) + ".");
      happy += 3;
      return finish();
    }
  }

  // 4. The show (0.18).
  if (h.musicFame >= 20 && studioRng_.chance(0.18) && ensureTable("a16_music-concert-venues-enc")) {
    const size_t r = pickWindow("a16_music-concert-venues-enc", "", "", "", "Min Popularity", "Max Popularity",
                               double(h.musicFame), studioRng_);
    if (r != size_t(-1)) {
      const Table& v = tbl("a16_music-concert-venues-enc");
      const std::string venue = lower(cell(v, r, "Venue"));
      const long long capacity = 500 + static_cast<long long>(studioRng_.below(15000));
      const long long pay = capacity * static_cast<long long>(v.num(r, "Money Score")) * 2;
      h.money += pay;
      h.musicEarned += pay;
      h.musicFame = std::min(100, h.musicFame + 2);
      mark("a16_music-concert-venues-enc", r);
      title = "A show";
      body = "Venue: " + venue + ".";
      say("I played " + articleFor(venue) + venue + " to " + std::to_string(capacity) + " people and took $" + std::to_string(pay) + ".");
      return finish();
    }
  }

  // 5. A fan (0.22).
  if (studioRng_.chance(0.22) && ensureTable("a16_music-fan-interaction-scenarios-enc")) {
    const Table& t = tbl("a16_music-fan-interaction-scenarios-enc");
    const bool fanFemale = studioRng_.chance(0.5);
    const int fanAge = 16 + int(studioRng_.below(40));
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      const std::string fg = lower(trim(t.has("Fan Gender") ? t.at(r, "Fan Gender") : std::string()));
      if (!fg.empty() && fg != "both" && fg != (fanFemale ? "female" : "male")) continue;
      const std::string hg = lower(trim(t.has("Hero Gender") ? t.at(r, "Hero Gender") : std::string()));
      if (!hg.empty() && hg != "both" && hg != (h.female ? "female" : "male")) continue;
      const std::string role = lower(trim(t.has("Role") ? t.at(r, "Role") : std::string()));
      if (!role.empty() && role != h.musicRole) continue;
      if (!musicSideFits(t, r, solo)) continue;
      if (t.has("Fan Age")) {
        const double fa = t.num(r, "Fan Age");
        if (fa > 0 && fanAge > int(fa)) continue;   // 'Fan Age' is the ceiling
      }
      if (t.has("Min Popularity") && t.num(r, "Min Popularity") > 0 && h.musicFame < int(t.num(r, "Min Popularity"))) continue;
      if (t.has("Max Popularity") && t.num(r, "Max Popularity") > 0 && h.musicFame > int(t.num(r, "Max Popularity"))) continue;
      if (t.has("Min Respect") && t.num(r, "Min Respect") > 0 && h.musicRespect < int(t.num(r, "Min Respect"))) continue;
      pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      const size_t r = pool[studioRng_.below(uint32_t(pool.size()))];
      const bool positive = t.has("Positive") ? t.flag(r, "Positive") : !t.flag(r, "Negative");
      // A fan row's Log Version Snippet says "<rb>" about an alternative drawn in
      // the Scenario or Feedback column of its own row, so the whole row is
      // resolved in column order first (REPORT.en.md 68.3).
      {
        const std::vector<std::string>& cols = t.columns();
        for (size_t c = 0; c < cols.size(); ++c) {
          if (!t.has(cols[c].c_str())) continue;
          const std::string raw = t.at(r, cols[c].c_str());
          if (raw.find('<') == std::string::npos) continue;
          resolve(raw);
        }
      }
      mark("a16_music-fan-interaction-scenarios-enc", r);
      const bool savedSet = pronounOtherSet_;
      const bool savedFemale = pronounOtherFemale_;
      pronounOtherSet_ = true;
      pronounOtherFemale_ = fanFemale;
      title = positive ? "A fan" : "A fan, of sorts";
      const std::string fb = cell(t, r, "Feedback Snippet");
      body = fb.empty() ? cell(t, r, "Scenario") : fb;
      std::string st = cell(t, r, "Log Version Snippet");
      if (st.empty()) st = cell(t, r, "Scenario");
      say(st);
      pronounOtherSet_ = savedSet;
      pronounOtherFemale_ = savedFemale;
      h.musicFame = std::max(0, std::min(100, h.musicFame + (positive ? 3 : -3)));
      h.musicRespect = std::max(0, std::min(100, h.musicRespect + (positive ? 2 : -2)));
      happy += positive ? 2 : -2;
      return finish();
    }
  }

  // 6. The band, when there is one (0.06 each way).
  if (!solo) {
    if (studioRng_.chance(0.06) && ensureTable("a16_music-band-breaks-up-scenarios-enc")) {
      const Table& t = tbl("a16_music-band-breaks-up-scenarios-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        if (t.has("Min Popularity") && t.num(r, "Min Popularity") > 0 && h.musicFame < int(t.num(r, "Min Popularity"))) continue;
        if (t.has("Max Popularity") && t.num(r, "Max Popularity") > 0 && h.musicFame > int(t.num(r, "Max Popularity"))) continue;
        pool.push_back(r);
      }
      if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
      if (!pool.empty()) {
        const size_t r = pool[studioRng_.below(uint32_t(pool.size()))];
        mark("a16_music-band-breaks-up-scenarios-enc", r);
        title = "The band is over";
        body = cell(t, r, "Reason");
        say(h.bandName + " broke up.");
        h.bandName.clear();
        happy -= 3;
        return finish();
      }
    }
    if (studioRng_.chance(0.06) && ensureTable("a16_music-band-kicks-out-scenarios-enc")) {
      const Table& t = tbl("a16_music-band-kicks-out-scenarios-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        if (t.has("Role")) {
          const std::string role = lower(trim(t.at(r, "Role")));
          if (!role.empty() && role != h.musicRole) continue;
        }
        if (t.has("Hero Min Age") && t.num(r, "Hero Min Age") > 0 && h.age < int(t.num(r, "Hero Min Age"))) continue;
        if (t.has("Hero Max Age") && t.num(r, "Hero Max Age") > 0 && h.age > int(t.num(r, "Hero Max Age"))) continue;
        pool.push_back(r);
      }
      if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
      if (!pool.empty()) {
        const size_t r = pool[studioRng_.below(uint32_t(pool.size()))];
        mark("a16_music-band-kicks-out-scenarios-enc", r);
        title = "Out of the band";
        body = cell(t, r, "Reason");
        say("I was out of " + h.bandName + ".");
        h.bandName.clear();
        happy -= 4;
        return finish();
      }
    }
  }

  // 7. A studio day, so that a musician always has something to show.
  title = "A session at the studio";
  body = h.musicSigned ? ("Label: " + h.musicLabel + ".") : std::string("Still unsigned.");
  if (!h.musicRole.empty()) body += " I play " + h.musicRole + ".";
  say("I put in a day at the studio.");
  h.musicRespect = std::min(100, h.musicRespect + 1);
  return finish();
}

// ---------------------------------------------------------------------------
// The studio trades (REPORT.en.md 69): the five music jobs that are not the
// artist line - producer, SoundCloud uploader, talent scout, composer, and the
// touring act whose shows turn into club takeovers. Each sits behind its own
// Hero flag; one yearly pass walks whichever of them is switched on, draws the
// artist the trade is working with, and renders one card out of the family's
// thirty tables.
// ---------------------------------------------------------------------------

std::string Sim::tradeToken(const std::string& key) {
  if (!hero_) return std::string();
  Hero& h = *const_cast<Hero*>(hero_);
  const std::string psuffix = "-possessive";
  bool own = false;
  std::string base = key;
  if (base.size() > psuffix.size() &&
      base.compare(base.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    own = true;
    base.resize(base.size() - psuffix.size());
  }
  if (base == "artist") {
    if (h.tradeArtist.empty()) return std::string();
    return own ? llPossessive(h.tradeArtist) : h.tradeArtist;
  }
  if (base == "track") {
    if (h.tradeTrack.empty()) return std::string();
    return own ? llPossessive(h.tradeTrack) : h.tradeTrack;
  }
  if (base == "musician-first-name") {
    return drawFirstName(tradeRng_.chance(0.5));
  }
  if (base == "musician-last-name") return drawSurname();
  if (base == "musician-name") {
    const bool fm = tradeRng_.chance(0.5);
    return drawFirstName(fm) + " " + drawSurname();
  }
  if (base == "musician-first-name-letter") {
    static const char* kLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char c = kLetters[tradeRng_.below(26)];
    return std::string(1, c);
  }
  if (base == "instrument") {
    if (h.tradeInstrument.empty()) return std::string();
    return own ? llPossessive(h.tradeInstrument) : h.tradeInstrument;
  }
  if (base == "project") {
    if (h.tradeProject.empty()) return std::string();
    return own ? llPossessive(h.tradeProject) : h.tradeProject;
  }
  if (base == "project-tone") return h.tradeTone;
  if (base == "trade-stage") {
    if (h.tradeStage.empty()) return std::string();
    return own ? llPossessive(h.tradeStage) : h.tradeStage;
  }
  return std::string();
}

// The genre the trade works in: whatever the artist line already drew, else one
// from the genre table (the trade can be the hero's only music flag).
std::string Sim::tradeGenre(Life& life) {
  Hero& h = life.hero;
  if (!h.musicGenre.empty()) return h.musicGenre;
  if (!ensureTable("a16_music-genres-enc")) return std::string("pop");
  const Table& t = tbl("a16_music-genres-enc");
  if (t.size() == 0) return std::string("pop");
  h.musicGenre = lower(trim(t.at(tradeRng_.below(uint32_t(t.size())), "Genre")));
  return h.musicGenre;
}

// The act the trade is working with: a name, a genre close to the hero's own,
// a popularity and a vocal quality for the rows that window on them.
void Sim::tradeDrawArtist(Life& life) {
  Hero& h = life.hero;
  const bool female = tradeRng_.chance(0.5);
  hero_ = &h;
  life_ = &life;
  h.tradeArtist = drawFirstName(female) + " " + drawSurname();
  // The stage-name table names the act ("Whitney Spears", "Mob Barley") when it
  // has a row for the gender and the genre; names-stage is the fallback source
  // and its titles are templates of their own (REPORT.en.md 69).
  const std::string want = tradeGenre(life);
  if (ensureTable("a16_music-artist-stage-name-enc")) {
    const Table& t = tbl("a16_music-artist-stage-name-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      const std::string g = lower(trim(t.at(r, "Gender")));
      if (!g.empty() && g != "both" && g != (female ? "female" : "male")) continue;
      if (!t.has("Genre") || !musicGenreFits(t.at(r, "Genre"), want)) continue;
      pool.push_back(r);
    }
    if (pool.empty()) {
      for (size_t r = 0; r < t.size(); ++r) {
        const std::string g = lower(trim(t.at(r, "Gender")));
        if (g.empty() || g == "both" || g == (female ? "female" : "male")) pool.push_back(r);
      }
    }
    if (!pool.empty()) {
      const size_t r = weightedPickOf(t, t.has("Commonness Score") ? "Commonness Score" : std::string(),
                                      pool, tradeRng_);
      const size_t pick = (r == size_t(-1) || r >= t.size()) ? pool[tradeRng_.below(uint32_t(pool.size()))] : r;
      const std::string nm = resolve(trim(t.at(pick, "Stage Name")));
      if (!nm.empty()) h.tradeArtist = nm;
    }
  } else if (ensureTable("a16_music-names-stage-enc")) {
    const Table& t = tbl("a16_music-names-stage-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has(musicGenreColumn(want)) && t.flag(r, musicGenreColumn(want).c_str())) pool.push_back(r);
    }
    if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    if (!pool.empty()) {
      const size_t r = pool[tradeRng_.below(uint32_t(pool.size()))];
      const std::string nm = resolve(trim(t.at(r, "Title")));
      if (!nm.empty()) h.tradeArtist = nm;
    }
  }
  h.tradeArtistFemale = female;
  h.tradeArtistPop = 20 + int(tradeRng_.below(61));
  h.tradeArtistVocal = 20 + int(tradeRng_.below(71));
  // A compatible genre of the hero's own, when the genre table has one.
  std::string genre = tradeGenre(life);
  if (ensureTable("a16_music-producer-genre-enc")) {
    const Table& g = tbl("a16_music-producer-genre-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < g.size(); ++r) {
      if (!musicGenreFits(g.at(r, "Genre"), genre)) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[tradeRng_.below(uint32_t(pool.size()))];
      static const char* kCols[] = {"Compatible Genre 1", "Compatible Genre 2", "Compatible Genre 3"};
      const size_t c = tradeRng_.below(3);
      if (g.has(kCols[c])) {
        const std::string alt = lower(trim(g.at(r, kCols[c])));
        if (!alt.empty()) genre = alt;
      }
    }
  }
  h.tradeGenre = genre;
}

// The instrument table (a16_music-instruments-enc) is a plain asset list: the
// hero learns one and plays it on every card after that.
void Sim::makeInstrument(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!ensureTable("a16_music-instruments-enc")) return;
  const Table& t = tbl("a16_music-instruments-enc");
  if (t.size() == 0) return;
  const std::vector<std::string> blank;
  const std::string col = t.has("Commonness Score") ? "Commonness Score" : std::string();
  size_t r = t.size();
  if (!col.empty()) r = weightedPick(t, col, tradeRng_);
  if (r == size_t(-1) || r >= t.size()) r = tradeRng_.below(uint32_t(t.size()));
  const std::string inst = lower(trim(t.at(r, t.has("Instrument") ? "Instrument" : "Singular Name")));
  if (!inst.empty()) h.tradeInstrument = inst;
}

void Sim::makeTrade(Life& life, int kind) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  tradeGenre(life);
  if (kind == 1) {
    h.tradeProducer = true;
    // A producer works under a stage name, as a 'B.I.G. <surname>' or a full
    // pseudonym (a16_music-producer-stage-name-enc).
    if (ensureTable("a16_music-producer-stage-name-enc")) {
      const Table& t = tbl("a16_music-producer-stage-name-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        const std::string g = lower(trim(t.has("Hero Gender") ? t.at(r, "Hero Gender") : std::string()));
        if (g.empty() || g == "both" || (g == "male") == !h.female) pool.push_back(r);
      }
      if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
      if (!pool.empty()) {
        const size_t r = pool[tradeRng_.below(uint32_t(pool.size()))];
        h.tradeStage = trim(resolve(trim(t.at(r, "Name"))));
      }
    }
    if (h.tradeStage.empty()) h.tradeStage = drawFirstName(h.female) + " " + drawSurname();
  } else if (kind == 2) {
    h.tradeSoundcloud = true;
    // The screen name is either hero-derived or a plain handle.
    if (ensureTable("a16_music-soundcloud-screen-name-enc")) {
      const Table& t = tbl("a16_music-soundcloud-screen-name-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        const std::string g = lower(trim(t.has("Gender") ? t.at(r, "Gender") : std::string()));
        if (!g.empty() && g != "both" && ((g == "male") == h.female)) continue;
        pool.push_back(r);
      }
      if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
      if (!pool.empty()) {
        const size_t r = pool[tradeRng_.below(uint32_t(pool.size()))];
        h.tradeStage = trim(resolve(trim(t.at(r, "Name"))));
      }
    }
    if (h.tradeStage.empty()) h.tradeStage = drawSurname() + std::to_string(100 + tradeRng_.below(900));
    h.tradeFollowers = 5 + int(tradeRng_.below(50));
  } else if (kind == 3) {
    h.tradeScout = true;
  } else if (kind == 4) {
    h.tradeComposer = true;
  } else {
    h.tradeStunts = true;
  }
  makeInstrument(life);
  tradeDrawArtist(life);
}

// One year of whichever trade is switched on (REPORT.en.md 69.3). The lines are
// ordered rarest first so no branch starves; when more than one trade is on the
// year picks one of them at random.
bool Sim::tradeYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.tradeProducer && !h.tradeSoundcloud && !h.tradeScout && !h.tradeComposer && !h.tradeStunts)
    return false;
  hero_ = &h;
  life_ = &life;
  if (h.age < 16) return false;
  if (!tradeRng_.chance(0.5)) return false;
  const std::string genre = tradeGenre(life);
  if (h.tradeArtist.empty()) tradeDrawArtist(life);

  std::string title, body, line, tableRef, rowRef, fallback;
  std::vector<std::string> extra;
  int happy = 0;

  auto allOf = [](const Table& t) {
    std::vector<size_t> v;
    for (size_t r = 0; r < t.size(); ++r) v.push_back(r);
    return v;
  };
  auto cell = [this](const Table& t, size_t r, const char* col) -> std::string {
    if (!t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  auto mark = [&](const char* table, size_t r) {
    tableRef = table;
    rowRef = std::to_string(r + 1);
  };
  auto say = [&](const std::string& s) {
    if (line.empty() && !s.empty()) line = spyUpperFirst(s);
  };
  auto tell = [&](const std::string& s) {
    if (!s.empty()) extra.push_back(spySentence(s));
  };
  // A weighted draw with an optional value filter and an optional numeric
  // window; a window miss falls back to the same table without the filter, the
  // way the philosophy-filtered pools of report 62 do.
  auto pick = [&](const char* tab, const char* weight, const char* fcol, const std::string& fval,
                  const char* lo, const char* hi, double val) -> size_t {
    const std::string name = tab;
    if (!ensureTable(name)) return size_t(-1);
    size_t r = pickWindow(name, weight ? weight : "", fcol ? fcol : "", fcol ? fval : "",
                          lo ? lo : "", hi ? hi : "", val, tradeRng_);
    if (r == size_t(-1) && fcol != nullptr && !fval.empty()) {
      r = pickWindow(name, weight ? weight : "", "", "", lo ? lo : "", hi ? hi : "", val, tradeRng_);
    }
    return r;
  };
  // Every bracketed cell of a row, resolved in column order, so a card whose log
  // line says '<rb>' about an alternative drawn in another column still fills
  // (the warm pass of reports 64 and 68).
  auto warm = [&](const Table& t, size_t r) {
    const std::vector<std::string>& cols = t.columns();
    for (size_t c = 0; c < cols.size(); ++c) {
      if (!t.has(cols[c])) continue;
      const std::string raw = t.at(r, cols[c]);
      if (raw.find('<') == std::string::npos) continue;
      resolve(raw);
    }
  };
  auto artistVoice = [&](bool on) {
    pronounOtherSet_ = on;
    pronounOtherFemale_ = h.tradeArtistFemale;
  };
  auto genreFlagFits = [&](const Table& t, size_t r, const char* col) {
    if (!t.has(col)) return true;
    return musicGenreFits(t.at(r, col), genre);
  };

  int kind = 0;
  if (h.tradeProducer && h.tradeSoundcloud && h.tradeScout && h.tradeComposer && h.tradeStunts)
    kind = int(tradeRng_.below(5)) + 1;
  else {
    std::vector<int> on;
    if (h.tradeProducer) on.push_back(1);
    if (h.tradeSoundcloud) on.push_back(2);
    if (h.tradeScout) on.push_back(3);
    if (h.tradeComposer) on.push_back(4);
    if (h.tradeStunts) on.push_back(5);
    kind = on[tradeRng_.below(uint32_t(on.size()))];
  }

  if (kind == 1) {
    // --- The producer ------------------------------------------------------
    fallback = "a16_music-producer-aspiring-artist-string-enc";
    if (h.tradeClients == 0) {
      const size_t r = pick("a16_music-producer-aspiring-artist-string-enc", "Commonness Score",
                            nullptr, "", "Min Vocal Quality", "Max Vocal Quality", double(h.tradeArtistVocal));
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_music-producer-aspiring-artist-string-enc");
        warm(t, r);
        mark("a16_music-producer-aspiring-artist-string-enc", r);
        artistVoice(true);
        const std::string verdict = toFirstPerson(cell(t, r, "String"));
        artistVoice(false);
        title = "Signing a client";
        body = "New client: " + h.tradeArtist + ".";
        h.tradeClients = 1;
        say(verdict);
        tell("I took on " + h.tradeArtist + " as a client.");
      }
    }
    if (line.empty() && tradeRng_.chance(0.18) && ensureTable("a16_music-producer-concert-act-enc")) {
      const Table& t = tbl("a16_music-producer-concert-act-enc");
      const size_t r = allOf(t)[tradeRng_.below(uint32_t(t.size()))];
      const std::string act = cell(t, r, "Name");
      if (!act.empty()) {
        mark("a16_music-producer-concert-act-enc", r);
        title = "A concert";
        body = "The act: " + act + ".";
        h.tradeRep = std::min(100, h.tradeRep + 2);
        say("I produced a concert for " + act + ".");
      }
    }
    if (line.empty() && tradeRng_.chance(0.18) && ensureTable("a16_music-producer-titles-enc")) {
      const Table& t = tbl("a16_music-producer-titles-enc");
      const size_t r = allOf(t)[tradeRng_.below(uint32_t(t.size()))];
      static const char* kOpts[] = {"Option 1", "Option 2", "Option 3", "Option 4", "Option 5"};
      std::vector<std::string> opts;
      for (size_t i = 0; i < 5; ++i) if (t.has(kOpts[i])) opts.push_back(trim(t.at(r, kOpts[i])));
      if (!opts.empty()) {
        mark("a16_music-producer-titles-enc", r);
        title = opts[tradeRng_.below(uint32_t(opts.size()))];
        body = heroToFirstPerson(cell(t, r, "Situation Description"));
        h.tradeRep = std::min(100, h.tradeRep + 1);
        say("I spent the week in the booth on that one.");
      }
    }
    if (line.empty() && tradeRng_.chance(0.16) && ensureTable("a16_music-producer-dumb-scenario-enc")) {
      const Table& t = tbl("a16_music-producer-dumb-scenario-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        if (t.has("Hero Min Age") && !trim(t.at(r, "Hero Min Age")).empty() &&
            h.age < std::atoi(trim(t.at(r, "Hero Min Age")).c_str())) continue;
        if (t.has("Hero Max Age") && !trim(t.at(r, "Hero Max Age")).empty() &&
            h.age > std::atoi(trim(t.at(r, "Hero Max Age")).c_str())) continue;
        const std::string g = t.has("Hero Gender") ? lower(trim(t.at(r, "Hero Gender"))) : std::string();
        if (!g.empty() && g != "both" && ((g == "male") == h.female)) continue;
        if (t.has("Hero Has Lover") && t.flag(r, "Hero Has Lover") && h.loverName.empty()) continue;
        const std::string lg = t.has("Lover Gender") ? lower(trim(t.at(r, "Lover Gender"))) : std::string();
        if (!lg.empty() && !h.loverName.empty() && ((lg == "male") == h.loverFemale)) continue;
        if (!genreFlagFits(t, r, "Genre")) continue;
        pool.push_back(r);
      }
      if (pool.empty()) pool = allOf(t);
      const size_t r = pool[tradeRng_.below(uint32_t(pool.size()))];
      warm(t, r);
      mark("a16_music-producer-dumb-scenario-enc", r);
      title = cell(t, r, "Title");
      body = cell(t, r, "Body");
      // A year-up card has no prompt to route a choice through, so the port takes
      // the first one (the same shortcut as the other dumb-scenario pools).
      static const char* kNum[] = {"1", "2", "3", "4"};
      const std::string choice = cell(t, r, (std::string("Choice ") + kNum[0]).c_str());
      const std::string narr = cell(t, r, (std::string("Narrative ") + kNum[0]).c_str());
      const double impact = t.num(r, std::string("Reputation Impact ") + kNum[0]);
      h.tradeRep = std::max(0, std::min(100, h.tradeRep + int(impact)));
      if (!choice.empty()) tell("The choice was: " + choice + ".");
      say(narr);
    }
    if (line.empty() && tradeRng_.chance(0.14) && ensureTable("a16_music-producer-artist-leave-string-enc")) {
      const size_t r = pick("a16_music-producer-artist-leave-string-enc", "Commonness Score",
                            nullptr, "", "Min Artist Popularity", "Max Artist Popularity",
                            double(h.tradeArtistPop));
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_music-producer-artist-leave-string-enc");
        warm(t, r);
        mark("a16_music-producer-artist-leave-string-enc", r);
        // The row opens on "<he/she> says <he/she>'s ...", so the artist's name
        // takes the first pronoun and the rest stay pronouns.
        std::string why = trim(t.at(r, "String"));
        const size_t np = why.find("<he/she>");
        if (np != std::string::npos) why.replace(np, 8, h.tradeArtist);
        artistVoice(true);
        why = resolve(why);
        artistVoice(false);
        title = "A client walks";
        body = h.tradeArtist + " left the label.";
        say(why);
        h.tradeArtist.clear();
        h.tradeClients = 0;
      }
    }
    if (line.empty() && tradeRng_.chance(0.12) && ensureTable("a16_music-dinner-location-enc")) {
      const Table& t = tbl("a16_music-dinner-location-enc");
      const size_t r = allOf(t)[tradeRng_.below(uint32_t(t.size()))];
      mark("a16_music-dinner-location-enc", r);
      const std::string where = cell(t, r, "String");
      const double gratitude = t.num(r, "Gratitude");
      title = "Dinner with the client";
      body = "I took " + h.tradeArtist + " to " + where + ".";
      h.tradeRep = std::min(100, h.tradeRep + int(gratitude));
      say("I took " + h.tradeArtist + " to " + where + " to talk about the next record.");
    }
    if (line.empty() && tradeRng_.chance(0.10) && ensureTable("a16_music-producer-termination-string-enc")) {
      const Table& t = tbl("a16_music-producer-termination-string-enc");
      const size_t r = allOf(t)[tradeRng_.below(uint32_t(t.size()))];
      mark("a16_music-producer-termination-string-enc", r);
      title = "The label lets me go";
      body = "I lost the production deal.";
      h.tradeRep = std::max(0, h.tradeRep - 8);
      h.tradeProducer = false;
      happy -= 4;
      say(toFirstPerson(cell(t, r, "String")));
    }
    if (line.empty()) {
      title = "A hit off the desk";
      body = "Studio: " + h.tradeStage + ".";
      h.tradeHits++;
      h.tradeRep = std::min(100, h.tradeRep + 3);
      h.money += 15000;
      h.tradeEarned += 15000;
      say("I produced a hit for " + h.tradeArtist + " and took my cut.");
    }
  } else if (kind == 2) {
    // --- SoundCloud --------------------------------------------------------
    fallback = "a16_music-soundcloud-track-reception-string-enc";
    // Every SoundCloud row names the track, so one is drawn once and kept when
    // the card is not an upload (REPORT.en.md 69).
    if (h.tradeTrack.empty() && ensureTable("a16_music-track-title-enc")) {
      const Table& tt = tbl("a16_music-track-title-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < tt.size(); ++r) if (genreFlagFits(tt, r, "Genre")) pool.push_back(r);
      if (pool.empty()) for (size_t r = 0; r < tt.size(); ++r) pool.push_back(r);
      if (!pool.empty()) h.tradeTrack = cell(tt, pool[tradeRng_.below(uint32_t(pool.size()))], "Track Title");
    }
    if (tradeRng_.chance(0.30) && ensureTable("a16_music-track-title-enc")) {
      const Table& tt = tbl("a16_music-track-title-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < tt.size(); ++r) if (genreFlagFits(tt, r, "Genre")) pool.push_back(r);
      if (pool.empty()) pool = allOf(tt);
      const size_t r = pool[tradeRng_.below(uint32_t(pool.size()))];
      const std::string trackName = cell(tt, r, "Track Title");
      if (!trackName.empty()) {
        h.tradeTrack = trackName;
        h.tradeHits++;
        h.tradeFollowers += 1 + int(tradeRng_.below(20));
        title = "Uploaded to SoundCloud";
        body = "Track: " + trackName + ".";
        // The instrument and tone of the track, then its reception.
        if (ensureTable("a16_music-soundcloud-track-instrument-enc")) {
          const Table& it = tbl("a16_music-soundcloud-track-instrument-enc");
          std::vector<size_t> ipool;
          for (size_t i = 0; i < it.size(); ++i) {
            if (it.has("Genre") && !llInList(it.at(i, "Genre"), genre)) continue;
            ipool.push_back(i);
          }
          if (ipool.empty()) ipool = allOf(it);
          const size_t ir = ipool[tradeRng_.below(uint32_t(ipool.size()))];
          const std::string inst = cell(it, ir, "Instrument");
          if (!inst.empty()) h.tradeInstrument = inst;
          const std::string tones = it.has("Project Tones") ? trim(it.at(ir, "Project Tones")) : std::string();
          if (!tones.empty()) {
            std::vector<std::string> ts;
            splitOn(tones, ',', ts);
            if (!ts.empty()) h.tradeTone = lower(trim(ts[tradeRng_.below(uint32_t(ts.size()))]));
          }
          mark("a16_music-soundcloud-track-instrument-enc", ir);
          if (!inst.empty()) tell("I played " + inst + " on it.");
        }
        const int reception = std::max(5, std::min(100, 20 + h.tradeFollowers / 8 + int(tradeRng_.below(31))));
        const size_t rr = pick("a16_music-soundcloud-track-reception-string-enc", "Commonness Score",
                               "Track Genre", lower(genre), "Min Reception", "Max Reception", double(reception));
        if (rr != size_t(-1)) {
          const Table& rt = tbl("a16_music-soundcloud-track-reception-string-enc");
          warm(rt, rr);
          artistVoice(true);
          const std::string told = toFirstPerson(cell(rt, rr, "String"));
          artistVoice(false);
          h.tradeFollowers += reception / 5;
          const long long pay = static_cast<long long>(reception) * 30;
          h.money += pay;
          h.tradeEarned += pay;
          say(told);
          tell("Reception: " + std::to_string(reception) + ", and " + std::to_string(h.tradeFollowers) + " followers after that.");
        }
      }
    }
    if (line.empty() && tradeRng_.chance(0.22) && ensureTable("a16_music-soundcloud-comment-enc")) {
      const double pop = std::min(100.0, double(h.tradeFollowers) / 2.0);
      const size_t r = pick("a16_music-soundcloud-comment-enc", "", "Track Genre", lower(genre),
                            "Min Popularity", "Max Popularity", pop);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_music-soundcloud-comment-enc");
        warm(t, r);
        mark("a16_music-soundcloud-comment-enc", r);
        title = "A comment";
        // The String is the comment as told, the Log Version is the comment
        // itself. The commenter is a stranger, so the pronouns are theirs.
        pronounOtherSet_ = true;
        pronounOtherFemale_ = tradeRng_.chance(0.5);
        say(cell(t, r, "String"));
        body = "Track: " + (h.tradeTrack.empty() ? std::string("an untitled demo") : h.tradeTrack) +
               ". Comment: " + spyLowerFirst(cell(t, r, "Log Version")) + ".";
        pronounOtherSet_ = false;
        h.tradeFollowers += 2;
      }
    }
    if (line.empty() && tradeRng_.chance(0.15) && ensureTable("a16_music-soundcloud-insult-enc")) {
      const Table& t = tbl("a16_music-soundcloud-insult-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) if (genreFlagFits(t, r, "Track Genre")) pool.push_back(r);
      if (pool.empty()) pool = allOf(t);
      const size_t r = pool[tradeRng_.below(uint32_t(pool.size()))];
      mark("a16_music-soundcloud-insult-enc", r);
      const std::string insult = cell(t, r, "Insult");
      title = "Hate in the comments";
      body = "Somebody called " + (h.tradeTrack.empty() ? std::string("my track") : h.tradeTrack) + " " + insult + ".";
      h.tradeFollowers = std::max(0, h.tradeFollowers - 1);
      happy -= 2;
      say("Somebody said my track was " + insult + ".");
    }
    if (line.empty() && tradeRng_.chance(0.13) && ensureTable("a16_music-soundcloud-collab-refusal-string-enc")) {
      const std::string partner = referenceToken("musician");
      const size_t r = allOf(tbl("a16_music-soundcloud-collab-refusal-string-enc"))[
          tradeRng_.below(uint32_t(tbl("a16_music-soundcloud-collab-refusal-string-enc").size()))];
      const Table& t = tbl("a16_music-soundcloud-collab-refusal-string-enc");
      mark("a16_music-soundcloud-collab-refusal-string-enc", r);
      h.tradeTalent = partner.empty() ? std::string("a producer I met online") : partner;
      title = "A collab, refused";
      body = h.tradeTalent + " turned down the joint track.";
      happy -= 1;
      say(cell(t, r, "String"));
    }
    if (line.empty() && tradeRng_.chance(0.20) && ensureTable("a16_music-soundcloud-dumb-scenario-enc")) {
      const Table& t = tbl("a16_music-soundcloud-dumb-scenario-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        if (t.has("Genre") && !musicGenreFits(t.at(r, "Genre"), genre)) continue;
        if (t.has("Hero Min Age") && !trim(t.at(r, "Hero Min Age")).empty() &&
            h.age < std::atoi(trim(t.at(r, "Hero Min Age")).c_str())) continue;
        if (t.has("Hero Max Age") && !trim(t.at(r, "Hero Max Age")).empty() &&
            h.age > std::atoi(trim(t.at(r, "Hero Max Age")).c_str())) continue;
        if (t.has("Hero Gender")) {
          const std::string g = lower(trim(t.at(r, "Hero Gender")));
          if (!g.empty() && g != "both" && g != std::string(h.female ? "female" : "male")) continue;
        }
        pool.push_back(r);
      }
      if (pool.empty()) pool = allOf(t);
      const size_t r = pool[tradeRng_.below(uint32_t(pool.size()))];
      mark("a16_music-soundcloud-dumb-scenario-enc", r);
      warm(t, r);
      title = cell(t, r, "Title");
      if (title.empty()) title = "A year on SoundCloud";
      body = cell(t, r, "Body");
      // The branch the hero takes is the first one this card offers, as a
      // year-up card has no prompt to route a choice through.
      std::string picked;
      for (int i = 1; i <= 4 && picked.empty(); ++i) {
        const std::string col = std::string("Choice ") + char(48 + i);
        if (!t.has(col)) continue;
        const std::string c = cell(t, r, col.c_str());
        if (c.empty()) continue;
        picked = c;
        const std::string ncol = std::string("Narrative ") + char(48 + i);
        if (t.has(ncol)) say(spyUpperFirst(cell(t, r, ncol.c_str())));
        const std::string icol = std::string("Reception Impact ") + char(48 + i);
        h.musicFame = std::max(0, std::min(100, h.musicFame + int(t.num(r, icol.c_str()))));
      }
      if (!picked.empty()) tell("The choice was: " + picked + ".");
    }
    if (line.empty()) {
      title = "Followers";
      body = "Screen name: " + h.tradeStage + ". Followers: " + std::to_string(h.tradeFollowers) + ".";
      h.tradeFollowers += 3;
      say("I picked up a few more followers on SoundCloud.");
    }
  } else if (kind == 3) {
    // --- The talent scout --------------------------------------------------
    fallback = "a16_music-talent-scout-potential-talent-string-enc";
    if (tradeRng_.chance(0.45) && ensureTable("a16_music-talent-scout-venue-name-enc")) {
      const Table& vt = tbl("a16_music-talent-scout-venue-name-enc");
      const size_t vr = allOf(vt)[tradeRng_.below(uint32_t(vt.size()))];
      const std::string venue = cell(vt, vr, "Venue");
      const std::string event = cell(vt, vr, "Event Type");
      mark("a16_music-talent-scout-venue-name-enc", vr);
      const size_t r = pick("a16_music-talent-scout-potential-talent-string-enc", "", nullptr, "",
                            "Min Vocal Quality", "Max Vocal Quality", double(h.tradeArtistVocal));
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_music-talent-scout-potential-talent-string-enc");
        warm(t, r);
        mark("a16_music-talent-scout-potential-talent-string-enc", r);
        artistVoice(true);
        const std::string act = h.tradeArtist;
        const std::string report =
            toFirstPerson(resolve(nameTakesSubject(trim(t.at(r, "String")), act)));
        artistVoice(false);
        title = "Scouting at " + (venue.empty() ? std::string("a venue") : venue);
        body = "I spent the night at " + (venue.empty() ? std::string("a venue") : venue) +
               (event.empty() ? std::string() : " (" + event + ")") + ".";
        say(report);
        // Half of the acts the scout likes are signed, the rest bounce off the
        // label with one of its reasons.
        if (tradeRng_.chance(0.5)) {
          h.tradeHits++;
          tell("The label signed " + act + ".");
        } else if (ensureTable("a16_music-talent-scout-artist-rejection-string-enc")) {
          const Table& rt = tbl("a16_music-talent-scout-artist-rejection-string-enc");
          const size_t rr = allOf(rt)[tradeRng_.below(uint32_t(rt.size()))];
          warm(rt, rr);
          artistVoice(true);
          const std::string no = toFirstPerson(cell(rt, rr, "String"));
          artistVoice(false);
          tell(no);
          h.tradeArtist.clear();
        }
      }
    }
    if (line.empty()) {
      h.tradeArtist.clear();
      tradeDrawArtist(life);
      title = "A night off";
      body = "Nothing at the venues worth signing.";
      say("I looked at a few acts and moved on.");
    }
  } else if (kind == 4) {
    // --- The composer ------------------------------------------------------
    fallback = "a16_music-score-production-project-title-enc";
    if (tradeRng_.chance(0.45) && ensureTable("a16_music-score-production-project-title-enc")) {
      const Table& pt = tbl("a16_music-score-production-project-title-enc");
      const size_t pr = weightedPick(pt, pt.has("Commonness Score") ? "Commonness Score" : std::string(), tradeRng_);
      const size_t r = (pr == size_t(-1) || pr >= pt.size()) ? tradeRng_.below(uint32_t(pt.size())) : pr;
      h.tradeProject = cell(pt, r, "Name");
      h.tradeProjectType = lower(cell(pt, r, "Project Type"));
      mark("a16_music-score-production-project-title-enc", r);
      if (!h.tradeProject.empty()) {
        // The tone and its BPM window, then the studio's answer.
        if (ensureTable("a16_music-score-production-project-tone-enc")) {
          const Table& tt = tbl("a16_music-score-production-project-tone-enc");
          const size_t tr = allOf(tt)[tradeRng_.below(uint32_t(tt.size()))];
          h.tradeTone = lower(trim(tt.at(tr, "Tone")));
          const int lo = int(tt.num(tr, "Min BPM"));
          const int hi = int(tt.num(tr, "Max BPM"));
          if (hi > lo) h.tradeBpm = lo + int(tradeRng_.below(uint32_t(hi - lo)));
        }
        const int quality = 30 + h.tradeRep / 2 + int(tradeRng_.below(31));
        const bool accepted = quality >= 55;
        title = accepted ? "The score is accepted" : "The score comes back";
        body = h.tradeProject + " (" + h.tradeProjectType + "), " + std::to_string(h.tradeBpm) + " BPM in " + h.tradeTone + ".";
        const char* tab = accepted ? "a16_music-score-production-acceptance-string-enc"
                                   : "a16_music-score-production-rejection-string-enc";
        if (ensureTable(tab)) {
          const Table& st = tbl(tab);
          std::vector<size_t> pool;
          for (size_t i = 0; i < st.size(); ++i) {
            if (st.has("Project Type")) {
              const std::string p = lower(trim(st.at(i, "Project Type")));
              if (!p.empty() && !h.tradeProjectType.empty() && p != h.tradeProjectType) continue;
            }
            pool.push_back(i);
          }
          if (pool.empty()) pool = allOf(st);
          const size_t sr = pool[tradeRng_.below(uint32_t(pool.size()))];
          mark(tab, sr);
          say(toFirstPerson(cell(st, sr, "String")));
        }
        if (accepted) {
          const long long fee = 8000 + static_cast<long long>(tradeRng_.below(20000));
          h.money += fee;
          h.tradeEarned += fee;
          h.tradeHits++;
          h.tradeRep = std::min(100, h.tradeRep + 3);
          tell("The fee came to " + money(fee) + ".");
        } else {
          h.tradeRep = std::max(0, h.tradeRep - 2);
          happy -= 2;
        }
      }
    }
    if (line.empty()) {
      title = "A quiet desk";
      body = "No project on the desk this year.";
      say("I spent the year waiting for a script worth scoring.");
    }
  } else {
    // --- The club takeover -------------------------------------------------
    fallback = "a16_music-club-takeover-string-enc";
    const bool stunted = tradeRng_.chance(0.35) && ensureTable("a16_music-club-takeover-stunt-enc");
    if (stunted) {
      const Table& st = tbl("a16_music-club-takeover-stunt-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < st.size(); ++r) if (genreFlagFits(st, r, "Artist Genre")) pool.push_back(r);
      if (pool.empty()) pool = allOf(st);
      const size_t sr = pool[tradeRng_.below(uint32_t(pool.size()))];
      warm(st, sr);
      mark("a16_music-club-takeover-stunt-enc", sr);
      artistVoice(true);
      const std::string act = cell(st, sr, "Scenario");
      const std::string own = cell(st, sr, "Log Version");
      artistVoice(false);
      const double pop = st.num(sr, "Popularity Impact");
      h.musicFame = std::max(0, std::min(100, h.musicFame + int(pop)));
      title = "A stunt in the club";
      body = act;
      say("The act: " + own + ".");
      if (tradeRng_.chance(0.5) && ensureTable("a16_music-club-takeover-stunt-outcome-enc")) {
        const Table& ot = tbl("a16_music-club-takeover-stunt-outcome-enc");
        const size_t orr = allOf(ot)[tradeRng_.below(uint32_t(ot.size()))];
        warm(ot, orr);
        mark("a16_music-club-takeover-stunt-outcome-enc", orr);
        artistVoice(true);
        const std::string outcome = cell(ot, orr, "Log Version");
        artistVoice(false);
        tell(outcome);
      }
    }
    if (line.empty() && tradeRng_.chance(0.25) && ensureTable("a16_music-club-takeover-string-enc")) {
      const Table& t = tbl("a16_music-club-takeover-string-enc");
      std::vector<size_t> good, bad;
      for (size_t r = 0; r < t.size(); ++r) (t.flag(r, "Positive") ? good : bad).push_back(r);
      std::vector<size_t>& pool = (h.musicFame >= 50 && !good.empty()) ? good : bad;
      if (pool.empty()) pool = allOf(t);
      const size_t r = pool[tradeRng_.below(uint32_t(pool.size()))];
      warm(t, r);
      mark("a16_music-club-takeover-string-enc", r);
      artistVoice(true);
      const std::string scen = cell(t, r, "Scenario");
      const std::string own = cell(t, r, "Log Version");
      artistVoice(false);
      const double pop = t.num(r, "Popularity Impact");
      h.musicFame = std::max(0, std::min(100, h.musicFame + int(pop)));
      title = "At the club";
      body = scen;
      say("The night: " + own + ".");
    }
    if (line.empty() && tradeRng_.chance(0.20) && ensureTable("a16_music-club-takeover-second-thought-enc")) {
      const Table& t = tbl("a16_music-club-takeover-second-thought-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        const std::string g = t.has("Artist Gender") ? lower(trim(t.at(r, "Artist Gender"))) : std::string();
        if (!g.empty() && g != "both" && ((g == "male") == h.tradeArtistFemale)) continue;
        if (!genreFlagFits(t, r, "Artist Genre")) continue;
        pool.push_back(r);
      }
      if (pool.empty()) pool = allOf(t);
      const size_t r = pool[tradeRng_.below(uint32_t(pool.size()))];
      warm(t, r);
      mark("a16_music-club-takeover-second-thought-enc", r);
      artistVoice(true);
      const std::string doubt =
          toFirstPerson(resolve(nameTakesSubject(trim(t.at(r, "String")), h.tradeArtist)));
      artistVoice(false);
      title = "Second thoughts";
      body = h.tradeArtist + " is not sure about this life.";
      happy -= 1;
      say(doubt);
    }
    if (line.empty()) {
      title = "A night at the club";
      body = "The tour went on.";
      h.musicFame = std::min(100, h.musicFame + 1);
      say("I took the act through another club and the crowd stayed to the end.");
    }
  }

  if (line.empty() && body.empty() && title.empty()) return false;
  const std::vector<std::string> keep = out.extra;
  out = AgeUp();
  out.table = tableRef.empty() ? fallback : tableRef;
  out.row_id = rowRef;
  out.title = title;
  out.body = body;
  out.narrative = line;
  out.extra = keep;
  for (size_t i = 0; i < extra.size(); ++i) {
    if (extra[i].empty()) continue;
    out.extra.push_back(extra[i]);
  }
  out.happiness += happy;
  return true;
}

// ---------------------------------------------------------------------------
// The vampire's own life (REPORT.en.md 70). Report 48 wired only the vampire's
// dumb-scenario pool; the family is forty-four tables that describe a hunt, a
// feed, a familiar, a council, a pack of hunters and the diseases the blood
// carries. The year below walks them in one chain, rarest first, and the
// dumb pool stays as the fallback.
// ---------------------------------------------------------------------------
std::string Sim::vampToken(const std::string& key) {
  if (hero_ == nullptr) return std::string();
  Hero& h = *const_cast<Hero*>(hero_);
  std::string k = key;
  const std::string psuffix = "-possessive";
  bool poss = false;
  if (k.size() > psuffix.size() && k.compare(k.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    poss = true;
    k = k.substr(0, k.size() - psuffix.size());
  }
  const auto own = [&](const std::string& name) { return poss ? llPossessive(name) : name; };
  const auto drawPerson = [&](bool female) { return drawFirstName(female) + " " + drawSurname(); };
  const auto speakAs = [&](bool female) {
    pronounOtherSet_ = true;
    pronounOtherFemale_ = female;
  };
  const auto cached = [&](const std::string& slot, bool female) -> std::string {
    std::map<std::string, std::string>::iterator it = vampCache_.find(slot);
    if (it != vampCache_.end()) return it->second;
    const std::string name = drawPerson(female);
    vampCache_[slot] = name;
    return name;
  };

  if (k == "victim") {
    // The dealer and the secret agent name victims of their own, so theirs win
    // when the card belongs to them (REPORT.en.md 70).
    if (!dealerVictim_.empty()) return own(dealerVictim_);
    if (!spyTarget_.empty()) return own(spyTarget_);
    if (h.vampireVictim.empty()) {
      vampVictimFemale_ = vampireRng_.chance(0.5);
      h.vampireVictim = drawPerson(vampVictimFemale_);
    }
    speakAs(vampVictimFemale_);
    return own(h.vampireVictim);
  }
  if (k == "familiar") {
    if (h.familiar.empty()) {
      h.vampireFamiliarFemale = vampireRng_.chance(0.5);
      h.familiar = drawFirstName(h.vampireFamiliarFemale);
    }
    speakAs(h.vampireFamiliarFemale);
    return own(h.familiar);
  }
  if (k == "vampire" || k == "vampire-name") {
    if (h.vampireName.empty()) {
      h.vampireName = vampireNpcName();
      h.vampireNpcFemale = vampireRng_.chance(0.5);
    }
    speakAs(h.vampireNpcFemale);
    return own(h.vampireName);
  }
  if (k == "maker") {
    if (h.vampireMaker.empty()) h.vampireMaker = cached("maker", vampireRng_.chance(0.5));
    speakAs(vampireRng_.chance(0.5));
    return own(h.vampireMaker);
  }
  if (k == "vampire-council-member") {
    const bool female = vampireRng_.chance(0.5);
    speakAs(female);
    return own(cached("council", female));
  }
  if (k == "amount") {
    // The mafia's temptation row and the vampire's hypnosis row both say
    // <amount>; whichever produced a number wins, else one is drawn.
    if (!mafiaAmount_.empty()) return mafiaAmount_;
    static const int kAmounts[] = {50, 100, 150, 200, 250, 500, 1000, 1500};
    const int pick = kAmounts[vampireRng_.below(8)];
    return "$" + std::to_string(pick);
  }
  if (k == "insult") {
    const Table& t = tbl("a16_insults-enc");
    std::vector<size_t> pool, vamp;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Gender")) {
        const std::string g = lower(trim(t.at(r, "Gender")));
        if (!g.empty() && g != "both" && g != "any" && g != "all" &&
            g != (h.female ? "female" : "male")) continue;
      }
      pool.push_back(r);
      if (t.has("Vampire") && t.flag(r, "Vampire")) vamp.push_back(r);
    }
    const std::vector<size_t>& use = vamp.empty() ? pool : vamp;
    if (use.empty()) return std::string("a fool");
    return trim(t.at(use[vampireRng_.below(uint32_t(use.size()))], "Insult"));
  }
  return std::string();
}

bool Sim::vampireYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.vampire) return false;
  if (!ensureTable("a16_vampire-dumb-scenario-enc")) return false;
  hero_ = &h;
  life_ = &life;

  // One vampire NPC for the whole year, so the card and its log line agree on
  // the name '<vampire>' renders (REPORT.en.md 48).
  h.vampireName = vampireNpcName();
  h.vampireNpcFemale = vampireRng_.chance(0.5);
  if (h.vampireFamiliarDesc.empty() && ensureTable("a16_vampire-familiar-color-enc")) {
    const size_t fr = pickRow("a16_vampire-familiar-color-enc", "", "", "", "", vampireRng_);
    if (fr != size_t(-1)) h.vampireFamiliarDesc = resolve(trim(tbl("a16_vampire-familiar-color-enc").at(fr, "Descriptive String")));
  }

  std::string title, body, line;
  std::vector<std::string> extra;
  std::string tableRef, rowRef;
  int happy = 0;

  const auto say = [&](const std::string& s) { if (line.empty() && !s.empty()) line = spyUpperFirst(s); };
  const auto tell = [&](const std::string& s) { if (!s.empty()) extra.push_back(spyUpperFirst(s)); };
  const auto cell = [&](const Table& t, size_t r, const std::string& col) -> std::string {
    if (!t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  const auto num = [&](const Table& t, size_t r, const std::string& col) -> int {
    if (!t.has(col)) return 0;
    return int(t.num(r, col));
  };
  const auto mark = [&](const std::string& tb, size_t r) {
    tableRef = tb;
    rowRef = std::to_string(r + 1);
  };
  const auto warm = [&](const Table& t, size_t r, const std::string& skip) {
    const std::vector<std::string>& cols = t.columns();
    for (size_t c = 0; c < cols.size(); ++c) {
      if (skip.size() && cols[c] == skip) continue;
      const std::string v = t.at(r, cols[c]);
      if (v.find('<') != std::string::npos) resolve(v);
    }
  };
  const auto rowOf = [&](const std::string& tab, const std::string& weight, const std::string& fcol,
                         const std::string& fval, const std::string& flag, bool fallbackAll) -> size_t {
    size_t r = pickRow(tab, weight, fcol, fval, flag, vampireRng_);
    if (r == size_t(-1) && fallbackAll) r = pickRow(tab, weight, "", "", flag, vampireRng_);
    return r;
  };
  const auto inWindow = [&](const Table& t, size_t r, const std::string& lo, const std::string& hi, int value) {
    if (t.has(lo)) { const std::string s = trim(t.at(r, lo)); if (!s.empty() && value < std::atoi(s.c_str())) return false; }
    if (t.has(hi)) { const std::string s = trim(t.at(r, hi)); if (!s.empty() && value > std::atoi(s.c_str())) return false; }
    return true;
  };
  const auto drawPerson = [&](bool female) { return drawFirstName(female) + " " + drawSurname(); };
  const auto genderFits = [&](const Table& t, size_t r, const std::string& col, bool female) {
    if (!t.has(col)) return true;
    const std::string g = lower(trim(t.at(r, col)));
    if (g.empty() || g == "both" || g == "all" || g == "any") return true;
    if (col == "Hero Gender") return g == (h.female ? "female" : "male");
    return g == (female ? "female" : "male");
  };

  // --- 21. A council decree: a code, its result and a member's opinion.
  if (line.empty() && vampireRng_.chance(0.06) && ensureTable("a16_vampire-code-data-enc")) {
    const Table& t = tbl("a16_vampire-code-data-enc");
    const size_t r = rowOf("a16_vampire-code-data-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-code-data-enc", r);
      title = "A decree of the council";
      body = "The code: " + trim(t.at(r, "Code"));
      say(cell(t, r, "Vampire NPC String"));
      tell(cell(t, r, "Results String"));
      h.vampireStanding += num(t, r, "Approval Impact");
      h.vampireTitle = trim(t.at(r, "Code"));
    }
  }
  // --- 20. V. Coli and the other indignities of the body.
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-vcoli-string-enc")) {
    const Table& t = tbl("a16_vampire-vcoli-string-enc");
    const size_t r = rowOf("a16_vampire-vcoli-string-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-vcoli-string-enc", r);
      warm(t, r, "Log Version");
      title = "The body keeps its receipts";
      say(cell(t, r, "Log Version"));
      tell(cell(t, r, "String"));
      h.vampireEssence -= 3;
    }
  }
  // --- 19. A vampire title, offered by the council (a16_vampire-titles).
  if (line.empty() && vampireRng_.chance(0.10) && ensureTable("a16_vampire-titles-enc")) {
    const Table& t = tbl("a16_vampire-titles-enc");
    const size_t r = rowOf("a16_vampire-titles-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-titles-enc", r);
      static const char* kOpts[] = {"Option 1", "Option 2", "Option 3", "Option 4", "Option 5"};
      std::vector<std::string> opts;
      for (int i = 0; i < 5; ++i) { const std::string o = trim(t.at(r, kOpts[i])); if (!o.empty()) opts.push_back(o); }
      const std::string tag = trim(t.at(r, "Scenario Tag"));
      title = opts.empty() ? trim(t.at(r, "Og Title")) : opts[vampireRng_.below(uint32_t(opts.size()))];
      body = "The council's ruling: " + tag + ".";
      say("The council now calls me " + title + ".");
      h.vampireTitle = title;
    }
  }
  // --- 18. An insult, thrown at the hero by another vampire.
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-insult-enc")) {
    const Table& t = tbl("a16_vampire-insult-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Gender")) {
        const std::string g = lower(trim(t.at(r, "Gender")));
        if (!g.empty() && g != "both" && g != "all" && g != "any" && g != (h.female ? "female" : "male")) continue;
      }
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      mark("a16_vampire-insult-enc", r);
      std::string word = trim(t.at(r, "Insult"));
      title = "An insult";
      body = "The word: " + word + ".";
      say(h.vampireName + " called me " + word + ".");
      happy -= 2;
    }
  }
  // --- 17. The lover asks to be turned (a16_vampire-lover-convert-nag).
  if (line.empty() && !h.loverName.empty() && vampireRng_.chance(0.10) &&
      ensureTable("a16_vampire-lover-convert-nag-enc")) {
    const Table& t = tbl("a16_vampire-lover-convert-nag-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!genderFits(t, r, "Hero Gender", h.female)) continue;
      if (!genderFits(t, r, "Lover Gender", h.loverFemale)) continue;
      if (!inWindow(t, r, "Min Relationship", "Max Relationship", h.relationship)) continue;
      const std::string ma = t.has("Married") ? lower(trim(t.at(r, "Married"))) : std::string();
      if (!ma.empty() && (ma == "x") != (h.loverStatus == "married")) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      mark("a16_vampire-lover-convert-nag-enc", r);
      warm(t, r, "Log Version");
      title = "A request";
      body = cell(t, r, "Scenario");
      say(cell(t, r, "Log Version"));
      h.relationship -= 4;
    }
  }
  // --- 16. Time spent with a person who bleeds (a16_vampire-spend-time and the
  // hunger the small accidents stir up).
  if (line.empty() && vampireRng_.chance(0.12) && ensureTable("a16_vampire-spend-time-enc")) {
    const Table& t = tbl("a16_vampire-spend-time-enc");
    const size_t r = rowOf("a16_vampire-spend-time-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-spend-time-enc", r);
      const std::string spent = toFirstPerson(cell(t, r, "String"));
      title = "An evening out";
      body = "I went " + spent + ".";
      say("I went " + spent + ".");
      h.vampireEssence += 1;
      if (vampireRng_.chance(0.45) && ensureTable("a16_vampire-spend-time-hunger-color-enc")) {
        const Table& u = tbl("a16_vampire-spend-time-hunger-color-enc");
        std::vector<size_t> pool;
        for (size_t k = 0; k < u.size(); ++k) {
          if (!genderFits(u, k, "Hero Gender", h.female)) continue;
          pool.push_back(k);
        }
        if (!pool.empty()) {
          const size_t k = pool[vampireRng_.below(uint32_t(pool.size()))];
          warm(u, k, "Log String");
          tell(cell(u, k, "Log String"));
          h.vampireHunger += 8;
        }
      }
    }
  }
  // --- 15. A night out, and the stranger who wants to turn the hero.
  if (line.empty() && vampireRng_.chance(0.10) && ensureTable("a16_vampire-clubbing-approache-enc")) {
    const Table& t = tbl("a16_vampire-clubbing-approache-enc");
    const size_t r = rowOf("a16_vampire-clubbing-approache-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-clubbing-approache-enc", r);
      warm(t, r, "Log Version");
      title = "A night out";
      body = cell(t, r, "String");
      say(cell(t, r, "Log Version"));
    }
  }
  // --- 14. The witch doctor's remedy, kept for the days the blood bites back.
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-witch-doctor-remedie-enc")) {
    const Table& t = tbl("a16_vampire-witch-doctor-remedie-enc");
    const size_t r = rowOf("a16_vampire-witch-doctor-remedie-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-witch-doctor-remedie-enc", r);
      const std::string item = trim(t.at(r, "Item"));
      const int eff = num(t, r, "Effectiveness Score");
      const int risk = num(t, r, "Risk Score");
      title = "The witch doctor";
      body = "The remedy: " + item + ".";
      if (eff * 20 > int(vampireRng_.below(100))) {
        say("I treated myself with " + item + " and it worked.");
        if (!h.vampireDisease.empty()) { h.vampireDisease.clear(); }
        h.vampireEssence += 4;
      } else {
        say("I tried " + item + " and it did nothing.");
        h.vampireEssence -= risk;
      }
    }
  }
  // --- 13. Disposing of what is left (a16_vampire-body-dump-color).
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-body-dump-color-enc")) {
    const Table& t = tbl("a16_vampire-body-dump-color-enc");
    const size_t r = rowOf("a16_vampire-body-dump-color-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-body-dump-color-enc", r);
      h.vampireVictim = drawPerson(vampVictimFemale_ = vampireRng_.chance(0.5));
      title = "Cleaning up";
      body = "Left at: " + cell(t, r, "Location") + ".";
      say(toFirstPerson(cell(t, r, "String")));
      h.vampireNotoriety += 2;
    }
  }
  // --- 12. The coffin (a16_vampire-coffin-string).
  if (line.empty() && vampireRng_.chance(0.10) && ensureTable("a16_vampire-coffin-string-enc")) {
    const Table& t = tbl("a16_vampire-coffin-string-enc");
    const size_t r = rowOf("a16_vampire-coffin-string-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-coffin-string-enc", r);
      warm(t, r, "Log Version");
      title = "Daylight hours";
      body = toFirstPerson(cell(t, r, "Scenario"));
      say(cell(t, r, "Log Version"));
      happy += 1;
    }
  }
  // --- 11. Blood offered freely, or refused (offering string and pact
  // rejection).
  if (line.empty() && vampireRng_.chance(0.10) && ensureTable("a16_vampire-blood-offering-string-enc") &&
      vampireRng_.chance(0.65)) {
    const Table& t = tbl("a16_vampire-blood-offering-string-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!genderFits(t, r, "Hero Gender", h.female)) continue;
      if (t.has("Offering Gender")) {
        const std::string g = lower(trim(t.at(r, "Offering Gender")));
        if (!g.empty() && g != "both" && g != "all" && g != "any") continue;   // the cell names the offerer
      }
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      mark("a16_vampire-blood-offering-string-enc", r);
      h.vampireVictim = drawPerson(vampVictimFemale_ = vampireRng_.chance(0.5));
      title = "An offering";
      body = "From: " + h.vampireVictim + ".";
      say(toFirstPerson(cell(t, r, "String")));
      h.vampireEssence += 6;
      h.vampireHunger -= 10;
    }
  }
  if (line.empty() && vampireRng_.chance(0.10) && ensureTable("a16_vampire-blood-pact-rejection-string-enc")) {
    const Table& t = tbl("a16_vampire-blood-pact-rejection-string-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!inWindow(t, r, "Min Relationship", "Max Relationship", h.relationship)) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      mark("a16_vampire-blood-pact-rejection-string-enc", r);
      const std::string who = t.has("Person Type") ? trim(t.at(r, "Person Type")) : std::string();
      title = "A refusal";
      body = who.empty() ? "Someone said no." : ("The " + who + " said no.");
      say(toFirstPerson(cell(t, r, "String")));
      h.vampireEssence -= 2;
    }
  }
  // --- 10. A blood bath with the council.
  if (line.empty() && vampireRng_.chance(0.06) && ensureTable("a16_vampire-blood-bath-scenario-enc")) {
    const Table& t = tbl("a16_vampire-blood-bath-scenario-enc");
    const size_t r = rowOf("a16_vampire-blood-bath-scenario-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-blood-bath-scenario-enc", r);
      warm(t, r, "Log Version");
      title = "A blood bath";
      body = "The activity: " + trim(t.at(r, "Activity")) + ".";
      say(cell(t, r, "Log Version"));
      h.vampireEssence += num(t, r, "Max Essence Impact");
      happy += num(t, r, "Max Happiness Impact");
      h.relationship += num(t, r, "Max Relationship Impact");
    }
  }
  // --- 9. Council business: a member is disciplined, and the hero may be the
  // one holding the whip.
  if (line.empty() && vampireRng_.chance(0.06) && ensureTable("a16_vampire-council-discipline-scenario-enc")) {
    const Table& t = tbl("a16_vampire-council-discipline-scenario-enc");
    const size_t r = rowOf("a16_vampire-council-discipline-scenario-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-council-discipline-scenario-enc", r);
      warm(t, r, "Log Version");
      title = "The council sits";
      body = cell(t, r, "String");
      const bool kill = t.has("Kill") && t.flag(r, "Kill");
      say(cell(t, r, "Log Version") + (kill ? " The council put them down for it." : ""));
      h.vampireStanding += kill ? 3 : 1;
      if (vampireRng_.chance(0.5) && ensureTable("a16_vampire-council-punishment-enc")) {
        const size_t p = rowOf("a16_vampire-council-punishment-enc", "Commonness Score", "", "", "", false);
        if (p != size_t(-1)) {
          warm(tbl("a16_vampire-council-punishment-enc"), p, "Log Version");
          tell(cell(tbl("a16_vampire-council-punishment-enc"), p, "Log Version"));
        }
      }
    }
  }
  if (line.empty() && vampireRng_.chance(0.05) && ensureTable("a16_vampire-council-reward-enc")) {
    const Table& t = tbl("a16_vampire-council-reward-enc");
    const size_t r = rowOf("a16_vampire-council-reward-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-council-reward-enc", r);
      warm(t, r, "Log Version");
      title = "A reward";
      body = cell(t, r, "Scenario");
      say(cell(t, r, "Log Version"));
      h.vampireStanding += 2;
    }
  }
  // --- 8. A council gathering, and whatever the guests overdosed on.
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-council-party-string-enc")) {
    const Table& t = tbl("a16_vampire-council-party-string-enc");
    const size_t r = rowOf("a16_vampire-council-party-string-enc", "Commonness Score", "", "", "", false);
    const size_t pt = rowOf("a16_vampire-council-party-type-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1) && pt != size_t(-1) && ensureTable("a16_vampire-council-party-type-enc")) {
      mark("a16_vampire-council-party-string-enc", r);
      warm(t, r, "Log Version");
      title = "A gathering";
      body = "The party: " + trim(tbl("a16_vampire-council-party-type-enc").at(pt, "Party Type")) + ".";
      say(cell(t, r, "Log Version"));
      h.vampireStanding += num(t, r, "Max Standing Impact");
      happy += num(t, r, "Max Looks Impact") + num(t, r, "Max Smarts Impact");
    }
  }
  // --- 7. Enticing and assaulting: the two ways a hunt ends.
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-entice-method-enc")) {
    const Table& t = tbl("a16_vampire-entice-method-enc");
    const size_t r = rowOf("a16_vampire-entice-method-enc", "", "", "", "", false);
    if (r != size_t(-1) && !h.vampireVictim.empty()) {
      mark("a16_vampire-entice-method-enc", r);
      warm(t, r, "Success Log Version");
      const bool ok = num(t, r, "Effectiveness Score") * 18 > int(vampireRng_.below(100));
      title = ok ? "The lure worked" : "The lure failed";
      pronounOtherSet_ = true;
      pronounOtherFemale_ = vampVictimFemale_;
      body = "Method: " + cell(t, r, "Method") + ".";
      pronounOtherSet_ = false;
      say(cell(t, r, ok ? "Success Log Version" : "Failure Log Version"));
      h.vampireEssence += ok ? 5 : -2;
      if (!ok) h.vampireNotoriety += 1;
    }
  }
  if (line.empty() && vampireRng_.chance(0.08) && ensureTable("a16_vampire-assault-method-enc")) {
    const Table& t = tbl("a16_vampire-assault-method-enc");
    const size_t r = rowOf("a16_vampire-assault-method-enc", "", "", "", "", false);
    if (r != size_t(-1) && !h.vampireVictim.empty()) {
      mark("a16_vampire-assault-method-enc", r);
      warm(t, r, "Success Log Version");
      const bool ok = num(t, r, "Effectiveness Score") * 18 > int(vampireRng_.below(100));
      title = ok ? "Taken by force" : "It went badly";
      pronounOtherSet_ = true;
      pronounOtherFemale_ = vampVictimFemale_;
      body = "Method: " + cell(t, r, "Method") + ".";
      pronounOtherSet_ = false;
      say(cell(t, r, ok ? "Success Log Version" : "Failure Log Version"));
      h.vampireEssence += ok ? 6 : -3;
      h.vampireNotoriety += ok ? 1 : 3;
    }
  }
  // --- 6. Hunted in turn: a hunter finds the hero.
  if (line.empty() && vampireRng_.chance(0.12) && ensureTable("a16_vampire-hunter-encounter-scenario-enc")) {
    const Table& t = tbl("a16_vampire-hunter-encounter-scenario-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!genderFits(t, r, "Hero Gender", h.female)) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
      mark("a16_vampire-hunter-encounter-scenario-enc", r);
      const bool female = vampireRng_.chance(0.5);
      const std::string hunter = drawFirstName(female) + " " + drawSurname();
      pronounOtherSet_ = true;
      pronounOtherFemale_ = female;
      title = "A hunter";
      body = "They gave a name: " + hunter + ".";
      say(cell(t, r, "Scenario"));
      pronounOtherSet_ = false;
      h.vampireNotoriety += 2;
      if (ensureTable("a16_vampire-hunter-appreciation-string-enc")) {
        const Table& a = tbl("a16_vampire-hunter-appreciation-string-enc");
        std::vector<size_t> apool;
        for (size_t k = 0; k < a.size(); ++k) {
          if (!genderFits(a, k, "Hero Gender", h.female)) continue;
          if (a.has("Vampire Hero") && a.flag(k, "Vampire Hero") && !h.vampire) continue;
          apool.push_back(k);
        }
        if (!apool.empty()) {
          const size_t k = apool[vampireRng_.below(uint32_t(apool.size()))];
          tell(cell(a, k, "String"));
        }
      }
    }
  }
  // --- 5. Hypnosis: an animal, a wallet, a lover or a breakup.
  if (line.empty() && vampireRng_.chance(0.12) && !h.vampireVictim.empty()) {
    const Table* ht = nullptr;
    const char* htable = nullptr;
    const int roll = int(vampireRng_.below(4));
    if (roll == 0 && ensureTable("a16_vampire-hypnosis-animal-scenario-enc")) { ht = &tbl("a16_vampire-hypnosis-animal-scenario-enc"); htable = "a16_vampire-hypnosis-animal-scenario-enc"; }
    else if (roll == 1 && ensureTable("a16_vampire-hypnosis-money-string-enc")) { ht = &tbl("a16_vampire-hypnosis-money-string-enc"); htable = "a16_vampire-hypnosis-money-string-enc"; }
    else if (roll == 2 && ensureTable("a16_vampire-hypnosis-love-string-enc")) { ht = &tbl("a16_vampire-hypnosis-love-string-enc"); htable = "a16_vampire-hypnosis-love-string-enc"; }
    else if (ensureTable("a16_vampire-hypnosis-breakup-string-enc")) { ht = &tbl("a16_vampire-hypnosis-breakup-string-enc"); htable = "a16_vampire-hypnosis-breakup-string-enc"; }
    if (ht != nullptr) {
      const Table& t = *ht;
      const size_t r = rowOf(htable, "Commonness Score", "", "", "", false);
      if (r != size_t(-1)) {
        mark(htable, r);
        warm(t, r, "Log Version");
        title = "A compulsion";
        body = "The victim: " + h.vampireVictim + ".";
        say(cell(t, r, "Log Version"));
        if (std::string(htable).find("money") != std::string::npos) {
          const int lo = num(t, r, "Min Money"), hi = num(t, r, "Max Money");
          const long long got = (lo + int(vampireRng_.below(uint32_t(hi > lo ? hi - lo + 1 : 1)))) * 100;
          h.money += got;
          tell("That was worth " + money(got) + " to me.");
        }
        h.vampireEssence -= 1;
      }
    }
  }
  // --- 4. The familiar: ordered about, confused, threatened or dumped.
  if (line.empty() && vampireRng_.chance(0.15)) {
    const int roll = int(vampireRng_.below(4));
    const char* ftable = nullptr;
    if (roll == 0 && ensureTable("a16_vampire-familiar-command-enc")) ftable = "a16_vampire-familiar-command-enc";
    else if (roll == 1 && ensureTable("a16_vampire-familiar-confused-string-enc")) ftable = "a16_vampire-familiar-confused-string-enc";
    else if (roll == 2 && ensureTable("a16_vampire-familiar-threat-enc")) ftable = "a16_vampire-familiar-threat-enc";
    else if (ensureTable("a16_vampire-familiar-dump-enc")) ftable = "a16_vampire-familiar-dump-enc";
    if (ftable != nullptr) {
      const Table& t = tbl(ftable);
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        if (!genderFits(t, r, "Familiar Gender", h.vampireFamiliarFemale)) continue;
        if (!inWindow(t, r, "Min Familiar Smarts", "Max Familiar Smarts", h.vampireFamiliarSmarts)) continue;
        pool.push_back(r);
      }
      if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
      if (!pool.empty()) {
        const size_t r = pool[vampireRng_.below(uint32_t(pool.size()))];
        mark(ftable, r);
        warm(t, r, "Log Version");
        title = "My familiar";
        body = h.vampireFamiliarDesc.empty() ? ("My familiar: " + h.familiar + ".") : ("My familiar " + h.vampireFamiliarDesc + ".");
        const std::string lg = t.has("Log Version") ? cell(t, r, "Log Version") : std::string();
        say(lg.empty() ? cell(t, r, "String") : lg);
        const int rel = t.has("Relationship Impact") ? num(t, r, "Relationship Impact") : 0;
        h.vampireFamiliarRel += rel;
        if (std::string(ftable).find("dump") != std::string::npos) {
          h.vampireNotoriety += num(t, r, "Risk Score");
          h.familiar.clear();
        }
      }
    }
  }
  // --- 3. A feed, the taste of it, and whatever the blood carried.
  if (line.empty() && vampireRng_.chance(0.20) && ensureTable("a16_vampire-feeding-location-color-enc")) {
    const Table& t = tbl("a16_vampire-feeding-location-color-enc");
    const size_t r = rowOf("a16_vampire-feeding-location-color-enc", "", "", "", "", false);
    if (r != size_t(-1)) {
      mark("a16_vampire-feeding-location-color-enc", r);
      warm(t, r, "Log Version");
      // The risk of feeding where the hero fed (a16_vampire-feeding-location).
      int risk = 1;
      if (ensureTable("a16_vampire-feeding-location-enc")) {
        const Table& l = tbl("a16_vampire-feeding-location-enc");
        const std::string want = lower(trim(t.at(r, "Location")));
        for (size_t k = 0; k < l.size(); ++k) {
          if (lower(trim(l.at(k, "Location"))) == want) { risk = int(l.num(k, "Risk Score")); break; }
        }
      }
      h.vampireVictim = drawPerson(vampVictimFemale_ = vampireRng_.chance(0.5));
      title = "A feed";
      body = "The victim: " + h.vampireVictim + " (risk " + std::to_string(risk) + ").";
      say(cell(t, r, "Log Version"));
      h.vampireEssence += 8;
      h.vampireHunger = 0;
      // The taste, and whether it carried something (REPORT.en.md 70).
      if (ensureTable("a16_vampire-blood-taste-color-enc")) {
        const Table& b = tbl("a16_vampire-blood-taste-color-enc");
        const size_t tr = b.size() ? vampireRng_.below(uint32_t(b.size())) : size_t(-1);
        if (tr != size_t(-1)) {
          tell("The blood tasted " + resolve(trim(b.at(tr, "Taste"))) + ".");
          if (b.has("Disease Indicator") && b.flag(tr, "Disease Indicator") && ensureTable("a16_vampire-disease-enc")) {
            const Table& d = tbl("a16_vampire-disease-enc");
            if (d.size()) {
              h.vampireDisease = trim(d.at(vampireRng_.below(uint32_t(d.size())), "Disease"));
              tell("I caught " + h.vampireDisease + " from it.");
              h.vampireEssence -= 10;
            }
          }
        }
      }
      // How healthy the victim was, and whether the hero should stop.
      if (ensureTable("a16_vampire-victim-status-color-enc")) {
        const Table& v = tbl("a16_vampire-victim-status-color-enc");
        const int health = 30 + int(vampireRng_.below(71));
        std::vector<size_t> pool;
        for (size_t k = 0; k < v.size(); ++k) if (inWindow(v, k, "Min Victim Health", "Max Victim Health", health)) pool.push_back(k);
        if (!pool.empty()) {
          pronounOtherSet_ = true;
          pronounOtherFemale_ = vampVictimFemale_;
          tell(cell(v, pool[vampireRng_.below(uint32_t(pool.size()))], "String"));
          pronounOtherSet_ = false;
        }
      }
      if (risk >= 3) h.vampireNotoriety += 1;
    }
  }
  // --- 2. A hunt: where it happened, what the hero preyed on and who joined.
  if (line.empty() && vampireRng_.chance(0.25) && ensureTable("a16_vampire-solo-hunt-location-enc")) {
    const Table& t = tbl("a16_vampire-solo-hunt-location-enc");
    const size_t r = rowOf("a16_vampire-solo-hunt-location-enc", "Commonness Score", "", "", "", false);
    if (r != size_t(-1) && ensureTable("a16_vampire-hunt-string-enc")) {
      const Table& u = tbl("a16_vampire-hunt-string-enc");
      const size_t hrow = rowOf("a16_vampire-hunt-string-enc", "", "", "", "", false);
      if (hrow != size_t(-1)) {
        mark("a16_vampire-hunt-string-enc", hrow);
        warm(u, hrow, "Log Version");
        title = "The hunt";
        body = "The night: " + trim(t.at(r, "String Version")) + ".";
        say(cell(u, hrow, "Log Version"));
        h.vampireEssence += num(u, hrow, "Essence Impact");
        h.relationship += num(u, hrow, "Relationship Impact");
        happy += num(u, hrow, "Enjoyment Score");
        if (ensureTable("a16_vampire-hunt-encounter-color-enc")) {
          const Table& e = tbl("a16_vampire-hunt-encounter-color-enc");
          std::vector<size_t> pool;
          for (size_t k = 0; k < e.size(); ++k) {
            if (!inWindow(e, k, "Min Vampire Notoriety", "Max Vampire Notoriety", h.vampireNotoriety)) continue;
            pool.push_back(k);
          }
          if (!pool.empty()) tell(cell(e, pool[vampireRng_.below(uint32_t(pool.size()))], "String"));
        }
      }
    }
  }
  // --- 1. The pool the hero already had: a16_vampire-dumb-scenario-enc.
  if (line.empty()) {
    const Table& t = tbl("a16_vampire-dumb-scenario-enc");
    // scenarioEligible covers the table's own age / gender / lover gates; its
    // Notoriety, Essence and Hero Has Familiar gates belong to the vampire
    // attribute system the port models with the hero's own numbers, so the
    // window is applied here (REPORT.en.md 48, 70).
    std::vector<size_t> rows;
    for (size_t r = 0; r < t.size(); ++r) {
      if (!scenarioEligible(t, r, life)) continue;
      if (!inWindow(t, r, "Min Notoriety", "Max Notoriety", h.vampireNotoriety)) continue;
      if (t.has("Hero Has Familiar") && t.flag(r, "Hero Has Familiar") && h.familiar.empty()) continue;
      rows.push_back(r);
    }
    const size_t r = pickFrom(rows, vampireRng_);
    if (r != size_t(-1)) {
      const std::vector<std::string> incoming = out.extra;
      if (!renderRow(t, r, life, out, forcedChoice)) return false;
      const std::vector<std::string> rendered = out.extra;
      out.extra = incoming;
      for (size_t k = 0; k < rendered.size(); ++k) out.extra.push_back(rendered[k]);
      for (size_t k = 0; k < extra.size(); ++k) out.extra.push_back(extra[k]);
      return true;
    }
    return false;
  }

  out.table = tableRef.empty() ? "a16_vampire-dumb-scenario-enc" : tableRef;
  out.row_id = rowRef;
  out.title = title;
  out.body = body;
  out.narrative = line;
  const std::vector<std::string> keep = out.extra;
  out.extra.clear();
  for (size_t k = 0; k < keep.size(); ++k) out.extra.push_back(keep[k]);
  for (size_t k = 0; k < extra.size(); ++k) out.extra.push_back(extra[k]);
  out.happiness += happy;
  return true;
}

// ── The great outdoors (REPORT.en.md 71) ────────────────────────────────────
// Forty-two tables around six activities: a hike, a campsite, a fishing rod,
// an off-roader, a bicycle and a snorkel. The hero owns an off-roader and a
// bicycle, takes one trip a year, and the card is assembled from the tables
// that describe the destination, its wildlife and the gear.
void Sim::makeOutdoor(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  h.outdoors = true;
  h.outdoorJoy = 55;
  h.outdoorBikeCondition = 85 + static_cast<int>(outdoorRng_.below(16));
  h.outdoorTrips = 0;
  h.outdoorCatches = 0;
  h.outdoorEarned = 0;
  if (ensureTable("a16_outdoor-vehicle-enc")) {
    const Table& v = tbl("a16_outdoor-vehicle-enc");
    std::vector<size_t> offroad, pedal;
    for (size_t r = 0; r < v.size(); ++r) {
      const std::string type = lower(trim(v.at(r, "Type")));
      if (type.find("atv") != std::string::npos) offroad.push_back(r);
      else if (type.find("dirt") == std::string::npos) pedal.push_back(r);
    }
    if (!offroad.empty()) {
      const size_t r = offroad[outdoorRng_.below(uint32_t(offroad.size()))];
      h.outdoorVehicle = trim(v.at(r, "Make")) + " " + trim(v.at(r, "Model"));
    }
    if (!pedal.empty()) {
      const size_t r = pedal[outdoorRng_.below(uint32_t(pedal.size()))];
      h.outdoorBike = trim(v.at(r, "Make")) + " " + trim(v.at(r, "Model"));
    }
  }
  if (h.outdoorVehicle.empty()) h.outdoorVehicle = "ATV";
  if (h.outdoorBike.empty()) h.outdoorBike = "Mountain Bike";
}

// The eight keys of the family, plus the '< pet>' spelling the data uses once.
// A species is drawn once per card and remembered, so a row that names the
// plant twice agrees with itself and its '<rb>' points back at the draw.
std::string Sim::outdoorToken(const std::string& key) {
  if (hero_ == nullptr) return std::string();
  Hero& h = *const_cast<Hero*>(hero_);
  std::string k = trim(key);
  const std::string psuffix = "-possessive";
  bool poss = false;
  if (k.size() > psuffix.size() && k.compare(k.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    poss = true;
    k = k.substr(0, k.size() - psuffix.size());
  }
  const auto own = [&](const std::string& name) { return poss ? llPossessive(name) : name; };
  const auto drawPerson = [&](bool female) { return drawFirstName(female) + " " + drawSurname(); };
  const auto remembered = [&](const std::string& slot) -> std::string {
    std::map<std::string, std::string>::iterator it = outdoorCache_.find(slot);
    return it == outdoorCache_.end() ? std::string() : it->second;
  };
  const auto remember = [&](const std::string& slot, const std::string& value) {
    if (!value.empty()) outdoorCache_[slot] = value;
    return value;
  };
  const auto drawSpecies = [&](const std::string& slot, const char* table, const char* column) -> std::string {
    const std::string have = remembered(slot);
    if (!have.empty()) return have;
    if (!ensureTable(table)) return std::string();
    const Table& t = tbl(table);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Habitat") && !h.outdoorHabitat.empty()) {
        const std::string v = lower(trim(t.at(r, "Habitat")));
        if (!v.empty() && !llInList(v, h.outdoorHabitat)) continue;
      }
      pool.push_back(r);
    }
    if (pool.empty()) {
      const Table& all = tbl(table);
      for (size_t r = 0; r < all.size(); ++r) pool.push_back(r);
    }
    if (pool.empty()) return std::string();
    return remember(slot, resolve(trim(t.at(pool[outdoorRng_.below(uint32_t(pool.size()))], column))));
  };

  if (k == "outdoor-location")
    return own(h.outdoorPlace.empty() ? std::string("the trail") : h.outdoorPlace);
  if (k == "outdoor-landmark")
    return own(h.outdoorLandmark.empty() ? std::string("the view") : h.outdoorLandmark);
  if (k == "bicycle")
    return own(h.outdoorBike.empty() ? std::string("bicycle") : h.outdoorBike);
  if (k == "outdoor-plant") {
    const std::string plant = drawSpecies("plant", "a16_outdoor-plant-enc", "Species");
    return own(plant.empty() ? std::string("wildflower") : plant);
  }
  if (k == "outdoor-bird") {
    const std::string bird = drawSpecies("bird", "a16_outdoor-bird-enc", "Species");
    return own(bird.empty() ? std::string("bird") : bird);
  }
  if (k == "outdoor-marine-life") {
    const std::string animal = drawSpecies("marine", "a16_outdoor-marine-life-enc", "Species");
    return own(animal.empty() ? std::string("fish") : animal);
  }
  if (k == "outdoor-cave") {
    if (h.outdoorCave.empty()) h.outdoorCave = remembered("cave");
    if (h.outdoorCave.empty() && ensureTable("a16_outdoor-cave-enc")) {
      const Table& t = tbl("a16_outdoor-cave-enc");
      std::vector<size_t> pool;
      for (size_t r = 0; r < t.size(); ++r) {
        if (lower(trim(t.at(r, "Location"))) == lower(h.country)) pool.push_back(r);
      }
      if (pool.empty()) for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
      if (!pool.empty()) h.outdoorCave = trim(t.at(pool[outdoorRng_.below(uint32_t(pool.size()))], "Name"));
      remember("cave", h.outdoorCave);
    }
    return own(h.outdoorCave.empty() ? std::string("the cave") : h.outdoorCave);
  }
  if (k == "outdoor-companion") {
    if (h.outdoorCompanion.empty()) {
      h.outdoorCompanionFemale = outdoorRng_.chance(0.5);
      h.outdoorCompanion = drawPerson(h.outdoorCompanionFemale);
    }
    return own(h.outdoorCompanion);
  }
  if (k == "pet") return own(h.petName.empty() ? h.petAnimal : h.petName);
  return std::string();
}

bool Sim::outdoorYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.outdoors) return false;
  if (h.age < 8) return false;
  if (!ensureTable("a16_outdoor-destination-enc")) return false;
  if (!outdoorRng_.chance(0.5)) return false;
  hero_ = &h;
  life_ = &life;

  std::string title, body, line;
  std::vector<std::string> extra;
  std::string tableRef, rowRef;
  int happy = 0;

  const auto say = [&](const std::string& s) {
    if (line.empty() && !s.empty()) line = spyUpperFirst(s);
  };
  const auto add = [&](const std::string& s) { if (!s.empty()) extra.push_back(spyUpperFirst(s)); };
  const auto cell = [&](const Table& t, size_t r, const char* col) -> std::string {
    if (r == size_t(-1) || !t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  const auto mark = [&](const char* tab, size_t r) { tableRef = tab; rowRef = std::to_string(r + 1); };
  // One draw from a table, filtered on an optional column, windowed on
  // optional Min/Max columns, weighted on an optional Commonness Score.
  const auto window = [&](const char* tab, const char* weight, const char* col,
                          const std::string& want, const char* lo, const char* hi,
                          double value, bool exact) -> size_t {
    if (!ensureTable(tab)) return size_t(-1);
    const Table& t = tbl(tab);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (col != nullptr && *col && t.has(col) && !want.empty()) {
        const std::string v = lower(trim(t.at(r, col)));
        if (!v.empty()) {
          const bool fits = exact ? (v == lower(want)) : llInList(v, lower(want));
          if (!fits) continue;
        }
      }
      if (lo != nullptr && *lo && t.has(lo)) { const double l = t.num(r, lo); if (l > 0 && value < l) continue; }
      if (hi != nullptr && *hi && t.has(hi)) { const double u = t.num(r, hi); if (u > 0 && value > u) continue; }
      pool.push_back(r);
    }
    if (pool.empty()) {
      const Table& all = tbl(tab);
      for (size_t r = 0; r < all.size(); ++r) pool.push_back(r);
    }
    if (pool.empty()) return size_t(-1);
    if (weight != nullptr && *weight && t.has(weight)) {
      const size_t r = weightedPickOf(t, weight, pool, outdoorRng_);
      if (r != size_t(-1)) return r;
    }
    return pool[outdoorRng_.below(uint32_t(pool.size()))];
  };
  // Rows whose log line refers back to an alternative drawn earlier in the row
  // ('<rb>' / '<rb2>' / '<rb3>'): every other bracketed column is resolved
  // first, in column order, which is the order the game draws them in, so the
  // reference lands on the alternative the card really drew (REPORT.en.md 57).
  const auto withBrackets = [&](const Table& t, size_t r, const char* wantCol) -> std::string {
    if (r == size_t(-1)) return std::string();
    for (const std::string& cc : t.columns()) {
      if (cc == wantCol) continue;
      const std::string& s = t.at(r, cc);
      if (s.find('<') == std::string::npos) continue;
      cell(t, r, cc.c_str());
    }
    return cell(t, r, wantCol);
  };
  // A row of one of the family's four dumb-scenario tables whose age and
  // companion gates fit; renderRow plays the rest of the card (report 56).
  const auto cardRow = [&](const char* tab) -> size_t {
    if (!ensureTable(tab)) return size_t(-1);
    const Table& t = tbl(tab);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Hero Min Age")) { const double lo = t.num(r, "Hero Min Age"); if (lo > 0 && h.age < lo) continue; }
      if (t.has("Hero Max Age")) { const double hi = t.num(r, "Hero Max Age"); if (hi > 0 && h.age > hi) continue; }
      if (t.has("Requires Companion") && t.flag(r, "Requires Companion") && h.outdoorCompanion.empty()) continue;
      pool.push_back(r);
    }
    if (pool.empty()) return size_t(-1);
    return pool[outdoorRng_.below(uint32_t(pool.size()))];
  };
  // A landmark row, gated on the age and on whether anyone came along.
  const auto landmarkRow = [&](const char* tab) -> size_t {
    if (!ensureTable(tab)) return size_t(-1);
    const Table& t = tbl(tab);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has("Min Hero Age")) { const double lo = t.num(r, "Min Hero Age"); if (lo > 0 && h.age < lo) continue; }
      if (t.has("Max Hero Age")) { const double hi = t.num(r, "Max Hero Age"); if (hi > 0 && h.age > hi) continue; }
      if (t.has("Requires Companion") && t.flag(r, "Requires Companion") &&
          h.outdoorCompanion.empty()) continue;
      if (t.has("Companion Gender") && !h.outdoorCompanion.empty()) {
        const std::string g = lower(trim(t.at(r, "Companion Gender")));
        if (!g.empty() && g != "both" && g != (h.outdoorCompanionFemale ? "female" : "male")) continue;
      }
      pool.push_back(r);
    }
    if (pool.empty()) return size_t(-1);
    const size_t r = weightedPickOf(t, "Commonness Score", pool, outdoorRng_);
    return r == size_t(-1) ? pool[outdoorRng_.below(uint32_t(pool.size()))] : r;
  };
  // One animal, plant or bird of the destination's habitat.
  const auto wildlife = [&](const char* tab) -> size_t {
    return window(tab, "Commonness Score", "Habitat", h.outdoorHabitat, "", "", 0, false);
  };

  // One companion for the trip, so every '<outdoor-companion>' on the card is
  // the same person (REPORT.en.md 71.4).
  h.outdoorCompanion.clear();
  if (h.relationshipOn && !h.loverName.empty() && outdoorRng_.chance(0.45)) {
    h.outdoorCompanion = h.loverName;
    h.outdoorCompanionFemale = h.loverFemale;
  } else if (outdoorRng_.chance(0.35)) {
    h.outdoorCompanionFemale = outdoorRng_.chance(0.5);
    h.outdoorCompanion = drawFirstName(h.outdoorCompanionFemale) + " " + drawSurname();
  }
  const bool savedSet = pronounOtherSet_;
  const bool savedFemale = pronounOtherFemale_;
  const auto companionOn = [&]() {
    pronounOtherSet_ = !h.outdoorCompanion.empty();
    pronounOtherFemale_ = h.outdoorCompanionFemale;
  };
  const auto companionOff = [&]() { pronounOtherSet_ = savedSet; pronounOtherFemale_ = savedFemale; };
  const bool hasDog = h.hasPet && !h.petDead && lower(h.petAnimal).find("dog") != std::string::npos;

  // The activity of the year.
  static const struct { const char* name; double weight; } kActivities[] = {
      {"hike", 0.30}, {"camp", 0.22}, {"fish", 0.20},
      {"offroad", 0.13}, {"bike", 0.10}, {"snorkel", 0.05}};
  std::string activity = "hike";
  {
    const double roll = outdoorRng_.unit();
    double acc = 0;
    for (size_t i = 0; i < sizeof(kActivities) / sizeof(kActivities[0]); ++i) {
      acc += kActivities[i].weight;
      if (roll < acc) { activity = kActivities[i].name; break; }
    }
  }

  // The destination: the hike and camp rows are gated on their Activity
  // column, everything else takes any row of the pool (REPORT.en.md 71.2).
  const bool gated = (activity == "hike" || activity == "camp");
  const size_t drow = window("a16_outdoor-destination-enc", "Commonness Score",
                             gated ? "Activity" : "", gated ? activity : "",
                             "", "", 0, false);
  const Table& d = tbl("a16_outdoor-destination-enc");
  h.outdoorPlace = drow == size_t(-1) ? std::string() : cell(d, drow, "Location");
  h.outdoorHabitat = drow == size_t(-1) ? std::string() : lower(trim(d.at(drow, "Habitat")));
  if (h.outdoorPlace.empty()) h.outdoorPlace = "the woods";
  h.outdoorLandmark.clear();
  if (drow != size_t(-1) && d.has("Landmarks")) {
    std::vector<std::string> landmarks;
    splitOn(trim(d.at(drow, "Landmarks")), ',', landmarks);
    if (!landmarks.empty()) h.outdoorLandmark = trim(landmarks[outdoorRng_.below(uint32_t(landmarks.size()))]);
  }
  h.outdoorTrips += 1;
  h.outdoorJoy = std::min(100, h.outdoorJoy + 3);

  if (activity == "hike") {
    title = "A hike";
    body = "Trail: " + h.outdoorPlace + ".";
    say("I hiked through " + h.outdoorPlace + ".");
    happy += 1;
    // A find on the trail (a16_outdoor-hiking-item-discoverie-enc).
    if (outdoorRng_.chance(0.45)) {
      const size_t r = window("a16_outdoor-hiking-item-discoverie-enc", "Commonness Score",
                              "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-hiking-item-discoverie-enc");
        mark("a16_outdoor-hiking-item-discoverie-enc", r);
        const long long lo = static_cast<long long>(t.num(r, "Min Item Value"));
        const long long hi = static_cast<long long>(t.num(r, "Max Item Value"));
        const long long span = hi > lo ? hi - lo + 1 : 1;
        const long long value = lo + static_cast<long long>(outdoorRng_.below(uint32_t(span)));
        const std::string item = cell(t, r, "Item");
        h.money += value;
        h.outdoorEarned += value;
        add("I found " + withArticle(item, true) + item + " worth " + money(value) + ".");
      }
    }
    // Something growing on the trail, and the plants the handbook flags.
    if (outdoorRng_.chance(0.35)) {
      const size_t r = wildlife("a16_outdoor-plant-enc");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-plant-enc");
        mark("a16_outdoor-plant-enc", r);
        const std::string plant = cell(t, r, "Species");
        const std::string plural = cell(t, r, "Plural");
        h.outdoorPlant = plant;
        h.outdoorJoy = std::min(100, h.outdoorJoy + static_cast<int>(t.num(r, "Enjoyment Score")));
        happy += static_cast<int>(t.num(r, "Happiness Impact"));
        h.health = std::max(0, std::min(100, h.health + static_cast<int>(t.num(r, "Health Impact"))));
        add("I came across " + (plural.empty() ? plant : plural) + " growing along the trail.");
        if (t.flag(r, "Hallucinations") && outdoorRng_.chance(0.6)) {
          const size_t hr = window("a16_outdoor-hallucination-enc", "Commonness Score",
                                   "", "", "", "", 0, false);
          if (hr != size_t(-1)) {
            const Table& u = tbl("a16_outdoor-hallucination-enc");
            mark("a16_outdoor-hallucination-enc", hr);
            add(withBrackets(u, hr, "Log Version"));
            h.outdoorJoy = std::min(100, h.outdoorJoy + static_cast<int>(u.num(hr, "Enjoyment Score")));
            const size_t sr = window("a16_outdoor-hallucination-symptom-scenario-enc",
                                     "Commonness Score", "", "", "", "", 0, false);
            if (sr != size_t(-1)) {
              const Table& s = tbl("a16_outdoor-hallucination-symptom-scenario-enc");
              add(withBrackets(s, sr, "Log Version"));
            }
          }
        }
      }
    }
    // The animals of the destination (a16_outdoor-animal-enc).
    if (outdoorRng_.chance(0.25)) {
      const size_t r = wildlife("a16_outdoor-animal-enc");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-animal-enc");
        mark("a16_outdoor-animal-enc", r);
        const std::string species = cell(t, r, "Species");
        const int danger = static_cast<int>(t.num(r, "Danger Score"));
        if (danger >= 3 && outdoorRng_.chance(0.35)) {
          add("A " + species + " " + cell(t, r, "Successful Attack String") + " me on the trail.");
          h.health = std::max(0, h.health - danger);
        } else {
          add("I spotted " + withArticle(species, true) + species + " and backed away slowly.");
        }
      }
    }
    // Birds (a16_outdoor-bird-enc and the behaviour pool behind it).
    if (outdoorRng_.chance(0.20)) {
      const size_t r = wildlife("a16_outdoor-bird-enc");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-bird-enc");
        mark("a16_outdoor-bird-enc", r);
        const std::string plural = cell(t, r, "Plural");
        h.outdoorBird = cell(t, r, "Species");
        add("A pair of " + (plural.empty() ? h.outdoorBird : plural) + " kept me company.");
        const size_t br = window("a16_outdoor-bird-behavior-enc", "Commonness Score", "Habitat",
                                 h.outdoorHabitat, "", "", 0, false);
        if (br != size_t(-1)) add("One of them decided to " + cell(tbl("a16_outdoor-bird-behavior-enc"), br, "String") + ".");
      }
    }
    // The landmark at the end of the trail: a view, a memento or a selfie.
    if (outdoorRng_.chance(0.15) && !h.outdoorLandmark.empty()) {
      if (outdoorRng_.chance(0.5)) {
        const size_t r = landmarkRow("a16_outdoor-landmark-view-reflection-enc");
        if (r != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-landmark-view-reflection-enc");
          mark("a16_outdoor-landmark-view-reflection-enc", r);
          companionOn();
          add(withBrackets(u, r, "Log Version"));
          companionOff();
          happy += static_cast<int>(u.num(r, "Happiness Impact"));
        }
      } else if (outdoorRng_.chance(0.5)) {
        const size_t r = landmarkRow("a16_outdoor-landmark-memento-scenario-enc");
        if (r != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-landmark-memento-scenario-enc");
          mark("a16_outdoor-landmark-memento-scenario-enc", r);
          companionOn();
          add(withBrackets(u, r, "Log Version"));
          companionOff();
          happy += static_cast<int>(u.num(r, "Happiness Impact"));
        }
      } else {
        const size_t r = window("a16_outdoor-landmark-selfie-color-enc", "Commonness Score",
                                "", "", "", "", 0, false);
        if (r != size_t(-1)) add(cell(tbl("a16_outdoor-landmark-selfie-color-enc"), r, "String"));
      }
    }
    // A cave on the way (the cave list plus its own discovery card).
    if (outdoorRng_.chance(0.10)) {
      const size_t cr = window("a16_outdoor-cave-enc", "", "Location", h.country, "", "", 0, false);
      if (cr != size_t(-1)) {
        const Table& c = tbl("a16_outdoor-cave-enc");
        mark("a16_outdoor-cave-enc", cr);
        h.outdoorCave = cell(c, cr, "Name");
        add("I took a detour into " + h.outdoorCave + ".");
        const size_t dr = cardRow("a16_outdoor-cave-discoverie-enc");
        if (dr != size_t(-1)) {
          const std::vector<std::string> keep = out.extra;
          for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
          const bool shown = renderRow(tbl("a16_outdoor-cave-discoverie-enc"), dr, life, out, forcedChoice);
          (void)keep;
          return shown;
        }
      }
    }
    // What the hero drank on the way (a16_outdoor-hiking-drink-enc).
    if (outdoorRng_.chance(0.10)) {
      const size_t r = window("a16_outdoor-hiking-drink-enc", "Commonness Score", "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-hiking-drink-enc");
        mark("a16_outdoor-hiking-drink-enc", r);
        add(toFirstPerson(cell(t, r, "Feedback Color String")));
        happy += static_cast<int>(t.num(r, "Happiness Impact"));
        h.health = std::max(0, std::min(100, h.health + static_cast<int>(t.num(r, "Health Impact"))));
      }
    }
    // A ghost at the campsite or on the trail (a16_outdoor-ghost-encounter-enc).
    if (outdoorRng_.chance(0.08)) {
      const size_t r = window("a16_outdoor-ghost-encounter-enc", "", "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-ghost-encounter-enc");
        mark("a16_outdoor-ghost-encounter-enc", r);
        add(withBrackets(t, r, "Log Version"));
      }
    }
    // The dog, when there is one.
    if (hasDog && outdoorRng_.chance(0.30)) {
      const size_t r = window("a16_outdoor-hiking-dog-discovery-scenario-enc", "Commonness Score",
                              "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-hiking-dog-discovery-scenario-enc");
        mark("a16_outdoor-hiking-dog-discovery-scenario-enc", r);
        add(withBrackets(t, r, "Log Version"));
        h.outdoorJoy = std::min(100, h.outdoorJoy + static_cast<int>(t.num(r, "Hero Enjoyment Score")));
        const size_t in = window("a16_outdoor-hiking-dog-fetch-item-enc", "", "", "", "", "", 0, false);
        if (in != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-hiking-dog-fetch-item-enc");
          const std::string object = withBrackets(u, in, "Object");
          add(h.petName.empty() ? "My dog" : h.petName + " fetched " + withArticle(object, true) + object + ".");
          h.outdoorJoy = std::min(100, h.outdoorJoy + static_cast<int>(u.num(in, "Pride Score")));
        }
      }
    }
  } else if (activity == "camp") {
    title = "A night under the stars";
    body = "Campsite: " + h.outdoorPlace + ".";
    say("I camped at " + h.outdoorPlace + ".");
    happy += 2;
    if (outdoorRng_.chance(0.35)) {
      const size_t r = cardRow("a16_outdoor-camping-dumb-scenario-enc");
      if (r != size_t(-1)) return renderRow(tbl("a16_outdoor-camping-dumb-scenario-enc"), r, life, out, forcedChoice);
    }
    if (outdoorRng_.chance(0.22)) {
      const size_t r = wildlife("a16_outdoor-animal-enc");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-animal-enc");
        mark("a16_outdoor-animal-enc", r);
        add("Something moved through the camp in the dark: " + cell(t, r, "Species") + ".");
      }
    }
    if (outdoorRng_.chance(0.18)) {
      const size_t r = wildlife("a16_outdoor-plant-enc");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-plant-enc");
        mark("a16_outdoor-plant-enc", r);
        h.outdoorPlant = cell(t, r, "Species");
        add("The campsite was surrounded by " + h.outdoorPlant + ".");
      }
    }
    if (outdoorRng_.chance(0.15)) {
      const size_t r = window("a16_outdoor-ghost-encounter-enc", "", "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-ghost-encounter-enc");
        mark("a16_outdoor-ghost-encounter-enc", r);
        add(withBrackets(t, r, "Log Version"));
      }
    }
    if (hasDog && outdoorRng_.chance(0.35)) {
      const size_t r = window("a16_outdoor-dog-behavior-enc", "Max Dog Craziness", "Activity",
                              "camping", "", "", 0, true);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-dog-behavior-enc");
        mark("a16_outdoor-dog-behavior-enc", r);
        add(withBrackets(t, r, "Log Version"));
        const size_t tr = window("a16_outdoor-camping-dog-treat-enc", "", "", "", "", "", 0, false);
        if (tr != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-camping-dog-treat-enc");
          add("I gave " + (h.petName.empty() ? std::string("the dog") : h.petName) + " " +
              cell(u, tr, "Treat") + ".");
          h.outdoorJoy = std::min(100, h.outdoorJoy + static_cast<int>(u.num(tr, "Enjoyment Score")));
        }
      }
    }
  } else if (activity == "fish") {
    say("I went fishing at " + h.outdoorPlace + ".");
    happy += 1;
    static const char* kWaters[] = {"lake", "river", "pond", "ocean"};
    const std::string water = kWaters[outdoorRng_.below(4)];
    const size_t r = window("a16_outdoor-catchable-fish-enc", "Commonness Score", "Habitat",
                            water, "", "", 0, true);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_outdoor-catchable-fish-enc");
      mark("a16_outdoor-catchable-fish-enc", r);
      const std::string species = cell(t, r, "Species");
      const int difficulty = static_cast<int>(t.num(r, "Difficulty Score"));
      // The rod talks back before the fish is landed: one of the two colour
      // pools describes the fight, keyed on the difficulty score (71.5).
      const bool hooked = outdoorRng_.chance(0.5);
      const char* fightTable = hooked ? "a16_outdoor-hooked-fish-color-string-enc"
                                      : "a16_outdoor-lure-fish-color-string-enc";
      const std::string difficultyText = std::to_string(difficulty);
      const size_t fr = window(fightTable, "Commonness Score", "Fish Difficulty",
                               difficultyText, "", "", 0, false);
      // The colour pools are written in the second person (the card's voice);
      // the log keeps the first person the rest of the family uses.
      if (fr != size_t(-1)) add(toFirstPerson(cell(tbl(fightTable), fr, "String")));
      const double lo = t.num(r, "Min Weight (lbs)");
      const double hi = t.num(r, "Max Weight (lbs)");
      const double weight = lo + outdoorRng_.unit() * (hi > lo ? hi - lo : 1.0);
      const long long lbs = static_cast<long long>(weight + 0.5);
      const long long vlo = static_cast<long long>(t.num(r, "Min Value"));
      const long long vhi = static_cast<long long>(t.num(r, "Max Value"));
      const long long span = vhi > vlo ? vhi - vlo + 1 : 1;
      const long long value = vlo + static_cast<long long>(outdoorRng_.below(uint32_t(span)));
      h.outdoorFish = species;
      h.outdoorCatches += 1;
      h.money += value;
      h.outdoorEarned += value;
      title = "A fish on the line";
      body = "Catch: " + species + ".";
      add("I landed a " + species + " that weighed " + std::to_string(lbs) + " lbs and sold it for " +
          money(value) + ".");
      // A trophy fish gets a name and a wall (a16_outdoor-mounted-fish-name-enc).
      if (lbs >= 20) {
        const size_t mr = window("a16_outdoor-mounted-fish-name-enc", "Commonness Score",
                                 "Gender", h.female ? "male" : "female", "", "", 0, false);
        if (mr != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-mounted-fish-name-enc");
          mark("a16_outdoor-mounted-fish-name-enc", mr);
          add("I had " + cell(u, mr, "Fish Name") + " mounted above the fireplace.");
        }
      }
      // And sometimes the hook brings up something older (fishing-treasure).
      if (outdoorRng_.chance(0.15)) {
        const size_t tr = window("a16_outdoor-fishing-treasure-enc", "Commonness Score",
                                 "Location", water, "", "", 0, false);
        if (tr != size_t(-1)) {
          const Table& u = tbl("a16_outdoor-fishing-treasure-enc");
          mark("a16_outdoor-fishing-treasure-enc", tr);
          const long long tlo = static_cast<long long>(u.num(tr, "Min Value"));
          const long long thi = static_cast<long long>(u.num(tr, "Max Value"));
          const long long tsp = thi > tlo ? thi - tlo + 1 : 1;
          const long long worth = tlo + static_cast<long long>(outdoorRng_.below(uint32_t(tsp)));
          const std::string item = cell(u, tr, "List Item Name");
          h.money += worth;
          h.outdoorEarned += worth;
          add("I pulled up " + withArticle(item, true) + item + ", worth " + money(worth) + ".");
        }
      }
    }
  } else if (activity == "offroad") {
    title = "A day off-road";
    body = "Route: " + h.outdoorPlace + ".";
    say("I took the " + h.outdoorVehicle + " out to " + h.outdoorPlace + ".");
    happy += 2;
    // A trick and its aftermath (a16_outdoor-offroading-trick-enc): the
    // Success Feedback column is already the first-person past tense, the
    // String column is the infinitive the failure line needs.
    if (outdoorRng_.chance(0.50)) {
      const size_t r = window("a16_outdoor-offroading-trick-enc", "Commonness Score",
                              "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-offroading-trick-enc");
        mark("a16_outdoor-offroading-trick-enc", r);
        const int difficulty = static_cast<int>(t.num(r, "Difficulty Score"));
        const std::string trick = withBrackets(t, r, "String");
        const std::string trickOk = cell(t, r, "Success Feedback Version");
        if (outdoorRng_.unit() < 0.95 - 0.15 * difficulty) {
          add("I " + trickOk + " in the " + h.outdoorVehicle + ".");
        } else {
          add("I tried to " + toFirstPerson(trick) + " and bent the bumper.");
          h.outdoorBikeCondition = std::max(0, h.outdoorBikeCondition - 5);
        }
      }
    }
    // Where the drive actually went (a16_outdoor-offroading-location-color-string-enc).
    if (outdoorRng_.chance(0.45)) {
      const size_t r = window("a16_outdoor-offroading-location-color-string-enc", "Commonness Score",
                              "Location", cell(tbl("a16_outdoor-offroading-trick-enc"), 0, "Location"),
                              "", "", 0, false);
      if (r != size_t(-1)) add("I drove " + cell(tbl("a16_outdoor-offroading-location-color-string-enc"), r, "String") + ".");
    }
    // Something stuck out there (a16_outdoor-offroading-encounter-enc).
    if (outdoorRng_.chance(0.35)) {
      const size_t r = window("a16_outdoor-offroading-encounter-enc", "Commonness Score",
                              "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-offroading-encounter-enc");
        mark("a16_outdoor-offroading-encounter-enc", r);
        const int danger = static_cast<int>(t.num(r, "Danger Score"));
        add("I came across a vehicle " + cell(t, r, "String") + " and winched it out.");
        if (danger >= 3) h.health = std::max(0, h.health - danger);
      }
    }
    // A little something for the trouble (a16_outdoor-offroading-reward-enc).
    if (outdoorRng_.chance(0.30)) {
      const size_t r = window("a16_outdoor-offroading-reward-enc", "Commonness Score",
                              "", "", "Min Hero Age", "Max Hero Age", h.age, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-offroading-reward-enc");
        mark("a16_outdoor-offroading-reward-enc", r);
        const std::string item = cell(t, r, "Item");
        add("Someone handed me " + withArticle(item, true) + item + ".");
        happy += static_cast<int>(t.num(r, "Happiness Impact"));
      }
    }
    // And the passenger, when there is one (a16_outdoor-offroading-make-love-string-enc).
    if (!h.outdoorCompanion.empty() && outdoorRng_.chance(0.25)) {
      const size_t r = window("a16_outdoor-offroading-make-love-string-enc", "Commonness Score",
                              "", "", "", "", 0, false);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_outdoor-offroading-make-love-string-enc");
        mark("a16_outdoor-offroading-make-love-string-enc", r);
        add("We found a quiet spot where we could " + toFirstPerson(cell(t, r, "Snippet")) + ".");
        happy += 2;
      }
    }
  } else if (activity == "bike") {
    title = "A bike ride";
    body = "Route: " + h.outdoorPlace + ".";
    say("I rode my " + h.outdoorBike + " around " + h.outdoorPlace + ".");
    happy += 1;
    if (outdoorRng_.chance(0.35)) {
      const size_t r = cardRow("a16_outdoor-bike-ride-dumb-enc");
      if (r != size_t(-1)) return renderRow(tbl("a16_outdoor-bike-ride-dumb-enc"), r, life, out, forcedChoice);
    }
    const size_t r = window("a16_outdoor-bike-ride-location-enc", "Commonness Score",
                            "", "", "", "", 0, false);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_outdoor-bike-ride-location-enc");
      mark("a16_outdoor-bike-ride-location-enc", r);
      add("I rode " + cell(t, r, "String") + " on the " + h.outdoorBike + ".");
    }
    h.outdoorBikeCondition = std::max(0, h.outdoorBikeCondition - static_cast<int>(outdoorRng_.below(6)));
  } else if (activity == "snorkel") {
    title = "A snorkel trip";
    body = "Site: " + h.outdoorPlace + ".";
    say("I went snorkeling off " + h.outdoorPlace + ".");
    happy += 2;
    const size_t dr = window("a16_outdoor-snorkeling-discovery-scenario-enc", "Commonness Score",
                             "", "", "", "", 0, false);
    if (dr != size_t(-1)) {
      const Table& u = tbl("a16_outdoor-snorkeling-discovery-scenario-enc");
      mark("a16_outdoor-snorkeling-discovery-scenario-enc", dr);
      add(cell(u, dr, "Scenario"));
    }
    const size_t r = window("a16_outdoor-marine-life-enc", "Commonness Score", "", "", "", "", 0, false);
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_outdoor-marine-life-enc");
      mark("a16_outdoor-marine-life-enc", r);
      h.outdoorMarine = cell(t, r, "Species");
      const int danger = static_cast<int>(t.num(r, "Danger Score"));
      // The play pool is a bare phrase whose own 'Log' column is the recorded
      // first person ("You swam next to" / "I swam next to"), so the log wins.
      const size_t pr = window("a16_outdoor-marine-life-play-string-enc", "Commonness Score",
                               "", "", "", "", 0, false);
      if (pr != size_t(-1)) {
        const Table& u = tbl("a16_outdoor-marine-life-play-string-enc");
        mark("a16_outdoor-marine-life-play-string-enc", pr);
        add(withBrackets(u, pr, "Log") + " a " + h.outdoorMarine + ".");
      }
      const size_t br = window("a16_outdoor-marine-behavior-enc", "Commonness Score",
                               "", "", "", "", 0, false);
      if (br != size_t(-1)) {
        const Table& u = tbl("a16_outdoor-marine-behavior-enc");
        add("It decided to " + withBrackets(u, br, "String") + ".");
      }
      if (danger >= 4 && outdoorRng_.chance(0.40)) {
        add("A " + h.outdoorMarine + " " + cell(t, r, "Attack String") + " me.");
        h.health = std::max(0, h.health - danger);
      }
    }
  }

  if (title.empty()) return false;
  out.table = tableRef.empty() ? "a16_outdoor-destination-enc" : tableRef;
  out.row_id = rowRef.empty() ? std::string("0") : rowRef;
  out.title = title;
  out.body = body;
  out.narrative = line;
  const std::vector<std::string> keep = out.extra;
  out.extra.clear();
  for (size_t i = 0; i < keep.size(); ++i) out.extra.push_back(keep[i]);
  for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
  out.happiness += happy;
  if (h.outdoorBikeCondition <= 20) out.happiness -= 1;
  return true;
}

// ---------------------------------------------------------------- the hustle
// REPORT.en.md 72. The family works the street three ways: panhandling, busking
// and scamming. The kind, the attire, the instrument, the act, the song and the
// props are drawn once when the hero takes the street up; the corner, the mark
// and the take belong to the year's card.
void Sim::makeHustler(Life& life) {
  Hero& h = life.hero;
  h.hustle = true;
  h.hustlerSkill = 40 + static_cast<int>(hustlerRng_.below(35));
  h.hustlerHeat = 0;
  h.hustlerEarned = 0;

  const double kind = hustlerRng_.unit();
  h.hustlerKind = kind < 0.45 ? "panhandle" : (kind < 0.80 ? "busk" : "scam");
  const std::string gender = h.female ? "female" : "male";

  const size_t att = pickRow("a16_hustler-attire-enc", "Attention Score", "Hero Gender", gender, "", hustlerRng_);
  if (att != size_t(-1)) h.hustlerAttire = resolve(trim(tbl("a16_hustler-attire-enc").at(att, "Attire")));
  if (h.hustlerAttire.empty()) h.hustlerAttire = "tattered clothes";

  const size_t mu = pickRow("a16_hustler-busk-music-types-enc", "Success Score", "", "", "", hustlerRng_);
  if (mu != size_t(-1)) {
    const Table& t = tbl("a16_hustler-busk-music-types-enc");
    h.hustlerMusic = resolve(trim(t.at(mu, "Music Type")));
    // 'Needs Music Appended' means the game writes the type as a genre
    // ('peaceful' -> 'peaceful music').
    if (t.flag(mu, "Needs Music Appended")) h.hustlerMusic += " music";
    const std::string inst = trim(t.at(mu, "Best Instruments"));
    std::vector<std::string> list;
    if (!inst.empty()) splitOn(inst, ',', list);
    if (!list.empty()) h.hustlerInstrument =
        resolve(trim(list[hustlerRng_.below(static_cast<uint32_t>(list.size()))]));
  }
  if (h.hustlerInstrument.empty()) h.hustlerInstrument = "guitar";
  if (h.hustlerMusic.empty()) h.hustlerMusic = "some tunes";

  const size_t act = pickRow("a16_hustler-perform-acts-enc", "Money Score", "", "", "", hustlerRng_);
  if (act != size_t(-1)) h.hustlerAct = resolve(trim(tbl("a16_hustler-perform-acts-enc").at(act, "Act")));
  if (h.hustlerAct.empty()) h.hustlerAct = "breakdancing";

  const size_t song = pickRow("a16_hustler-serenade-songs-enc", "Reaction Score", "", "", "", hustlerRng_);
  if (song != size_t(-1)) h.hustlerSong = resolve(trim(tbl("a16_hustler-serenade-songs-enc").at(song, "Song")));

  // The sign: gender-filtered and age-windowed ('Hungry old man' belongs to a
  // hero of 60 or more), then weighted on the effectiveness score that matches
  // the hero's gender.
  if (ensureTable("a16_hustler-panhandle-signs-enc")) {
    const Table& t = tbl("a16_hustler-panhandle-signs-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      const std::string g = lower(trim(t.at(r, "Hero Gender")));
      if (!g.empty() && g != "both" && g != gender) continue;
      const double lo = t.num(r, "Hero Min Age");
      const double hi = t.num(r, "Hero Max Age");
      if (lo > 0 && h.age < lo) continue;
      if (hi > 0 && h.age > hi) continue;
      pool.push_back(r);
    }
    if (!pool.empty()) {
      const size_t r = weightedPickOf(t, h.female ? "Female Effectiveness Score" : "Male Effectiveness Score",
                                      pool, hustlerRng_);
      const size_t row = r == size_t(-1) ? pool[0] : r;
      // 'Full String' is the sign as it hangs; 'String' is the teaser the game
      // shows in a list, so it is only a fallback.
      h.hustlerSign = resolve(trim(t.at(row, "Full String")));
      if (h.hustlerSign.empty()) h.hustlerSign = resolve(trim(t.at(row, "String")));
    }
  }
  if (h.hustlerSign.empty()) h.hustlerSign = "Spare some change?";

  const size_t st = pickRow("a16_hustler-panhandle-strategies-enc", "Effectiveness Score",
                            "Hero Gender", gender, "", hustlerRng_);
  if (st != size_t(-1)) h.hustlerStrategy = resolve(trim(tbl("a16_hustler-panhandle-strategies-enc").at(st, "Strategy")));
  if (h.hustlerStrategy.empty()) h.hustlerStrategy = "ask for money on a corner";

  if (ensureTable("a16_hustler-panhandle-props-enc")) {
    const Table& t = tbl("a16_hustler-panhandle-props-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    const size_t r = weightedPickOf(t, h.female ? "Female Effectiveness Score" : "Male Effectiveness Score",
                                    pool, hustlerRng_);
    if (r != size_t(-1)) h.hustlerProp = resolve(trim(t.at(r, "Prop")));
  }
  if (h.hustlerProp.empty()) h.hustlerProp = "cardboard box";

  const size_t bx = pickRow("a16_hustler-collection-boxes-enc", "Panhandler Effectiveness Score",
                            "", "", "Panhandler", hustlerRng_);
  if (bx != size_t(-1)) h.hustlerBox = resolve(trim(tbl("a16_hustler-collection-boxes-enc").at(bx, "Item")));
  if (h.hustlerBox.empty()) h.hustlerBox = "hat";
}

std::string Sim::hustlerToken(const std::string& token) {
  Hero* h = const_cast<Hero*>(hero_);
  std::string key = trim(token);
  const std::string psuffix = "-possessive";
  bool poss = false;
  if (key.size() > psuffix.size() &&
      key.compare(key.size() - psuffix.size(), psuffix.size(), psuffix) == 0) {
    key.resize(key.size() - psuffix.size());
    poss = true;
  }
  if (key == "victim-possesive") { key = "victim"; poss = true; }   // the data's spelling
  // One draw per slot per card, so a screen that names the same thing twice
  // names the same thing.
  const auto remembered = [&](const char* slot, const std::string& value) -> std::string {
    auto it = hustlerCache_.find(slot);
    if (it == hustlerCache_.end()) it = hustlerCache_.emplace(slot, value).first;
    return it->second;
  };
  if (h != nullptr) {
    if (key == "hustle-act") return h->hustlerAct.empty() ? std::string("street performing") : h->hustlerAct;
    if (key == "hustle-street") return h->hustlerStreet.empty() ? std::string("the street") : h->hustlerStreet;
    if (key == "hustle-instrument") return h->hustlerInstrument.empty() ? std::string("guitar") : h->hustlerInstrument;
    if (key == "hustle-box") return h->hustlerBox.empty() ? std::string("hat") : h->hustlerBox;
    if (key == "victim") {
      std::string who = h->hustlerVictim;
      if (who.empty()) who = referenceToken("random_person_indefinite");
      if (who.empty()) who = "a passer-by";
      return poss ? who + "'s" : who;
    }
    if (key == "amount") return money(h->hustlerAmount > 0 ? h->hustlerAmount : 20);
    if (key == "item") {
      const size_t r = pickRow("a16_hustler-scam-items-enc", "", "", "", "", hustlerRng_);
      std::string v;
      if (r != size_t(-1)) v = trim(tbl("a16_hustler-scam-items-enc").at(r, "Item Singular"));
      if (v.empty()) v = "trinket";
      return remembered("item", v);
    }
    if (key == "insult") {
      // The insult is aimed at the hero ('he called you <insult>'), so the pool
      // is filtered on the hero's own gender.
      const size_t r = pickRow("a16_hustler-insults-enc", "", "Gender",
                               lower(trim(heroToken("gender", false))), "", hustlerRng_);
      std::string v;
      if (r != size_t(-1)) v = trim(tbl("a16_hustler-insults-enc").at(r, "Insult"));
      if (v.empty()) v = "a bum";
      return remembered("insult", v);
    }
  }
  return std::string();
}

bool Sim::hustlerYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.hustle) return false;
  if (h.age < 13) return false;
  if (!ensureTable("a16_hustler-attire-enc")) return false;
  if (!hustlerRng_.chance(0.5)) return false;
  hero_ = &h;
  life_ = &life;

  std::string title, body, line;
  std::vector<std::string> extra;
  std::string tableRef, rowRef;
  int happy = 0;
  long long take = 0;
  const std::string gender = h.female ? "female" : "male";

  const auto say = [&](const std::string& s) {
    if (line.empty() && !s.empty()) line = spyUpperFirst(s);
  };
  const auto add = [&](const std::string& s) { if (!s.empty()) extra.push_back(spyUpperFirst(s)); };
  const auto cell = [&](const Table& t, size_t r, const char* col) -> std::string {
    if (r == size_t(-1) || !t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  const auto mark = [&](const char* tab, size_t r) { tableRef = tab; rowRef = std::to_string(r + 1); };
  // Rows whose log line points back at an alternative drawn earlier in the row
  // ('<rb>', '<rb2>'): every other bracketed column is resolved first, in column
  // order, which is the order the game draws them in (REPORT.en.md 57).
  const auto withBrackets = [&](const Table& t, size_t r, const char* wantCol) -> std::string {
    if (r == size_t(-1)) return std::string();
    for (const std::string& cc : t.columns()) {
      if (cc == wantCol) continue;
      if (t.at(r, cc).find('<') == std::string::npos) continue;
      cell(t, r, cc.c_str());
    }
    return cell(t, r, wantCol);
  };
  // A pool that writes about somebody else: the pronoun pair follows that person
  // rather than the hero while the row renders.
  const auto scene = [&](const char* tab, size_t r, bool otherFemale, bool other) -> std::string {
    if (r == size_t(-1)) return std::string();
    const Table& t = tbl(tab);
    const bool oSave = pronounOtherSet_, fSave = pronounOtherFemale_;
    pronounOtherSet_ = other;
    pronounOtherFemale_ = otherFemale;
    std::string s = withBrackets(t, r, "Log Version");
    if (s.empty()) s = cell(t, r, "Scenario");
    pronounOtherSet_ = oSave;
    pronounOtherFemale_ = fSave;
    return s;
  };
  // A face-to-face pool ('a cop', 'a stranger') carries the gender of the person
  // speaking in its own column.
  const auto sceneOf = [&](const char* tab, size_t r) -> std::string {
    if (r == size_t(-1)) return std::string();
    const Table& t = tbl(tab);
    bool female = hustlerRng_.chance(0.5);
    if (t.has("Person Gender")) {
      const std::string g = lower(trim(t.at(r, "Person Gender")));
      if (g == "female") female = true;
      else if (g == "male") female = false;
    }
    return scene(tab, r, female, true);
  };
  const auto delegate = [&](const char* tab, size_t r) -> bool {
    const bool p = hustlerLive_;
    hustlerLive_ = true;
    const bool ok = renderRow(tbl(tab), r, life, out, forcedChoice);
    hustlerLive_ = p;
    return ok;
  };

  const bool live0 = hustlerLive_;
  hustlerLive_ = true;
  // The street corner of the year: no table holds the pool, which the game keeps
  // inline in the scenario cells, so the port carries its own (shortcut).
  static const char* kStreets[] = {"the sidewalk", "the subway platform", "the park bench",
                                   "the plaza", "the boardwalk", "the bus stop",
                                   "the market square", "the overpass", "the tourist pier",
                                   "the alley behind the diner"};
  h.hustlerStreet = kStreets[hustlerRng_.below(10)];
  h.hustlerAmount = 0;
  h.hustlerVictim.clear();
  h.hustlerPerson.clear();
  h.hustlerPersonFemale = hustlerRng_.chance(0.5);
  if (h.hustlerKind.empty()) h.hustlerKind = "panhandle";

  if (h.hustlerKind == "panhandle") {
    title = "Panhandling";
    const size_t st = pickRow("a16_hustler-panhandle-strategies-enc", "Effectiveness Score",
                              "Hero Gender", gender, "", hustlerRng_);
    double eff = 3;   // the props, the sign and the box are worth a flat bonus
    std::string strategy = h.hustlerStrategy;
    if (st != size_t(-1)) {
      const Table& t = tbl("a16_hustler-panhandle-strategies-enc");
      eff += t.num(st, "Effectiveness Score");
      strategy = resolve(trim(t.at(st, "Strategy")));
      h.hustlerStrategy = strategy;
      mark("a16_hustler-panhandle-strategies-enc", st);
    }
    eff += double(h.hustlerSkill - 50) / 8.0;
    // The strategy already carries its own corner ("stand outside the lottery
    // headquarters"), so the body names it without repeating the street.
    body = "I spent the day trying to " + strategy + " in my " +
           h.hustlerAttire + ", holding up my sign: " + h.hustlerSign;
    const bool good = hustlerRng_.unit() < 0.30 + eff / 25.0;
    take = good ? 6 + static_cast<long long>(hustlerRng_.below(25)) + static_cast<long long>(eff * 3.0)
                : static_cast<long long>(hustlerRng_.below(5));
    h.hustlerAmount = take;
    line = good ? ("I worked " + h.hustlerStreet + " all day and came away with " + money(take) + ".")
                : ("I worked " + h.hustlerStreet + " all day and only made " + money(take) + ".");

    h.hustlerPerson = referenceToken("random_person_indefinite");
    if (h.hustlerPerson.empty()) h.hustlerPerson = "a passer-by";

    // A handout that lands in the box.
    if (hustlerRng_.chance(0.35)) {
      const size_t it = pickRow("a16_hustler-panhandle-items-enc", "", "", "", "Fits In Collection Box", hustlerRng_);
      if (it != size_t(-1)) {
        const Table& t = tbl("a16_hustler-panhandle-items-enc");
        const double lo = t.num(it, "Min Value"), hi = t.num(it, "Max Value");
        const long long worth = static_cast<long long>(lo) +
            static_cast<long long>(hustlerRng_.below(static_cast<uint32_t>(hi > lo ? hi - lo + 1 : 1)));
        const std::string item = resolve(trim(t.at(it, "Item")));
        take += worth;
        h.hustlerAmount = take;
        add("Someone dropped " + withArticle(item, true) + item + " in my " + h.hustlerBox +
            ", worth " + money(worth) + ".");
        mark("a16_hustler-panhandle-items-enc", it);
      }
    }
    // A meal from a stranger.
    if (hustlerRng_.chance(0.30)) {
      const size_t fd = pickRow("a16_hustler-panhandle-food-enc", "", "", "", "", hustlerRng_);
      if (fd != size_t(-1)) {
        const Table& t = tbl("a16_hustler-panhandle-food-enc");
        const std::string item = resolve(trim(t.at(fd, "Item")));
        const double poison = t.num(fd, "Poison Risk Score");
        happy += static_cast<int>(t.num(fd, "Happiness Impact"));
        h.health = std::max(0, std::min(100, h.health + static_cast<int>(t.num(fd, "Health Impact"))));
        add("Someone handed me " + withArticle(item, true) + item + " and I ate it.");
        if (poison >= 3 && hustlerRng_.chance(0.30)) {
          add("It did not agree with me.");
          h.health = std::max(0, h.health - 2);
        }
        mark("a16_hustler-panhandle-food-enc", fd);
      }
    }
    // The story the hero tells, from the pool the child condition picks.
    if (hustlerRng_.chance(0.22)) {
      // shortcut: the port models no children, so the "child" condition of the excuse
      // pool never fits and only the "default" rows are drawn; revisit when children land.
      const bool child = false;
      const size_t ex = pickRow("a16_hustler-panhandle-excuses-enc", "Commonness Score", "Condition",
                                child ? "child" : "default", "", hustlerRng_);
      if (ex != size_t(-1)) add(scene("a16_hustler-panhandle-excuses-enc", ex, hustlerRng_.chance(0.5), true));
    }
    // Somebody stops to lecture, to shout, or to call a cop.
    if (hustlerRng_.chance(0.18)) {
      const size_t cf = pickRow("a16_hustler-panhandle-confrontation-scenarios-enc", "Anger Score",
                                "", "", "", hustlerRng_);
      if (cf != size_t(-1)) {
        add(sceneOf("a16_hustler-panhandle-confrontation-scenarios-enc", cf));
        mark("a16_hustler-panhandle-confrontation-scenarios-enc", cf);
      }
    }
    if (hustlerRng_.chance(0.15)) {
      const size_t ins = pickRow("a16_hustler-insults-enc", "", "Gender", gender, "", hustlerRng_);
      if (ins != size_t(-1))
        add("On the way home somebody called me " +
            resolve(trim(tbl("a16_hustler-insults-enc").at(ins, "Insult"))) + ".");
    }
    if (hustlerRng_.chance(0.10)) {
      const size_t p = pickRow("a16_hustler-police-scenarios-enc", "", "", "", "", hustlerRng_);
      if (p != size_t(-1)) {
        add(sceneOf("a16_hustler-police-scenarios-enc", p));
        h.hustlerHeat += 1;
        mark("a16_hustler-police-scenarios-enc", p);
      }
    }
    // The box the collection goes in, named once for the card.
    if (hustlerRng_.chance(0.10)) add("My " + h.hustlerBox + " was rattling with change by the end of it.");
  } else if (h.hustlerKind == "busk") {
    title = "Busking";
    const size_t act = pickRow("a16_hustler-perform-acts-enc", "", "", "", "", hustlerRng_);
    double diff = 2, moneyScore = 3, danger = 0;
    if (act != size_t(-1)) {
      const Table& t = tbl("a16_hustler-perform-acts-enc");
      h.hustlerAct = resolve(trim(t.at(act, "Act")));
      diff = t.num(act, "Difficulty Score");
      moneyScore = t.num(act, "Money Score");
      danger = t.num(act, "Danger Score");
      mark("a16_hustler-perform-acts-enc", act);
    }
    const double mood = hustlerRng_.unit() + double(h.hustlerSkill - 50) / 200.0 - (diff - 3.0) / 20.0;
    const bool good = mood > 0.35;
    take = good ? static_cast<long long>(moneyScore) * (2 + static_cast<long long>(hustlerRng_.below(5)))
                : static_cast<long long>(hustlerRng_.below(4));
    h.hustlerAmount = take;
    body = "I set up " + h.hustlerStreet + " in my " + h.hustlerAttire + " with my " +
           h.hustlerInstrument + " and played " + h.hustlerMusic + " while I did my " +
           h.hustlerAct + " act.";
    line = good ? ("I busked " + h.hustlerStreet + " and the crowd left me " + money(take) + ".")
                : ("I busked " + h.hustlerStreet + " and only picked up " + money(take) + ".");

    h.hustlerPerson = referenceToken("random_person_indefinite");
    if (h.hustlerPerson.empty()) h.hustlerPerson = "a passer-by";

    // The crowd. 'Can Be Unskilled' rows are the ones the game allows a hero who
    // cannot really play, so an unskilled hero only draws those.
    const char* unskilled = h.hustlerSkill < 50 ? "Can Be Unskilled" : "";
    if (good && hustlerRng_.chance(0.45)) {
      const size_t r = pickRow("a16_hustler-busk-positive-scenarios-enc", "Positive Score", "", "", "", hustlerRng_);
      if (r != size_t(-1)) {
        add(scene("a16_hustler-busk-positive-scenarios-enc", r, h.hustlerPersonFemale, true));
        mark("a16_hustler-busk-positive-scenarios-enc", r);
      }
    }
    if (!good && hustlerRng_.chance(0.45)) {
      const size_t r = pickRow("a16_hustler-busk-negative-scenarios-enc", "Negative Score", "", "", unskilled, hustlerRng_);
      if (r != size_t(-1)) {
        add(scene("a16_hustler-busk-negative-scenarios-enc", r, h.hustlerPersonFemale, true));
        mark("a16_hustler-busk-negative-scenarios-enc", r);
      }
    }
    if (hustlerRng_.chance(0.35)) {
      const size_t r = pickRow("a16_hustler-perform-respect-scenarios-enc", "Positive Score", "", "", "", hustlerRng_);
      if (r != size_t(-1)) {
        add(scene("a16_hustler-perform-respect-scenarios-enc", r, h.hustlerPersonFemale, true));
        mark("a16_hustler-perform-respect-scenarios-enc", r);
      }
    }
    if (hustlerRng_.chance(0.25)) {
      const size_t r = pickRow("a16_hustler-perform-disrespect-scenarios-enc", "Negative Score", "", "", unskilled, hustlerRng_);
      if (r != size_t(-1)) {
        add(scene("a16_hustler-perform-disrespect-scenarios-enc", r, h.hustlerPersonFemale, true));
        mark("a16_hustler-perform-disrespect-scenarios-enc", r);
      }
    }
    if (hustlerRng_.chance(0.25)) {
      const size_t r = pickRow("a16_hustler-busk-requests-enc", "", "", "", "", hustlerRng_);
      if (r != size_t(-1)) {
        add("Somebody asked me to play " +
            resolve(trim(tbl("a16_hustler-busk-requests-enc").at(r, "Request"))) + ".");
        mark("a16_hustler-busk-requests-enc", r);
      }
    }
    // The song of the year: 'Reaction Score' is how the crowd takes it.
    if (!h.hustlerSong.empty() && hustlerRng_.chance(0.25)) {
      const size_t r = pickRow("a16_hustler-serenade-songs-enc", "Reaction Score", "", "", "", hustlerRng_);
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_hustler-serenade-songs-enc");
        const std::string song = resolve(trim(t.at(r, "Song")));
        const double reaction = t.num(r, "Reaction Score");
        h.hustlerSong = song;
        mark("a16_hustler-serenade-songs-enc", r);
        if (reaction >= 4) {
          add("I sang " + song + " and the crowd loved it.");
          take += 5;
          h.hustlerAmount = take;
        } else {
          add("I tried " + song + " and it did not land.");
        }
      }
    }
    // A prop act can go wrong.
    if (!good && danger >= 3 && hustlerRng_.chance(0.25)) {
      const Table& t = tbl("a16_hustler-perform-acts-enc");
      std::vector<std::string> parts, verbs;
      if (t.has("Danger Body Parts")) splitOn(trim(t.at(act, "Danger Body Parts")), ',', parts);
      if (t.has("Danger Verbs")) splitOn(trim(t.at(act, "Danger Verbs")), ',', verbs);
      if (!parts.empty() && !verbs.empty()) {
        const std::string part = trim(parts[hustlerRng_.below(static_cast<uint32_t>(parts.size()))]);
        const std::string verb = trim(verbs[hustlerRng_.below(static_cast<uint32_t>(verbs.size()))]);
        add("I " + verb + " my " + part + " in the middle of it.");
        h.health = std::max(0, h.health - 3);
      }
    }
    if (hustlerRng_.chance(0.15)) {
      const size_t ins = pickRow("a16_hustler-insults-enc", "", "Gender", gender, "", hustlerRng_);
      if (ins != size_t(-1))
        add("Somebody in the crowd called me " +
            resolve(trim(tbl("a16_hustler-insults-enc").at(ins, "Insult"))) + ".");
    }
    if (hustlerRng_.chance(0.10)) {
      const size_t p = pickRow("a16_hustler-police-scenarios-enc", "", "", "", "", hustlerRng_);
      if (p != size_t(-1)) {
        add(sceneOf("a16_hustler-police-scenarios-enc", p));
        h.hustlerHeat += 1;
        mark("a16_hustler-police-scenarios-enc", p);
      }
    }
  } else {
    title = "A scam";
    const size_t st = pickRow("a16_hustler-scam-types-enc", "Money Score", "", "", "", hustlerRng_);
    std::string listDesc, successS, refuseS, insistS;
    double diff = 1, moneyScore = 2, policeScore = 2;
    if (st != size_t(-1)) {
      const Table& t = tbl("a16_hustler-scam-types-enc");
      listDesc = trim(t.at(st, "List Description"));
      successS = trim(t.at(st, "Success String"));
      refuseS = trim(t.at(st, "Refuse String"));
      insistS = trim(t.at(st, "Insist String"));
      diff = t.num(st, "Difficulty Score");
      moneyScore = t.num(st, "Money Score");
      policeScore = t.num(st, "Police Score");
      mark("a16_hustler-scam-types-enc", st);
    }
    if (listDesc.empty()) listDesc = "work a mark";
    // The mark is a person with an article, because the pool's strings write the
    // victim straight into a sentence.
    h.hustlerVictim = referenceToken("random_person_indefinite");
    if (h.hustlerVictim.empty()) h.hustlerVictim = "a passer-by";
    h.hustlerVictimFemale = hustlerRng_.chance(0.5);
    const size_t it = pickRow("a16_hustler-scam-items-enc", "", "Victim Gender",
                              h.hustlerVictimFemale ? "female" : "male", "", hustlerRng_);
    std::string itemName = "valuables";
    long long worthLo = 10, worthHi = 200;
    if (it != size_t(-1)) {
      const Table& t = tbl("a16_hustler-scam-items-enc");
      itemName = resolve(trim(t.at(it, "Item Singular")));
      worthLo = static_cast<long long>(t.num(it, "Min Value"));
      worthHi = static_cast<long long>(t.num(it, "Max Value"));
      if (worthHi < worthLo) worthHi = worthLo;
      mark("a16_hustler-scam-items-enc", it);
    }
    if (itemName.empty()) itemName = "valuables";
    hustlerCache_["item"] = itemName;   // '<item>' inside the pool's own strings
    take = worthLo + static_cast<long long>(hustlerRng_.below(static_cast<uint32_t>(worthHi - worthLo + 1)));
    h.hustlerAmount = take;
    body = "My scam of the day: " + listDesc + ". My mark was " + h.hustlerVictim + " in my " +
           h.hustlerAttire + ".";
    const bool wins = hustlerRng_.unit() <
        0.45 + double(h.hustlerSkill) / 220.0 - diff / 25.0 - moneyScore / 40.0;
    // The strings speak about the mark ('<he/she> was not interested'), so the
    // pronoun pair follows the mark rather than the hero.
    const bool oSave = pronounOtherSet_, fSave = pronounOtherFemale_;
    pronounOtherSet_ = true;
    pronounOtherFemale_ = h.hustlerVictimFemale;
    line = resolve(wins ? successS : refuseS);
    if (line.empty())
      line = wins ? ("I sold " + h.hustlerVictim + " " + itemName + " for " + money(take) + ".")
                  : ("I tried to sell " + h.hustlerVictim + " " + itemName + " but it did not work out.");
    if (!wins) {
      take = 0;
      h.hustlerAmount = 0;
      if (!insistS.empty() && hustlerRng_.chance(0.5)) add(resolve(insistS));
    }
    pronounOtherSet_ = oSave;
    pronounOtherFemale_ = fSave;
    if (policeScore >= 3 || h.hustlerHeat > 0) h.hustlerHeat += 1;
    // The mark's own scene, from the pool the friendliness window picks.
    if (hustlerRng_.chance(0.30)) {
      const double friendliness = 1 + hustlerRng_.below(6);
      const size_t v = pickRow("a16_hustler-scam-victim-scenarios-enc", "", "", "", "", hustlerRng_);
      if (v != size_t(-1)) {
        add(scene("a16_hustler-scam-victim-scenarios-enc", v, h.hustlerVictimFemale, true));
        mark("a16_hustler-scam-victim-scenarios-enc", v);
      }
      (void)friendliness;
    }
    if (policeScore >= 3 && hustlerRng_.chance(0.10 + double(h.hustlerHeat) / 40.0)) {
      const size_t p = pickRow("a16_hustler-police-scenarios-enc", "", "", "", "", hustlerRng_);
      if (p != size_t(-1)) {
        add(sceneOf("a16_hustler-police-scenarios-enc", p));
        h.hustlerHeat += 1;
        mark("a16_hustler-police-scenarios-enc", p);
      }
    }
    if (hustlerRng_.chance(0.15)) {
      const size_t ins = pickRow("a16_hustler-insults-enc", "", "Gender", gender, "", hustlerRng_);
      if (ins != size_t(-1))
        add("My mark called me " +
            resolve(trim(tbl("a16_hustler-insults-enc").at(ins, "Insult"))) + " on the way out.");
    }
  }

  if (title.empty()) { hustlerLive_ = live0; return false; }
  out.table = tableRef.empty() ? "a16_hustler-attire-enc" : tableRef;
  out.row_id = rowRef.empty() ? std::string("0") : rowRef;
  out.title = title;
  out.body = body;
  out.narrative = line;
  const std::vector<std::string> keep = out.extra;
  out.extra.clear();
  for (size_t i = 0; i < keep.size(); ++i) out.extra.push_back(keep[i]);
  for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
  h.money += take;
  h.hustlerEarned += take;
  out.happiness += happy;
  // The police keep an eye on a hero who is known on the street.
  if (h.hustlerHeat >= 3) h.hustlerHeat -= 1;
  hustlerLive_ = live0;
  return true;
}

// ===== The space program (REPORT.en.md 73) =====

// '$65,000' -> 65000, for the a16_astronaut-career-rank Min/Max Salary cells.
static long long astroMoney(const std::string& s) {
  std::string d;
  for (char c : s) if (c >= '0' && c <= '9') d.push_back(c);
  return d.empty() ? 0 : std::atoll(d.c_str());
}

void Sim::makeAstronaut(Life& life) {
  Hero& h = life.hero;
  h.astronaut = true;
  h.astroRank = 1;
  h.astroRankTitle = "Cadet";
  h.astroSalary = 65000;
  h.astroRespect = 40;
  h.astroFitness = 40;
  h.astroTechnical = 40;
  h.astroFlight = 40;
  h.astroMissions = 0;
  h.astroYearsInAgency = 0;
  h.astroStress = 0;
  h.astroAnxiety = 0;
  h.astroEarned = 0;
  h.astroBeenToSpace = false;
  h.astroAgency = "the space agency";
  h.astroShuttle = "the shuttle";
  // The hero joins the agency of their own country (a16_astronaut-countries).
  if (ensureTable("a16_astronaut-countries-enc")) {
    const Table& t = tbl("a16_astronaut-countries-enc");
    for (size_t r = 0; r < t.size(); ++r) {
      if (trim(t.at(r, "Country")) != h.country) continue;
      const std::string name = resolve(trim(t.at(r, "Space Agency Name")));
      if (!name.empty()) h.astroAgency = name;
      break;
    }
    if (h.astroAgency == "the space agency" && t.size() > 0)
      h.astroAgency = resolve(trim(t.at(0, "Space Agency Name")));
  }
  // The rank ladder starts at its first row (Cadet), pay inside its window.
  if (ensureTable("a16_astronaut-career-rank-enc")) {
    const Table& t = tbl("a16_astronaut-career-rank-enc");
    if (t.size() > 0) {
      h.astroRankTitle = trim(t.at(0, "Rank"));
      const long long lo = astroMoney(t.at(0, "Min Salary"));
      const long long hi = astroMoney(t.at(0, "Max Salary"));
      h.astroSalary = hi > lo ? lo + static_cast<long long>(astroRng_.below(unsigned(hi - lo + 1))) : lo;
    }
  }
  if (ensureTable("a16_astronaut-shuttle-names-enc")) {
    const Table& t = tbl("a16_astronaut-shuttle-names-enc");
    const size_t r = weightedPick(t, t.has("Commonness Score") ? "Commonness Score" : std::string(), astroRng_);
    if (r != size_t(-1)) {
      const std::string name = resolve(trim(t.at(r, "Name")));
      if (!name.empty()) h.astroShuttle = name;
    }
  }
  h.astroCrewmate = personName("crewmate");
  h.astroColleague = personName("colleague");
  h.astroDirector = personName("space-director");
  h.astroCrewmateFemale = astroRng_.chance(0.5);
  h.astroColleagueFemale = astroRng_.chance(0.5);
}

std::string Sim::astroToken(const std::string& token) {
  Hero* h = const_cast<Hero*>(hero_);
  if (h == nullptr) return std::string();
  std::string key = trim(token);
  bool possessive = false;
  static const std::string suffix = "-possessive";
  if (key.size() > suffix.size() && key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0) {
    possessive = true;
    key.resize(key.size() - suffix.size());
  }
  // One draw per slot per card, so a name or a body repeated in the same card
  // agrees with itself.
  if (key == "crewmate" || key == "colleague" || key == "space-director") {
    std::string name = key == "crewmate" ? h->astroCrewmate
                     : key == "colleague" ? h->astroColleague : h->astroDirector;
    if (name.empty()) name = personName(key);
    if (name.empty()) name = key == "space-director" ? "the director" : "a colleague";
    return possessive ? name + "'s" : name;
  }
  if (key == "shuttle") return h->astroShuttle.empty() ? std::string("the shuttle") : h->astroShuttle;
  if (key == "space-agency") return h->astroAgency.empty() ? std::string("the space agency") : h->astroAgency;
  if (key == "destination-moon" || key == "destination-mars") {
    if (!h->astroDestination.empty()) return h->astroDestination;
    const char* tab = key == "destination-moon" ? "a16_astronaut-moon-destinations-enc"
                                                : "a16_astronaut-mars-destinations-enc";
    std::string v;
    if (ensureTable(tab)) {
      const Table& t = tbl(tab);
      const size_t r = weightedPick(t, t.has("Commonness Score") ? "Commonness Score" : std::string(), astroRng_);
      if (r != size_t(-1)) {
        v = trim(t.at(r, "Location"));
        const bool wants = t.has("Uses Definite Article") ? t.flag(r, "Uses Definite Article")
                                                          : t.flag(r, "Requires Definite Article");
        if (wants) v = std::string("the ") + v;
      }
    }
    h->astroDestination = v;
    return v;
  }
  if (key == "discovery-name") {
    auto it = astroCache_.find("discovery");
    if (it != astroCache_.end()) return it->second;
    std::string v;
    if (ensureTable("a16_astronaut-celestial-body-names-enc")) {
      const Table& t = tbl("a16_astronaut-celestial-body-names-enc");
      const size_t r = weightedPick(t, t.has("Commonness Score") ? "Commonness Score" : std::string(), astroRng_);
      if (r != size_t(-1)) v = resolve(trim(t.at(r, "Name")));
    }
    astroCache_["discovery"] = v;
    return v.empty() ? std::string("an unnamed object") : v;
  }
  return std::string();
}

bool Sim::astroYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.astronaut) return false;
  if (h.age < 8) return false;
  if (!ensureTable("a16_astronaut-dumb-scenarios-enc")) return false;
  if (!astroRng_.chance(0.5)) return false;
  hero_ = &h;
  life_ = &life;

  std::string title, body, line;
  std::vector<std::string> extra;
  std::string tableRef, rowRef;
  int happy = 0;
  long long take = 0;

  const auto say = [&](const std::string& s) {
    if (line.empty() && !s.empty()) line = spyUpperFirst(s);
  };
  const auto add = [&](const std::string& s) { if (!s.empty()) extra.push_back(spyUpperFirst(s)); };
  const auto cell = [&](const Table& t, size_t r, const char* col) -> std::string {
    if (r == size_t(-1) || !t.has(col)) return std::string();
    return resolve(trim(t.at(r, col)));
  };
  const auto mark = [&](const char* tab, size_t r) { tableRef = tab; rowRef = std::to_string(r + 1); };
  // Rows whose log line points back at an alternative drawn earlier in the row
  // ('<rb>', '<rb2>'): every other bracketed column is resolved first, in column
  // order, which is the order the game draws them in (REPORT.en.md 57).
  const auto withBrackets = [&](const Table& t, size_t r, const char* wantCol) -> std::string {
    if (r == size_t(-1)) return std::string();
    for (const std::string& cc : t.columns()) {
      if (cc == wantCol) continue;
      if (t.at(r, cc).find('<') == std::string::npos) continue;
      cell(t, r, cc.c_str());
    }
    return cell(t, r, wantCol);
  };
  // A pool that writes about somebody else: the pronoun pair follows that person
  // rather than the hero while the row renders.
  const auto scene = [&](const Table& t, size_t r, bool otherFemale, bool other, const char* wantCol) -> std::string {
    if (r == size_t(-1)) return std::string();
    const bool oSave = pronounOtherSet_, fSave = pronounOtherFemale_;
    pronounOtherSet_ = other;
    pronounOtherFemale_ = otherFemale;
    std::string s = withBrackets(t, r, wantCol);
    pronounOtherSet_ = oSave;
    pronounOtherFemale_ = fSave;
    return s;
  };
  const auto draw = [&](const char* tab, const char* weightCol) -> size_t {
    return pickRow(tab, weightCol, "", "", "", astroRng_);
  };
  const auto flush = [&]() {
    for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
    extra.clear();
  };
  const auto delegate = [&](const char* tab, size_t r) -> bool {
    flush();
    const bool p = astroLive_;
    astroLive_ = true;
    const bool ok = renderRow(tbl(tab), r, life, out, forcedChoice);
    astroLive_ = p;
    return ok;
  };
  const auto hurt = [&](int n) {
    h.health = std::max(0, std::min(100, h.health + n));
  };
  const auto bump = [&](int& stat, int n) {
    stat = std::max(0, std::min(100, stat + n));
  };
  // '<country-demonym>' occurs once in the whole corpus, in the probe names of
  // this family, so it is answered here rather than in the shared hero tokens.
  const auto demonym = [&]() -> std::string {
    for (size_t ci = 0; ci < countries_.size(); ++ci)
      if (countries_[ci].name == h.country && !countries_[ci].demonym.empty())
        return countries_[ci].demonym;
    return "Earth";
  };

  const bool live0 = astroLive_;
  astroLive_ = true;

  h.astroYearsInAgency += 1;
  if (h.astroCrewmate.empty()) h.astroCrewmate = personName("crewmate");
  if (h.astroColleague.empty()) h.astroColleague = personName("colleague");
  if (h.astroDirector.empty()) h.astroDirector = personName("space-director");
  h.astroCrewmateFemale = astroRng_.chance(0.5);
  h.astroColleagueFemale = astroRng_.chance(0.5);
  h.astroDestination.clear();
  h.astroMission.clear();
  // The year's assignment is drawn here rather than inside the mission card:
  // the goofing-off pool and the mission's own dumb scenario need a mission and
  // a destination to attach to, and a card that set them as it rendered them
  // would leave both pools unreachable (REPORT.en.md 73).
  bool missionToMoon = false;
  const char* missionTab = nullptr;
  size_t missionRow = size_t(-1);
  if (astroRng_.chance(0.30)) {
    missionToMoon = !h.astroBeenToSpace || astroRng_.chance(0.5);
    missionTab = missionToMoon ? "a16_astronaut-lunar-missions-enc"
                               : "a16_astronaut-martian-missions-enc";
    missionRow = draw(missionTab, h.astroBeenToSpace ? "Danger Score" : "");
    if (missionRow != size_t(-1)) {
      const Table& mt = tbl(missionTab);
      h.astroMission = cell(mt, missionRow, mt.has("Mission Type") ? "Mission Type" : "Type");
      if (missionToMoon) astroToken("destination-moon");
      else astroToken("destination-mars");
    } else {
      missionTab = nullptr;
    }
  }

  // (1) A warning from the director (rarest first, so the common cards cannot
  //     starve it).
  if (title.empty() && astroRng_.chance(0.04)) {
    const size_t r = draw("a16_astronaut-agency-director-warning-enc", "");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_astronaut-agency-director-warning-enc");
      title = "A warning from the director";
      body = cell(t, r, "Warning");
      mark("a16_astronaut-agency-director-warning-enc", r);
      say("My director pulled me aside to warn me about my conduct in the agency.");
      bump(h.astroRespect, -3);
      h.astroStress += 2;
    }
  }

  // (2) An artifact brought home from the last mission.
  if (title.empty() && astroRng_.chance(0.06)) {
    const size_t r = draw("a16_astronaut-artifacts-enc", "Commonness Score");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_astronaut-artifacts-enc");
      const std::string item = resolve(trim(t.at(r, "Item Name")));
      const std::string where = trim(t.at(r, "Location"));
      const long long lo = std::atoll(trim(t.at(r, "Min Value")).c_str());
      const long long hi = std::atoll(trim(t.at(r, "Max Value")).c_str());
      const long long worth = hi > lo ? lo + static_cast<long long>(astroRng_.below(unsigned(hi - lo + 1))) : lo;
      h.astroArtifact = item;
      title = "An artifact from " + where;
      body = "I brought back " + withArticle(item, t.flag(r, "Indefinite Article")) + item + " from " + where + ".";
      mark("a16_astronaut-artifacts-enc", r);
      say("I smuggled a " + item + " back from " + where + " and sold it for " + money(worth) + ".");
      take += worth;
      happy += 2;
    }
  }

  // (3) A new object in the sky, named and filed.
  if (title.empty() && astroRng_.chance(0.06)) {
    const std::string name = astroToken("discovery-name");
    const char* kData[] = {"a16_astronaut-star-data-enc", "a16_astronaut-planet-data-enc",
                           "a16_astronaut-galaxy-data-enc", "a16_astronaut-black-hole-data-enc",
                           "a16_astronaut-asteroid-data-enc", "a16_astronaut-comet-data-enc"};
    const char* tab = kData[astroRng_.below(6)];
    h.astroDiscovery = name;
    title = "A new object in the sky";
    body = "I catalogued " + name + ".";
    if (ensureTable(tab)) {
      const Table& t = tbl(tab);
      const size_t r = draw(tab, "");
      if (r != size_t(-1)) {
        std::string detail;
        for (const std::string& cc : t.columns()) {
          if (cc == "Name") continue;
          const std::string v = cell(t, r, cc.c_str());
          if (v.empty()) continue;
          detail += (detail.empty() ? " " : ", ") + cc + ": " + v;
        }
        if (!detail.empty()) body += "\n" + name + detail + ".";
        mark(tab, r);
      }
    }
    if (astroRng_.chance(0.25)) {
      const size_t ns = draw("a16_astronaut-nebula-shapes-enc", "");
      if (ns != size_t(-1)) {
        const Table& nt = tbl("a16_astronaut-nebula-shapes-enc");
        const std::string shape = resolve(trim(nt.at(ns, "Shape")));
        add("Its shape is " + withArticle(shape, nt.flag(ns, "Uses Indefinite Article")) + shape + ".");
      }
    }
    if (astroRng_.chance(0.20)) {
      const size_t pr = draw("a16_astronaut-probe-names-enc", "Commonness Score");
      if (pr != size_t(-1)) {
        const Table& pt = tbl("a16_astronaut-probe-names-enc");
        std::string probe = resolve(trim(pt.at(pr, "Name")));
        const size_t dpos = probe.find("<country-demonym>");
        if (dpos != std::string::npos)
          probe = probe.substr(0, dpos) + demonym() + probe.substr(dpos + 17);
        add("The agency's " + probe + " made the pass.");
      }
    }
    say("I discovered " + name + " through the observatory telescope.");
    bump(h.astroTechnical, 3);
    happy += 3;
  }

  // (4) Repairing the station hardware.
  if (title.empty() && astroRng_.chance(0.08)) {
    const size_t r = draw("a16_astronaut-satellite-parts-enc", "Commonness Score");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_astronaut-satellite-parts-enc");
      const std::string part = resolve(trim(t.at(r, "Part")));
      const double difficulty = t.num(r, "Repair Difficulty");
      const bool ok = astroRng_.unit() < 0.85 - difficulty * 0.12 + double(h.astroTechnical) / 400.0;
      title = "A repair in orbit";
      body = "The " + part + " needed a repair.";
      mark("a16_astronaut-satellite-parts-enc", r);
      if (ok) {
        say("I repaired the " + part + " on the station.");
        bump(h.astroTechnical, 2);
      } else {
        const double damage = t.num(r, "Damage Score");
        say("I broke the " + part + " while trying to repair it.");
        hurt(-int(damage) - 1);
        bump(h.astroRespect, -2);
      }
    }
  }

  // (5) A night on the scanner.
  if (title.empty() && astroRng_.chance(0.08)) {
    const size_t r = draw("a16_astronaut-scanner-observations-enc", "Commonness Score");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_astronaut-scanner-observations-enc");
      // The Feedback Snippet owns the alternatives and the Log Snippet points
      // back at them ('a black hole <devouring/slurping up> a <star/nebula>'
      // then 'a black hole <rb> a <rb2>'), so the feedback column is resolved
      // first, in the row's own column order (REPORT.en.md 73).
      const std::string seen = cell(t, r, "Object");
      const std::string feedback = cell(t, r, "Feedback Snippet");
      const std::string logged = cell(t, r, "Log Snippet");
      title = "A night on the scanner";
      body = "I watched " + seen + " from the observatory.";
      say("I studied " + logged + ".");
      mark("a16_astronaut-scanner-observations-enc", r);
      add(feedback + ".");
      bump(h.astroTechnical, t.num(r, "Research Score") >= 3 ? 2 : 1);
      happy += 1;
    }
  }

  // (6) The people at home are worried.
  if (title.empty() && astroRng_.chance(0.08)) {
    const size_t r = draw("a16_astronaut-anxiety-scenarios-enc", "");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_astronaut-anxiety-scenarios-enc");
      const std::string audience = resolve(trim(t.at(r, "Audience")));
      // The Audience cell is a list ('Child, Lover'); the line is written about
      // the first of them, and the whole card keys on one gender so that the
      // body and the log cannot disagree (REPORT.en.md 73).
      std::vector<std::string> who;
      splitOn(audience, ',', who);
      std::string one = lower(trim(who.empty() ? audience : who[0]));
      // Five rows leave Audience blank, and the log line always needs a subject
      // ('was afraid I'd ...'), so it falls back to a family member.
      if (one.empty()) one = "loved one";
      const bool whose = astroRng_.chance(0.5);
      // Thirty-one of the forty-three Snippets carry their own subject
      // ('<he/she>'s afraid you'll ...'); the other twelve are a bare verb
      // phrase ('believes that ...') and need the audience named in front.
      const std::string raw = trim(t.at(r, "Snippet"));
      const std::string snip = scene(t, r, whose, true, "Snippet");
      title = "Everyone worries";
      body = "Audience: " + (audience.empty() ? std::string("Family") : audience) + ".\n" +
             (raw.empty() || raw[0] == '<' ? spyUpperFirst(snip) : ("My " + one + " " + snip));
      say("My " + one + " " + scene(t, r, whose, true, "Log Version") + ".");
      mark("a16_astronaut-anxiety-scenarios-enc", r);
      h.astroAnxiety += 1;
      happy -= 2;
    }
  }

  // (7) Talking shop with a colleague.
  if (title.empty() && astroRng_.chance(0.08)) {
    size_t r = draw("a16_astronaut-conversations-enc", "");
    const Table* ct = ensureTable("a16_astronaut-conversations-enc") ? &tbl("a16_astronaut-conversations-enc") : nullptr;
    if (ct != nullptr && r != size_t(-1) && !h.astroBeenToSpace && ct->flag(r, "Hero Has Been to Space")) {
      r = size_t(-1);
      for (int i = 0; i < 3 && r == size_t(-1); ++i) {
        const size_t cand = draw("a16_astronaut-conversations-enc", "");
        if (cand != size_t(-1) && !ct->flag(cand, "Hero Has Been to Space")) r = cand;
      }
    }
    if (ct != nullptr && r != size_t(-1)) {
      const std::string audience = resolve(trim(ct->at(r, "Audience")));
      title = "Talking shop";
      body = "Audience: " + audience + ".\nWe got onto the subject of " +
             resolve(trim(ct->at(r, "Conversation"))) + ".";
      say("I chatted with my " + lower(audience) + " about " +
          resolve(trim(ct->at(r, "Conversation"))) + ".");
      mark("a16_astronaut-conversations-enc", r);
      happy += 1;
    }
  }

  // (8) A prank, inbound or outbound.
  if (title.empty() && astroRng_.chance(0.08)) {
    const size_t r = draw("a16_astronaut-pranks-enc", "");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_astronaut-pranks-enc");
      const bool outbound = astroRng_.chance(0.4);
      title = outbound ? "A prank played" : "A prank discovered";
      body = outbound ? ("I " + scene(t, r, h.astroColleagueFemale, true, "Outbound Snippet"))
                      : scene(t, r, h.astroColleagueFemale, true, "Inbound Scenario");
      if (!body.empty() && body[body.size() - 1] != '.') body += ".";
      say(outbound ? ("I " + scene(t, r, h.astroColleagueFemale, true, "Outbound Snippet") + ".")
                   : (scene(t, r, h.astroColleagueFemale, true, "Inbound Log Version") + "."));
      mark("a16_astronaut-pranks-enc", r);
      happy += 1;
    }
  }

  // (9) A paper in the agency's journal, and its reception.
  if (title.empty() && astroRng_.chance(0.10)) {
    const bool review = !h.astroPublication.empty() || astroRng_.chance(0.5);
    if (review) {
      const size_t r = draw("a16_astronaut-peer-review-color-enc", "");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_astronaut-peer-review-color-enc");
        const std::string s = cell(t, r, "String");
        const double reception = t.num(r, "Max Reception");
        title = "Peer review";
        body = s;
        mark("a16_astronaut-peer-review-color-enc", r);
        say("My peers reviewed my work: " + lower(s));
        if (reception >= 50) { bump(h.astroRespect, 3); happy += 2; }
        else { bump(h.astroRespect, -2); h.astroStress += 2; }
      }
    } else {
      const size_t r = draw("a16_astronaut-publication-titles-enc", "Commonness Score");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_astronaut-publication-titles-enc");
        h.astroPublication = resolve(trim(t.at(r, "Name")));
        title = "A paper published";
        body = "Title: " + h.astroPublication + ".";
        mark("a16_astronaut-publication-titles-enc", r);
        say("I published my research in the agency journal.");
        bump(h.astroTechnical, 2);
        bump(h.astroRespect, 2);
      }
    }
  }

  // (10) Traffic with the ground.
  if (title.empty() && astroRng_.chance(0.10)) {
    if (astroRng_.chance(0.5)) {
      const size_t r = draw("a16_astronaut-inbound-messages-enc", "");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_astronaut-inbound-messages-enc");
        title = "A message from the void";
        body = "The signal repeated: " + resolve(trim(t.at(r, "Message"))) + ".";
        mark("a16_astronaut-inbound-messages-enc", r);
        say("I recorded a strange transmission out past the belt.");
        happy += 1;
      }
    } else {
      const size_t r = draw("a16_astronaut-outbound-messages-enc", "Commonness Score");
      if (r != size_t(-1)) {
        const Table& t = tbl("a16_astronaut-outbound-messages-enc");
        title = "A message home";
        body = "I broadcast: " + resolve(trim(t.at(r, "Message"))) + ".";
        mark("a16_astronaut-outbound-messages-enc", r);
        say("I sent a transmission back to " + h.astroAgency + ".");
        bump(h.astroRespect, t.num(r, "Success Score") >= 4 ? 2 : -1);
      }
    }
  }

  // (11) Goofing off during a mission. It sits before the mission card, so a
  //      year with an assignment can be spent shirking it.
  if (title.empty() && !h.astroDestination.empty() && astroRng_.chance(0.12)) {
    const size_t r = draw("a16_astronaut-mission-goof-off-enc", "Commonness Score");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_astronaut-mission-goof-off-enc");
      title = "Goofing off at " + h.astroDestination;
      body = "I " + cell(t, r, "Snippet") + ".";
      say("I " + cell(t, r, "Log Version") + " while the crew was working.");
      mark("a16_astronaut-mission-goof-off-enc", r);
      const double impact = t.num(r, "Performance Impact");
      bump(h.astroFitness, int(impact));
      bump(h.astroRespect, impact < 0 ? -2 : 1);
      happy += 1;
    }
  }

  // (12) Something dumb happens on the assignment instead of the mission. The
  //      mission pool is drawn here rather than in the dumb-scenario branch
  //      below, which can never see an assignment: the mission card has no
  //      chance roll of its own, so every assigned year reaches it first
  //      (REPORT.en.md 73).
  if (title.empty() && missionRow != size_t(-1) && astroRng_.chance(0.30)) {
    const size_t r = draw("a16_astronaut-mission-dumb-scenarios-enc", "");
    if (r != size_t(-1) && delegate("a16_astronaut-mission-dumb-scenarios-enc", r)) {
      bump(h.astroRespect, 1);
      astroLive_ = live0;
      return true;
    }
  }

  // (13) The mission itself, from the assignment drawn above.
  if (title.empty() && missionTab != nullptr && missionRow != size_t(-1)) {
    const Table& t = tbl(missionTab);
    const size_t r = missionRow;
    const std::string dest = h.astroDestination;
    const double danger = t.has("Danger Score") ? t.num(r, "Danger Score") : 2;
    const double odds = std::max(0.15, std::min(0.92,
               0.55 + double(h.astroFlight + h.astroTechnical - 80) / 200.0 - danger * 0.05));
    const bool ok = astroRng_.unit() < odds;
    title = missionToMoon ? "A mission to the Moon" : "A mission to Mars";
    body = "Mission: " + h.astroMission + ".\nDestination: " + dest + ".";
    if (!missionToMoon) body += "\nThe crew: " + h.astroCrewmate + " and " + h.astroColleague + ".";
    mark(missionTab, r);
    const bool oSave = pronounOtherSet_, fSave = pronounOtherFemale_;
    pronounOtherSet_ = true;
    pronounOtherFemale_ = astroRng_.chance(0.5);
    const std::string brief = spyUpperFirst(cell(t, r, "Snippet"));
    pronounOtherSet_ = oSave;
    pronounOtherFemale_ = fSave;
    if (!brief.empty()) body += "\n" + brief;
    if (ok) {
      say("I " + cell(t, r, "Log Success") + ".");
      h.astroMissions += 1;
      h.astroBeenToSpace = true;
      take += 5000;
      bump(h.astroFlight, 3);
      bump(h.astroTechnical, 2);
      bump(h.astroRespect, 2);
      happy += 3;
    } else {
      say("My mission failed " + cell(t, r, "Log Failure") + ".");
      h.astroStress += 4;
      hurt(-int(danger));
      bump(h.astroRespect, -2);
    }
  }

  // (14) Something goes wrong on the way up or down.
  if (title.empty() && astroRng_.chance(0.12)) {
    const size_t r = draw("a16_astronaut-flight-scenario-enc", "");
    if (r != size_t(-1)) {
      // The card's four options each carry their own Flight / Performance
      // Impact, which the shared renderer does not know about, so the year
      // applies its own training effects (shortcut: flat impacts).
      if (delegate("a16_astronaut-flight-scenario-enc", r)) {
        if (out.title.empty()) out.title = "Trouble on the way";  // the pool has no Title column
        bump(h.astroFlight, astroRng_.chance(0.5) ? 2 : -2);
        bump(h.astroTechnical, 1);
        astroLive_ = live0;
        return true;
      }
    }
  }

  // (15) An experiment in weightlessness.
  if (title.empty() && astroRng_.chance(0.12)) {
    const size_t r = draw("a16_astronaut-experiment-scenarios-enc", "");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_astronaut-experiment-scenarios-enc");
      const bool secret = t.num(r, "Deception Score") >= 3 && astroRng_.chance(0.5);
      const std::string what = cell(t, r, "Snippet");
      const double risk = t.num(r, "Risk Score");
      title = "An experiment in orbit";
      body = "The experiment: " + what + ".";
      if (secret) body += "\n" + cell(t, r, "Deception Log Version") + ".";
      say(secret ? ("I " + cell(t, r, "Deception Log Version") + " and kept it off the record.")
                 : ("I decided to " + what + "."));
      mark("a16_astronaut-experiment-scenarios-enc", r);
      bump(h.astroTechnical, int(t.num(r, "Performance Impact")));
      if (risk >= 3 && astroRng_.chance(0.10 * risk)) {
        add("The experiment went badly: " + cell(t, r, "Feedback Snippet") + ".");
        hurt(-int(risk));
      } else if (!cell(t, r, "Feedback Snippet").empty()) {
        say(cell(t, r, "Feedback Snippet") + ".");
      }
      if (secret) bump(h.astroRespect, -2);
    }
  }

  // (16) A dumb scenario from the family's own pool. The mission pool is drawn
  //      in its own branch above, which every assigned year reaches first.
  if (title.empty() && astroRng_.chance(0.12)) {
    const char* tab = "a16_astronaut-dumb-scenarios-enc";
    const size_t r = draw(tab, "");
    if (r != size_t(-1) && delegate(tab, r)) {
      bump(h.astroRespect, 1);
      astroLive_ = live0;
      return true;
    }
  }

  // (17) The training programme at the academy.
  if (title.empty() && astroRng_.chance(0.14)) {
    const bool onlyProgram = h.astroRank == 1;
    size_t r = size_t(-1);
    for (int i = 0; i < 3 && r == size_t(-1); ++i) {
      const size_t cand = draw("a16_astronaut-fitness-training-scenario-enc", "Commonness Score");
      if (cand == size_t(-1)) break;
      const Table& t = tbl("a16_astronaut-fitness-training-scenario-enc");
      if (t.flag(cand, "Training Program Only") && !onlyProgram) continue;
      r = cand;
    }
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_astronaut-fitness-training-scenario-enc");
      const std::string type = lower(trim(t.at(r, "Training Type")));
      title = type == "cardio" ? "Training day" : "Training session";
      body = scene(t, r, false, true, "Scenario");
      say(scene(t, r, false, true, "Log Version"));
      mark("a16_astronaut-fitness-training-scenario-enc", r);
      const int impact = int(t.num(r, "Impact Score"));
      if (type == "cardio") bump(h.astroFitness, impact);
      else bump(h.astroTechnical, impact);
      happy += 1;
    }
  }

  // (18) The written exam.
  if (title.empty() && astroRng_.chance(0.18)) {
    const size_t r = draw("a16_astronaut-training-questions-enc", "Commonness Score");
    if (r != size_t(-1)) {
      const Table& t = tbl("a16_astronaut-training-questions-enc");
      const bool pass = astroRng_.unit() < 0.40 + double(h.astroTechnical) / 250.0;
      title = "A training exam";
      body = resolve(trim(t.at(r, "Question"))) + "\nCorrect answer: " +
             resolve(trim(t.at(r, "Correct Answers"))) + ".";
      if (!pass) {
        const std::string dummies = resolve(trim(t.at(r, "Dummy Answers")));
        const size_t comma = dummies.find(',');
        body += "\nI wrote down: " + (comma == std::string::npos ? dummies : dummies.substr(0, comma)) + ".";
      }
      mark("a16_astronaut-training-questions-enc", r);
      if (pass) {
        say("I passed the agency's training exam.");
        bump(h.astroTechnical, 3);
        bump(h.astroRespect, 2);
      } else {
        say("I failed the agency's training exam.");
        bump(h.astroRespect, -2);
        h.astroStress += 2;
      }
    }
  }

  if (title.empty()) { astroLive_ = live0; return false; }

  // Promotion needs the rank's own gates, from a16_astronaut-career-rank.
  if (ensureTable("a16_astronaut-career-rank-enc")) {
    const Table& t = tbl("a16_astronaut-career-rank-enc");
    const size_t row = std::min<size_t>(size_t(h.astroRank) - 1, t.size() - 1);
    const long long lo = astroMoney(t.at(row, "Min Salary"));
    const long long hi = astroMoney(t.at(row, "Max Salary"));
    take += hi > lo ? lo + static_cast<long long>(astroRng_.below(unsigned(hi - lo + 1))) : lo;
    if (h.astroRank < int(t.size()) && h.astroMissions >= int(t.num(row, "Missions to Promotion")) &&
        h.astroYearsInAgency >= int(t.num(row, "Min Years in Agency")) &&
        h.smarts >= int(t.num(row, "Minimum Intelligence")) &&
        h.discipline >= int(t.num(row, "Minimum Discipline")) &&
        h.health >= int(t.num(row, "Minimum Health"))) {
      h.astroRank += 1;
      h.astroMissions = 0;
      h.astroYearsInAgency = 0;
      const size_t next = size_t(h.astroRank) - 1;
      h.astroRankTitle = trim(t.at(next, "Rank"));
      h.astroSalary = astroMoney(t.at(next, "Min Salary"));
      add("I was promoted to " + h.astroRankTitle + " of " + h.astroAgency + ".");
      bump(h.astroRespect, 5);
      happy += 3;
    }
  }

  out.table = tableRef.empty() ? "a16_astronaut-dumb-scenarios-enc" : tableRef;
  out.row_id = rowRef.empty() ? std::string("0") : rowRef;
  out.title = title;
  out.body = body;
  out.narrative = line;
  const std::vector<std::string> keep = out.extra;
  out.extra.clear();
  for (size_t i = 0; i < keep.size(); ++i) out.extra.push_back(keep[i]);
  for (size_t i = 0; i < extra.size(); ++i) out.extra.push_back(extra[i]);
  h.money += take;
  h.astroEarned += take;
  out.happiness += happy;
  if (h.astroStress > 0) h.astroStress -= 1;
  astroLive_ = live0;
  return true;
}

void Sim::makeMisc(Life& life, unsigned mask) { life.hero.miscMask |= mask; }

bool Sim::miscPoolYear(Life& life, AgeUp& out, int forcedChoice) {
  static const struct MiscPool { const char* table; int minAge; double chance; } kPools[] = {
    {"a16_dumb-scenarios-mafia-enc", 18, 0.30},
    {"a16_dumb-scenarios-dealer-enc", 18, 0.30},
    {"a16_dumb-scenarios-hustler-enc", 16, 0.35},
    {"a16_dumb-scenarios-landlord-enc", 18, 0.35},
    {"a16_dumb-scenarios-zoo-enc", 18, 0.30},
    {"a16_dumb-scenarios-model-enc", 16, 0.30},
    {"a16_dumb-scenarios-actor-enc", 16, 0.30},
    {"a16_dumb-scenarios-secret-agent-enc", 21, 0.25},
    {"a16_dumb-scenarios-casino-enc", 21, 0.30},
    {"a16_black-market-exhibit-dumb-scenarios-enc", 18, 0.25},
    {"a16_black-market-museum-dumb-scenarios-enc", 18, 0.25},
    {"a16_black-market-vendor-dumb-scenarios-enc", 18, 0.25},
  };
  hero_ = &life.hero;
  life_ = &life;
  for (size_t i = 0; i < sizeof(kPools) / sizeof(kPools[0]); ++i) {
    if ((life.hero.miscMask & (1u << i)) == 0) continue;
    if (life.hero.age < kPools[i].minAge) continue;
    if (!miscRng_.chance(kPools[i].chance)) continue;
    if (!ensureTable(kPools[i].table)) continue;
    const Table& t = tbl(kPools[i].table);
    if (t.size() == 0) continue;
    return renderRow(t, miscRng_.below(uint32_t(t.size())), life, out, forcedChoice);
  }
  return false;
}

std::string Sim::politicalToken(const std::string& token) {
  if (!hero_) return std::string();
  const Hero& h = *hero_;
  std::string office = h.office;
  std::string juris = h.jurisdiction;
  std::string jurisAdj = h.jurisdictionAdj;
  if (office.empty() && life_) {
    ensureOfficeFields(*const_cast<Life*>(life_));
    office = hero_->office;
    juris = hero_->jurisdiction;
    jurisAdj = hero_->jurisdictionAdj;
  }
  const std::string psuffix = "-possessive";
  const bool possessive =
      token.size() > psuffix.size() &&
      token.compare(token.size() - psuffix.size(), psuffix.size(), psuffix) == 0;
  const std::string key = possessive ? token.substr(0, token.size() - psuffix.size()) : token;
  if (key == "country-other") return heroToken("country-other", possessive);
  if (key == "opponent") {
    const std::string name = drawFirstName(politicsRng_.chance(0.5)) + " " + drawSurname();
    return possessive ? name + "'s" : name;
  }
  if (key == "jurisdiction") return possessive ? juris + "'s" : juris;
  if (key == "jurisdiction adjective" || key == "jurisdiction-adjective") return jurisAdj;
  // The office itself is only needed for these two keys, so the guard sits here
  // and not above: the title column's own '<jurisdiction adjective>' token is
  // resolved while the office string is still being built.
  if (office.empty()) return std::string();
  if (key == "political-office") return possessive ? office + "'s" : office;
  if (key == "opponent") {
    const std::string name = drawFirstName(politicsRng_.chance(0.5)) + " " + drawSurname();
    return possessive ? name + "'s" : name;
  }
  return std::string();
}

bool Sim::politicalYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (h.age < 18) return false;
  // The campaign pool runs whether or not the hero holds office; the office
  // pool needs the office. Both take the same coin.
  const bool campaign = !h.politician || politicsRng_.chance(0.5);
  const char* pool = campaign ? "a16_dumb-scenarios-political-campaign-enc"
                              : "a16_dumb-scenarios-political-office-enc";
  if (!h.politician && !politicsRng_.chance(0.25)) return false;
  if (!ensureTable(pool)) return false;
  if (!politicsRng_.chance(0.4)) return false;
  const Table& t = tbl(pool);
  std::string ccode;
  for (const Country& c : countries_) {
    if (lower(c.name) == lower(h.country)) { ccode = lower(c.code); break; }
  }
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (t.flag(r, "US Only") && ccode != "us") continue;
    if (t.flag(r, "UK Only") && ccode != "gb" && ccode != "uk") continue;
    if (t.has("Hero Gender")) {
      const std::string g = lower(trim(t.at(r, "Hero Gender")));
      if (!g.empty() && g != "both" && g != "all" && g != (h.female ? "female" : "male")) continue;
    }
    // Rows about a partner need one: 'Hero Has Lover' plus the partner's own
    // gender column, which is what the campaign pool gates on.
    const bool hasLover = !h.loverName.empty();
    if (t.has("Hero Has Lover") && t.flag(r, "Hero Has Lover") && !hasLover) continue;
    if (t.has("Lover Gender")) {
      const std::string g = lower(trim(t.at(r, "Lover Gender")));
      if (!g.empty() && g != "both" && g != "all") {
        if (!hasLover) continue;
        if (g != (h.loverFemale ? "female" : "male")) continue;
      }
    }
    if (h.politician) {
      if (t.flag(r, "National") && h.officeLevel != "national") continue;
      if (t.flag(r, "Regional") && h.officeLevel != "regional") continue;
      if (t.flag(r, "City") && h.officeLevel != "city") continue;
      if (t.flag(r, "Local") && h.officeLevel != "local") continue;
    }
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  return renderRow(t, rows[politicsRng_.below(uint32_t(rows.size()))], life, out, forcedChoice);
}

void Sim::makeMusician(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!ensureTable("a16_music-genres-enc")) return;
  const Table& g = tbl("a16_music-genres-enc");
  if (!g.size()) return;
  h.musicGenre = trim(g.at(musicRng_.below(uint32_t(g.size())), "Genre"));
  if (h.musicGenre.empty()) return;
  h.musician = true;
  const std::string band = musicTitle("a16_music-names-bands-enc", h.musicGenre);
  if (!band.empty()) h.bandName = band;
  ensureStudioFields(life);   // the role: singer, guitarist, bassist, drummer ...
}

// A title from a music names table, restricted to the genre's own flag column
// when that table has one ('Pop', 'Rap', 'Country', 'Folk', 'R&B', 'Worship',
// 'Rock'), otherwise drawn from every row.
std::string Sim::musicTitle(const char* table, const std::string& genre) {
  if (!ensureTable(table)) return std::string();
  const Table& t = tbl(table);
  if (!t.size()) return std::string();
  std::vector<size_t> pool;
  const bool flagged = !genre.empty() && t.has(genre.c_str());
  for (size_t r = 0; r < t.size(); ++r) {
    if (flagged && !t.flag(r, genre.c_str())) continue;
    pool.push_back(r);
  }
  if (pool.empty()) {
    for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
  }
  if (pool.empty()) return std::string();
  const std::string title = trim(t.at(pool[musicRng_.below(uint32_t(pool.size()))], "Title"));
  if (title.empty()) return std::string();
  return resolve(title);  // <a/b/c> alternates and <hero-name> slots
}

std::string Sim::musicToken(const std::string& token) {
  if (!hero_) return std::string();
  const Hero& h = *hero_;
  if (token == "music-genre") return h.musicGenre;
  if (token == "band-member") {
    const bool female = musicRng_.chance(0.5);
    return drawFirstName(female) + " " + drawSurname();
  }
  // A studio trade works with musicians even when the hero never took the
  // artist line (REPORT.en.md 69), so the music tokens answer for it too.
  if (!h.musician && !h.tradeSoundcloud && !h.tradeProducer && !h.tradeScout &&
      !h.tradeComposer && !h.tradeStunts) return std::string();
  if (token == "album-title") return musicTitle("a16_music-names-albums-enc", h.musicGenre);
  if (token == "music-role") return h.musicRole;
  if (token == "artist-name" || token == "artist-name-possessive") {
    const std::string n = h.bandName.empty() ? (h.firstName + " " + h.lastName) : h.bandName;
    return token == "artist-name" ? n : llPossessive(n);
  }
  if (token == "album-description") return musicAlbumDescription();
  if (token == "musician-soundcloud") {
    // The collab partner the card already named, else a reference-pool draw,
    // else a name: the row must never leak the bracket (REPORT.en.md 69).
    if (!h.tradeTalent.empty()) return h.tradeTalent;
    const std::string v = referenceToken("musician_soundcloud");
    if (!v.empty()) return v;
    const std::string m = referenceToken("musician");
    if (!m.empty()) return m;
    return drawFirstName(tradeRng_.chance(0.5)) + " " + drawSurname();
  }
  if (token.compare(0, 10, "song-title") == 0) return musicTitle("a16_music-names-songs-enc", h.musicGenre);
  return std::string();
}

bool Sim::musicYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.musician) return false;
  hero_ = &h;
  life_ = &life;
  if (h.age < 16) return false;
  // The album pool is the one that needs a record; both pools run off one coin.
  const char* pool = musicRng_.chance(0.5) ? "a16_dumb-scenarios-album-enc"
                                           : "a16_dumb-scenarios-music-enc";
  if (!ensureTable(pool)) return false;
  if (!musicRng_.chance(0.4)) return false;
  const Table& t = tbl(pool);
  std::vector<size_t> rows;
  for (size_t r = 0; r < t.size(); ++r) {
    if (t.has("Hero Min Age") && h.age < int(t.num(r, "Hero Min Age"))) continue;
    rows.push_back(r);
  }
  if (rows.empty()) return false;
  return renderRow(t, rows[musicRng_.below(uint32_t(rows.size()))], life, out, forcedChoice);
}

void Sim::makeBusiness(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!ensureTable("a16_business-industries-enc")) return;
  const Table& t = tbl("a16_business-industries-enc");
  if (!t.size()) return;
  h.industry = trim(t.at(bizRng_.below(uint32_t(t.size())), "Industry"));
  if (h.industry.empty()) return;
  h.ownsBusiness = true;
}

// A rival's name, using the same mechanical Format/Prefix/Suffix substitution as
// companyName() but reading a16_business-name-structure-enc.
std::string Sim::businessName(const std::string& industry) {
  if (!ensureTable("a16_business-name-structure-enc")) return std::string();
  const Table& st = tbl("a16_business-name-structure-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < st.size(); ++r) {
    if (industry.empty() || lower(trim(st.at(r, "Industry"))) == lower(industry)) pool.push_back(r);
  }
  if (pool.empty()) {
    for (size_t r = 0; r < st.size(); ++r) pool.push_back(r);
  }
  if (pool.empty()) return std::string();
  const size_t row = pool[bizRng_.below(uint32_t(pool.size()))];
  const auto pickList = [this](const std::string& list) {
    std::vector<std::string> parts;
    splitOn(list, ',', parts);
    return parts.empty() ? std::string() : trim(parts[bizRng_.below(uint32_t(parts.size()))]);
  };
  const auto replaceAll = [](std::string s, const std::string& from, const std::string& to) {
    if (from.empty() || to.empty()) return s;
    for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
      s.replace(at, from.size(), to);
    return s;
  };
  std::string fmt = trim(st.at(row, "Predefined"));
  if (fmt.empty()) fmt = pickList(st.at(row, "Format"));
  if (fmt.empty()) return std::string();
  if (fmt.find("<color>") != std::string::npos && ensureTable("a16_company-name-color-enc")) {
    const Table& colors = tbl("a16_company-name-color-enc");
    if (colors.size())
      fmt = replaceAll(fmt, "<color>", trim(colors.at(bizRng_.below(uint32_t(colors.size())), "Color")));
  }
  if (fmt.find("<suffix>") != std::string::npos) fmt = replaceAll(fmt, "<suffix>", pickList(st.at(row, "Suffix")));
  if (fmt.find("<prefix>") != std::string::npos) fmt = replaceAll(fmt, "<prefix>", pickList(st.at(row, "Prefix")));
  if (fmt.find("<city>") != std::string::npos) fmt = replaceAll(fmt, "<city>", hero_ ? hero_->city : std::string());
  if (fmt.find("<random-first-name-possessive>") != std::string::npos)
    fmt = replaceAll(fmt, "<random-first-name-possessive>", drawFirstName(bizRng_.chance(0.5)) + "'s");
  if (fmt.find("<random-first-name>") != std::string::npos)
    fmt = replaceAll(fmt, "<random-first-name>", drawFirstName(bizRng_.chance(0.5)));
  if (fmt.find("<random-surname>") != std::string::npos) fmt = replaceAll(fmt, "<random-surname>", drawSurname());
  // Anything still unfilled (an unknown bracket) is dropped rather than printed.
  std::string clean;
  for (size_t i = 0; i < fmt.size(); ++i) {
    if (fmt[i] == '<') {
      const size_t close = fmt.find('>', i);
      if (close != std::string::npos) { i = close; continue; }
    }
    clean += fmt[i];
  }
  while (!clean.empty() && clean.find("  ") != std::string::npos) clean.replace(clean.find("  "), 2, " ");
  return trim(clean);
}

std::string Sim::businessToken(const std::string& token) {
  if (!hero_) return std::string();
  const Hero& h = *hero_;
  const std::string psuffix = "-possessive";
  const bool possessive =
      token.size() > psuffix.size() &&
      token.compare(token.size() - psuffix.size(), psuffix.size(), psuffix) == 0;
  const std::string key = possessive ? token.substr(0, token.size() - psuffix.size()) : token;
  if (key == "employee" || key == "competitor") {
    const std::string who = key == "competitor" ? businessName(std::string()) : std::string();
    const std::string name = who.empty()
                                 ? drawFirstName(bizRng_.chance(0.5)) + " " + drawSurname()
                                 : who;
    return possessive ? name + "'s" : name;
  }
  if (!h.ownsBusiness) return std::string();
  if (!ensureTable("a16_business-industries-enc")) return std::string();
  const Table& ind = tbl("a16_business-industries-enc");
  if (key == "facility-singular" || key == "facility-multiple") {
    const char* col = key == "facility-singular" ? "Facility Name" : "Facility Name Plural";
    for (size_t r = 0; r < ind.size(); ++r) {
      if (lower(trim(ind.at(r, "Industry"))) == lower(h.industry) && ind.has(col))
        return trim(ind.at(r, col));
    }
    return std::string();
  }
  if (key == "product") {
    if (!ensureTable("a16_business-products-enc")) return std::string();
    const Table& prod = tbl("a16_business-products-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < prod.size(); ++r) {
      if (lower(trim(prod.at(r, "Industry"))) == lower(h.industry)) pool.push_back(r);
    }
    if (pool.empty()) return std::string();
    const std::string list = prod.at(pool[bizRng_.below(uint32_t(pool.size()))], "Product");
    std::vector<std::string> parts;
    splitOn(list, ',', parts);
    if (parts.empty()) return std::string();
    return trim(parts[bizRng_.below(uint32_t(parts.size()))]);
  }
  return std::string();
}

bool Sim::businessYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.ownsBusiness) return false;
  hero_ = &h;
  life_ = &life;
  if (h.age < 18) return false;
  if (!ensureTable("a16_dumb-scenarios-business-enc")) return false;
  if (!bizRng_.chance(0.4)) return false;
  const Table& t = tbl("a16_dumb-scenarios-business-enc");
  const auto listHas = [](const std::string& list, const std::string& want) {
    if (trim(list).empty()) return false;
    std::vector<std::string> parts;
    splitOn(list, ',', parts);
    for (std::string& p : parts)
      if (lower(trim(p)) == want) return true;
    return false;
  };
  const std::string mine = lower(trim(h.industry));
  std::vector<size_t> pool;
  for (size_t r = 0; r < t.size(); ++r) {
    if (t.has("Hero Min Age") && h.age < int(t.num(r, "Hero Min Age"))) continue;
    if (t.has("Industry Whitelist") && !trim(t.at(r, "Industry Whitelist")).empty() &&
        !listHas(t.at(r, "Industry Whitelist"), mine)) continue;
    if (t.has("Industry Blacklist") && listHas(t.at(r, "Industry Blacklist"), mine)) continue;
    pool.push_back(r);
  }
  if (pool.empty()) return false;
  return renderRow(t, pool[bizRng_.below(uint32_t(pool.size()))], life, out, forcedChoice);
}

void Sim::makeAthlete(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!ensureTable("a16_sports-enc")) return;
  const Table& sp = tbl("a16_sports-enc");
  if (!sp.size()) return;
  h.sport = trim(sp.at(sportsRng_.below(uint32_t(sp.size())), "Sport"));
  if (h.sport.empty()) return;
  h.athlete = true;
  const std::string gender = h.female ? "female" : "male";
  const auto pickWhere = [this](const char* table, const char* sportCol, const std::string& want,
                                const char* nameCol) {
    ensureTable(table);
    const Table& t = tbl(table);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) {
      if (t.has(sportCol) && lower(trim(t.at(r, sportCol))) != want) continue;
      pool.push_back(r);
    }
    if (pool.empty()) {
      for (size_t r = 0; r < t.size(); ++r) pool.push_back(r);
    }
    if (pool.empty()) return std::string();
    return trim(t.at(pool[sportsRng_.below(uint32_t(pool.size()))], nameCol));
  };
  // Leagues are per sport and per gender; teams are per league index, so the
  // league name is stored on the hero and the team is drawn from the same sport.
  if (ensureTable("a16_sports-leagues-enc")) {
    const Table& lg = tbl("a16_sports-leagues-enc");
    std::vector<size_t> pool;
    for (size_t r = 0; r < lg.size(); ++r) {
      if (lower(trim(lg.at(r, "Sport"))) != h.sport) continue;
      if (lg.has("Gender") && lower(trim(lg.at(r, "Gender"))) != gender) continue;
      pool.push_back(r);
    }
    if (pool.empty()) {
      for (size_t r = 0; r < lg.size(); ++r) pool.push_back(r);
    }
    if (!pool.empty()) h.league = trim(lg.at(pool[sportsRng_.below(uint32_t(pool.size()))], "Name"));
  }
  h.team = pickWhere("a16_sports-teams-enc", "Sport", h.sport, "Name");
  h.position = pickWhere("a16_sports-positions-enc", "Sport", h.sport, "Position");
  h.employed = true;
  if (!h.position.empty()) h.job = h.position;
}

// The sports pool's own words. '<game>' is the row's 'Game Bracket' for the
// hero's sport ('pre-<game>' reads 'pre-game'), '<field>' its 'Field Bracket',
// '<stadium>' its 'Stadium Bracket', and '<ball>' / '<referee>' the same way.
std::string Sim::sportsToken(const std::string& token) {
  if (!hero_) return std::string();
  const Hero& h = *hero_;
  if (!h.athlete && token != "coach" && token != "teammate") return std::string();
  if (token == "sport") return h.sport;
  if (token == "league") return h.league;
  if (token == "team") {
    if (h.team.empty()) return std::string();
    return resolve(h.team);  // team names carry <a/b/c> alternates
  }
  if (token == "coach" || token == "teammate") {
    return drawFirstName(sportsRng_.chance(0.5)) + " " + drawSurname();
  }
  if (!ensureTable("a16_sports-enc")) return std::string();
  const Table& sp = tbl("a16_sports-enc");
  const char* col = nullptr;
  if (token == "game") col = "Game Bracket";
  else if (token == "field") col = "Field Bracket";
  else if (token == "stadium") col = "Stadium Bracket";
  else if (token == "ball") col = "Ball Bracket";
  else if (token == "referee") col = "Referee Bracket";
  if (!col) return std::string();
  for (size_t r = 0; r < sp.size(); ++r) {
    if (lower(trim(sp.at(r, "Sport"))) == h.sport && sp.has(col)) return trim(sp.at(r, col));
  }
  return std::string();
}

bool Sim::sportsYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.athlete) return false;
  hero_ = &h;
  life_ = &life;
  if (h.age < 18) return false;
  if (!ensureTable("a16_dumb-scenarios-sports-enc")) return false;
  if (!sportsRng_.chance(0.45)) return false;
  const Table& t = tbl("a16_dumb-scenarios-sports-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < t.size(); ++r) {
    if (!t.has("Hero Min Age") || h.age >= int(t.num(r, "Hero Min Age"))) pool.push_back(r);
  }
  if (pool.empty()) return false;
  return renderRow(t, pool[sportsRng_.below(uint32_t(pool.size()))], life, out, forcedChoice);
}

void Sim::makeMilitary(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  if (!ensureTable("a16_military-ranks-enc")) return;
  const Table& t = tbl("a16_military-ranks-enc");
  static const char* kBranches[] = {"Air Force", "Army", "Coast Guard", "Marines", "Navy"};
  h.militaryBranch = kBranches[militaryRng_.below(5)];
  h.militaryOfficer = militaryRng_.chance(0.25);
  const std::string status = h.militaryOfficer ? "Officer" : "Enlisted";
  size_t row = size_t(-1);
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Status")) != status) continue;
    if (row == size_t(-1) || t.num(r, "Minimum Military Years") < t.num(row, "Minimum Military Years")) row = r;
  }
  if (row == size_t(-1)) return;
  // The switch means "this life serves"; the enlistment itself happens at 18,
  // so nothing here sets the job yet (REPORT.en.md 50).
  h.militaryPending = true;
  h.militaryYears = 0;
  h.militaryRank = trim(t.at(row, h.militaryBranch + " Longhand"));
  h.militaryPay = trim(t.at(row, "Salary Average"));
}

std::string Sim::warToken(const std::string& token) {
  if (!hero_) return std::string();
  const Hero& h = *hero_;
  if (token == "war-age") return std::to_string(h.age);
  if (token == "war-city1") {
    for (const Country& c : countries_) {
      if (c.name == h.country && !c.cities.empty())
        return c.cities[militaryRng_.below(uint32_t(c.cities.size()))];
    }
    return h.city;
  }
  // <war-country1..3> are three different countries, none of them the hero's.
  // note: compare(pos, len, literal) needs len == literal length; 12 vs an
  // 11-character literal is never equal, so the prefix test is a plain search.
  if (token.rfind("war-country", 0) == 0) {
    while (warCountries_.size() < 3 && !countries_.empty()) {
      const std::string c = countries_[militaryRng_.below(uint32_t(countries_.size()))].name;
      if (c == h.country) continue;
      bool dup = false;
      for (const std::string& w : warCountries_) if (w == c) dup = true;
      if (dup) {
        if (warCountries_.size() >= countries_.size()) break;
        continue;
      }
      warCountries_.push_back(c);
    }
    const char* suffix = token.c_str() + 11;  // '', '1', '2', '3', '-leader', '-possessive'
    int slot = 1;
    if (suffix[0] >= '1' && suffix[0] <= '3') slot = suffix[0] - '0';
    if (size_t(slot) > warCountries_.size()) return std::string();
    const std::string base = warCountries_[size_t(slot) - 1];
    const std::string rest = suffix[0] >= '1' && suffix[0] <= '3' ? std::string(suffix + 1) : std::string(suffix);
    if (rest == "-possessive") return base + "'s";
    if (rest == "-leader") return drawFirstName(militaryRng_.chance(0.5)) + " " + drawSurname();
    return base;
  }
  return std::string();
}

bool Sim::militaryYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.military && !h.militaryPending) return false;
  hero_ = &h;
  life_ = &life;
  if (!ensureTable("a16_military-ranks-enc")) return false;
  // Enlistment: literals #24115 'I enlisted in the {0} with a salary of {1}.'
  // and #24507 'I joined the {0} as an officer with a salary of {1}.'
  if (!h.military) {
    if (h.age < 18) return false;
    h.military = true;
    h.militaryPending = false;
    h.militaryYears = 0;
    h.job = h.militaryBranch;
    h.employed = true;
    out.narrative = std::string(h.militaryOfficer
                                    ? "I joined the " + h.militaryBranch + " as an officer"
                                    : "I enlisted in the " + h.militaryBranch) +
                    " with a salary of " + h.militaryPay + ".";
    return true;
  }
  const Table& t = tbl("a16_military-ranks-enc");
  ++h.militaryYears;

  // Promotion: the highest rank row of the hero's status whose Minimum Military
  // Years the service has reached.
  const std::string status = h.militaryOfficer ? "Officer" : "Enlisted";
  size_t best = size_t(-1);
  for (size_t r = 0; r < t.size(); ++r) {
    if (trim(t.at(r, "Status")) != status) continue;
    if (t.num(r, "Minimum Military Years") > double(h.militaryYears)) continue;
    if (best == size_t(-1) || t.num(r, "Minimum Military Years") > t.num(best, "Minimum Military Years")) best = r;
  }
  bool promoted = false;
  if (best != size_t(-1)) {
    const std::string rank = trim(t.at(best, h.militaryBranch + " Longhand"));
    if (!rank.empty() && rank != h.militaryRank) {
      h.militaryRank = rank;
      h.militaryPay = trim(t.at(best, "Salary Average"));
      promoted = true;
    }
  }

  // shortcut: twenty years of service is the retirement rule; the game's own
  // discharge/retirement condition was not read. The port then leaves the
  // service and the war pool stops applying.
  if (h.militaryYears >= 20) {
    const std::string line = "I retired from the " + h.militaryBranch + ".";
    h.military = false;
    h.employed = false;
    out.narrative = line;
    out.choice.clear();
    return true;
  }

  if (promoted) {
    out.narrative = "I have been promoted to " + h.militaryRank + " with a salary of " + h.militaryPay + ".";
    return true;
  }

  // Otherwise a war row now and then: the stage follows the years of service.
  if (!ensureTable("a16_war-scenarios-enc")) return false;
  if (!militaryRng_.chance(0.35)) return false;
  const Table& w = tbl("a16_war-scenarios-enc");
  const int stage = 1 + std::min(3, h.militaryYears / 5);
  std::vector<size_t> pool;
  for (size_t r = 0; r < w.size(); ++r) {
    if (int(w.num(r, "Stage")) == stage) pool.push_back(r);
  }
  if (pool.empty()) return false;
  const size_t row = pool[militaryRng_.below(uint32_t(pool.size()))];
  warCountries_.clear();
  const bool ok = renderRow(w, row, life, out, forcedChoice);
  warCountries_.clear();
  return ok;
}

void Sim::makeRoyal(Life& life) {
  Hero& h = life.hero;
  hero_ = &h;  // resolve() reads the live hero for '<random-surname> House'
  life_ = &life;
  if (!ensureTable("a16_country-royal-systems-enc") || !ensureTable("a16_royal-house-names-enc")) return;
  const Table& rs = tbl("a16_country-royal-systems-enc");
  size_t row = size_t(-1);
  for (size_t r = 0; r < rs.size(); ++r) {
    if (lower(trim(rs.at(r, "Country"))) == lower(trim(h.country))) { row = r; break; }
  }
  // Countries without a royal system stay commoners rather than getting an
  // invented seat (17 of them have one: Belgium, Denmark, Japan, ...).
  if (row == size_t(-1)) return;
  h.royal = true;
  h.royalRank = 1;
  h.royalSeat = trim(rs.at(row, h.female ? "Rank 1 Female" : "Rank 1 Male"));
  const std::string code = lower(trim(rs.at(row, "Country Code")));
  const Table& hn = tbl("a16_royal-house-names-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < hn.size(); ++r) {
    const std::string codes = lower(hn.at(r, "Country Code"));
    if (codes != "all" && (code.empty() || codes.find(code) == std::string::npos)) continue;
    pool.push_back(r);
  }
  const size_t house = weightedPickOf(hn, "Commonness Score", pool, rng_);
  if (house != size_t(-1)) h.royalHouse = resolve(hn.at(house, "Name"));
}

std::string Sim::royalToken(const std::string& token) {
  if (hero_ == nullptr || !hero_->royal) return std::string();
  if (token == "circumstance-royal") return royalCircumstance();
  if (!ensureTable("a16_country-royal-systems-enc")) return std::string();
  const Table& rs = tbl("a16_country-royal-systems-enc");
  size_t row = size_t(-1);
  for (size_t r = 0; r < rs.size(); ++r) {
    if (lower(trim(rs.at(r, "Country"))) == lower(trim(hero_->country))) { row = r; break; }
  }
  if (row == size_t(-1)) return std::string();
  const bool female = hero_->female;
  std::string seat;
  if (token == "highest-royal-name") {
    seat = trim(rs.at(row, female ? "Rank 1 Female" : "Rank 1 Male"));
  } else {
    // '<other-royal-name>' = any seat below the top one; every country fills
    // ranks 2..7 from Prince/Duke/Marquess/Count down to Baron.
    for (int rank = 2; rank <= 7 && seat.empty(); ++rank) {
      const std::string col = "Rank " + std::to_string(rank) + (female ? " Female" : " Male");
      if (rs.has(col)) seat = trim(rs.at(row, col));
    }
  }
  if (seat.empty()) return std::string();
  return seat + " " + drawFirstName(female);
}

std::string Sim::royalCircumstance() {
  // shortcut: the royal circumstance pool is built in code
  // (DataHub.GetRandomCircumstanceRoyalWithCastleName @0x73f2ce4050 takes the
  // castle name, so every phrase in it names the seat) and was not sampled, so
  // the port composes one from the country's own Seats column instead of
  // leaving a bare '<circumstance-royal>' in the card (report 47).
  if (hero_ == nullptr) return std::string();
  if (!ensureTable("a16_country-royal-systems-enc")) return std::string();
  const Table& rs = tbl("a16_country-royal-systems-enc");
  size_t row = size_t(-1);
  for (size_t r = 0; r < rs.size(); ++r) {
    if (lower(trim(rs.at(r, "Country"))) == lower(trim(hero_->country))) { row = r; break; }
  }
  if (row == size_t(-1)) return std::string();
  std::vector<std::string> seats;
  splitOn(rs.at(row, "Seats"), ',', seats);
  std::vector<std::string> keep;
  for (const std::string& s : seats) if (!trim(s).empty()) keep.push_back(trim(s));
  if (keep.empty()) return std::string();
  static const char* const kOpeners[] = {"While leaving ", "On your way out of ", "While walking the grounds of "};
  const std::string place = keep[rng_.below(uint32_t(keep.size()))];
  return std::string(kOpeners[rng_.below(3)]) + place;
}

bool Sim::checkDeath(const Life& life, std::string& type, std::string& sentence) {
  const Hero& h = life.hero;
  hero_ = &h;
  life_ = &life;
  // Port-side mortality rule (the real game spreads this over many systems, not
  // one table): a small annual risk that rises steeply after 55.
  const double annual = h.age < 55 ? 0.001 : 0.004 * double(h.age - 54);
  if (!rng_.chance(annual)) return false;

  const Table& t = tbl("a16_death-types-enc");
  std::vector<size_t> pool;
  for (size_t r = 0; r < t.size(); ++r) {
    if (h.age >= int(t.num(r, "Min Age"))) pool.push_back(r);
  }
  // Weighted pick inside the eligible death causes.
  double total = 0.0;
  for (size_t r : pool) total += t.num(r, "Commonness Score");
  if (pool.empty() || total <= 0.0) return false;
  double roll = rng_.unit() * total;
  size_t row = pool.back();
  for (size_t r : pool) {
    roll -= t.num(r, "Commonness Score");
    if (roll <= 0.0) { row = r; break; }
  }
  type = t.at(row, "Type");
  sentence = resolve(t.at(row, "Sentence"));
  return true;
}

// ------------------------------------------------------------------ lovers
// REPORT.en.md 57. The lover family is a pool per stage of a relationship, and
// every row gates on the score, the partner's gender or the partner's age.
std::string Sim::relationshipToken(const std::string& key) {
  if (hero_ == nullptr) return std::string();
  std::string base = key;
  const std::string suffix = "-possessive";
  bool possessive = false;
  if (base.size() > suffix.size() && base.compare(base.size() - suffix.size(), suffix.size(), suffix) == 0) {
    possessive = true;
    base.resize(base.size() - suffix.size());
  }
  const Hero& h = *hero_;
  auto shape = [&](const std::string& name) { return possessive ? name + "'s" : name; };
  // A pool can name a partner the hero has not met yet (the fight outcome rows
  // do), and a leaked token is worse than a stranger, so draw one.
  if (base == "lover") {
    if (h.loverName.empty()) return shape(drawFirstName(!h.female));
    return shape(h.loverName);
  }
  if (base == "ex" || base == "ex-lover")
                             return h.exName.empty() ? std::string() : shape(h.exName);
  if (base == "gender")      return h.loverFemale ? "woman" : "man";
  if (base == "house") {
    // shortcut: the port keeps no property list yet, so '<house>' is one house
    // type out of a16_house-types-enc rather than the hero's own home.
    if (!ensureTable("a16_house-types-enc")) return std::string();
    const Table& t = tbl("a16_house-types-enc");
    if (t.size() == 0) return std::string();
    return trim(t.at(relRng_.below(uint32_t(t.size())), "Type"));
  }
  // The two health nouns the counseling and marriage rows address. The port
  // models neither, so they draw a real disease name and one of the game's own
  // addiction words (a16_dumb-scenarios-dealer-enc writes '<addiction>' beside
  // alcohol and drugs).
  if (base == "disease") {
    if (!ensureTable("a16_disease-types-enc")) return std::string();
    const Table& t = tbl("a16_disease-types-enc");
    if (t.size() == 0) return std::string();
    return trim(t.at(relRng_.below(uint32_t(t.size())), t.has("Disease") ? "Disease" : "Type"));
  }
  if (base == "addiction") {
    static const char* kAddictions[] = {"alcohol", "drugs", "gambling", "sex"};
    return kAddictions[relRng_.below(4)];
  }
  if (base == "name") {
    // '<name>' is the given name of the stranger a row introduces ('a teacher
    // named <name>'). One per card, so a row that names the person twice keeps
    // the same person.
    if (relNameCache_.empty()) relNameCache_ = drawFirstName(relRng_.chance(0.5));
    return relNameCache_;
  }
  if (base == "friend-lover") {
    // shortcut: no friend list, so the friend who becomes a lover is a fresh
    // name rather than an existing friend.
    const std::string name = drawFirstName(h.loverFemale);
    return name.empty() ? std::string() : shape(name);
  }
  return std::string();
}

bool Sim::relEligible(const Table& t, size_t row, const Life& life) const {
  const Hero& h = life.hero;
  auto genderOk = [&](const char* col, bool subjectFemale) {
    if (!t.has(col)) return true;
    const std::string v = lower(trim(t.at(row, col)));
    if (v.empty() || v == "both" || v == "all") return true;
    if (v == "male") return !subjectFemale;
    if (v == "female") return subjectFemale;
    return true;
  };
  if (!genderOk("Hero Gender", h.female)) return false;
  if (!genderOk("Lover Gender", h.loverFemale)) return false;
  if (!genderOk("Ex Gender", h.exFemale)) return false;
  if (!genderOk("Gender", h.loverFemale)) return false;         // the partner column
  if (!genderOk("Partner Gender", h.loverFemale)) return false;
  if (!genderOk("Friend Gender", !h.loverFemale)) return false;
  if (!genderOk("Classmate Gender", !h.loverFemale)) return false;
  if (!genderOk("Sender Gender", !h.female)) return false;
  auto ageOk = [&](const char* minCol, const char* maxCol, int age) {
    if (t.has(minCol)) {
      const double lo = t.num(row, minCol);
      if (lo > 0 && age < int(lo)) return false;
    }
    if (t.has(maxCol)) {
      const double hi = t.num(row, maxCol);
      if (hi > 0 && age > int(hi)) return false;
    }
    return true;
  };
  if (!ageOk("Hero Min Age", "Hero Max Age", h.age)) return false;
  if (!ageOk("Min Age", "Max Age", h.age)) return false;
  if (!ageOk("Lover Min Age", "Lover Max Age", h.loverAge)) return false;
  if (!ageOk("Friend Min Age", "Friend Max Age", h.loverAge)) return false;
  auto rangeOk = [&](const char* minCol, const char* maxCol, int value) {
    if (t.has(minCol)) {
      const double lo = t.num(row, minCol);
      if (lo > 0 && value < int(lo)) return false;
    }
    if (t.has(maxCol)) {
      const double hi = t.num(row, maxCol);
      if (hi > 0 && value > int(hi)) return false;
    }
    return true;
  };
  if (!rangeOk("Min Relationship", "Max Relationship", h.relationship)) return false;
  if (!rangeOk("Min Craziness", "Max Craziness", h.loverCraziness)) return false;
  if (t.has("Hero Min Smarts") && h.smarts < int(t.num(row, "Hero Min Smarts"))) return false;
  if (t.has("Hero Max Smarts")) {
    const double hi = t.num(row, "Hero Max Smarts");
    if (hi > 0 && h.smarts > int(hi)) return false;
  }
  if (t.has("Hero Min Looks") && h.looks < int(t.num(row, "Hero Min Looks"))) return false;
  if (t.has("Hero Max Looks")) {
    const double hi = t.num(row, "Hero Max Looks");
    if (hi > 0 && h.looks > int(hi)) return false;
  }
  if (t.has("Min Smarts") && h.smarts < int(t.num(row, "Min Smarts"))) return false;
  if (t.has("Max Smarts")) {
    const double hi = t.num(row, "Max Smarts");
    if (hi > 0 && h.smarts > int(hi)) return false;
  }
  if (t.has("Max Looks")) {
    const double hi = t.num(row, "Max Looks");
    if (hi > 0 && h.looks > int(hi)) return false;
  }
  const bool married = h.loverStatus == "married";
  if (t.has("Married") && t.flag(row, "Married") != married) return false;
  if (t.has("Hero Virgin") && t.flag(row, "Hero Virgin") != h.virgin) return false;
  if (t.has("Has Job") && t.flag(row, "Has Job") && !h.employed) return false;
  // 'Straight Only' marks the rows that only a hero in an opposite-sex pairing
  // takes; the flag is the game's own column, not an age or state proxy.
  if (t.has("Straight Only") && t.flag(row, "Straight Only") && h.loverFemale == h.female) return false;
  // shortcut: Clean, the pregnancy columns, Multiple Children and the vampire
  // and prison flags are not read here; the port has no pregnancy, child-count
  // or prison-relationship model, and every row would otherwise be excluded.
  return true;
}

void Sim::makeRelationships(Life& life) { life.hero.relationshipOn = true; }

bool Sim::relationshipYear(Life& life, AgeUp& out, int forcedChoice) {
  hero_ = &life.hero;
  life_ = &life;
  Hero& h = life.hero;
  if (!h.relationshipOn || h.age < 15) return false;
  if (h.loverStatus.empty() && !h.loverName.empty()) h.loverStatus = "dating";
  // The score and the partner's craziness move every year; the pools gate on
  // them, so they have to be settled before a row is picked.
  h.relationship += int(relRng_.below(9)) - 4;
  if (h.relationship < 0) h.relationship = 0;
  if (h.relationship > 100) h.relationship = 100;
  if (h.loverCraziness == 0) h.loverCraziness = int(relRng_.below(101));

  // One row per year: the first pool that fires wins, in the game's own order
  // (ask out -> hook up -> make love -> suspicion -> cheating -> nags ->
  // marriage -> anniversary -> counseling -> breakup, then the ex pools).
  auto fire = [&](const char* table, double chance) -> bool {
    if (!ensureTable(table)) return false;
    if (!relRng_.chance(chance)) return false;
    const Table& t = tbl(table);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r) if (relEligible(t, r, life)) pool.push_back(r);
    const size_t row = pickFrom(pool, relRng_);
    if (row == size_t(-1)) return false;
    // The row writes about the partner, so the pronoun brackets follow the ex
    // when the relationship is over and the current partner otherwise.
    pronounOtherSet_ = true;
    pronounOtherFemale_ = h.loverName.empty() ? h.exFemale : h.loverFemale;
    const bool ok = renderRow(t, row, life, out, forcedChoice);
    pronounOtherSet_ = false;
    return ok;
  };
  auto narrate = [&](const std::string& text, const char* table) {
    out = AgeUp();
    out.table = table;
    out.row_id = "0";
    out.narrative = text;
    return true;
  };

  if (h.loverName.empty()) {
    // (1) Single: the year may bring a partner, or a rejection.
    if (relRng_.chance(0.35)) {
      if (fire("a16_date-rejections-enc", 0.45)) return true;
      h.loverFemale = !h.female;
      h.loverAge = h.age + int(relRng_.below(5)) - 2;
      h.loverName = drawFirstName(h.loverFemale);
      h.loverStatus = "dating";
      h.relationship = 60;
      h.loverCraziness = int(relRng_.below(101));
      // shortcut: the game announces the pairing from the Love menu's own
      // code-built string, which is not in the tables, so the port writes it.
      return narrate("I am now dating " + h.loverName + ".", "relationship");
    }
    // (2) The ex pools, once there is an ex to remember.
    if (h.exName.empty()) return false;
    if (fire("a16_ex-lover-booty-call-scenarios-enc", 0.10)) return true;
    if (fire("a16_ex-lover-rekindle-scenarios-enc", 0.10)) {
      h.loverName = h.exName;
      h.loverFemale = h.exFemale;
      h.loverStatus = "dating";
      h.relationship = 45;
      return true;
    }
    if (fire("a16_ex-lover-stalking-scenarios-enc", 0.08)) return true;
    if (fire("a16_ex-lover-hate-scenarios-enc", 0.08)) return true;
    return false;
  }

  // (3) A couple's year.
  auto bump = [&](int delta) {
    h.relationship += delta;
    if (h.relationship < 0) h.relationship = 0;
    if (h.relationship > 100) h.relationship = 100;
  };
  // Each row moves the score the way the game's own relationship meter does:
  // betrayal and suspicion erode it, affection restores it, and a score that
  // falls under the breakup gate is what ends the relationship.
  if (!h.loverStatus.empty() && fire("a16_hookup-scenarios-enc", 0.06)) { bump(4); return true; }
  if (fire("a16_lover-cheat-scenarios-enc", 0.08)) { bump(-14); return true; }
  if (fire("a16_lover-suspicious-scenarios-enc", 0.08)) { bump(-12); return true; }
  if (fire("a16_make-love-rejections-enc", 0.10)) { bump(-6); return true; }
  if (fire("a16_lover-make-love-nags-enc", 0.07)) { bump(-4); return true; }
  if (fire("a16_parent-lover-disapprovals-enc", 0.06)) { bump(-3); return true; }
  if (!h.orphan && h.age <= 25 && fire("a16_school-classmate-flirt-scenarios-enc", 0.05)) return true;
  if (h.employed && fire("a16_coworker-hookup-scenarios-enc", 0.05)) return true;
  if (h.loverStatus == "dating" && h.relationship >= 55 && fire("a16_lover-marriage-nags-enc", 0.10)) {
    h.loverStatus = "married";
    h.relationship = 75;
    return true;
  }
  if (h.loverStatus == "married" && fire("a16_anniversary-date-enc", 0.12)) { bump(8); return true; }
  if (h.relationship < 35 && fire("a16_relationship-counseling-scenarios-enc", 0.10)) { bump(12); return true; }
  if (h.relationship < 30 && fire("a16_breakup-scenarios-enc", 0.30)) {
    h.exName = h.loverName;
    h.exFemale = h.loverFemale;
    ++h.exes;
    h.loverName.clear();
    h.loverStatus.clear();
    h.relationship = 0;
    return true;
  }
  return false;
}

}  // namespace bl
