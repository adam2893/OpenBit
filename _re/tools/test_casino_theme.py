#!/usr/bin/env python3
# The casino's theme card (REPORT.en.md 375): the menu card the native never pushes,
# offered as its own family. Checks the recovered body, the theme row, the single OK
# answer, the five-year cooldown, that a card never prints the theme it already has,
# and the no-casino control.
import json, os, re, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port", "build", "blsim")

TABLE = "registry:ScenarioCasinoTheme"
BODY = re.compile(
    r"^Pick a new theme for your (?P<name>.+?) today!\n\n"
    r"Theme: (?P<theme>.+)$")


def run(seed, ages=55, casino=True):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": 2000000, "only": "casino-theme"}
    if casino:
        spec["casino"] = True
    p = os.path.join(tempfile.gettempdir(), "_th375_%d.json" % seed)
    with open(p, "w") as f:
        json.dump(spec, f)
    r = subprocess.run([PORT, "--script", p], capture_output=True, text=True)
    assert r.returncode == 0, (seed, r.returncode, r.stderr[:400])
    return json.loads(r.stdout)["events"]


def main():
    total = 0
    themes = set()
    moved = 0
    for seed in range(1, 21):
        found = [e for e in run(seed) if (e.get("table") or "") == TABLE]
        total += len(found)
        ages = [e["age"] for e in found]
        for i in range(1, len(ages)):
            assert ages[i] - ages[i - 1] >= 5, (seed, ages)
        seen = []
        for e in found:
            m = BODY.match(e.get("body") or "")
            assert m, (seed, e.get("age"), e.get("body"))
            assert e.get("title") == "Casino", e.get("title")
            assert (e.get("choice") or "") == "OK", e.get("choice")
            assert m.group("name").strip(), e.get("body")
            theme = m.group("theme")
            assert theme.strip(), e.get("body")
            seen.append(theme)
            themes.add(theme)
        if len(set(seen)) == len(seen) and len(seen) > 1:
            moved += 1
        for i in range(1, len(seen)):
            assert seen[i] != seen[i - 1], (seed, seen)

    assert total >= 5, total
    assert len(themes) >= 3, sorted(themes)
    assert [e for e in run(4, casino=False) if (e.get("table") or "") == TABLE] == []
    print("cards: %d | themes seen: %d | seeds with two moves: %d"
          % (total, len(themes), moved))
    print("ok")


if __name__ == "__main__":
    main()
