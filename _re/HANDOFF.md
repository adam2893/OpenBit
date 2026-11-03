# BitLife port — handoff brief (continue in a new session)

Paste this into a fresh session:

> Read `_re/HANDOFF.md` and continue the BitLife port from §400. Standing objective:
> "go and keep going until its 100% done, all dlc/content packs/iap etc etc". Keep the
> todo list visible; commit and push each verified, documented section.

## Standing objective and rhythm

- **Goal (user, m31924):** "go and keep going until its 100% done, all dlc/content packs/iap etc etc".
- **Crash rhythm (user, m41359):** "dsh crashed, dsh crashed, analyse where you were and create and continue the todo" — on any restart, rebuild the todo list from this file and continue.
- **Method:** one verified + documented `## NNN.` section in `_re/REPORT.en.md`, tests green first, ceilings written down as `shortcut:`/REPORT comments, explicit `git add` paths (never `git add -A`), commit + push per section.
- **User complaint to never repeat (m51511):** "ffs stop getting rid of the to-do list" — keep a `todo_write` list and refresh it every unit.

## Environment

| thing | path |
| --- | --- |
| repo | `/Users/Adam/Documents/bitlife` |
| port (C++17, cmake) | `/Users/Adam/Documents/bitlife/_re/port` |
| binary | `/Users/Adam/Documents/bitlife/_re/port/build/blsim` |
| report | `/Users/Adam/Documents/bitlife/_re/REPORT.en.md` |
| decoded tables | `/Users/Adam/Documents/bitlife/_re/gamedata_plain/*-enc` |
| literal dump | `/tmp/qdir/lits.txt` (may be gone after a reboot — regenerate, see below) |
| test scripts | `/Users/Adam/Documents/bitlife/_re/tools/test_*.py` (181 files) |

Regenerate the literal dump and the helper tools' inputs:

```bash
cd /Users/Adam/Documents/bitlife/_re
python3 tools/metadata_strings.py list > /tmp/qdir/lits.txt     # "offset<TAB>#idx<TAB>'text'"
python3 tools/disasm.py <vaddr> <lines> --lits                  # disassembly with literal slots
python3 tools/lit_cell.py 0x5c38e90 0x5c38e98                   # slot -> literal
python3 tools/addr2method.py <vaddr>                            # vaddr -> class::method
```

`il2cpp_dump.txt` sits at `/Users/Adam/Documents/bitlife/_re/il2cpp_dump.txt` (class blocks with fields and method vaddrs).

## Build / test / commit recipes

```bash
# build — ALWAYS check the RC
cd /Users/Adam/Documents/bitlife/_re/port && cmake --build build -j 4; echo "BUILD_RC=$?"

# full suite: clone the runner, hash the binary, launch detached, poll
sed 's/suite399/suite400/g; s/s399_/s400_/g' /tmp/qdir/suite399.sh > /tmp/qdir/suite400.sh
cd /Users/Adam/Documents/bitlife/_re && shasum -a 256 port/build/blsim | awk '{print $1}' > /tmp/qdir/suite400_binary.sha256
nohup bash /tmp/qdir/suite400.sh > /tmp/qdir/suite400.log 2>&1 &

# poll — ONE bash call with an internal loop, timeoutMs <= 590000
f=/tmp/qdir/suite400.txt; i=0
while ! grep -q DONE $f 2>/dev/null && [ $i -lt 56 ]; do sleep 10; i=$((i+1)); done
grep -c '^PASS ' $f; grep '^FAIL ' $f; tail -1 $f

# commit
cd /Users/Adam/Documents/bitlife
git add _re/REPORT.en.md <the code and test paths>
git commit -F /tmp/qdir/msg400.txt -q && git push -q origin main
```

If `/tmp/qdir/suite399.sh` is gone, rebuild the runner shape: it writes `PASS <name>` / `FAIL <name>` lines into `/tmp/qdir/suiteNNN.txt`, per-test output into `/tmp/qdir/sNNN_<name>.out`, and ends with `PASS n FAIL m DONE 1`; it runs every `tools/test_*.py`.

## Hard rules and traps (learned the hard way)

