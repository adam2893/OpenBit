# BitLife Life Simulator 3.25.1 — reverse-engineering workspace

Target: `/Users/Adam/Documents/bitlife/BitLife-Life-Simulator-v3.25.1-mod_sdk.apk`
(317,793,590 bytes, 2786 zip entries, sha256 `c2111dee01a5d28f0a5f6cfb61bcf2d513a53d8f8ef0f530265da9240f215c1d`)
Package `com.candywriter.bitlife`, versionName 3.25.1, versionCode 1063, minSdk 28, targetSdk 35.
Engine: Unity IL2CPP, arm64-v8a only.

This directory is the working tree for that analysis: extracted artifacts, decompiled Java,
the decrypted game content database, live-runtime dumps, and a native C++ port scaffold.

Every claim below is labelled the way the reports label it — **[obs]** observed, **[inf]** inferred,
**[unk]** unknown. Full write-ups: `REPORT.en.md` (English) and `REPORT.md` (Chinese); both carry
the same three parts (static, dynamic, port).

---

## 1. Layout

| path | size | what |
|---|---|---|
| `dex/` | 82 MB | the 15 root `classes*.dex` extracted from the APK |
| `lib/` | 135 MB | `lib/arm64-v8a/*.so` — 21 native libraries |
| `assets/` | 161 MB | Unity data: `bin/Data/datapack.unity3d`, `global-metadata.dat`, resource files |
| `assets_mod/` | 1.2 MB | the 12 PairIP-style `00 49 41 50` (\x00IAP) payload blobs from `assets/` |
| `jadx_res/` | 339 MB | `jadx --no-src` run: decoded `AndroidManifest.xml`, resources |
| `jadx_c10/` | 39 MB | full decompile of `dex/classes10.dex` (holds UnityPlayerActivity) |
| `jadx_small/` | 224 KB | decompiles of classes12/13/14/15 (killsign, mod stubs, second-stage loader) |
| `gamedata/` | 21 MB | 1086 raw Unity `TextAsset`s exported out of `datapack.unity3d` (`dump_ta.py`) |
| `gamedata_plain/` | 15 MB | **1071 decrypted content tables** (13,315,430 B of text) |
| `gamedata_json/` | 29 MB | 1082 files: the 11 .NET BinaryFormatter saves as JSON + all plain tables |
| `gamedata_bf/` | 2.7 MB | the 13 raw BinaryFormatter `TextAsset`s |
| `port/` | 1.4 MB | the C++17 port scaffold (see `port/README.md`) |
| `il2cpp_dump.txt` | 19,665,271 B | live il2cpp inventory: 128 assemblies, 285,819 lines of classes/fields/methods/addresses |
| `gamedata_CATALOG.md` | — | the 1071 tables grouped into 183 families with headers |
| `gamedata_plain_index.json` | — | file / bytes / lines / header for each decrypted table |
| `hookbytes.json`, `hooktramp.json`, `decrypt_log.json`, `unresolved.json` | — | machine-readable results of the dynamic passes |
| `*.py` (catalog, conv, bfparse, dump_ta, hookbytes, tramp, verify_hooks, …) | — | every step of the pipeline, re-runnable |

## 2. What the app is made of

**[obs]** Manifest `<application android:name="com.pairip.application.Application">`, `com.pairip`
Play-Integrity-style shell, signed with the **public AOSP testkey** (`META-INF/TESTKEY.RSA`,
subject `CN=Android`, cert SHA-256 `A4:0D:A8:0A:59:D1:70:CA:A9:50:CF:15:C1:8C:45:4D:47:A3:9B:26:98:9D:8B:64:0E:CD:74:5B:A7:1B:F5:DC`)
— not the original Candywriter key, i.e. this file is a repack.

**[obs]** `com.pairip.application.Application` **extends `com.android.support.killsign`**, the
signature-bypass class from `github.com/L-JINBIN/ApkSignatureKillerEx` (a 102 KB class whose inner
`Parcelable.Creator` overwrites `PackageInfo.signatures[0]` and `signingInfo.getApkContentsSigners()[0]`
with a spoofed `Signature`). PairIP expects signer `13Gtz+XU0d5zweeer/N1aoPBO89tvA3T4ZndjymXIxg=`;
the check can only pass because killsign lies to it.

**[obs]** The mod's own Java lives nowhere on disk. `com.unity3d.player.UnityPlayerActivity.onCreate`
calls `ModPlayerprefs.Modded(this)` → `System.loadLibrary("menu")` → `libmenu.so`, which
LZMA-decompresses and `InMemoryDexClassLoader`-loads **three in-memory dexes** (core menu 159,748 B,
an app dex 5,237,600 B, a DNS/GMS dex 1,334,156 B). All lifecycle methods are rewritten into
reflective `MethodHandle.invoke(null, this, …)` calls on obfuscated holders to hide the hook.

