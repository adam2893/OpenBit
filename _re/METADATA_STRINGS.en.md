# BitLife global-metadata.dat: string-literal layout and lookup

Target: com.candywriter.bitlife (il2cpp / Unity, arm64). Everything below is for the
metadata file we actually have:

* /Users/Adam/Documents/bitlife/_re/assets/bin/Data/Managed/Metadata/global-metadata.dat
  18,847,828 bytes, magic 0xFAB11BAF, version word 39 (0x27),
  sha256 92da4664ac8dd5e812d0392111a9418d5ca21c57d38e4757925e5c846fc29b91
* /Users/Adam/Documents/bitlife/_re/jadx_res/resources/assets/bin/Data/Managed/Metadata/global-metadata.dat
  byte-identical (same sha256)

Tool: /Users/Adam/Documents/bitlife/_re/tools/metadata_strings.py — pure python3, no
dependencies, commands: info | list | grep | dump.

## 1. Why the older header reading failed

REPORT.en.md §18.4 recorded that this build's header does not match the documented
il2cpp field order, and the earlier script /Users/Adam/Documents/bitlife/_re/meta_strings.py
read 8-byte (uint32 length, uint32 dataIndex) literal entries at the offset held in
header field 0x08 with the text at a separate data offset. That produces garbage.

The file is not opaque. The literal table is a plain uint32 offset array plus one
UTF-8 text blob, and the header does describe it — as a five-field group (offset,
size, count, data offset, data size) at 0x08..0x18, not as the (offset, count, data
offset, data count) pairs the older reader assumed. Nothing in this group is a
length-prefixed entry table, which is exactly why an 8-byte entry reader fails.

## 2. Header fields, verified

| file offset | field | value | check |
|---|---|---|---|
| 0x00 | magic | 0xFAB11BAF | il2cpp magic |
| 0x04 | version | 39 (0x27) | |
| 0x08 | literal index offset | 380 | start of the uint32 offset array |
| 0x0C | literal index bytes | 284892 | == 4 x 71223 == count x 4 |
| 0x10 | literal count | 71223 | |
| 0x14 | literal data offset | 285272 | == 380 + 284892 |
| 0x18 | literal data bytes | 1892233 | == the last value of the offset array |
| 0x1C | literal count - 1 | 71222 | mirrors 0x10 - 1 |

So the offset array occupies [380, 285272) and the literal text blob occupies
[285272, 285272 + 1892233) = [285272, 2177505).

Neighbouring field, easy to confuse with it: 0x20 = 2,177,508 starts the NUL-separated
metadata *name* table (type, assembly and member names). Its first bytes are
"Assembly-CSharp\x00Assembly-CSharp.dll\x00<Module>\x00<>f__AnonymousType0\x603\x00..."
and the region [2177508, 4069741) contains 91,753 NUL bytes. The literal blob contains
only 63 NUL bytes, all inside binary literals, so the two tables are distinguishable by
position and by NUL density.

## 3. How the literal table is located

```
metadata      /Users/Adam/Documents/bitlife/_re/assets/bin/Data/Managed/Metadata/global-metadata.dat (18847828 bytes)
magic         0xFAB11BAF   version 39
  0x08 literal index offset   380
  0x0C literal index bytes    284892
  0x10 literal count          71223
  0x14 literal data offset    285272
  0x18 literal data bytes     1892233
  0x1C literal count - 1      71222
located index 380 .. 285272 (71223 entries), data 285272 .. 2177505
literals      71221 non-empty of 71223 entries (the last entry is the empty sentinel)
utf-8         blob decodes clean
sorted        no, 2 violation(s) at [71205, 71206]
exit=0
```

find_table() in the tool does not merely trust the header: it validates the tuple
(4 x count == index bytes, index offset + index bytes == data offset, offsets
non-decreasing, first offset 0, last offset == data bytes, blob decodes as UTF-8) and,
if the header tuple fails, scans the first 4 KiB at every 4-byte alignment for a
self-consistent tuple of that shape. When neither matches it exits with an error
instead of printing garbage, so a different build fails loudly rather than silently.

