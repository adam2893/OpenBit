#!/usr/bin/env python3
"""Regenerate the self-contained static research snapshot from the local workspace."""
import base64, csv, gzip, json, shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "site" / "data.json"
REPO_URL = "https://github.com/adam2893/OpenBit"

def read(path, default=None):
    try:
        return json.loads((ROOT / path).read_text())
    except Exception:
        return default

def records(path):
    out = []
    for line in (ROOT / path).read_text(errors="replace").splitlines():
        try:
            out.append(json.loads(line))
        except Exception:
            pass
    return out

manifest = read("decomp/target_manifest.json", {})
rounds = read("decomp/full_verifier_round256.json", {})
names = records("decomp/names.jsonl")
methods = records("decomp/method_ledger.jsonl")
source_files = sorted((ROOT / "decomp/source").glob("*.md"))
verifiers = sorted((ROOT / "decomp").glob("verify_*.py"))
coverage_dir = ROOT / "decomp/coverage"
port_units = sorted((ROOT / "port/src").rglob("*.cpp"))
notes = []
open_items = []

def add_note(note_id, title, path, text, category):
    notes.append({"id": note_id, "title": title, "path": path,
                  "text": text[:18000], "category": category})

for p in source_files:
    text = p.read_text(errors="replace")
    title = next((x.lstrip("# ").strip() for x in text.splitlines() if x.startswith("#")), p.stem.replace("_", " "))
    add_note(p.stem, title, str(p.relative_to(ROOT)), text, "Source note")

for p in sorted((ROOT / "decomp/evidence").rglob("*")):
    if not p.is_file() or p.stat().st_size > 1_000_000 or p.suffix.lower() not in {".md", ".json", ".txt", ".log"}:
        continue
    text = p.read_text(errors="replace")
    title = next((x.lstrip("# ").strip() for x in text.splitlines() if x.startswith("#")), p.stem.replace("_", " "))
    add_note("evidence-" + p.stem, title, str(p.relative_to(ROOT)), text, "Evidence")

for relative in ["CURRENT_TASK.md", "TODO.md", "README.md", "method_ledger.jsonl",
                 "names.jsonl", "target_manifest.json", "full_verifier_round256.json"]:
    p = ROOT / "decomp" / relative
    if p.exists():
        add_note("record-" + p.stem, p.stem.replace("_", " ").title(),
                 str(p.relative_to(ROOT)), p.read_text(errors="replace"), "Project record")

for p in verifiers:
    add_note("verifier-" + p.stem, p.name, str(p.relative_to(ROOT)),
             p.read_text(errors="replace"), "Bounded verifier source")

for p in sorted(coverage_dir.iterdir()):
    if not p.is_file():
        continue
    if p.suffix.lower() == ".csv":
        with p.open(newline="", errors="replace") as handle:
            rows = list(csv.DictReader(handle))
        if p.name == "managed_methods.csv":
            # Preserve full exact census in the artifact; expose representative searchable rows in-browser.
            text = json.dumps(rows[:2500], separators=(",", ":"))
            title = f"Managed method census · first 2,500 of {len(rows):,} rows"
        elif p.name == "module_slots.csv":
            text = json.dumps(rows[:500], separators=(",", ":"))
            title = f"Generated module slots · first 500 of {len(rows):,} rows"
        elif p.name == "native_bodies.csv":
            text = json.dumps(rows[:2500], separators=(",", ":"))
            title = f"Native body candidates · first 2,500 of {len(rows):,} rows"
        else:
            text = p.read_text(errors="replace")
            title = p.stem.replace("_", " ").title()
    else:
        text = p.read_text(errors="replace")
        title = next((x.lstrip("# ").strip() for x in text.splitlines() if x.startswith("#")), p.stem.replace("_", " ").title())
    add_note("coverage-" + p.stem, title, str(p.relative_to(ROOT)), text, "Coverage audit")

for p in port_units:
    add_note("port-" + p.stem, p.name, str(p.relative_to(ROOT)),
             p.read_text(errors="replace"), "Native C++ port unit")

port_readme = ROOT / "port/README.md"
if port_readme.exists():
    add_note("port-readme", "Native C++ port README", str(port_readme.relative_to(ROOT)),
             port_readme.read_text(errors="replace"), "Native C++ port")

trace_paths = sorted((ROOT / "obs_corpus").glob("*.json")) + sorted(ROOT.glob("trace_*.json"))
traces = []
for p in trace_paths:
    if p.stat().st_size > 2_000_000:
        continue
    traces.append({"name": p.name, "path": str(p.relative_to(ROOT)), "bytes": p.stat().st_size})
    add_note("trace-" + p.stem, p.name, str(p.relative_to(ROOT)),
             p.read_text(errors="replace"), "Captured trace")

todo = (ROOT / "decomp/TODO.md").read_text(errors="replace")
for line in todo.splitlines():
    if line.startswith("- [ ]"):
        item = line[5:].strip()
        if item:
            open_items.append(item)
            notes.append({"id": "open-" + str(len(open_items)).zfill(3), "title": item[:180],
                          "path": "decomp/TODO.md", "text": item, "category": "Open obligation"})

current_text = (ROOT / "decomp/CURRENT_TASK.md").read_text(errors="replace")
coverage_summary = json.loads((coverage_dir / "summary.json").read_text())
coverage_tables = {}
for filename in ["assembly_coverage.csv", "candidate_system_coverage.csv"]:
    with (coverage_dir / filename).open(newline="", errors="replace") as handle:
        coverage_tables[filename] = list(csv.DictReader(handle))
