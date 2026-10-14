"""Recover the real (table, row) behind each game-side scenario.

The game's Scenario object carries no table/row identity (its fields are UI and
choice data only, verified in REPORT §20), but it does carry the rendered
Title/Body. Matching those against the real tables recovers the row, and that is
what blsim --script needs in its "push" array to replay the game's own pick.

Matching is deliberately strict, because the bench normalization (lower-case +
digits/names -> placeholders) collapses titles such as "Area 69" and
"Canyon Neuron 5" onto the same key. Here titles are compared as exact
lower-cased word/digit sequences, and candidates are ranked by how many long
words the table body shares with the rendered body -- a table body keeps
'<parent>' where the game wrote a real word, so placeholders are dropped rather
than compared.
"""
import argparse, csv, json, os, re, sys

RE = "/Users/Adam/Documents/bitlife/_re"


def nkey(s):
    s = (s or "").lower()
    s = re.sub(r"[^a-z0-9]+", " ", s)
    return re.sub(r"s+", " ", s).strip()


def words(s):
    return [w for w in nkey(re.sub(r"<[^>]*>", " ", s or "")).split() if len(w) >= 4]


# Scripted-event scenarios the port renders from its own registry instead of a
# table row (port/src/sim.cpp kScenarios). The log templates are the same
# literals; they are what identifies which button the game's own log line came
# from, so the port can be told the exact branch index.
REGISTRY = {
    "ScenarioChildhoodVaccination": [
        "I threw a tantrum when my {parent} took me to the doctor's office to get vaccinated.",
        "I bit my {parent} when {parent} took me to the doctor's office to get vaccinated.",
        "I remained calm when my {parent} took me to the doctor's office to get vaccinated.",
    ],
    "ScenarioEthicsBreakSomething": [
        "I admitted to my parents that I broke their {item}.",
        "I broke my parents's {item} and then lied about it.",
    ],
    "ScenarioSchoolEnroll": [],
    "ScenarioFriendMeet": ["I became friends with {name}.", ""],
    "ScenarioRetentionCrime": ["", ""],
    "ScenarioSchoolClassmateActUp": ["", "", "", ""],
    "ScenarioSocialMediaJoinComplete": ["I signed up for {channel} account."],
    # Auto-advancing scenes the port renders from kScenarios: the game writes the
    # sentence straight into its LifeLog, so the template below is only used to
    # identify the scene's own log line, never to branch on a button.
    "ScenarioScriptEventSiblingBorn": [
        "My mother and my father had a baby boy named {name}, my new brother.",
        "My mother and my father had a baby girl named {name}, my new sister.",
    ],
    "ScenarioPetDeath": ["My {animal}, {name}, died while sleeping peacefully."],
    "ScenarioScriptEventDiseaseContract": [
        "My mother has been diagnosed with {disease}.",
        "My mother is suffering from {disease}.",
        "My mother is no longer suffering from {disease}.",
    ],
    "ScenarioFriendNew": ["I am now friends with {name}."],
    "ScenarioScriptEventParentsBreakUp": ["My parents have divorced."],
    "ScenarioScriptEventParentMarriesStepparent": [
        "My mother has married my new stepfather, {name}.",
    ],
    # Menu scenes ("a16_stepparent-cruelty-scenarios-enc"-style cards) hold only
    # the card; their option labels and outcome lines are C# literals. These
    # templates are the outcome lines recovered by pairing each captured card
    # with the Scenario record that follows it in the trace (REPORT.en.md 41).
    "ScenarioLoveFirstKiss": [
        "I had my first kiss with {name}. I made sure to not slip {partnerobj} any tongue.",
        "I tongue-kissed {name}. It was my first kiss.",
        "I could have had my first kiss with {name} but instead I ran away from {partnerobj}.",
    ],
    "ScenarioOfferFood": ["I started eating {food}.", ""],
    "ScenarioOpportunityCasino": ["I went with some friends to {casino}.", ""],
    "ScenarioCustomAttack": ["I {attackverb} my {victim}'s {attackpart}!"],
    "ScenarioEthicsFindObject": ["I turned in {findarticle} that I found.", ""],
    # Casino minigames: only the outcome card writes a line, the bet and play
    # cards are silent (their branch index therefore never matters).
    "ScenarioCasinoOutcome": ["I {casinoverb} {casinoamount} {casinotail} at {casino}."],
    "ScenarioCasinoBet": [""],
    "ScenarioGameBlackjack": [""],
    "ScenarioFailingEyesight": [""],
    "ScenarioGameEyeExam": [""],
    "ScenarioProfileHero": [""],
    "ScenarioEyeExamOutcome": ["I {eyeexamverb} my eye exam!"],
    # ---- The illness chain and the other menu classes recovered from the
    # trace_game10 captures (REPORT.en.md 43). The log templates are the port's
    # own branch lines in port/src/sim.cpp kScenarios, so a match also tells the
    # port which button the game tapped. Classes the port does not render yet are
    # listed too: identifying them is what makes the coverage number honest, and
    # their templates come from the captured card + the metadata literals.
    "ScenarioDoctors": ["I made a doctor's appointment.",
                        "I made a doctor's appointment.", ""],
    "ScenarioDoctorTreatments": [
        "Dr. {doctor} diagnosed me with {diagnosis}. Dr. {doctor} treated my {diagnosis}. "
        "I continue to suffer from {diagnosis}.",
        "Dr. {doctor} diagnosed me with {diagnosis}. I decided not to get treated.",
    ],
    "ScenarioJobTermination": [
        "I was let go from my job as {job} for {company}. I called my supervisor a {insult} "
        "on my way out.",
        "I was let go from my job as {job} for {company}. I wished my supervisor well on my "
        "way out.",
        "I was let go from my job as {job} for {company}. I begged my supervisor to let me stay.",
    ],
    "ScenarioInsultRelative": ["I forgave {stranger}.",
                               "I insulted {stranger} right back.",
                               "I attacked {stranger}."],
    "ScenarioCasinoGoldenNekoAd": [
        "I considered purchasing a Golden Neko Statue to increase my casino winnings.",
        "I ignored an opportunity to purchase a Golden Neko Statue to increase my casino "
        "winnings.",
    ],
    # Captured cards whose port renderer is still missing (kScenarios has no entry
    # yet): a push for them identifies the scene, the port skips it.
    "ScenarioPetDiscipline": ["My {animal}, {name}, ate my {food}.", ""],
    "ScenarioPushSpendTime": ["I went with my {parent} to {activity}.", ""],
    "ScenarioStepparentCruelty": ["My stepfather {cruelty}. I told my mother about his behavior.",
                                  "My stepfather {cruelty}."],
    "ScenarioFriendUnfriendInbound": [
        "{name} unfriended me. I tried to salvage our relationship but was unsuccessful.",
        "{name} unfriended me. I called him mentally defective.",
        "I unfriended {name}.",
    ],
    "ScenarioLoverLeaves": ["{name} broke up with me. I begged her to stay and she changed her mind.",
                            "{name} broke up with me."],
    "ScenarioLoveDate": ["I am now dating {name}."],
    "ScenarioSchoolBully": ["I reported the bully to my teacher.",
                            "I was bullied at school by my classmate, {name}."],
    "ScenarioSchoolActivityJoin": ["I tried out for my {activity} but did not make the cut.",
                                   "I turned down the opportunity to join the {activity}.",
                                   "I thought about joining an extracurricular activity but decided "
                                   "against it."],
    "ScenarioEncounterProstitute": ["I solicited {name} that I met on the street.",
                                    "I ignored a working girl who was sitting on the curb."],
    "ScenarioMediaOnlyFansArguePush": ["My girlfriend argued with me because I create content for "
                                       "OnlyFans. I apologized to her."],
    "ScenarioSocialMediaJoinPush": ["I signed up for an OnlyFans account.", ""],
    "ScenarioCarRepair": ["I paid a shifty mechanic {amount}.", ""],
    "ScenarioIAPConfiguredPush": ["I chose not to learn about becoming a movie director.", ""],
    "ScenarioRetentionFreelanceGig": ["I decided against picking up a freelance gig.", ""],
    "ScenarioRetentionLover": ["", ""],
    "ScenarioBase": ["I got my shot to direct a film and chose to make {genre} movie.", ""],
    "ScenarioSchoolEnrollActivity": [
        "I thought about joining an extracurricular activity at my new high school but decided "
        "against it.",
        "I joined the {club} at my high school.",
    ],
}