- **Never rebuild or edit sources while a suite runs** — the suite hashes `port/build/blsim` at start, and two harness tests (`test_funeral`, `test_witch_doctor`) compile against the working tree, so mid-suite edits produce phantom failures.
- `_re/log` is a 41 MB untracked regular **file**, not a directory. Leave it, plus the untracked `_re/*_findings.txt`.
- Suite failure policy: **fix test-side only**, never weaken the port; widen seed ranges or skip degenerate seeds, and write the reason beside the assertion.
- `Rng` API (`_re/port/include/bl/table.h:47-56`): `explicit Rng(uint64_t)`, `uint32_t below(uint32_t n)`, `double unit()`, `bool chance(double p)`. No `below(lo,hi)`; use `lo + int(rng.below(hi - lo))`.
- `GetRandomNumber(min,max)` is `System.Random.Next` = `[min,max)`; every written `(0,2)` third arm is dead code (REPORT 23.4).
- New families must draw from their **own** `Rng` stream (`seed ^ 0xA ^ 0xB`), or every existing seeded test shifts.
- `--script <spec.json>`, never `--play` (that takes a data root). Spec keys include `seed, country, gender, ages, cash, only, choices, casino*, zoo*, engaged`; `choices[i]` is the 0-based branch for card `i`, `-1` leaves the card alone; `only` matches the kFamilies 4th field and requires it non-null.
- Events carry `choice`, not `options`; journals are `logLines` (a list, and earlier lines of the same year come first — the card's own line is usually last); no `logs` key, no `deltas.money`.
- kFamilies rows must never be spliced inside another row's initialiser (`excess elements in scalar initializer`).
- Tool-output quirks: backticks inside TS template literals break `run_code` — use quoted heredocs (`cat > f <<'PYEOF' … PYEOF`), `chr(96)`, or `String.fromCharCode(92)` for backslashes; large `read` results get elided ("head and tail preserved") so use ≤48-line windows; a heredoc can print `OK` without persisting, so verify with `sed -n` afterwards.
- `grep` a real anchor line before patching: scripts that write at the end lose **all** edits when one assert fails.

## Where the work stands

- `_re/REPORT.en.md` holds **395 sections**; the newest are §399 at the top.
- Suite: **181 test files**, last full run 180 PASS / 1 FAIL with the failure fixed test-side.
- Last commits: `96d257a` §399 adopted-child naming, `9c1e5d9` §398 legal name change, `5e86f61` §397 divorce identity card, `fa4da59` §395+§396.

## Next unit — §400, the spouse name card

`ScenarioNameChangeSpouse` is the identity family's last member: it tells the hero what the
spouse decided about surnames (and sometimes reports the hero's own change in the same
body).

Recovered so far:

- fields: `+0x170 _heroNewLastName`, `+0x178 _spouseNewLastName`, `+0x180 _narrative`
- ctors `0x73f28b8f04` (`()`), `0x73f28b907c` (`(String)`), `0x73f28b91c8` (`(String,Bool)`)
- `ShouldForceHyphenatedName 0x73f28ba350`, `RefreshScenario 0x73f28ba358`, `DismissSelected 0x73f28bae70`
- literals (slot → text):
  - `0x5c38e78` #69914 `{0} decided to take my last name. {1} is now {2}.`
  - `0x5c38e88` #69752 `{0} also decided to hyphenate {1} last name.`
  - `0x5c38e90` #50418 `Your name is now {0}.`
  - `0x5c38e98` #70763 `{0}Your {1} has decided to keep {2} name, {3}.`
  - `0x5c38ea0` #69913 `{0} decided to keep {1} name.`
  - `0x5c38ea8` #70762 `{0}Your {1} has decided to change {2} name to {3}.`
  - `0x5c38e80` #70761 `{0}Your {1} has also decided to change {2} name to {3}.`
  - buttons/titles: `0x5c38eb0` #46468 `What's my name again?`, `0x5c38eb8` #44059 `True love`, `0x5c38ec0` #28703 `Legally bound`

Unrecovered and needed before shipping: the `Format` argument mapping in
`RefreshScenario 0x73f28ba358` (which sentence is chosen when, and what `{0}`…`{3}` bind
to). The branch logic is a chain of `String::Equals` on the couple's surnames around
`0x73f28ba574`; `SimPerson::PronounStringForType` is called at `0x73f28ba66c`. Do not guess
the wording — either finish the mapping or leave §400 for another unit.

## Queued and blocked

- Blocked: casino `GamblingBoard 0x3573894` (fine = `LastYearGrossRevenue * 1.5`, 3-tier
  payment cascade, FRAUD crime on refuse) and `HouseRulesComplaint 0x3575bfc` — both need a
  house-rules skew menu, and the recovered house-rules card adjusts **minimum bet**, not the
  win rate.
- Blocked: black-market PUSH cards (need auction/museum subsystems); astronomy Noble Prize
  accept body; casino punter dating (#12176) and casino wedding (#10906) need relationship
  and chapel models.
- Open: ~200 menu-only zoo/casino classes; ~25–35 unreferenced tables of 939; the invented
  theft coin in `Sim::zooTheftYear` (still unverified against native `0x29aa4a8`).
- Easier wins first: the 336-candidate scan of full-sentence `today!` bodies is regenerable with
  `grep -nE 'today!|What will you do' /tmp/qdir/lits.txt`; pick cards whose subsystem already
  exists (zoo, casino, garden, fame, astronaut) and whose class block is small.

## Useful reference points in the report

- §23.4 the dead third arm; §347–§351 casino punter suite; §368–§370 gift-shop chain;
  §383–§387 zoo attraction/install cards; §388–§393 zoo staff, university and raise;
  §396–§399 the identity family.
