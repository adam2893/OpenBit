"""What the DataHub layer is made of (REPORT.en.md 176).

§175 found 2,344 DataHub methods and asked whether the layer is portable or needs one
reading per method. This classifies them by name, which is enough to answer the shape
question: Load*Set builds a pool, Get* picks from one.
"""
import collections
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    syms = json.loads((ROOT / "tools" / "symbols.json").read_text())
    names = {v for v in syms.values() if v.startswith("DataHub::")}
    loads = {n for n in names if n.startswith("DataHub::Load")}
    gets = {n for n in names if n.startswith("DataHub::Get")}
    print("DataHub methods:", len(names))
    print("  Load*Set  :", len(loads), " (build a pool)")
    print("  Get*      :", len(gets), " (pick from one)")
    print("  other     :", len(names) - len(loads) - len(gets))
    print()
    # A Getter whose name ends in a gender/age qualifier is the filtered kind the
    # cards depend on; the bare ones just pick.
    filtered = collections.Counter()
    for n in gets:
        for q in ("Gender", "Age", "For"):
            if q in n:
                filtered[q] += 1
                break
        else:
            filtered["unfiltered"] += 1
    for k, v in filtered.most_common():
        print("  getter %-11s %4d" % (k, v))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())