coverage_audit = {
    "generatedAt": "2026-11-13",
    "artifactPath": "https://github.com/adam2893/OpenBit/blob/main/_re/decomp/coverage/summary.json",
    "summary": coverage_summary,
    "tables": coverage_tables,
    "files": {
        p.name: {"bytes": p.stat().st_size, "path": "decomp/coverage/" + p.name}
        for p in sorted(coverage_dir.iterdir()) if p.is_file()
    },
}
entities = []
for n in names:
    entities.append({
        "id": n.get("id"), "kind": n.get("kind", "entity"),
        "label": n.get("semantic_label") or n.get("id"),
        "address": n.get("linked_va") or n.get("va") or "",
        "size": n.get("size"), "confidence": n.get("confidence", ""),
        "acceptance": n.get("acceptance", ""), "authority": n.get("name_authority", ""),
        "originalHint": n.get("original_name_hint", ""),
        "path": "decomp/names.jsonl"
    })

function_records = [n for n in names if n.get("kind") in ("function", "function_candidate")]
round_id = rounds.get("recovery_round")
out = {
    "generatedFrom": "local repository snapshot",
    "repositoryUrl": REPO_URL,
    "target": {"id": manifest.get("target_id"), "provenance": manifest.get("provenance"),
               "checkpoint": manifest.get("repository_checkpoint"), "artifacts": manifest.get("artifacts", []),
               "scope": manifest.get("scope", {})},
    "coverageAudit": coverage_audit,
    "current": {
        "title": "AddYear native recovery", "updated": "2026-11-13",
        "checkpoint": "Round 257 recovered the 156-byte UTF-16 appender body, including exact short-tag behavior, low-16-bit writes, and the terminator. Heap growth and allocation contracts still block integration.",
        "task": current_text[:24000]
    },
    "verifier": {
        "round": round_id, "goalRound": rounds.get("goal_round"), "checks": rounds.get("bounded_checks"),
        "entities": rounds.get("named_provenance_bearing_entities"), "elapsedMs": rounds.get("elapsed_ms"),
        "status": rounds.get("job_status"), "exitCode": rounds.get("exit_code"),
        "acceptance": rounds.get("acceptance"), "remaining": rounds.get("remaining", []),
        "path": "decomp/full_verifier_round256.json"
    },
    "counts": {
        "entities": len(entities),
        "functions": sum(1 for n in names if n.get("kind") in ("function", "function_candidate")),
        "blocks": sum(1 for n in names if n.get("kind") in ("native_block", "exception_landing_pad")),
        "sourceNotes": len(source_files), "verifiers": len(verifiers),
        "openItems": len(open_items), "methods": len(methods)
    },
    "byteInventory": {
        "sizedFunctions": sum(1 for n in function_records if isinstance(n.get("size"), int)),
        "sizedFunctionBytes": sum(n.get("size", 0) for n in function_records if isinstance(n.get("size"), int)),
        "nativeBlocks": sum(1 for n in names if n.get("kind") == "native_block"),
        "landingPads": sum(1 for n in names if n.get("kind") == "exception_landing_pad"),
        "portUnits": len(port_units),
        "decompAccepted": sum(1 for m in methods if m.get("recovery_state") == "accepted")
    },
    "entities": entities, "notes": notes, "traces": traces,
    "openItems": open_items, "methods": methods,
    "cycles": {
        "observe": "Repository traces and captured evidence are indexed; the latest checkpoint makes no target-runtime execution claim.",
        "lift": "Round 257 is the active bounded native lift; the last full persisted verifier snapshot is Round 256.",
        "build": "A separate C++17 port exists in port/src; the decomp method ledger does not mark source integration.",
        "verify": "Round 256 full cohort: 231 bounded checks, exit 0. Round 257 is a targeted run, not a new full-cohort result."
    },
    "differential": {
        "status": "No current differential bench result is attached to the decomp checkpoint.",
        "path": "tools/bench.sh",
        "note": "Historical xdiff files cover data/string evidence, not native C++ runtime equivalence."
    },
    "portScenarioCoverage": {
        "recoverable": 21, "captured": 90, "percent": 23,
        "scope": "Scenario coverage reported for the separate C++ port; not native observed-block coverage.",
        "source": "OpenBit README.md"
    },
    "claims": {
        "coverage": "No exhaustive denominator is established; no global percentage is shown.",
        "execution": "Latest checkpoint reports no target execution. Local verifier status does not imply runtime equivalence.",
        "port": "A separate C++17 port exists; the current decomp method ledger records no source integration for its three entries."
    }
}
OUT.write_text(json.dumps(out, separators=(",", ":")))
compressed = gzip.compress(OUT.read_bytes(), compresslevel=9, mtime=0)
parts = OUT.parent / "snapshot-parts"
if parts.exists():
    shutil.rmtree(parts)
parts.mkdir()
encoded = base64.b64encode(compressed).decode("ascii")
chunk_size = 100_000
for i in range(0, len(encoded), chunk_size):
    (parts / ("part-" + str(i // chunk_size).zfill(2) + ".txt")).write_text(encoded[i:i + chunk_size])
(parts / "manifest.json").write_text(json.dumps({"parts": (len(encoded) + chunk_size - 1) // chunk_size,
                                                 "encoding": "base64+gzip"}, separators=(",", ":")))
print(f"Wrote {OUT.relative_to(ROOT)}: {len(entities)} entities, {len(notes)} searchable records, {len(verifiers)} verifier scripts")
