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
      vampireRng_(seed ^ 0x27d4eb2f165667c5ULL) {}

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
      "a16_vacation-destinations-enc",               // Destination,Context,Country,...
      "a16_cruises-enc",                             // Type,Continent,Cost Score,Appeal Score
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
                                  "neighbour", "neighbor", "stranger", "child", "bully"};
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

static std::string articleFor(const std::string& word) {
  const bool vowel = !word.empty() && std::string("aeiouAEIOU").find(word[0]) != std::string::npos;
  return vowel ? "an " : "a ";
}

std::string Sim::circumstancePublic() {
  const size_t n = sizeof(kCircumstancePublicScenes) / sizeof(*kCircumstancePublicScenes);
  const size_t slot = rng_.below(uint32_t(n + 2));  // two of the 43 slots draw a musician
  if (slot < n) return kCircumstancePublicScenes[slot];
  const std::string who = referenceToken("musician");
  if (who.empty()) return kCircumstancePublicScenes[rng_.below(uint32_t(n))];
  return std::string(slot == n ? "While at " : "On your way to ") + articleFor(who) + who + " concert";
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
    {"Himself/Herself", "Himself", "Herself"},
  };
  for (const auto& p : kPairs) {
    if (token == p.token) {
      out += (hero_ && hero_->female) ? p.female : p.male;
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
    if (genderToken(token, out)) {
      // handled: a pronoun bracket keyed on the hero's gender
    } else if (token.find('/') != std::string::npos) {
      // "<I got a flat tire/I ran out of gas/my battery died>" -> pick one, and
      // cache the draw for "<rb>" / "<rb1>" / "<rb2>" (report 33).
      std::vector<std::string> options;
      splitOn(token, '/', options);
      std::string drawn;
      for (const std::string& o : options) if (o.empty()) drawn = "<>";  // "</b>": not an alternative list
      if (drawn == "<>") out += text.substr(i, close - i + 1);
      else {
        std::vector<size_t> usable;
        for (size_t k = 0; k < options.size(); ++k)
          if (!hasUnresolvedToken(options[k])) usable.push_back(k);
        if (usable.empty()) for (size_t k = 0; k < options.size(); ++k) usable.push_back(k);
        drawn = options[usable[rng_.below(uint32_t(usable.size()))]];
        // An alternative can hold tokens of its own, so the drawn text goes back
        // through the resolver before it is cached for '<rb>' (report 33) and
        // appended.
        const std::string picked = resolveInto(drawn, memo);
        if (rndCache_.size() < 8) rndCache_.push_back(picked);
        out += picked;
      }
    } else if (token.size() >= 2 && token[0] == 'r' && token[1] == 'b' &&
               token.find_first_not_of("0123456789", 2) == std::string::npos) {
      // "<rb>" / "<rb1>" = the first alternative drawn for this scenario, "<rb2>"
      // the second, up to "<rb4>". Out of range means the game never drew that
      // many, so the token stays verbatim.
      const int n = token.size() == 2 ? 1 : std::atoi(token.substr(2).c_str());
      if (n >= 1 && size_t(n) <= rndCache_.size()) out += rndCache_[size_t(n) - 1];
      else out += text.substr(i, close - i + 1);
    } else if (token == "br") {
      out += '\n';
    } else if (token.compare(0, 5, "pick:") == 0) {
      // "<pick:Option 1|Option 2|Option 3>" -> one alternative.
      std::vector<std::string> options;
      splitOn(token.substr(5), '|', options);
      out += resolveInto(options[rng_.below(uint32_t(options.size()))], memo);
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
    } else if (token == "vampire" || token == "vampire-possessive" ||
               token == "familiar" || token == "familiar-possessive") {
      const std::string value = vampireToken(token);
      if (value.empty()) out += text.substr(i, close - i + 1);  // unresolved: verbatim
      else out += value;
    } else if (token == "highest-royal-name" || token == "other-royal-name" ||
               token == "circumstance-royal") {
      const std::string value = royalToken(token);
      if (value.empty()) out += text.substr(i, close - i + 1);  // unresolved: verbatim
      else out += value;
    } else if (token == "circumstance-public") {
      out += circumstancePublic();
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
  rndCache_.clear();  // one scenario per year: the '<rbN>' cache starts empty
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
  rndCache_.clear();  // the body is parsed first, its draws feed the log line
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
  if (!t.has("Title") && !t.has("Body")) {
    // Narrative-only tables keep the card's wording in "Scenario" and, in the
    // menu families (a16_stepparent-cruelty-scenarios-enc, a16_pet-discipline-enc),
    // the first-person sentence the game writes into the LifeLog in "Log Version"
    // ("Your stepfather pranked you..." vs "My stepfather pranked me..."). The log
    // is what this port records, so the log column wins; a16_bully-scenarios-enc
    // and a16_first-kiss-reactions-enc spell it "Log".
    static const char* kNarrativeCols[] = {"Log Version", "Log", "Narrative", "Scenario", "String"};
    for (const char* c : kNarrativeCols) {
      if (t.has(c)) {
        out.narrative = resolve(t.at(row, c));
        return true;
      }
    }
    return false;
  }

  out.title = resolve(scriptedFamilyTokens(t.at(row, "Title")));
  out.body = resolve(scriptedFamilyTokens(t.at(row, "Body")));
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
  h.vampire = true;
  // shortcut: the familiar is a character the player meets in the vampire
  // screens, and there is no familiar-name table, so one name is drawn from the
  // same person pool the rest of the port draws people from.
  h.familiar = drawFirstName(vampireRng_.chance(0.5));
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

std::string Sim::vampireToken(const std::string& token) {
  if (hero_ == nullptr || !hero_->vampire) return std::string();
  const std::string suffix = "-possessive";
  const bool possessive =
      token.size() > suffix.size() && token.compare(token.size() - suffix.size(), suffix.size(), suffix) == 0;
  const bool familiar = token.compare(0, 8, "familiar") == 0;
  const std::string name = familiar ? hero_->familiar : hero_->vampireName;
  if (name.empty()) return std::string();
  return possessive ? name + "'s" : name;
}

bool Sim::vampireYear(Life& life, AgeUp& out, int forcedChoice) {
  Hero& h = life.hero;
  if (!h.vampire) return false;
  if (!ensureTable("a16_vampire-dumb-scenario-enc")) return false;
  // One vampire NPC for the whole year, so the card and its log line agree on
  // the name '<vampire>' renders.
  h.vampireName = vampireNpcName();
  const Table& t = tbl("a16_vampire-dumb-scenario-enc");
  // scenarioEligible covers the table's own age / gender / lover gates; its
  // Notoriety, Essence and Hero Has Familiar gates belong to the vampire
  // attribute system the port does not model, so they are ignored here
  // (shortcut, REPORT.en.md 48).
  std::vector<size_t> rows = dumbSetRows("a16_vampire-dumb-scenario-enc", life);
  const size_t r = pickFrom(rows, rng_);
  if (r == size_t(-1)) return false;
  return renderRow(t, r, life, out, forcedChoice);
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

}  // namespace bl
