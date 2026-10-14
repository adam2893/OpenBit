# BitLife 3.25.1 — reverse-engineering notes and a native C++ recompilation

Static + dynamic analysis of the Android build of *BitLife Life Simulator* 3.25.1
(`com.candywriter.bitlife`, Unity IL2CPP arm64-v8a), and a from-scratch C++17 port of its
life-sim engine — data layer, age-up loop, scenario draw, text engine — that replays the
game's own output under a differential bench.

**Bring your own copy.** No APK, no `.so`, no unpacked game data is committed here. The
extraction recipes are in [`_re/README.en.md`](_re/README.en.md) (dex decrypt → gamedata
tables, IL2CPP dump, metadata literals); every excluded path is listed in
[`.gitignore`](.gitignore) and regenerates from the APK in the same revision.

## Where the answers are

| file | what it holds |
|---|---|
| [`_re/REPORT.en.md`](_re/REPORT.en.md) | the whole investigation, sections 1–38 |
| [`_re/README.en.md`](_re/README.en.md) | workspace layout, artifact inventory, every capture command |
| [`_re/PORT_PLAN.en.md`](_re/PORT_PLAN.en.md) | the port's design and phasing |
| [`_re/METADATA_STRINGS.en.md`](_re/METADATA_STRINGS.en.md) | `global-metadata.dat` literal table (71223 strings) |
| [`_re/port/README.md`](_re/port/README.md) | how to build and run the port |
| [`_re/circumstance_pools.txt`](_re/circumstance_pools.txt) | live-sampled `GetRandomCircumstance*` pools |

Report sections that carry the port-relevant mechanics: §27 the yearly scenario is a
6-way roulette, §28 a year is several lines, §29–30 the BracketParser text engine, §32
impact cells are scores, §33 `<rb>` scenario-local draw cache, §34 the token vocabulary,
§35 the death sentence, §36–37 per-person lifecycle lines and the family model behind
them, §38 the circumstance pools.

## Coverage, measured

`tools/coverage.py` over every captured device trace:

```
captured scenarios: 90  recoverable: 21     -> 23%
```

Loop engine, RNG parity, text engine and per-person lifecycle are done; 12 of 1016 data
tables are wired (the auto-drive pools the traces exercise), where all residue tokens
resolve to 0. Player-driven menus are not implemented — the port is auto-drive only.

## Build

```
cd _re/port
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
./build/blsim                 # text mode
./build/blsim --script FILE   # JSON mode for the differential bench
```

Differential bench (needs a captured trace; `_re/trace_game*.json` are committed):

```
cd _re && bash tools/bench.sh /tmp/trace_c10.json
```

## Next work

Phase 2 hero-state branches (prison, royal, vampire, orphanhood, military, occupation,
see `REPORT.en.md` §31.3), the film-director `genesis_*` tables, and the per-scenario
recovery grind that moves the 23% above.
