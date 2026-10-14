#!/usr/bin/env python3
"""Add the tables the app names in its own code to scen_source's candidate lists.

scen_source.py guesses a scenario's source table from token similarity over the
whole gamedata directory, which for menu scenes usually lands on nothing. The app
is less shy than that: global-metadata.dat keeps a literal "data/<table>" for
every table the code loads by name (282 of them, see the dump produced by
    python3 tools/metadata_strings.py list --limit 100000 --raw
). A class whose CamelCase tokens cover a code-path table's name tokens (>= 0.6
of the table's own tokens) is a much stronger claim than a similarity score, so
those tables are merged into the candidate list with score 2.0.

Usage: python3 tools/scen_source_code_tables.py [--write]
"""
import json, os, re, sys

RE = "/Users/Adam/Documents/bitlife/_re"
SCEN = os.path.join(RE, "obs_corpus", "scen_source.json")
LITS = "/tmp/meta_lits.txt"
WRAP = {"scenario", "scenarios", "enc", "dumb", "a16", "genesis", "event"}


def toks(name):
    """CamelCase-split lower-case token set: ScenarioStepparentCruelty ->
    {scenario, stepparent, cruelty}."""
    s = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", name)
    return set(re.sub(r"[^a-z0-9]+", " ", s.lower()).split())


def code_tables(path=LITS):
    """Every table name that appears in an authored 'data/...' literal."""
    if not os.path.exists(path):
        sys.exit(path + " missing; dump it with tools/metadata_strings.py list")
    out = set()
    for line in open(path, errors="replace"):
        m = re.match(r"#\d+\t(.*?)\s*$", line.rstrip("\n"))
        if m and m.group(1).startswith("data/"):
            out.add(m.group(1).split("/")[-1])
    return sorted(out)


def merge(map_obj, tables):
    added = 0
    for cls, info in map_obj.items():
        base = toks(cls) - WRAP
        if not base:
            continue
        have = {c.get("table") for c in info.get("candidates") or []}
        for t in tables:
            core = toks(t) - WRAP
            if not core or len(base & core) / len(core) < 0.6:
                continue
            if t in have:
                info["code_path"] = True
                continue
            info.setdefault("candidates", []).append(
                {"table": t, "score": 2.0, "shared": sorted(base & core), "code_path": True})
            added += 1
    return added


def main():
    write = "--write" in sys.argv
    doc = json.load(open(SCEN))
    tables = code_tables()
    added = merge(doc["map"], tables)
    print("code-path tables:", len(tables), "| candidates added:", added)
    hit = sum(1 for v in doc["map"].values()
              if any(c.get("code_path") for c in v.get("candidates") or []))
    print("classes with a code-path candidate:", hit, "of", len(doc["map"]))
    if write:
        json.dump(doc, open(SCEN, "w"), indent=1)
        print("wrote", SCEN)
    return 0


if __name__ == "__main__":
    sys.exit(main())