def log_words(s):
    """Tokens of a log sentence, with the game's fill points dropped."""
    s = re.sub(r"\{[a-z]+\}", " ", s or "")
    return set(w for w in nkey(s).split() if len(w) >= 3)


def match_choice(cls, logs):
    """0-based index of the branch the game took, or None when it cannot tell."""
    tmpl = REGISTRY.get(cls)
    if not tmpl:
        return None
    got = log_words(" ".join(logs))
    best, best_score = None, 0.0
    for i, t in enumerate(tmpl):
        tw = log_words(t)
        if not tw:
            continue
        score = len(tw & got) / len(tw)
        if score > best_score:
            best, best_score = i, score
    return best if best_score >= 0.6 else None

def load_tables(re_dir):
    out = {}
    for name in sorted(os.listdir(os.path.join(re_dir, "gamedata_plain"))):
        try:
            rows = list(csv.reader(open(os.path.join(re_dir, "gamedata_plain", name), newline="")))
        except Exception:
            continue
        if not rows:
            continue
        head = rows[0]
        ti = head.index("Title") if "Title" in head else None
        bi = head.index("Body") if "Body" in head else None
        # Narrative-only tables keep their sentence in another column: the dumb and
        # casino tables use "Scenario", the menu tables ("a16_stepparent-cruelty-
        # scenarios-enc") use "Scenario" for the third-person card and "Log Version"
        # for the first-person log. Skipping those tables is why a menu class could
        # have a perfect body match and still be reported unrecoverable.
        if bi is None:
            for col in ("Scenario", "Narrative", "String", "Log Version"):
                if col in head:
                    bi = head.index(col)
                    break
        if ti is None and bi is None:
            continue
        by_title = {}
        for i, r in enumerate(rows[1:], 1):
            key = nkey(r[ti]) if ti is not None and len(r) > ti else ""
            if ti is not None and not key:
                continue
            by_title.setdefault(key, []).append((i, r[bi] if bi is not None and len(r) > bi else ""))
        out[name] = by_title
    return out


