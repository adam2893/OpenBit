#!/usr/bin/env python3
"""The gift shop's shelves and its popularity (REPORT.en.md 366).

SimZooGiftShopItem::GetPopularity (0x2b661f8) is derived per call, never stored:
a rare breed weighs cool at 0.75 and health at 0.10, a common one spreads its
weight over cool, health, happiness, craziness and charisma. The yearly trade
band keys off that number, so a script can only see it through the gift block.
"""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed=3, ages=60, zoo=True, gender="female"):
    spec = {"seed": seed, "country": "United States", "gender": gender,
            "ages": ages, "cash": 300000}
    if zoo:
        spec["zoo"] = True
    path = Path("/tmp/qdir") / ("gift_%d_%d_%s.json" % (seed, ages, zoo))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(spec))
    r = subprocess.run([str(PORT), "--script", str(path)], cwd=str(ROOT / "port"),
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("blsim failed: " + r.stderr[-400:])
    return json.loads(r.stdout)


def expected_popularity(it):
    """The native formula, recomputed from the animal's own stats."""
    if not it["inZoo"]:
        return 0.0
    if it["rare"]:
        return 0.75 * it["cool"] + 0.10 * it["health"] + 0.05 * it["craziness"] \
            + 0.10 * it["charisma"]
    return 0.45 * it["cool"] + 0.25 * it["health"] + 0.10 * it["happiness"] \
        + 0.10 * it["craziness"] + 0.10 * it["charisma"]


def main():
    counts = []  # seeds whose shop was still stocked at the end
    rare = 0
    gone = 0
    seen = 0
    for seed in range(1, 11):
        doc = run(seed=seed)
        gift = doc.get("gift")
        assert isinstance(gift, list), gift
        # A park of plot size 1, 2 or 3 fills 4, 6 or 8 shelves; a duplicate draw
        # leaves a line out, so a seeded shop holds at most eight of them.
        if not gift:
            # The shop was stocked and then sold every line out before the life
            # ended: the yearly ledger retires an empty product. Only accept the
            # empty list when the same life shows the shop running.
            live = [e for e in doc["events"] if "GiftShop" in (e.get("table") or "")]
            assert live, (seed, "an empty gift shop with no shop card in its life")
            continue
        assert 1 <= len(gift) <= 8, (seed, len(gift))
        counts.append(len(gift))
        for it in gift:
            assert it["product"], it
            assert it["animal"], it
            assert it["min"] <= it["sell"] <= it["max"], it
            assert it["units"] >= 0, it
            seen += 1
            if it["rare"]:
                rare += 1
            if not it["inZoo"]:
                gone += 1
            got = it["animalPopularity"]
            want = max(0.0, min(100.0, expected_popularity(it)))
            assert abs(got - want) < 1e-6, (seed, it["animal"], got, want)

    # The shelves track the plot size, so the counts vary from park to park.
    assert len(set(counts)) >= 2, counts
    assert max(counts) <= 8, counts
    assert seen >= 20, seen
    assert rare >= 1, rare
    # Long lives lose featured animals, and a line without its animal in the park
    # scores no animal share at all.
    assert gone >= 1, gone

    # No park, no shelves.
    bare = run(seed=3, zoo=False)
    assert not bare.get("gift"), bare.get("gift")

    print("shops: %d | lines: %d | counts: %s | rare lines: %d | animal gone: %d"
          % (len(counts), seen, counts, rare, gone))
    print("ok")


if __name__ == "__main__":
    main()
