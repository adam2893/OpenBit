// src/fam/spy.cpp -- moved out of src/sim.cpp (REPORT.en.md 131).
//
// Cut verbatim by tools/split_family.py; the only edits are the includes.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace bl {

bool Sim::spyYear(Life& life, AgeUp& out, int forcedChoice) {
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

  // The secret-agent colour tables (REPORT.en.md 311). resolve() draws its
  // '<a/b>' alternatives, names and pool tokens from the main rng_, so the two
  // streams trade places for each colour: every new draw lands on spyRng_ and
  // the main stream is left exactly where it was. Spy tokens (which draw from
  // spyRng_ by name) are swapped in before the call for that reason.
  const auto colourResolve = [this](const std::string& raw, bool female) {
    const bool savedSet = pronounOtherSet_, savedFemale = pronounOtherFemale_;
    pronounOtherSet_ = true;  // the parser's gender: target or client, never the hero
    pronounOtherFemale_ = female;
    std::swap(rng_, spyRng_);
    const std::string s = resolve(raw);
    std::swap(rng_, spyRng_);
    pronounOtherSet_ = savedSet;
    pronounOtherFemale_ = savedFemale;
    return s;
  };
  // Native getters weight by Commonness Score copies and draw Next(0, count).
  const auto colourPick = [this](const char* name, auto keep) {
    if (!ensureTable(name)) return size_t(-1);
    const Table& t = tbl(name);
    std::vector<size_t> pool;
    for (size_t r = 0; r < t.size(); ++r)
      if (keep(t, r)) pool.push_back(r);
    return weightedPickOf(t, "Commonness Score", pool, spyRng_);
  };
  // SimSecretAgentMission.Client {Type, PersonName, PersonGender}: drawn once per
  // card, only when a colour names the client.
  // shortcut: the port has no client record; name and gender are drawn here.
  std::string clientName;
  bool clientFemale = false;
  const auto client = [&]() -> const std::string& {
    if (clientName.empty()) {
      clientFemale = spyRng_.chance(0.5);
      std::swap(rng_, spyRng_);
      clientName = drawFirstName(clientFemale) + " " + drawSurname();
      std::swap(rng_, spyRng_);
    }
    return clientName;
  };
  std::string honeypotWhere;  // mission.Location, keys the meeting-scenario colour
  std::string wireLine;       // the stealth alarm's 'Correct' report
  std::vector<std::string> wireOptions;
  std::string wireChoice;

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
      // The mark's own gender drives the honeypot pronouns; the table knows it
      // ('mistress' female, 'a male stripper' male). Rows without a gender
      // keep the opposite-of-hero fallback used below.
      spyTargetFemale_ = lower(trim(people.at(row, "Target Gender"))) == "female";
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
    honeypotWhere = where;
    if (!preference.empty()) {
      briefing = "Meet " + (spyTarget_.empty() ? std::string("the mark") : spyTarget_) + " at the " +
                 (where.empty() ? std::string("usual place") : where) + " and work the fact that " +
                 (spyTargetFemale_ ? "she" : "he") + " cannot resist " + preference + ".";
    } else {
      briefing = "Meet " + (spyTarget_.empty() ? std::string("the mark") : spyTarget_) + " at the " +
                 (where.empty() ? std::string("usual place") : where) + ".";
    }
    const std::string approach = spySnippet("a16_secret-agent-honeypot-agent-approach-style-enc",
                                            "Approach", "Commonness Score", "", "", "");
    if (!approach.empty()) briefing += " Approach: " + spySentence(approach);
    // honeypot-misssion-briefing colour: ScenarioSecretAgentHoneypotMissionBriefing::
    // RefreshScenario @0x73f2a9ba48, GetRandomSecretAgentHoneypotMissionBriefingColor
    // 0x73f2c5dc2c (unfiltered, weighted), parsed with the target's gender.
    const size_t hb = colourPick("a16_secret-agent-honeypot-misssion-briefing-color-enc",
                                 [](const Table&, size_t) { return true; });
    if (hb != size_t(-1)) {
      const std::string want = colourResolve(
          trim(tbl("a16_secret-agent-honeypot-misssion-briefing-color-enc").at(hb, "Snippet")), spyTargetFemale_);
      // shortcut: Client.BriefingDescriptor() is the client's name and
      // mission.TargetDescription() the port's own target text; joined by ' '
      // rather than the native '\n\n'. Pronoun types 5/0 read as He/he [I].
      briefing += " Your client, " + client() + ", wants you to seduce " +
                  (spyTarget_.empty() ? std::string("the target") : spyTarget_) + ".";
      briefing += std::string(" ") + (clientFemale ? "She" : "He") + " says " +
                  (clientFemale ? "she" : "he") + " wants " + want + ".";
    }
    // honeypot-approach colour: ScenarioSecretAgentHoneypotMissionMeetDecisions::
    // RefreshScenario @0x73f2a9f3b8, GetRandomSecretAgentHoneypotApproachColor
    // 0x73f2c617cc (unfiltered, weighted), parsed with the target's gender.
    // shortcut: the native approach dropdown (+25 when it matches the row's
    // Compatible Approach Styles) has no port counterpart; the port's own drawn
    // approach above stands, and the outcome roll is unchanged.
    const size_t ha = colourPick("a16_secret-agent-honeypot-approach-color-enc",
                                 [](const Table&, size_t) { return true; });
    if (ha != size_t(-1))
      briefing += " According to your client, " + client() + ", the target prefers lovers who " +
                  colourResolve(trim(tbl("a16_secret-agent-honeypot-approach-color-enc").at(ha, "Snippet")),
                                spyTargetFemale_) + ".";
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
    // Client.Type, enum civilian 1 / corporation 2 / government 3.
    // shortcut: the native client generator was not traced; the type is uniform.
    static const char* kClientTypes[] = {"civilian", "corporation", "government"};
    const std::string clientType = kClientTypes[spyRng_.below(3)];
    // hack-mission-file colour: SimSecretAgentHackingMission::.ctor @0x73f2f9c00c,
    // GetRandomHackMissionFileColorForType 0x73f2c55a70: (file type matches) and
    // (blank Client Type or match); '<client>' -> 'your client, ' + name (L68392);
    // parsed with the client's gender. Native keeps it as mission.FileColor.
    const size_t hf = colourPick("a16_secret-agent-hack-mission-file-color-enc", [&](const Table& t, size_t r) {
      const std::string ct = lower(trim(t.at(r, "Client Type")));
      return (kind.empty() || lower(trim(t.at(r, "File Type"))) == kind) && (ct.empty() || ct == clientType);
    });
    if (hf != size_t(-1)) {
      const std::string raw = spySwap(trim(tbl("a16_secret-agent-hack-mission-file-color-enc").at(hf, "Snippet")),
                                      "<client>", "your client, " + client());
      const std::string fileColour = colourResolve(raw, clientFemale);
      // shortcut: the native drive/file browser (one (FileName, FileColor) list
      // item per file, decoys from GenerateUniqueFileAndColor) has no port
      // screen; the mission's own file is shown inline as 'name: colour.'.
      if (!fileColour.empty()) briefing += " " + file + ": " + fileColour + ".";
    }
    // hack-mission-briefing colour: ScenarioSecretAgentHackingMissionBriefing::
    // RefreshScenario @0x73f2a97b20, GetRandomHackMissionBriefingColorForType
    // 0x73f2c53df8: exact Client Type, matching File Type; client's gender.
    // shortcut: the native first sentence needs mission.ComputerType and
    // FileAction, which the port does not draw; only the colour sentence (L109
    // ... L6312) is kept, joined by ' ' rather than '\n\n'.
    const size_t hbr = colourPick("a16_secret-agent-hack-mission-briefing-color-enc", [&](const Table& t, size_t r) {
      return lower(trim(t.at(r, "Client Type"))) == clientType &&
             (kind.empty() || lower(trim(t.at(r, "File Type"))) == kind);
    });
    if (hbr != size_t(-1))
      briefing += " Reportedly, the file contains " +
                  colourResolve(trim(tbl("a16_secret-agent-hack-mission-briefing-color-enc").at(hbr, "Snippet")),
                                clientFemale) + ".";
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
        // stealth-alarm colour: ScenarioSecretAgentStealthMissionDisableSecurity::
        // RefreshScenario @0x73f2aa780c, GetRandomSecretAgentStealthAlarmColor
        // 0x73f2c58fa0: uniform row, door and correct wire Next(0,count), two
        // incorrect wires OrderBy(random).Take(2) plus the correct one, reshuffled.
        if (ensureTable("a16_secret-agent-stealth-alarm-color-enc") &&
            tbl("a16_secret-agent-stealth-alarm-color-enc").size()) {
          const Table& al = tbl("a16_secret-agent-stealth-alarm-color-enc");
          const size_t ar = spyRng_.below(uint32_t(al.size()));
          std::vector<std::string> doors, rights, wrongs;
          splitOn(al.at(ar, "Door Color"), ',', doors);
          splitOn(al.at(ar, "Correct Wire Colors"), ',', rights);
          splitOn(al.at(ar, "Incorrect Wire Colors"), ',', wrongs);
          // shortcut: OrderBy's random key lambda is not traced; one unit() key per
          // element, stable-sorted, stands in for it.
          const auto shuffled = [this](const std::vector<std::string>& in) {
            std::vector<std::pair<double, std::string>> keyed;
            for (const std::string& s : in) keyed.emplace_back(spyRng_.unit(), trim(s));
            std::stable_sort(keyed.begin(), keyed.end(),
                             [](const auto& a, const auto& b) { return a.first < b.first; });
            std::vector<std::string> out;
            for (const auto& k : keyed) out.push_back(k.second);
            return out;
          };
          if (!doors.empty() && !rights.empty() && wrongs.size() >= 2) {
            const std::string door = trim(doors[spyRng_.below(uint32_t(doors.size()))]);
            const std::string correct = trim(rights[spyRng_.below(uint32_t(rights.size()))]);
            std::vector<std::string> wires{correct};
            const std::vector<std::string> wrong = shuffled(wrongs);
            wires.push_back(wrong[0]);
            wires.push_back(wrong[1]);
            wires = shuffled(wires);
            const bool fem = h.female;
            const std::string he = fem ? "she" : "he", He = fem ? "She" : "He";
            // Info row 'Door Color' (L18123) with CapitalizeAllMajorWordsInString.
            // shortcut: the native first sentence ('Your <relationship>, <titled code
            // name>, is reporting <pronoun> standing in front of <a door> door ...')
            // has an untraced pronoun form and a relationship string the hero
            // lacks; the door shows as its info row instead.
            std::string doorShown = door;
            for (size_t k = 0; k < doorShown.size(); ++k)
              if (k == 0 || doorShown[k - 1] == ' ')
                doorShown[k] = static_cast<char>(toupper(static_cast<unsigned char>(doorShown[k])));
            briefing += " Door Color: " + doorShown + ".";
            briefing += " " + He + " informed you that " + he +
                        " removed the access point's protective covering and sees three wires: " +
                        wires[0] + ", " + wires[1] + ", and " + wires[2] + ". Which should " + he + " cut?";
            for (const std::string& w : wires) wireOptions.push_back("The " + spyUpperFirst(w) + " wire");
            const size_t lane = (forcedChoice >= 0 && forcedChoice < 3) ? size_t(forcedChoice)
                                                                         : size_t(spyRng_.below(3));
            wireChoice = wireOptions[lane];
            // ChoiceSelectedAtIndex 0x73f2aa7ea0: correct iff the lower-cased choice
            // text contains the correct colour.
            if (lower(wireChoice).find(lower(correct)) != std::string::npos && !h.spyCodeName.empty()) {
              // shortcut: 'Your <relationship>, <titled code name>, ' becomes the
              // hero's code name (the hero is the agent); the snipped wire is
              // written as '<colour> wire' (the native argument is untraced); the
              // native '\n\n' between the two sentences is a space here.
              wireLine = h.spyCodeName + " reported that the security system died the moment " + he +
                         " snipped the " + correct + " wire. " + He + " gave the all-clear before resuming " +
                         (fem ? "her" : "his") + " mission.";
            }
            // shortcut: a wrong wire leads natively to an interruption fight or
            // gadget feedback whose narrative is unknown; the port writes nothing
            // and the mission roll above stands.
          }
        }
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
      std::string sting = spyRawSnippet("a16_secret-agent-honeypot-sting-actions-enc", "String", "Commonness Score", "", "", "");
      if (!hobby.empty()) briefing += " Their weak spot is " + hobby + ".";
      if (!place.empty())
        briefing += " Take " + (spyTarget_.empty() ? std::string("the mark") : spyTarget_) + " to " + place + ".";
      const std::string stingWhere = spySnippet("a16_secret-agent-honeypot-sting-locations-enc",
                                                "String", "Commonness Score", "", "", "");
      if (!stingWhere.empty()) sting += (sting.empty() ? std::string() : " ") + stingWhere;
      if (!sting.empty()) {
        // The sting strings address the mark, so the pronouns are the mark's
        // own: swapped while the tokens are still raw (a post-resolve swap
        // never fires, the resolver has already expanded them).
        sting = spySwap(sting, "<him/her>", spyTargetFemale_ ? "him" : "her");
        sting = spySwap(sting, "<his/her>", spyTargetFemale_ ? "his" : "her");
        sting = resolve(sting);
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
  // honeypot-meeting-scenario colour: MeetDecisions::ChoiceSelectedAtIndex
  // @0x73f2aa0238, success branch only; GetRandomSecretAgentHoneypotMeetingScenario-
  // ColorForLocation 0x73f2c607cc: Location OrdinalIgnoreCase match or blank row;
  // parsed with the target's gender.
  if (success && type == "honeypot") {
    const size_t mr = colourPick("a16_secret-agent-honeypot-meeting-scenario-color-enc", [&](const Table& t, size_t r) {
      const std::string loc = lower(trim(t.at(r, "Location")));
      return loc.empty() || loc == lower(honeypotWhere);
    });
    if (mr != size_t(-1)) {
      const std::string met = colourResolve(
          trim(tbl("a16_secret-agent-honeypot-meeting-scenario-color-enc").at(mr, "Snippet")), spyTargetFemale_);
      // shortcut: the sentence ends before ' disguised as <disguise>' (the port
      // has no disguise pick) and the opening 'Your <relationship>, <code name>,
      // made contact ...' sentence is dropped; agent pronouns are the hero's.
      line += (line.empty() ? std::string() : " ") + "According to " + (h.female ? "her" : "his") +
              " report, " + (h.female ? "she" : "he") + " caught the target's eye " + met + ".";
    }
  }
  // client-mission-terminated colour: ScenarioSecretAgentMissionFailure::
  // RefreshScenario @0x73f2a95ce0, GetRandomSecretAgentClientMissionTerminatedColor
  // 0x73f2c56308: exact Mission Type (blank rows never match a real mission);
  // '<client-spy>' -> 'Your client, ' + name + ', ' (L50286, L5282); client's gender.
  // shortcut: native shows it only when the agent returns (IsSuccessfulMissionAction()
  // or a double agent); the hero always comes home, so every failure qualifies.
  // 'intel' is not a native specialization and has no rows.
  if (!success) {
    const size_t tr = colourPick("a16_secret-agent-client-mission-terminated-color-enc", [&](const Table& t, size_t r) {
      return lower(trim(t.at(r, "Mission Type"))) == type;
    });
    if (tr != size_t(-1)) {
      const std::string raw = spySwap(trim(tbl("a16_secret-agent-client-mission-terminated-color-enc").at(tr, "Snippet")),
                                      "<client-spy>", "Your client, " + client() + ", ");
      const std::string said = colourResolve(raw, clientFemale);
      if (!said.empty()) line += (line.empty() ? std::string() : " ") + said;
    }
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
  if (!wireLine.empty()) line = wireLine + (line.empty() ? std::string() : " " + line);
  if (!line.empty()) out.extra.push_back(line);
  if (!wireOptions.empty()) {
    out.options = wireOptions;
    out.choice = wireChoice;
  }
  if (!summary.empty()) out.narrative = summary;
  h.happiness += success ? 2 : -2;
  return true;
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

std::string Sim::spySnippet(const std::string& table, const std::string& column,
                            const std::string& weightColumn, const std::string& filterColumn,
                            const std::string& filterValue, const std::string& flagColumn) {
  return pickSnippet(table, column, weightColumn, filterColumn, filterValue, flagColumn, spyRng_);
}

std::string Sim::spyRawSnippet(const std::string& table, const std::string& column,
                               const std::string& weightColumn, const std::string& filterColumn,
                               const std::string& filterValue, const std::string& flagColumn) {
  // Identical draw to spySnippet (one pickRow), the row text just stays raw.
  const size_t row = pickRow(table, weightColumn, filterColumn, filterValue, flagColumn, spyRng_);
  if (row == size_t(-1)) return std::string();
  const Table& t = tbl(table.c_str());
  if (!t.has(column)) return std::string();
  return trim(t.at(row, column));
}

size_t Sim::spyRow(const std::string& table, const std::string& weightColumn,
                   const std::string& filterColumn, const std::string& filterValue,
                   const std::string& flagColumn) {
  return pickRow(table, weightColumn, filterColumn, filterValue, flagColumn, spyRng_);
}

}  // namespace bl
