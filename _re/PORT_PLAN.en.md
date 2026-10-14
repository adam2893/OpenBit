# Plan: from this RE workspace to a native C++ BitLife

Companion to `README.en.md` (what exists) and `port/README.md` (what is built).
All numbers below are measured from this workspace, not estimated.

## 0. Definition of done (pick this, or the project never ends)

"Fully decompiled C++ BitLife" must not mean *every method translated* — that is 62,736 methods in
`Assembly-CSharp.dll` alone, most of them data plumbing and Unity glue, and a literal 1:1 translation
of Unity IL2CPP code produces C++ that still needs Unity's object model, renderer, and uGUI.

Define done as **behavioural coverage**, measured by a differential harness (§4):

> A native C++ build in which a scripted life run (seed + fixed choices) produces the same observable
> state and the same log entries as the same scripted run in the original APK — for the systems listed
> in the coverage ledger, with expansions enabled.

That is objective, testable per-system, and it is the only definition under which work can stop.

## 1. Scale (measured)

| item | count |
|---|---|
| `Assembly-CSharp.dll` classes / methods | 7,867 / 62,736 |
| classes with >=50 methods | 131 (16,241 methods) |
| classes with >=20 methods | 568 (29,438 methods) |
| `Sim*` system classes (SimPerson, SimZoo, SimLover, SimVampire, SimCasino, SimBusiness, …) | 353 classes / 10,554 methods |
| `Game.*` / `BitLife.*` / `Life` / `DataHub` family | 1,263 classes / 14,790 methods |
| engine/BCL/other assemblies (do **not** port; replace) | ~123 assemblies / ~88,000 methods |
| decrypted content tables already in hand | 1071 tables / 13,315,430 B |
| Unity assets still unexported | 8133 Texture2D, 8094 Sprite, 327 Canvas (`datapack.unity3d`) |

The port target is the `Sim*` + `Game.*`/`BitLife.*`/`Life`/`DataHub` intersection, roughly
**20,000–25,000 methods**, and inside that a much smaller set does actual simulation work.

`coverage_classes.csv` (this directory) is the ledger: `methods,fields,class,base,first_addr` for all
7,867 game classes, sorted by method count. Every row is a work item with a known IL2CPP address.

## 2. Three routes, and the one to take

| route | what it is | verdict |
|---|---|---|
| **A. Mechanical translation** | Il2CppDumper → Ghidra headless → decompile all methods → rewrite each into C++ | produces correct-looking code for 60k+ methods, most of it dead weight; Unity types must be re-invented anyway; no verification path. Use it as a **reference corpus**, not as the deliverable |
| **B. Clean-room reimplementation** | write systems from the tables + field layouts, no code reading | fast, shippable, but numbers drift from the original (fairness, balance, RNG order) and each divergence is invisible until someone compares |
| **C. Hybrid (recommended)** | implement in C++, but decompile the *specific* methods that own the rules, and verify each system numerically against the running original | bounded scope, evidence per system, and the verification tool already exists (§4) |

Route C: the decompiler output is the **specification** for the methods that matter; everything else stays
table-driven; every ported system ships with a passing differential test.

## 3. Phases

**Phase 0 — tooling + ledger (1–2 days)**
- `Il2CppDumper` on `lib/arm64-v8a/libil2cpp.so` + `global-metadata.dat` → `dump.cs` (real names,
  offsets, signatures) and `script.json` (address→name map for Ghidra).
- Ghidra headless (or the REA Ghidra provider) with the Il2CppDumper script → named symbols inside
  libil2cpp, so the 20k target methods decompile with real class/method names instead of `FUN_xxx`.
- Extend `coverage_classes.csv` into a work board: one row per target method with `class, method,
  addr, ported(bool), verified(bool), test id`.
- Deliverable: `tools/` scripts that regenerate this from scratch.

**Phase 1 — differential harness (2–3 days)** *the instrument that makes the rest verifiable*
- Drive: `adb` taps + a frida agent that exposes the game's real state as JSON
  (`Game.GameVars`, hero `SimPerson`, `Life.lifeLog`, `Finances`, `MonetizationVars` via the same
  `il2cpp_runtime_invoke` path already proven for `EncryptionManager`).
- Script format: `{seed, actions:[...], expect:[stateAfterEachAge, logLinesDiff]}`.
- Port side: `blsim --script` emits the same JSON.
- `tools/diff.py` compares the two traces and prints the first divergence.
- Deliverable: one passing end-to-end trace for "new life → 3 age-ups" (see §5, step 1).

