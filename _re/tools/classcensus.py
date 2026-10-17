"""Which scenario classes the game has, and which the port mentions.

Every other instrument audits what the port already renders. This one looks the other
way: it reads the game's class list out of the il2cpp dump and the port's sources for
the classes they name, so the difference is visible (REPORT.en.md 169).

A mention is not a port -- the port names a class in a comment, in a table list, or
in the code that builds its card -- so the number is an upper bound on coverage and a
lower bound on what is missing:
"""
import argparse
import collections
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DUMP = ROOT / "il2cpp_dump.txt"
CLS = re.compile(r"^class (Scenario[A-Za-z0-9_]*) :", re.M)
NAME = re.compile(r"\b(Scenario[A-Za-z0-9_]+)")


def game_classes():
    text = DUMP.read_text(encoding="utf-8", errors="replace")
    return set(CLS.findall(text))


def port_names():
    seen = set()
    for path in (ROOT / "port" / "src").rglob("*.cpp"):
        seen.update(NAME.findall(path.read_text(encoding="utf-8", errors="replace")))
    return seen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", type=int, default=0, help="print N unported names")
    args = ap.parse_args()
    game = game_classes()
    named = port_names()
    known = game & named
    missing = game - named
    print("scenario classes in the game:", len(game))
    print("named somewhere in port/src:", len(known))
    print("not named at all:", len(missing))
    by_family = collections.Counter()
    for name in missing:
        tail = name[len("Scenario"):]
        parts = re.findall(r"[A-Z][a-z0-9]*", tail)
        by_family[parts[0] if parts else "?"] += 1
    print()
    print("unported by family head:")
    for head, n in by_family.most_common(24):
        print("  %-14s %4d" % (head, n))
    # The port renders most cards from tables rather than from class names, so the
    # family-level measure is: how many of a family's data tables does port/src touch?
    data = ROOT / "gamedata_plain"
    src = "".join(p.read_text(encoding="utf-8", errors="replace")
                  for p in (ROOT / "port" / "src").rglob("*.cpp"))
    print()
    print("family tables touched (of the tables that exist):")
    for head, _n in by_family.most_common(16):
        prefix = "a16_" + re.sub(r"(?<!^)(?=[A-Z])", "-", head).lower()
        names = [p.name for p in data.glob(prefix + "*")]
        if not names:
            continue
        touched = sum(1 for name in names if name in src)
        print("  %-12s %3d / %3d" % (head, touched, len(names)))
    if args.list:
        for name in sorted(missing)[: args.list]:
            print("  " + name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())