**[obs]** Telemetry: `com.bamen.report.ReportProvider` ContentProvider → `Reporter.start()` posts to
`https://api.gamekillerapp.com/api/overseas/v1/single-game/{online-log,launch-log}` with
`Settings.Secure` `android_id`, package, version and a daemon thread `bamen-online-report`.

**[obs]** Ad/monetization SDK inventory (class-reference counts): `com/inmobi/media` 1769,
`com/applovin/impl` 742, `com/explorestack/protobuf` 728, `com/google/protobuf` 631,
`com/flurry/sdk` 524, `com/appsflyer/internal` 456, `com/vungle/ads` 228, `com/unity3d/ads` 176,
plus ironsource, chartboost, mintegral/mbridge, bidmachine, pubnative, gadsme, gameanalytics and the
IAB OMID verification client.

## 3. The mod menu — what it actually does

**[obs]** `libmenu.so` (1.2 MB, sha256 `ab7e98cd5c9d12178e07fc0c0cee48b1bff209a1970134680c477229fd537239`)
links GL/EGL (in-game overlay), logs through tag `Mod_Menu`, and on every start prints
`shadowhook vshadowhook version 2.0.1` plus **59 `shadowhook HOOK(addr) OK` lines**.

**[obs]** Java side is the public **LGL mod-menu template** (`com.android.support.{Main,Menu,Preferences,DialogHelper,CrashHandler}`),
whose feature list is *not* in Java — `Menu.SettingsList()` and `Menu.sub2025s()` are `native` and come from
`libmenu.so`. Dumped at runtime, the whole menu is:

```
NOTE / RichTextView "You need to install Google Play Services for the mod to work."
MOD FEATURES → Button "Free In-App Purchase"
CONTACT US   → Telegram feedback (token + chat id literals present in Menu.java)
Settings     → Toggle "Save Feature" (id -1), Toggle "Auto Menu Size" (id -3)
Game Info    → version info
```

**[obs]** So the mod has exactly **one cheat feature**. Its mechanism: resolve `Game.UIIAPManager`'s
~60 `*Purchased` grant methods and ~58 `Purchase*` initiators (and
`BitLife.UI.Containers.UIDarklyPaywallGenericIAP.BuyButtonPress`) **by name at runtime** through the il2cpp
API, then inline-hook them with ShadowHook. The method names are encrypted at rest; they were recovered
by dumping `libmenu.so`'s live image (907 previously-invisible strings).

**[obs]** Hook re-verification (correcting an earlier mis-read): all **59 sites are patched in place** —
first instruction replaced by an ARM64 `B` (`0x17xxxxxx`), the bytes after it match the on-disk ELF, and the
containing pages are `rwxp` (mprotect-raised) while the rest of the library stays `r-xp`. The patched
addresses are **inside** the methods (0x31ddad0–0x31f5040), not at their entries. The branch targets
(trampolines) could not be resolved **[unk]**.

**[obs]** A separate gate in the mod's in-memory dex blocks the game until package
`com.adb.apkinstaller` (9ADB Installer, `liteapks.com`) exists — `Main.isPackageInstalled` as a
blocking `AlertDialog`; it re-checks on Activity resume.

### The mod is **not** needed for a port

Expansion packs in BitLife are **content that is already in the base game** plus a boolean entitlement.
All the content ships in the same tables this repo has decrypted (§5); the mod only ever flipped those
booleans. So the port keeps no hooking, no `libmenu`, no PairIP/killsign, no 9ADB gate — it reads the
real tables and exposes the entitlement array as its own "everything unlocked" switch. `port/src/game.cpp`
already carries the full 60-product catalogue for exactly that reason; `Mod::grant()` in the port is
a plain state write, not a hook.

## 4. Content pipeline — how the data was recovered

**[obs]** The game ships its whole content database as Unity `TextAsset`s inside
`assets/bin/Data/datapack.unity3d` (121,483,836 B). UnityPy 1.25.4 enumerated 8133 Texture2D,
8094 Sprite, 7653 MonoBehaviour, 3988 GameObject, **1086 TextAsset**, 327 Canvas (the UI-only
`data.unity3d` adds 7854 MonoScript, 1726 MonoBehaviour, 0 TextAsset).

1. `dump_ta.py` exported all 1086 TextAssets → `gamedata/` (1084 unique names, 19,321,948 B).
2. Most are Base64 text; 11 are raw .NET BinaryFormatter streams (`00 01 00 00 00 FF FF FF FF`,
   `Assembly-CSharp`).
