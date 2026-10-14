"""Shared normalization for the Phase 1 differential bench (tools/).

Two traces are compared in one canonical shape:

    {"source": "game"|"port", "ages": [ {age, logs:[str], scenarios:[{title,body,cls}],
                                        stats:{...}|null } ] }

Normalization rules (both sides go through the same function):
  - lower-case, digits -> '#', quoted strings and capitalised name runs -> '{name}',
    all other punctuation -> space, whitespace collapsed.
That is deliberately lossy: it compares *what happened*, not how a name or a number
was spelled, because the port and the game pick different names from their own tables.
"""
import json, re

NAME = re.compile(r"\b[A-Z][a-z]+(?:\s+[A-Z][a-z]+)*\b")
QSTR = re.compile(r"[\"']([^\"']+)[\"']")


def norm(s):
    if s is None:
        return ""
    s = QSTR.sub(" {name} ", s)
    s = NAME.sub(" {name} ", s)
    s = s.lower()
    s = re.sub(r"\d+", "#", s)
    s = re.sub(r"[^a-z#{}]+", " ", s)
    return re.sub(r"\s+", " ", s).strip()


def keyset(lines):
    return {norm(l) for l in lines if norm(l)}


def load(path):
    with open(path) as f:
        return json.load(f)


def dump(obj, path):
    with open(path, "w") as f:
        json.dump(obj, f, indent=1)
    return path