def find_row(tables, title, body, prefer):
    key = nkey(title)
    if not key:
        return None, None
    gw = set(words(body))
    best = None
    for rank, name in enumerate([p for p in (prefer or []) if p in tables] +
                               [n for n in tables if n not in (prefer or [])]):
        rows = tables[name].get(key)
        if not rows:
            continue
        for row, tbody in rows:
            tw = words(tbody)
            score = (sum(1 for w in tw if w in gw) / len(tw)) if tw else 0.5
            cand = (score, -rank, name, row)
            if best is None or cand[:2] > best[:2]:
                best = cand
    if best is None:
        return None, None
    return best[2], best[3]



def find_row_body(tables, body, prefer, min_share=0.7, min_words=4):
    """Title-less fallback: rank rows by the fraction of the row's long words the
    captured body spells out. Only used when no title key matched, and only over the
    class's candidate tables from scen_source (a name-similarity hint, not proof), so a
    row still has to agree on most of its own long words to win."""
    gw = set(words(body))
    if len(gw) < min_words:
        return None, None
    best = None
    for rank, name in enumerate([p for p in (prefer or []) if p in tables]):
        for key, rows in tables[name].items():
            for row, tbody in rows:
                tw = words(tbody)
                if len(tw) < min_words:
                    continue
                shared = sum(1 for w in tw if w in gw)
                share = shared / len(tw)
                if share < min_share or shared < min_words:
                    continue
                cand = (share, shared, -rank, name, row)
                if best is None or cand[:3] > best[:3]:
                    best = cand
    if best is None:
        return None, None
    return best[3], best[4]



