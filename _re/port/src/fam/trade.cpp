// src/fam/trade.cpp -- moved out of src/sim.cpp (REPORT.en.md 131).
//
// Cut verbatim by tools/split_family.py; the only edits are the includes.
#include "bl/sim.h"
#include "bl/table.h"
#include "bl/text.h"

#include <string>
#include <vector>

namespace bl {

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

std::string Sim::tradeGenre(Life& life) {
  Hero& h = life.hero;
  if (!h.musicGenre.empty()) return h.musicGenre;
  if (!ensureTable("a16_music-genres-enc")) return std::string("pop");
  const Table& t = tbl("a16_music-genres-enc");
  if (t.size() == 0) return std::string("pop");
  h.musicGenre = lower(trim(t.at(tradeRng_.below(uint32_t(t.size())), "Genre")));
  return h.musicGenre;
}

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

}  // namespace bl
