# tools/ — Phase 1 differential bench

## Acceptance commands (local or CI)

From the repository root, with Python 3.11+ and a POSIX host:

```bash
cmake -S _re/port -B _re/port/build -DCMAKE_BUILD_TYPE=Release
cmake --build _re/port/build -j 4
python3 _re/tools/test_diff.py
python3 _re/tools/test_suite_runner.py
python3 _re/tools/run_suite.py --output /absolute/new/suite-output
```

Use a fresh output directory. Do not edit sources, headers, tools, content, or rebuild during acceptance. The runner executes every tools/test_*.py, stores each exit code and log, persists inputs_before.json before tests, then writes summary.json with counts and before/after SHA-256 hashes. Manifests include root capture Python/JavaScript and JSON backing data, in addition to port sources/headers, Python tooling, decoded plain data, metadata and build artifacts. Exit 0 means all scripts passed with matching endpoint hashes; 1 means script failures; 2 means invalid setup, changed inputs, or unavailable final hashes. SIGINT/SIGTERM exits nonzero and retains partial results. Owned process groups are released after normal and interrupted exits. Windows execution remains separate work.

Integrity scope is endpoint-sha256: this detects persistent changes, not a modify/use/restore operation between snapshots. It does not provide immutable execution or certify native parity. Strict capture ordering, full before/after state, RNG and rewind epoch evidence remain open requirements.

The comparator defaults to diagnostic mode (parity_certified=false). Exit 0 means no divergence within its normalized sets, age window and tolerant stat-drift contract, not behavioral equivalence. Missing reference ages, malformed observations, scenario-count differences and extra/missing normalized logs fail. Verbose mode emits complete JSON-lines: summary first, then detail records.

Use `python3 _re/tools/diff.py GAME PORT --strict-logs` for exact ordered per-age observed text, including duplicates and empty strings. Both traces must carry `log_provenance: "source-array-order-v1"` and each age a string-array `raw_logs`; invalid/legacy inputs exit2, differences exit1, matching observations exit0. New game round projection preserves raw occurrences separately from diagnostic logs, but repeated same-age rounds are ineligible because overlap/disjointness is unknown. New port projection validates original birth/event log arrays. Canonical passthrough never upgrades legacy captures; fresh_trace remains ineligible. The marker is a producer declaration of supplied-array order, not authenticated provenance, capture completeness or native emission order. Strict mode leaves age-window/scenario-count/tolerant-stat behavior unchanged and ALWAYS reports parity_certified=false. Full state, global event order, lossless cursors and native parity remain open.

Compares a life run **inside the original APK** with the same run in the native port
(`../port/build/blsim`). The point is to make every later porting decision falsifiable:
the first divergence is the work item.

## Pipeline

```
drive9.py (adb taps)  ──►  trace_game*.json        ──►  game_trace.py  ──►  GAME.norm.json
blsim --script S.json ──►  stdout (events+logLines) ──►  port_trace.py  ──►  PORT.norm.json
                                                                            │
                                                        diff.py GAME PORT ◄─┘
```

| script | role |
|---|---|
| `trace_common.py` | the one normalization both sides go through: lower-case, digits→`#`, quoted strings and capitalised name runs→`{name}`, other punctuation→space. Compares *what happened*, not how a name or number was spelled. |
| `game_trace.py` | drive rounds → canonical trace. Reads the six UI stats out of the il2cpp backing fields (`<Att_intelligence>k__BackingField` is UI "Smarts"). |
| `port_trace.py` | runs `blsim --script` and folds each event's `deltas` forward so every age has a stat line. |
| `diff.py` | first divergence, in priority order: age coverage → per-age normalized log sets → scenario counts → per-age stats drift. |

## Usage

```bash
cd /Users/Adam/Documents/bitlife/_re/tools
python3 game_trace.py ../trace_game9.json /tmp/game9.norm.json
echo '{"seed":12345,"country":"Finland","gender":"male","ages":42}' > /tmp/script42.json
python3 port_trace.py /tmp/script42.json /tmp/port42.port.json
python3 diff.py /tmp/game9.norm.json /tmp/port42.port.json -v
```

`diff.py` flags: `--stat-tol N` (default 3), `-v` prints every per-age record.

## How to read a result

- `window` is the age span the game trace actually covers; a drive trace is a slice of one life, so only
  overlapping ages are compared.
- `stat_offset` is the port-minus-game difference measured at the **first** compared age, because the two
  runs start from different lives. Every later age is therefore reported as *drift from the aligned start*.
  The first age is uninformative by construction.
- `stream: logs` diverging first means the two runs selected different events — that is the selection
  layer, not a rule bug. `stream: stats` diverging while logs match means the same events were applied
  with different numbers — that is a rule bug.

## Status (2026-10-13)

First divergence on the captured trace (game ages 39–42 vs a 42-year port run, seed 12345):

```
window [39, 42]
stream=logs age=39: game logged 6 line(s) the port did not
  game-only first: "i followed some customers outside and passive aggressively told them that we
                    typically get tipped and asked them if our ..."
  port-only first: "{name} told {name} she is integral to the company s success"
stream=stats age=40: happiness drifts 9 from the aligned start (game 79, port 88)
```

That is the expected shape today: the port has the RNG stream and 5 scenario tables, the game mixes
table-driven sets with script-coded scenarios. A *passing* trace needs the selection layer ported
(§2 of `PORT_PLAN.en.md`), which is what the divergence report now measures.
