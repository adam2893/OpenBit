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
        if not rows or "Title" not in rows[0]:
            continue
        ti = rows[0].index("Title")
        bi = rows[0].index("Body") if "Body" in rows[0] else None
        by_title = {}
        for i, r in enumerate(rows[1:], 1):
            if len(r) <= ti or not r[ti].strip():
                continue
            by_title.setdefault(nkey(r[ti]), []).append((i, r[bi] if bi is not None and len(r) > bi else ""))
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

    push = [None] * max(x["age"] for x in trace["ages"])
    report = []
    for rec in trace["ages"]:
        logs = [l for l in rec.get("logs", [])]
        for s in rec["scenarios"]:
            cls = s.get("cls") or ""
            cand = classes.get(cls, {}).get("candidates") or []
            prefer = [c["table"] if isinstance(c, dict) else c for c in cand]
            name, row = find_row(tables, s.get("title") or "", s.get("body") or "", prefer)
            idx = rec["age"] - 1
            if cls in REGISTRY:
                choice = match_choice(cls, logs)
                if 0 <= idx < len(push):
                    entry = {"scenario": cls}
                    if choice is not None:
                        entry["choice"] = choice
                    push[idx] = entry
                report.append({"age": rec["age"], "cls": cls, "title": s.get("title"),
                               "table": "registry", "row": choice})
                continue
            if name and 0 <= idx < len(push):
                push[idx] = {"table": name, "row": row}
            report.append({"age": rec["age"], "cls": cls, "title": s.get("title"), "table": name, "row": row})
    json.dump({"push": push}, open(a.out, "w"), indent=1)
    print("scenarios=%d recovered=%d -> %s" % (len(report), sum(1 for r in report if r["table"]), a.out))
    for r in report:
        where = ("%s row %s" % (r["table"], r["row"])) if r["table"] else "UNRECOVERED"
        print("  age %-3s %-34s %-30s %s" % (r["age"], (r["cls"] or "")[:34], (r["title"] or "")[:30], where))


if __name__ == "__main__":
    main()
