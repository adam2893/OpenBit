# blport — native C++ port scaffold for BitLife 3.25.1 (com.candywriter.bitlife)

Re-implements, in standard C++17 with no third-party dependencies, the parts of the
Unity IL2CPP game that a platform port needs: the decrypted content-table data layer,
the life-sim state + age-up loop, and the mod menu's cheat features. Nothing here is
invented — every string printed by the demo comes from _re/gamedata_plain/ or
_re/gamedata_json/, and every field/method name is the verbatim IL2CPP name from
_re/il2cpp_dump.txt.

## The mod is not required for this port

Expansion packs in BitLife are shipped content plus a boolean entitlement: all of their tables are
already in `_re/gamedata_plain/` (vampire 44 tables, cult 45, secret-agent 60, casino 39, zoo 35,
astronaut 35, racing 31, landlord 22, black-market 24, actor 20, music 57, …). The original mod only
flipped `Game.MonetizationVars` / `Game.GameVars` flags by inline-hooking `Game.UIIAPManager`.
This port therefore carries no hooking, no `libmenu.so`, no PairIP/killsign and no install gate:
`kEntitlements[]` is plain state and `Mod::grant`/`grantAll` are ordinary writes. Treat
`Mod` as the "everything unlocked" switch, not as the cheat.

## Build and run

    cd /Users/Adam/Documents/bitlife/_re/port
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j4
    ./build/blsim

Verified with Apple clang 17.0.0 (clang-1700.0.13.5, arm64-apple-darwin24.4.0) and
cmake 4.4.3. blsim exits 0. The data root defaults to the parent directory of the
build tree; pass another root as argv[1], or as `--data` in the scripted form below.

## Scripted run mode (`--script`) — the Phase 1 harness, port side

    ./build/blsim --script script.json [--data /path/to/_re]

This mode does not run the demo: stdout carries exactly one JSON object for one
life and nothing else, errors go to stderr, and the exit code is 0 ok, 1 when a
table or the data root cannot be read, 2 when the script is unreadable or
malformed. The life is aged by the same loop the demo uses, driven by a seeded
`std::mt19937_64` — never by the clock — so one script always produces the same
bytes (verified by rerunning and diffing).

The script; every key is optional (`seed` 0, empty `country` = the seed picks
one the way createLife always has, `gender` male, `ages` 0, no forced choices):

    {"seed": 12345, "country": "Finland", "gender": "male", "ages": 3, "choices": [0,1,2]}

`choices[i]` is the 0-based index into the choices the row drawn at age-up `i`
actually fills in — not a "Choice N" column number. A missing entry, an index past
the last filled choice, or a row with no choice at all falls back to the seeded
pick, so a run stays reproducible either way. `country` must be a
`<Name>k__BackingField` of `gamedata_json/a16_countries-enc.json`; pinning it also
draws the birth city from that country's own `Cities` array, and a country the
file does not have is an error rather than a silent random one.

The object it prints:

    {
      "seed": 12345,
      "hero": {
        "name": "Minji Pak",
        "gender": "male",
        "country": "Finland",
        "city": "Helsinki",
        "stats": {"happiness":70,"health":80,"smarts":65,"looks":69,"karma":0,"discipline":0}
      },
      "events": [
        {
          "age": 1,
          "table": "a16_dumb-scenarios-universal-enc",
          "row": 97,
          "title": "Toying Around",
          "body": "You're at home, lying inside your crib. Luckily, there are a few things nearby to keep you occupied.",
          "choice": "The firetruck",
          "logLines": [
            "Toying Around: You're at home, ...",
            "Which will you reach for? -> The firetruck",
            "I played with a toy firetruck that I found in my crib."
          ],
          "deltas": {"karma":0,"discipline":0,"happiness":2,"smarts":1,"looks":0}
        }
      ],
      "final": {"age": 3, "alive": true, "stats": {"happiness":76,"health":80,"smarts":64,"looks":67,"karma":0,"discipline":0}, "money": 0}
    }

| field | where it comes from |
|---|---|
| `hero.name` | `First Name` + `Last Name` of a16_casino-casino-celebrity-name-enc, filtered by `Gender` |
| `hero.city` | one entry of the birth country's own `Cities` array (a16_countries-enc.json) |
| `hero.stats` | the stats as created; `final.stats` is the same fields after the last age-up |
| `events[].age` | the hero's age after that age-up |
| `events[].table`, `.row` | the real table and its 1-based data row |
| `events[].title`, `.body` | `Title`, `Body`; **null** when the drawn row has none |
| `events[].choice` | the chosen branch's `Choice N` text; null when the row has no choice |
| `events[].logLines` | the lines the demo prints for that event: title/body, `CTA -> Choice`, `Narrative N` |
| `events[].deltas` | `Karma/Discipline/Happiness/Intelligence/Appearance Impact N` of that branch |
| `final` | end state; `alive` goes false when the a16_death-types-enc mortality roll fires |

Every string is resolved real table content; a field the drawn row does not fill
in is `null`, never an empty string or a guess. Unresolved placeholders such as
`<rb>` stay verbatim, exactly as documented below.

## Layout