Literal addressing:

* literal i = blob[off[i] : off[i+1]], with off[count] := data bytes
* file offset of literal i = 285272 + off[i]
* literals are stored back to back: no length prefix, no NUL terminator, no escaping,
  and a literal that contains NUL bytes (binary data) is stored as-is
* the blob is kept in ascending byte order, because this table is the sorted literal
  set il2cpp binary-searches. Consequence: literal index order is sorted-text order,
  NOT source order, and a phrase is found by grep rather than by walking the index.
  Sortedness holds for 71,219 of 71,221 adjacent pairs; the only two exceptions are
  indices 71205/71206, where the blob stores b'\xf4\x8f\xbf\xbd', b'\xef\xbf\xbd\xee\x80\x80'
  and b'\xee\x80\x80' (unicode-replacement artefacts), so index order is only
  approximate at that one boundary
* 71,221 of the 71,223 entries are non-empty; entry 71222 (offset 1892233) is the empty
  sentinel. 366 literals contain non-ASCII text

## 4. Commands

```
cd /Users/Adam/Documents/bitlife/_re
python3 tools/metadata_strings.py info
python3 tools/metadata_strings.py list --start 0 --limit 20
python3 tools/metadata_strings.py list --limit 20 --raw
python3 tools/metadata_strings.py grep "doctor"
python3 tools/metadata_strings.py grep --regex "started <elementary-school>"
python3 tools/metadata_strings.py grep --regex --ignore-case "vaccinat"
python3 tools/metadata_strings.py grep --count "mailbox"
python3 tools/metadata_strings.py grep --limit 5 "the "
python3 tools/metadata_strings.py dump --index 2306
python3 tools/metadata_strings.py dump --file-offset 1754375
python3 tools/metadata_strings.py dump --data-offset 1469103
# the two copies of the file give identical results
python3 tools/metadata_strings.py --metadata /Users/Adam/Documents/bitlife/_re/jadx_res/resources/assets/bin/Data/Managed/Metadata/global-metadata.dat grep "doctor"
```

grep prints one line per match: literal file offset, literal index in sorted-text
order, byte delta of the match inside that literal, then the literal (python repr, so
newlines appear escaped). grep exits 0 if it printed at least one hit and 1 if it
printed none, so the zero-hit answers below are machine-checkable. Two consecutive runs
produce byte-identical output (hashes in section 7).

## 5. The seven searches

### 5.1 "doctor" — 51 matches in 51 literals

