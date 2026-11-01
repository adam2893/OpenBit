#!/usr/bin/env python3
"""The back-to-school title pool (REPORT.en.md 332)."""
import csv, json, subprocess
from pathlib import Path

ROOT = Path("/Users/Adam/Documents/bitlife/_re")
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"
COLS = ["Og Title", "Option 1", "Option 2", "Option 3", "Option 4", "Option 5"]


def table(name):
    with open(DATA / name, encoding="utf-8", newline="") as fh:
        return list(csv.DictReader(fh))


def pool(rows, tag):
    row = rows[tag]
    return {row[c].strip() for c in COLS if row.get(c, "").strip()}


def color(name):
    with open(DATA / name, encoding="utf-8", newline="") as fh:
        return list(csv.DictReader(fh))


def run(seed, ages=90):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages}
    path = Path("/tmp/qdir/b2s_spec.json")
    path.write_text(json.dumps(spec), encoding="utf-8")
    out = subprocess.run([str(PORT), "--script", str(path)], cwd=str(ROOT / "port"),
                         capture_output=True, text=True, check=True).stdout
    return json.loads(out).get("events") or []


def main():
    titles = table("a16_back-to-school-titles-enc")
    assert len(titles) == 55, len(titles)
    trips = color("a16_backtoschool-field-trip-color-enc")
    dance_rej = color("a16_backtoschool-school-dance-rejection-color-enc")
    dance_ev = color("a16_backtoschool-school-dance-event-feedback-enc")
    party = color("a16_backtoschool-house-party-color-enc")
    sleep_rej = color("a16_backtoschool-sleepover-invitation-rejection-color-enc")
    seen = {}
    prom = {"win": 0, "lose": 0, "no": 0}
    for seed in range(1, 121):
        for e in run(seed):
            table_name = e.get("table") or ""
            title = e.get("title") or ""
            row = e.get("row")
            if table_name not in {
                "a16_backtoschool-field-trip-color-enc",
                "a16_backtoschool-school-dance-rejection-color-enc",
                "a16_backtoschool-school-dance-event-feedback-enc",
                "a16_backtoschool-house-party-color-enc",
                "a16_backtoschool-sleepover-invitation-rejection-color-enc",
                "registry:ScenarioBackToSchoolAgeUpPromRoyal",
            }:
                continue
            assert title, e
            if table_name == "a16_backtoschool-field-trip-color-enc":
                focus = (trips[int(row) - 1]["Focus"] or "").strip().lower()
                tag = {"learning": 51, "socializing": 52}.get(focus, 53)
                assert title in pool(titles, tag), (title, tag, focus)
            elif table_name == "a16_backtoschool-school-dance-rejection-color-enc":
                assert title in pool(titles, 8), title
            elif table_name == "a16_backtoschool-school-dance-event-feedback-enc":
                ct = (dance_ev[int(row) - 1]["Companion Type"] or "").strip().lower()
                tag = 19 if ct == "none" else (18 if "friend" in ct else 9)
                assert title in pool(titles, tag), (title, tag, ct)
            elif table_name == "a16_backtoschool-house-party-color-enc":
                level = (party[int(row) - 1]["Rowdiness Level"] or "").strip().lower()
                tag = {"wild": 2, "lively": 3}.get(level, 4)
                assert title in pool(titles, tag), (title, tag, level)
            elif table_name == "a16_backtoschool-sleepover-invitation-rejection-color-enc":
                assert title in pool(titles, 40) | pool(titles, 28), title
            else:
                line = " ".join(e.get("logLines") or [])
                body = e.get("body") or ""
                if "elected prom" in line:
                    prom["win"] += 1
                    assert title in pool(titles, 49), title
                elif "lost to" in line:
                    prom["lose"] += 1
                    assert title in pool(titles, 47), title
                elif "didn't care" in body or "didn't care" in line:
                    prom["no"] += 1
                    assert title in pool(titles, 50), title
                else:
                    continue
            seen[title] = seen.get(title, 0) + 1
    assert len(seen) >= 8, seen
    assert sum(prom.values()) >= 5, prom
    print(f"titles {len(seen)} | prom {prom} | sample {list(seen)[:5]}")
    print("ok")


if __name__ == "__main__":
    main()