| file | lines | what |
|---|---|---|
| CMakeLists.txt | 20 | static lib blport + demo blsim |
| include/bl/table.h | 63 | generic content-table loader (class Table, class Rng) |
| src/table.cpp | 153 | CSV/TSV scanner, quoted fields, '#' comments, weighted pick |
| include/bl/game.h | 103 | GameVars (IL2CPP backing fields), Hero, Entitlement, GameState |
| src/game.cpp | 125 | the 60-entry entitlement catalogue generated from Game.UIIAPManager |
| include/bl/sim.h | 91 | Country, AgeUp, Life, class Sim |
| src/sim.cpp | 460 | country JSON reader, createLife, ageUp, mortality, placeholder resolver |
| include/bl/script.h | 49 | ScriptSpec + the --script entry points |
| src/script.cpp | 328 | script JSON reader, deterministic run, JSON writer for --script |
| include/bl/mod.h | 92 | Widget enum, MenuNode/Menu, class Mod |
| src/mod.cpp | 228 | LGL grammar parser/renderer, buildMenu, grant/grantAll |
| src/main.cpp | 167 | argv dispatch + 6-section demo |

## Mapping — RE finding to C++ module

| RE finding (file / class / method) | port module |
|---|---|
| gamedata_plain/* — 1071 tables decrypted via EncryptionManager.DecodeAndDecryptString(text, "com.wtfapps.apollo16") | src/table.cpp (Table::load); index read from gamedata_plain_index.json |
| gamedata_json/a16_countries-enc.json — DataCountry (il2cpp_dump.txt:7619), BinaryFormatter backing fields, 102 countries | Sim::loadCountries in src/sim.cpp (minimal self-written JSON reader) |
| Game.GameVars (il2cpp_dump.txt:105337) — &lt;UserBoughtBitizenship&gt; 0x23, &lt;UserBoughtGodMode&gt; 0x25, &lt;Ads&gt; 0x30, &lt;UserGivenGodMode&gt;, &lt;DateOfLastInterstitial&gt;, &lt;SettingTheme&gt;, &lt;FavoriteActivitiesArray&gt;, &lt;LivesStarted&gt;, &lt;NumTimesPurchasesMade&gt; … | struct bl::GameVars in include/bl/game.h — one member per field, each comment carries the verbatim IL2CPP name and offset |
| Game.UIIAPManager (il2cpp_dump.txt:128447) — the 60 XxxPurchased grant points and the 58 PurchaseXxx initiators the original mod hooks | kEntitlements[] in src/game.cpp, surfaced as bl::GameState::entitlements |
| IAPManager (il2cpp_dump.txt:447) — the ~110 System.String product-id fields (NonConsume_Bitizenship, Consume_TimeMachine, the sale variants, …) | the iapField column of each Entitlement (include/bl/game.h) |
| libmenu.so ShadowHook 2.0.1 inline hooks on Game.UIIAPManager::*Purchased / Purchase* and BitLife.UI.Containers.UIDarklyPaywallGenericIAP::BuyButtonPress (REPORT.en.md s.13) | Mod::grant / Mod::grantAll in src/mod.cpp — the native analogue is setting Entitlement::owned; the original does it by inline-hooking those methods so they report already-owned |
| com.android.support.Menu.SettingsList() / .sub2025s() runtime strings (REPORT.en.md s.12) | Mod::buildMenu in src/mod.cpp — the same tree, verbatim (including the HTML font labels) |
| LGL mod-menu grammar (Toggle_X, SeekBar_X_min_max, CheckBox_X, Category_X, Button_X, Collapse_X, CollapseAdd_RichTextView_X, RichTextView_X, InputNum_X, …) | parseMenuNode + renderNode + enum class Widget in src/mod.cpp / include/bl/mod.h |
| com.android.support.Preferences.changeFeatureBool(String,int,boolean) — toggles persisted by widget index in <pkg>_preferences | Mod::saveFeature / Mod::autoMenuSize (include/bl/mod.h) |

## Real tables and columns wired up

| table | columns actually read | used for |
|---|---|---|
| a16_dumb-scenarios-universal-enc (258 rows, 42 cols) | Index, Hero Min Age, Hero Max Age, Student, Job, Hero Gender, Exterior Title, Title, Body, CTA, Choice 1..4, Narrative 1..4, {Karma,Discipline,Happiness,Intelligence,Appearance} Impact 1..4 | the age-up event loop |
| a16_employee-recognition-scenarios-enc (14 rows) | Scenario, Commonness Score | the Commonness-Score-weighted pick, used once the hero is employed |
| a16_casino-casino-celebrity-name-enc (106 rows) | First Name, Last Name, Gender, Commonness Score | hero + NPC name pool (no dedicated hero-name table exists) |
| a16_hero-filler-enc (1132 rows) | Condition, String, Hero Min Age, Hero Max Age, Hero Gender | filler narrative between scripted events |
| a16_death-types-enc (110 rows) | Type, Commonness Score, Min Age, Sentence | mortality check + death sentence |
| gamedata_json/a16_countries-enc.json | Name/Continent/Cities/Population/Currency k__BackingField, … | hero birth country and city |

Placeholder handling in real scenario strings (Sim::resolve): <a/b/c> alternation,
<num2-5> numeric range, and the person tokens <friend>, <friend-possessive>, <teacher>,
<classmate>, <coworker>, <parent>, <lover>, <company>, <employee> are substituted from
the name pool. Any token it cannot resolve is left verbatim rather than guessed.

## How to extend

1. Add a table. sim.table("a16_<name>") loads and caches it; read cells with
   t.at(row, "Column Name"), t.num(row, "Column Name"), t.flag(row, "Column Name")
   (a bare x is the boolean convention in these files). Always take the column name
   from gamedata_plain_index.json — never invent one.
2. Add a game field. Copy the verbatim IL2CPP field name and its offset from the class
   block in il2cpp_dump.txt and add the comment with it.
3. Add an entitlement. A new XxxPurchased / PurchaseXxx pair in Game.UIIAPManager and
   the matching IAPManager System.String field become one more kEntitlements[] row.
4. Add a menu widget. Register the prefix in headFor / kindFor (src/mod.cpp).

## Unknowns not ported

* The promised mod entitlement/product list does not exist on disk.
  _re/decrypt_log.json contains only a fail array of {"ok": false, "empty": true} table
  names (plus an empty bf array), and neither REPORT file has the expected
  "MOD FEATURES / entitlement" section. The catalogue here is therefore derived from
  Game.UIIAPManager + IAPManager in il2cpp_dump.txt (60 grant points, 58 initiators,
  ~110 product-id fields), not from the mod.
* The mod's native feature catalogue is not statically recoverable. libmenu.so's
  .rodata holds 1958 NUL-terminated strings, 946 of them encrypted by the mod's own
  encoder; single-byte XOR, address-derived XOR, repeating-key XOR and keystream reuse
  were all ruled out. It stores no hook-offset table either — it resolves
  Game.UIIAPManager methods by name at runtime via il2cpp_method_get_name, which is why
  the names are encrypted on disk. Only the 59 hook sites observed live in
  REPORT.en.md s.13 are known, and the menu modelled from that evidence is what Mod
  reproduces: exactly one cheat (Free In-App Purchase) plus the two settings.
* Unresolved placeholders — <rb> (254 tables), <musician-indefinite>,
  <circumstance-public> — stay verbatim in output. <rb> echoes a noun the game resolved earlier in the same scenario (the body
  says "a cookie", the narrative says "the <rb>"), but no column in the scenario tables
  carries it — it is engine-side state, not data.
* a16_dumb-scenarios-universal-enc has no Commonness Score column, so the main age-up
  pick is uniform. Weighted picking is exercised on
  a16_employee-recognition-scenarios-enc, which does have the column. No a16_* table
  combines Log Version + Hero Min Age + Commonness Score.
* No hero name table exists. The name pool is a real but topically wrong table,
  a16_casino-casino-celebrity-name-enc, chosen because it is the only one with
  First Name + Last Name + Commonness Score. Names come out intentionally absurd
  ("Sugar Moon", "Beyawnce Gnolls").
* Mortality is a port-side rule, not the original multi-system one: it weights
  a16_death-types-enc by Commonness Score and applies the hero's age against the
  table's Min Age. The real death pipeline (health, illness, jail, accidents) is spread
  across many systems and is not reconstructed.
* Career, education, relationships, money and property are not ported.
  a16_careers-enc (Career, Required Education, Starting Salary, Salary Cap,
  Seniority Track, 33 columns) is loadable but unused, so Hero::job is just Student or
  Worker. No table scan for jobs, raises, promotions, schooling, marriage or housing has
  been done.
* Scenario gating is partial. Hero Min Age, Hero Max Age, Hero Gender, Student and Job
  are honoured. Vampire, Both Parents Alive and Randomize Choices are parsed but ignored.
* Eleven-plus tables failed BinaryFormatter deserialisation (the fail list in
  decrypt_log.json): BillingMode, IAPProductCatalog, a16_countries-enc (recovered
  separately into gamedata_json/), PrivateSchoolList, PublicSchoolList, SocialMediaList*,
  all "Fight Club Scenarios - *", most genesis_*, male0/1/2-age4|age13 and
  female0/1/2-age4|age13 character saves, candidates, login, package, gpResponse,
  senior_center_*, privacyPolicy. IAPProductCatalog in particular holds exactly ONE
  product (com.wtfapps.apollo.16.timemachine), so it is not the real store product list.
* Not ported at all: mini-games (astronaut/casino/life/secret agent/racing/ultimate
  fighter — the GameVars flags exist, the tables are not wired), heirlooms/artifacts/
  auction items, darkly paywalls, time machine, battle pass, daily quests, notifications,
  ads/interstitials, and the whole UI layer. The port renders the menu as text; Menu's
  widget geometry (MENU_WIDTH 290, MENU_HEIGHT 210, MENU_CORNER 35.0f, ICON_SIZE 45,
  ACCENT_COLOR #7C4DFF) is recorded in REPORT.en.md but not reproduced.
* No art, audio or localisation assets were exported from the Unity bundles, so the port
  has no presentation layer to feed.