```
290676	#172	+24	'\n\nYou went to the witch doctor, who says you have V. Coli and is offering you '
320603	#1298	+38	" approaches you as you're leaving the doctor's office and asks if you want to become a vampire and escape death."
335434	#1832	+45	' donation.\n\nThey encouraged you to visit the doctor.'
555300	#10269	+8	'A witch doctor offered me {0}.'
637417	#13103	+9	'Become a doctor'
641470	#13242	+27	'Become best friends with a doctor'
859739	#21372	+26	'Get attacked by the witch doctor'
860260	#21391	+36	'Get cured of a disease by the witch doctor'
872677	#21936	+10	'Go to the doctor'
888656	#22720	+15	'Have the witch doctor cure an illness'
914628	#23636	+37	"I bit my {0} when {1} took me to the doctor's office to get vaccinated."
949174	#24548	+86	'I learned how to perform the <move name> after obtaining the knowledge from the witch doctor.'
952500	#24632	+9	"I made a doctor's appointment for my {0}, {1}."
952546	#24633	+9	"I made a doctor's appointment for my {0}."
952587	#24634	+9	"I made a doctor's appointment."
956424	#24761	+33	'I paid a college official {0} to doctor my {1}, {2}, college entrance exam scores.'
964794	#24970	+43	"I remained calm when my {0} took me to the doctor's office to get vaccinated."
974712	#25238	+45	"I threw a tantrum when my {0} took me to the doctor's office to get vaccinated."
1099199	#29755	+8	'Marry a doctor'
1102416	#29894	+12	'Maybe see a doctor instead'
1159354	#32263	+22	'Now my wallet needs a doctor'
1172694	#32761	+16	'On to the witch doctor...'
1350081	#40271	+14	'Study to be a doctor'
1365801	#40944	+16	'Take {0} to the doctor'
1456280	#42693	+10	'The witch doctor appeared and force fed me {0}!'
1456327	#42694	+10	'The witch doctor force fed you {0}.'
1456362	#42695	+10	'The witch doctor has offered you {0}.'
1456399	#42696	+10	'The witch doctor just appeared, grabbed your throat, and started force feeding you {0}.\n\nWhat will you do?'
1555829	#45770	+10	'Visit the doctor'
1555921	#45774	+22	'Visit the homeopathic doctor today!'
1556206	#45785	+16	'Visit the witch doctor'
1576261	#46574	+12	'Which local doctor will you schedule an appointment to see?'
1576320	#46575	+12	'Which local doctor will you take your {0} to see?'
1576369	#46576	+12	'Which local doctor will you take your {0}, {1}, to see?'
1592376	#46858	+10	'Who needs doctors?'
1599021	#47117	+10	'Work as a doctor or surgeon for 15+ years'
1639406	#48120	+31	"You can't take your {0} to the doctor because {1} lives in {2} and you live in {3}."
1639489	#48121	+37	"You can't take your {0}, {1}, to the doctor because {2} lives in {3} and you live in {4}."
1647816	#48267	+23	"You declined the witch doctor's remedy and recovered after spending some time in your coffin."
1716326	#49779	+36	'You were treated by the homeopathic doctor.'
1718222	#49810	+36	'You were unable to resist the witch doctor!'
1754375	#50582	+30	"Your {0} is taking you to the doctor's office to get vaccinated.\n\nHow will you behave?"
1835437	#53118	+18	'a16_vampire-witch-doctor-remedie-enc'
1923329	#57647	+0	'doctor'
1990304	#62102	+4	'man-doctor'
2062602	#67027	+17	'to pay the witch doctor for {0}'
2063798	#67082	+25	'to visit the homeopathic doctor'
2078124	#68122	+6	'woman-doctor'
2103449	#69046	+27	'{0}\nI decided to visit the doctor.'
2148953	#70152	+34	'{0} life was cut short by a witch doctor at the {1}age of {2}.'
2158513	#70394	+28	'{0} was murdered by a witch doctor at the {1}age of {2}.'
exit=0
```

### 5.2 "Elementary School" — 0 matches

```
python3 tools/metadata_strings.py grep "Elementary School"
```

```
exit=1
```

No literal in the table contains that phrase (grep exits 1). The observed
"You are starting elementary school" is therefore not binary text; see section 6, scenario 2.

### 5.3 "vaccinated" — 4 matches

```
python3 tools/metadata_strings.py grep "vaccinated"
```

```
914628	#23636	+60	"I bit my {0} when {1} took me to the doctor's office to get vaccinated."
964794	#24970	+66	"I remained calm when my {0} took me to the doctor's office to get vaccinated."
974712	#25238	+68	"I threw a tantrum when my {0} took me to the doctor's office to get vaccinated."
1754375	#50582	+53	"Your {0} is taking you to the doctor's office to get vaccinated.\n\nHow will you behave?"
exit=0
```

All four are game text, not engine noise: three are the outcome log lines of the
vaccination scenario and one is its prompt. File offsets 914628, 964794, 974712, 1754375.

### 5.4 "mailbox" — 2 matches

```
python3 tools/metadata_strings.py grep "mailbox"
```

```
1989612	#62054	+0	'mailbox'
2004927	#63259	+5	'open-mailbox'
exit=0
```

