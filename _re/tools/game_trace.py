"""Normalize a game-side drive trace (/…/_re/trace_game*.json) into the canonical shape."""
import sys, json
from trace_common import dump, keyset

STAT_KEYS = ["happiness", "health", "smarts", "looks", "karma", "discipline"]
# the game stores il2cpp backing fields: <Att_intelligence>k__BackingField is "smarts" in the UI
FIELD_MAP = {"happiness": "Att_happiness", "health": "Att_health", "smarts": "Att_intelligence",
             "looks": "Att_appearance", "karma": "Att_karma", "discipline": "Att_discipline"}


def stats_of(obj):
    """Pull the six UI stats out of a captured hero/tick dict (tick snapshots nest them under 's')."""
    if not isinstance(obj, dict):
        return None
    for key in ("s", "hero", "fields"):
        if isinstance(obj.get(key), dict):
            obj = obj[key]
            break
    src = {k.replace("<", "").replace(">", "").replace("k__BackingField", ""): v
           for k, v in obj.items()}
    out = {}
    for k, f in FIELD_MAP.items():
        v = src.get(f)
        if isinstance(v, (int, float)):
            out[k] = int(v)
    return out or None


def normalize(rounds):
    # Already-canonical input (e.g. tools/fresh_trace.py output) passes through:
    # the drive-round shape is the only other thing this module understands.
    if isinstance(rounds, dict) and "ages" in rounds and "source" in rounds:
        return rounds
    raw_valid = isinstance(rounds, list) or (isinstance(rounds, dict) and isinstance(rounds.get("rounds"), list))
    if isinstance(rounds, dict):
        rounds = rounds.get("rounds", [])
    by_age = {}
    order = []
    observed_ages = set()
    for r in rounds:
        age = r.get("age")
        if age is None:
            raw_valid = False
            continue
        if age in observed_ages or type(age) is not int or age < 0:
            raw_valid = False
        observed_ages.add(age)
        rec = by_age.get(age)
        if rec is None:
            rec = {"age": age, "logs": [], "raw_logs": [], "scenarios": [], "stats": None, "rounds": []}
            by_age[age] = rec
            order.append(age)
        rec["rounds"].append(r.get("round"))
        if not isinstance(r.get("logs"), list):
            raw_valid = False
        for l in r.get("logs") or []:
            txt = l.get("text") if isinstance(l, dict) else l
            if isinstance(txt, str):
                rec["raw_logs"].append(txt)
            else:
                raw_valid = False
            if txt and txt not in rec["logs"]:
                rec["logs"].append(txt)
        for s in r.get("scenarios") or []:
            rec["scenarios"].append({"title": s.get("Title"), "body": s.get("Body"),
                                     "cls": s.get("_class") or s.get("_Type")})
        st = stats_of((r.get("ticks") or [None])[-1]) or stats_of(r.get("hero"))
        if st:
            rec["stats"] = st
    result = {"source": "game", "ages": [by_age[a] for a in sorted(order)]}
    # ponytail: supplied array order only; overlapping rounds/capture completeness unknown.
    if raw_valid:
        result["log_provenance"] = "source-array-order-v1"
    return result


if __name__ == "__main__":
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else src.replace(".json", ".norm.json")
    trace = normalize(json.load(open(src)))
    dump(trace, out)
    print("ages=%d logs=%d scenarios=%d -> %s" % (
        len(trace["ages"]),
        sum(len(a["logs"]) for a in trace["ages"]),
        sum(len(a["scenarios"]) for a in trace["ages"]), out))