def symptom_index(re_dir):
    """Title -> 1-based row of the symptom table.

    The symptom card's title is the row's 'Abbreviated Symptom' (falling back to
    the capitalised 'Symptom'), so it can never match a 'Title' column and needs
    its own index.
    """
    p = os.path.join(re_dir, "gamedata_plain/a16_disease-symptom-scenarios-enc")
    out = {}
    if not os.path.exists(p):
        return out
    rows = list(csv.reader(open(p, newline="")))
    head = rows[0]
    si, ai = head.index("Symptom"), head.index("Abbreviated Symptom")
    for i, r in enumerate(rows[1:], 1):
        if len(r) <= ai or not r[si]:
            continue
        out.setdefault(nkey(r[ai] or r[si].capitalize()), i)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace", nargs="?", default=os.path.join(RE, "trace_fresh.json"))
    ap.add_argument("--out", default="/tmp/push.json")
    ap.add_argument("--re", default=RE)
    a = ap.parse_args()
    trace = json.load(open(a.trace))
    src = os.path.join(a.re, "obs_corpus/scen_source.json")
    classes = json.load(open(src)).get("map", {}) if os.path.exists(src) else {}
    tables = load_tables(a.re)
    sym = symptom_index(a.re)

    push = [None] * max(x["age"] for x in trace["ages"])
    report = []
    # The record a following outcome card belongs to. A trace round records the
    # menu card and then the card that shows what the tapped button did, so the
    # outcome's own Title is one of the parent's option labels.
    last_card = None
    for rec in trace["ages"]:
        logs = [l for l in rec.get("logs", [])]
        for s in rec["scenarios"]:
            cls = s.get("cls") or ""
            title = s.get("title") or ""
            body = s.get("body") or ""
            labels = s.get("choices") or []
            idx = rec["age"] - 1
            inside = 0 <= idx < len(push)
            entry, how = None, "title"

            if cls in REGISTRY:
                choice = match_choice(cls, logs)
                entry = {"scenario": cls}
                if choice is not None:
                    entry["choice"] = choice
                how = "registry"
            elif cls == "ScenarioDiseaseContract":
                row = sym.get(nkey(title))
                if row:
                    entry = {"table": "a16_disease-symptom-scenarios-enc", "row": row}
                how = "symptom"
            elif cls == "Scenario" and last_card is not None:
                # Outcome card: push whatever produced it, with the parent's
                # branch index when the parent is a code-built class.
                parent = last_card
                hit = None
                for i, lab in enumerate(parent["choices"]):
                    if title and nkey(lab) == nkey(title):
                        hit = i
                        break
                # The outcome card's Title is a pun on the tapped label ("Treat
                # me" -> "Treat me right"), so it often does not match textually;
                # the pairing is positional either way, and the branch index is
                # used whenever the label does match.
                if parent["cls"] in REGISTRY:
                    entry = {"scenario": parent["cls"]}
                    if hit is not None:
                        entry["choice"] = hit
                elif parent["entry"]:
                    entry = dict(parent["entry"])
                if entry:
                    how = "outcome:" + parent["cls"]
            if entry is None and cls not in REGISTRY and cls != "Scenario":
                cand = classes.get(cls, {}).get("candidates") or []
                prefer = [c["table"] if isinstance(c, dict) else c for c in cand]
                name, row = find_row(tables, title, body, prefer)
                how = "title"
                if not name:
                    name, row = find_row_body(tables, body, prefer)
                    how = "body"
                if name:
                    entry = {"table": name, "row": row}

            if entry and inside:
                push[idx] = entry
            if cls != "Scenario":
                last_card = {"cls": cls, "choices": labels, "entry": entry}
            elif entry is None:
                last_card = None
            report.append({"age": rec["age"], "cls": cls, "title": title,
                           "table": (entry or {}).get("table") or (("registry:" + entry["scenario"])
                                                                   if entry else None),
                           "row": (entry or {}).get("row", (entry or {}).get("choice")), "how": how})
    json.dump({"push": push}, open(a.out, "w"), indent=1)
    print("scenarios=%d recovered=%d -> %s" % (len(report), sum(1 for r in report if r["table"]), a.out))
    for r in report:
        where = ("%s row %s" % (r["table"], r["row"])) if r["table"] else "UNRECOVERED"
        print("  age %-3s %-34s %-30s %s" % (r["age"], (r["cls"] or "")[:34], (r["title"] or "")[:30], where))


if __name__ == "__main__":
    main()