Both hits are UI / asset keys with no surrounding sentence (the literal IS the token
"mailbox" / "open-mailbox"), so they are not scenario prose.

### 5.5 "broke your parents" — 0 matches

```
python3 tools/metadata_strings.py grep "broke your parents"
```

```
exit=1
```

The phrase is not in the table. The wider "broke your" search has only two unrelated
literals:

```
571349	#10855	+0	'Admit that I broke it'
903936	#23408	+0	'I admitted to my {0} that I broke {1} {2}.'
915185	#23652	+0	'I broke my {0} {1} and then lied about it.'
997913	#25736	+66	'I was {0} from my job as {1} for {2} after it was discovered that I broke my vow of celibacy.'
1474173	#43196	+11	'This catch broke your previous record for the '
1740566	#50294	+58	'Your communard, {0}, told some of your followers that you broke your own rule!\n\nYou lost {1} followers.'
2118470	#69401	+34	'{0}\n{1} told some of my followers I broke my own rule! I lost {2} followers.'
exit=0
```

so ScenarioEthicsBreakSomething's rendered sentence is not a binary literal (section 6,
scenario 3).

### 5.6 "Canyon Neuron" — 0 matches

```
python3 tools/metadata_strings.py grep "Canyon Neuron"
```

```
exit=1
```

The bike model from the observed run is not in the literal table, and it is also not in
the parsed tables ("Canyon Neuron" has 0 occurrences in /Users/Adam/Documents/bitlife/_re/gamedata_plain/
and /Users/Adam/Documents/bitlife/_re/gamedata/). It does exist as *rendered* text in
/Users/Adam/Documents/bitlife/_re/trace_fresh.json and is quoted in
tools/row_recover.py's docstring, and the "Canyon" brand appears in the data packs
(e.g. /Users/Adam/Documents/bitlife/_re/gamedata_plain/a16_outdoor-vehicle-enc, plus
street names such as "Tapo Canyon Rd" in a16_street-names-enc). Conclusion: the bike
name is a data-table value (SimRecreationalVehicle row), not a metadata literal.

### 5.7 "have decided you" — 1 match, a fragment

```
python3 tools/metadata_strings.py grep "have decided you"
```

```
349484	#2306	+1	" have decided you're old enough to have a bicycle and want to buy you a "
exit=0
```

The literal exists but is a concatenation fragment: it starts with a space and ends with
"a ", so the game builds the sentence from this piece plus a parent string and a
recreational-vehicle name at runtime (section 6, scenario 4).

```
literal      #2306  (index 2306)
file offset  349484 .. 349556
blob offset  64212 .. 64284
bytes        72
text         " have decided you're old enough to have a bicycle and want to buy you a "
exit=0
```

The same literal dumped by file offset (dump --file-offset 1754375 gives the
vaccination prompt):

```
literal      #50582  (index 50582)
file offset  1754375 .. 1754461
blob offset  1469103 .. 1469189
bytes        86
text         "Your {0} is taking you to the doctor's office to get vaccinated.\n\nHow will you behave?"
exit=0
```

## 6. The four scenarios: what is recoverable from the literal table