3. **[obs]** The game decrypts them itself: `EncryptionManager.DecodeAndDecryptString(text, key)`
   with `EncryptionManager.DefaultCipherKey() == "com.wtfapps.apollo16"`. Verified live through frida
   against the running process (the oracle is the game's own code, not a guess at the cipher).
4. `/tmp/enc2.js` + `/tmp/bulk2.py` ran that oracle over every file: **966 OK, 118 failures**, all
   `{"ok":false,"empty":true}`. `fixpass.py` resolved them: **105 were never encrypted** (copied
   verbatim) and **13 were BinaryFormatter** → `gamedata_bf/`. Unresolved: 0.
5. `gamedata_plain/` = **1071 tables, 13,315,430 B**. The 13 BinaryFormatter blobs were parsed with
   the `nrbf` package (`bfparse.py`) into `gamedata_json/`; **11 parsed** (e.g. `a16_countries-enc`,
   102 countries, `<Name>k__BackingField`-style names), **2 still fail** with
   `RecursionError: maximum recursion depth exceeded` even at `setrecursionlimit(1000000)`
   (`female0-age13`, `male1-age13`) **[unk]**.

**[obs]** Table formats are real CSV/TSV with quoted fields and `#` comments. Examples:
`genesis_master_tags_reference_2` (1,303,319 B / 21,772 rows / 14 cols:
`tag_id,tag_value,weight,categorization_a..c,follow_up_a..e,json_cost_dropdown,is_starting_example,narrative`),
`genesis_movie_director` (905,057 B / 190 rows / 53 cols of branching film scenarios),
`a16_dumb-scenarios-universal-enc` (258 rows / 42 cols, the generic age-up scenario set).

## 5. Expansions — all present in the shipped content

**[obs]** Every paid expansion's content is in the 1071 decrypted tables. File counts and byte totals
measured from `gamedata_plain_index.json` (group = the token after `a16_`):

| pack / module | tables | bytes | | pack / module | tables | bytes |
|---|---:|---:|---|---|---:|---:|
| genesis (core) | 77 | 4,520,633 | | astronaut | 35 | 171,144 |
| dumb (generic scenarios) | 24 | 1,982,682 | | actor / movie director | 20 | 170,035 |
| music | 57 | 427,771 | | dealer | 38 | 161,810 |
| vampire | 44 | 330,920 | | model | 25 | 159,993 |
| fight | 37 | 328,458 | | casino | 39 | 151,538 |
| sports | 13 | 241,450 | | secret agent | 60 | 125,249 |
| cult | 45 | 239,701 | | hustler | 22 | 108,630 |
| outdoor | 42 | 219,229 | | podcast | 5 | 106,129 |
| pet | 9 | 215,152 | | back to school | 14 | 105,551 |
| zoo | 35 | 189,034 | | mafia | 17 | 102,262 |
| racing | 31 | 182,124 | | black market | 24 | 101,604 |
| business | 12 | 176,270 | | royal | 12 | 92,721 |
| landlord | 22 | 173,445 | | school | 11 | 91,660 |
| luxury | 22 | 87,031 | | financial / investor | 18 | 82,637 |

(+ `hero-filler` 170,531 B, `social-media` 8/66,407, `disease` 2/86,194, `employee` 8/78,667,
`friend` 5/76,593, `lover` 4/76,068, `coworker` 5/62,817, `ex-lover` 4/57,813 … 145 first-token
groups in total. Full per-family breakdown: `gamedata_CATALOG.md`.)

**[obs]** Entitlements are plain state, not data:
`Game.MonetizationVars` (`il2cpp_dump.txt:105863`) holds `<UserBoughtLegacyBitizenship>` 0x10,
`<UserBoughtNewBitizenship>` 0x11, `<UserBoughtGodMode>` 0x13, `<UserBoughtBossMode>` 0x1a,
`<UserBoughtSpecialCareer{…}>` 0x1c–0x25 …, and `Game.GameVars` (`:105337`) holds
`<UserBoughtBitizenship>` 0x23, `<UserBoughtGodMode>` 0x25, `<UserHasPurchasedBefore>` 0x27,
`<Ads>` 0x30; product ids live as ~110 `System.String` fields on `IAPManager`. The mod hooked the
methods that write those flags.

**[obs]** They are *not* in SharedPreferences: `com.candywriter.bitlife.v2.playerprefs.xml` contains only
Unity/AppsFlyer/Unity-Connect keys (full key list in `REPORT.en.md`). The real save lives in
`/sdcard/Android/data/com.candywriter.bitlife/files/` — `MonetizationVars` (15,478 B, base64 key:value
lines that are the game's own `ObfuscateString` output, **[unk]** undecoded), `MissionVars`, `ChallengeData.data`,
`saveslotarray`, plus a 376 KB `bitlife-prod.confidence.snapshot`.

## 6. C++ port

See `port/README.md` for the module map and the not-ported list. Summary:

```
cd port && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4 && ./build/blsim
```

**[obs]** Builds and runs clean (Apple clang 17.0.0 arm64, cmake 4.4.3, exit 0, 183 output lines).
Wired to real data: `a16_dumb-scenarios-universal-enc` (age-up loop), `a16_employee-recognition-scenarios-enc`
(Commonness-Score weighted pick), `a16_casino-casino-celebrity-name-enc` (name pool),
`a16_hero-filler-enc`, `a16_death-types-enc`, and the 102-country JSON. The mod's two toggles and the
full 60-product entitlement catalogue are reproduced as plain state — no hooking.

**Not ported (honest list)**: careers/education/relationships/money/property systems, mini-games,
heirlooms/artifacts, Darkly paywalls, time machine, Battle Pass, daily quests, notifications, ads, the
whole UI layer (geometry known: `MENU_WIDTH 290`, `HEIGHT 210`, `CORNER 35.0f`, `ICON_SIZE 45`, accent
`#7C4DFF`), art/audio (8133 Texture2D not exported), and a port-side simplified mortality rule.

## 7. How the dynamic work was run

**[obs]** Host: Apple M1 macOS; phone-side work on **MuMu Player** (`/Applications/MuMuPlayer.app`),
VM `我的安卓` (Android 12 / SDK 32, arm64-v8a, model `SM-S9180`), root enabled by setting
`vmRootEnable: true` in `bundles/mumu_0_*/setting/vm.json`; adb endpoint is
`127.0.0.1:16384` (it was `5555` before the cold boot — `vm.json:adbPort` is authoritative).
`adb install -r -g` succeeded; the game must be run as **arm64** (the APK has no armeabi-v7a).

**Toolchain**: `jadx 1.5.6`, `radare2`/`rabin2`, `UnityPy 1.25.4`, `nrbf`, and
**frida 16.7.19 client + frida-server 16.7.19/android-arm64** (frida 17 dropped the Java bridge, which
this work needs). Attach by pid — `get_usb_device()` picks the wrong device, `dev.attach(name)` fails.

**Gate bypass on the bench copy**: hook `com.android.support.Main.isPackageInstalled` → `true`.

**Gotchas worth keeping** (all cost real time):

* libil2cpp's mapped segments drift 0x1000–0x4000 from `base+file_offset`: always convert through
  `/proc/<pid>/maps` `start/end/file_offset`. Reading `base+offset` silently gives a wrong page.
* Run python from this directory, not `/private/tmp` — `/private/tmp/glob.py` shadows the stdlib
  `glob` module and breaks imports with `ModuleNotFoundError: No module named 'lldb'`.
* `il2cpp_runtime_invoke` returns a **pointer to** the return value (read bool with `readU8()`), and
  passing NULL args makes the callee dereference 0 — that, not threading, was the cause of the early
  `access violation accessing 0x0`.
* `NativePointer 0x0` is truthy in frida: use `.isNull()`.
* Read a returned `Il2CppString` at `+0x10` (u32 length) / `+0x14` (UTF-16); build one with `il2cpp_string_new`.
* frida JS has no `btoa`; pass `null` (not `{}`) as the data argument of `send()`.
* `pidof` is missing in the guest shell — use `ps -A | grep bitlife`.

## 8. Known unknowns

1. ShadowHook trampoline layout — the 59 branch targets decode into read-only data / segment gaps.
2. Two age-13 BinaryFormatter saves (`female0-age13`, `male1-age13`) hit `RecursionError`.
3. `MonetizationVars` / `MissionVars` on-disk encoding (`ObfuscateString`/XOR) is undecoded.
4. `libmenu.so`'s remaining encrypted strings (946 of ~1958 `.rodata` candidates); no offset table is
   stored anywhere, confirming name-based runtime resolution.
5. Unresolved scenario placeholders: `<rb>`, `<musician-indefinite>`, `<circumstance-public>`.
6. Art/audio not exported (8133 Texture2D, 8094 Sprite) — no sprites, no audio tables parsed.
7. End-to-end proof that pressing *Free In-App Purchase* flips a real entitlement was never obtained:
   the grant methods throw managed exceptions outside a real store callback, and the entitlement store
   is not in SharedPreferences. The mod's semantics rest on the 59 hook sites + the recovered method-name
   table + the LGL template, which agree with each other.

## 9. Reading order

1. `REPORT.en.md` §1–§9 — static analysis (packaging, PairIP/killsign, native libs, assets, telemetry).
2. `REPORT.en.md` §10–§16 — dynamic analysis (MuMu, frida, the mod menu, the 59 hooks, the content DB).
3. `REPORT.en.md` §17 + `port/README.md` — the C++ port.
4. `gamedata_CATALOG.md` — the content database, grouped.