**Phase 2 — data + state layer (1 week)**
- `DataHub` (2365 methods) is the table accessor layer — port its accessor surface, not its body count.
- Port `Game.GameVars` (364) and `Game.MonetizationVars` (304) fully: every field already enumerated with
  offsets in `il2cpp_dump.txt`, and the entitlement catalogue already exists in `port/src/game.cpp`.
- Serialization: match the game's own save format (`MonetizationVars`, `MissionVars`, `saveslotarray`) or
  define a documented new one + importer. **[unk]** the on-disk obfuscation is still undecoded.
- Done when a life created in the original loads in the port and ages identically.

**Phase 3 — simulation systems, one at a time (the bulk; weeks)**
Order by player-visible weight, each with its differential test:
1. `Life` (755 methods) + `SimPerson` (325) + `SimPlace` (221) — aging, stats, relationships, death.
2. Scenarios/choices: `genesis_master_tags_reference_2` tag engine + `dumb-scenarios-*` (the CSV tables
   are already the rule source; the C# side is mostly selection + impact application).
3. Money/careers/education/property: `Finances`, `Occupation`, `_Portfolio`, landlord.
4. Expansions in the order the user cares about (all content tables are already decrypted):
   vampire, cult, casino, zoo, racing, landlord, black market, secret agent, astronaut, actor/movie
   director, business, dealer, model, music, sports, mafia, hustler, pet, outdoor, royal, school.
5. `Apollo16ChallengeTracker` (1137) — challenges/achievements.
- Rule: a system is "ported" only when its differential test passes; otherwise it stays in a
  `not-ported` list that ships with the build.

**Phase 4 — content and presentation (2–4 weeks)**
- `UnityPy` export of Texture2D/Sprite/AudioClip → PNG/atlas/OGG + a manifest.
- UI: 327 Canvas hierarchies in `datapack.unity3d` are uGUI prefabs; decide per screen: rebuild in a
  C++ toolkit (SDL3 + Dear ImGui, or a retained-mode custom canvas) using the exported art and the
  measured geometry (`MENU_WIDTH 290 / HEIGHT 210 / CORNER 35.0f / ICON_SIZE 45 / accent #7C4DFF`).
  Rebuilding beats interpreting uGUI prefabs.
- Text: the game's tables carry its own copy; no localization system needed beyond the existing tables.

**Phase 5 — platform packaging (days)**
- Build per target (macOS/iOS/Windows/Android/Linux); saves in a portable format; entitlement flags
  default on (all expansions unlocked), no store, no ads.

**Explicitly dropped** (this is most of the 123 other assemblies): ads and mediation (inmobi 1769,
applovin 742, unity ads, vungle, moloco, …), analytics/attribution (AppsFlyer, Flurry, GameAnalytics,
Firebase), crash reporting, IAB OMID verification, didomi consent, the bamen telemetry provider, AWS
SDK, PairIP, killsign, the 9ADB gate, the LGL mod menu, and Unity itself.

## 4. Why route C is credible: the verification loop

Everything needed for it is already proven in this session:

1. root adb on MuMu (`127.0.0.1:16384`) + frida 16.7.19 + frida-server → attach to the live process.
2. il2cpp API reachable from frida (domain/assemblies/classes/methods, `runtime_invoke`,
   `il2cpp_string_new`), used to call `EncryptionManager.DecodeAndDecryptString` successfully.
3. Game state readable/writable in-process; `il2cpp_dump.txt` gives every field offset.
4. Scripted input via `adb shell input tap`, screenshots via `screencap`.
5. Tables already decrypted to plaintext, so expected content is known without the game.

So each system port is: run both, diff, fix, repeat — with the original as the oracle. No guessing.

## 5. Immediate next actions (in order, first one is small)

1. **Prove the harness end to end**: one trace — spawn the game, hook the gate, create a male life in
   Finland, age up 3 times, dump `{age, stats, money, first 5 life-log lines}`; make `port/blsim`
   emit the same shape for the same seed; diff. This validates the whole approach in a day.
2. **Phase 0 tooling**: Il2CppDumper + Ghidra naming pass, and generate the method-level work board.
3. **Asset export**: UnityPy pull of sprites/audio + the Canvas list, so the UI work can be scoped.

## 6. Effort, honestly

| phase | calendar estimate (with agents doing the mechanical parts) |
|---|---|
| 0 tooling | 1–2 days |
| 1 harness | 2–3 days |
| 2 data/state | ~1 week |
| 3 systems (20k methods, ~350 Sim classes) | 4–10 weeks, gated by differential tests |
| 4 assets + UI | 2–4 weeks |
| 5 packaging | days |

A playable, behaviourally faithful C++ BitLife with all expansions is a **~2–3 month** effort at agent
speed; pixel-identical UI is the long pole and is optional. What is *not* optional is the harness in
Phase 1 — without it "fully decompiled" is unmeasurable.