Evidence for the class shapes: /Users/Adam/Documents/bitlife/_re/il2cpp_dump.txt
(il2cpp class/method dump that matches the binary's addresses).

| scenario | il2cpp_dump.txt | recoverable here | why |
|---|---|---|---|
| ScenarioChildhoodVaccination | line 35543 | YES, complete | prompt and all three outcome lines are plain literals |
| ScenarioSchoolEnroll | line 50305 | NO (templates only) | class holds only SimOccupationStudent _student; no narrative string field |
| ScenarioEthicsBreakSomething | line 38364 | NO (outcomes only) | prompt assembled from DataChildBreakItem _brokenItem + _parentsString + _parentsPronoun |
| ScenarioOutdoorAgeUpParentsGiveBike | line 46283 | PARTIAL | narrative is literal fragments + SimRecreationalVehicle _bike + _parentsString |

### 6.1 ScenarioChildhoodVaccination — recoverable, completely

Prompt, literal #50582 at file offset 1754375:

```
"Your {0} is taking you to the doctor's office to get vaccinated.\n\nHow will you behave?"
```

The three choice outcomes are literals too:

| literal file offset | index | text |
|---|---|---|
| 914628 | #23636 | "I bit my {0} when {1} took me to the doctor's office to get vaccinated." |
| 964794 | #24970 | "I remained calm when my {0} took me to the doctor's office to get vaccinated." |
| 974712 | #25238 | "I threw a tantrum when my {0} took me to the doctor's office to get vaccinated." |

The {0}/{1} slots take sim parents' names, so the emulator needs the parents' name
strings (also literals, but those are per-sim values, not scenario text), and nothing
else from the binary.

### 6.2 ScenarioSchoolEnroll — not recoverable; templates only

You are starting elementary school" and "Elementary School" both have 0 matches in the
table. The class (il2cpp_dump.txt:50305) has a single field, the student occupation
handle, plus .ctor/RefreshScenario/DismissSelected/EmojiString, so the displayed
sentence is assembled from the student/school data at runtime. Only these templates
touch school enrolment text:

```
349484	#2306	+42	" have decided you're old enough to have a bicycle and want to buy you a "
359806	#2611	+41	' is refusing to buy you a more expensive bicycle.\n\nHow will you react?'
372134	#3159	+22	' refused to buy you a bicycle.'
373080	#3187	+38	' refusing to buy you a more expensive bicycle.\n\nHow will you react?'
385921	#3634	+13	' to buy me a bicycle and she offered to buy me a '
385970	#3635	+13	' to buy me a bicycle, but '
385996	#3636	+28	' to buy me a more expensive bicycle and '
386036	#3637	+28	' to buy me a more expensive bicycle, but '
386150	#3640	+14	' to buy you a bicycle.'
494168	#8613	+0	'<bicycle>'
508450	#8917	+0	'<elementary-school>'
520466	#9330	+0	'<parent-possessive>'
524044	#9404	+17	'<person> started <elementary-school>.'
616891	#12207	+14	'Ask {0} for a bicycle'
1182268	#33138	+6	'Own a bicycle'
1318600	#38902	+9	'Shop for bicycles'
1866205	#54926	+0	'bicycle'
1939605	#58471	+12	'et-Kuwahara-bicycle'
2030479	#64963	+23	'scavenger-hunt-america-bicycle'
2060172	#66930	+9	'to buy a bicycle'
exit=0
```

i.e. the log-line template "<person> started <elementary-school>." plus the token
"<elementary-school>" that the runtime substitutes, and the class-name literal
"ScenarioSchoolEnrollActivity" (#37953 at 1294584). The port must either take the
rendered string from a game trace or reimplement the composition.

### 6.3 ScenarioEthicsBreakSomething — not recoverable; outcomes only

Fields (il2cpp_dump.txt:38364): DataChildBreakItem _brokenItem, System.String
_parentsString, System.String _parentsPronoun. No literal holds the rendered prompt
("You broke your parents" = 0 matches, and "broke your" only matches two unrelated
lines, see 5.5). What the table does give:

```
571349	#10855	+0	'Admit that I broke it'
903936	#23408	+0	'I admitted to my {0} that I broke {1} {2}.'
915185	#23652	+0	'I broke my {0} {1} and then lied about it.'
997913	#25736	+66	'I was {0} from my job as {1} for {2} after it was discovered that I broke my vow of celibacy.'
1474173	#43196	+11	'This catch broke your previous record for the '
1740566	#50294	+58	'Your communard, {0}, told some of your followers that you broke your own rule!\n\nYou lost {1} followers.'
2118470	#69401	+34	'{0}\n{1} told some of my followers I broke my own rule! I lost {2} followers.'
exit=0
```

Those are the choice label and the two outcome lines; the prompt sentence itself is
built from the DataChildBreakItem row plus _parentsString, so it has to come from a
trace or from reimplementing that composition.

### 6.4 ScenarioOutdoorAgeUpParentsGiveBike — partially recoverable

Fields (il2cpp_dump.txt:46283): System.String _narrative, SimRecreationalVehicle _bike,
System.String _parentsString. The narrative is a concatenation of literal fragments, and
the seams are visible as leading/trailing spaces:

| literal file offset | index | literal (fragment) |
|---|---|---|
| 349484 | #2306 | " have decided you're old enough to have a bicycle and want to buy you a " |
| 372134 | #3159 | " refused to buy you a bicycle." |
| 359806 | #2611 | " is refusing to buy you a more expensive bicycle.\n\nHow will you react?" |
| 385921 | #3634 | " to buy me a bicycle and she offered to buy me a " |
| 385970 | #3635 | " to buy me a bicycle, but " |
| 385996 | #3636 | " to buy me a more expensive bicycle and " |
| 386036 | #3637 | " to buy me a more expensive bicycle, but " |
| 386150 | #3640 | " to buy you a bicycle." |
| 494168 | #8613 | "<bicycle>" |
| 616891 | #12207 | "Ask {0} for a bicycle" |

So the port gets every fixed word of the scenario (the sentences, the choices) but not
the ordering or the substituted values: the parent string and the bike model
("Canyon Neuron 5" in the observed run) are table rows, and the concatenation order
lives in libil2cpp.so.

## 7. Reproducibility

Exact run (from /Users/Adam/Documents/bitlife/_re), twice, plus the file hashes:

```
python3 tools/metadata_strings.py grep "doctor" | shasum -a 256
python3 tools/metadata_strings.py grep "doctor" | shasum -a 256
shasum -a 256 assets/bin/Data/Managed/Metadata/global-metadata.dat jadx_res/resources/assets/bin/Data/Managed/Metadata/global-metadata.dat

f4cb20ad80d95f134332fd9e6c09dd0320453b913c83d5d5bd954627032c4220  -
f4cb20ad80d95f134332fd9e6c09dd0320453b913c83d5d5bd954627032c4220  -
92da4664ac8dd5e812d0392111a9418d5ca21c57d38e4757925e5c846fc29b91  assets/bin/Data/Managed/Metadata/global-metadata.dat
92da4664ac8dd5e812d0392111a9418d5ca21c57d38e4757925e5c846fc29b91  jadx_res/resources/assets/bin/Data/Managed/Metadata/global-metadata.dat
```

Both runs give the same digest, so the offsets are deterministic; the two copies of
global-metadata.dat are identical, so either path can be used.

## 8. What this does not give you

* Concatenation order and slot filler choice for the assembled scenarios: that logic is
  compiled into /Users/Adam/Documents/bitlife/_re/lib/arm64-v8a/libil2cpp.so
  (sha256 2514c4d8bfcb2bc824767c0b98d026242bf914c881537a427a18d62d83579ec1);
  /Users/Adam/Documents/bitlife/_re/il2cpp_dump.txt has the method addresses
  (ScenarioOutdoorAgeUpParentsGiveBike.RefreshScenario @0x73f28d07d4,
  ScenarioEthicsBreakSomething.RefreshScenario @0x73f3bc84d4,
  ScenarioSchoolEnroll.RefreshScenario @0x73f2a556a8,
  ScenarioChildhoodVaccination.RefreshScenario @0x73f3a7d000).
* Table values behind those slots (SimRecreationalVehicle, DataChildBreakItem,
  school/student rows) — those are data-pack rows, not binary strings.
* Literals are the sorted dedup set: the same text appears once, so a literal's index
  says nothing about where in the game it is used. Use grep plus the game traces for
  usage.
