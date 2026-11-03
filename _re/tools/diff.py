"""Compare a game-side and a port-side canonical trace; print the first divergence.

usage: python3 diff.py GAME.norm.json PORT.norm.json [--stat-tol 3]

Streams, in priority order (the first stream that diverges is reported as *the* first divergence):
  1. age coverage               - does the port even reach the game's ages
  2. per-age log lines          - normalized text sets (exact, then token-Jaccard near-miss)
  3. per-age scenario count     - events the game showed that the port has no counterpart for
  4. per-age stats              - max |delta| over the six stats, ignoring the port's base offset
"""
import argparse, math, sys, json
from trace_common import keyset, norm

STAT_KEYS = ["happiness", "health", "smarts", "looks", "karma", "discipline"]


def jac(a, b):
    ta, tb = set(a.split()), set(b.split())
    return 0.0 if not ta or not tb else len(ta & tb) / len(ta | tb)


def validate_trace(trace, label, require_ages=False):
    if not isinstance(trace, dict) or not isinstance(trace.get("ages"), list):
        raise ValueError(label + " needs an ages array")
    if require_ages and not trace["ages"]:
        raise ValueError(label + " has no observations")
    seen = set()
    for record in trace["ages"]:
        if not isinstance(record, dict) or type(record.get("age")) is not int or record["age"] < 0:
            raise ValueError(label + " has an invalid age")
        age = record["age"]
        if age in seen:
            raise ValueError(label + " has duplicate age " + str(age))
        seen.add(age)
        if not isinstance(record.get("logs"), list) or not all(isinstance(x, str) for x in record["logs"]):
            raise ValueError(label + " needs string logs at age " + str(age))
        if not isinstance(record.get("scenarios"), list) or not all(isinstance(x, dict) for x in record["scenarios"]):
            raise ValueError(label + " needs scenarios at age " + str(age))
        stats = record.get("stats")
        if not isinstance(stats, dict) or any(type(stats.get(k)) not in (int, float) or not math.isfinite(stats[k]) for k in STAT_KEYS):
            raise ValueError(label + " needs six finite stats at age " + str(age))


def compare(game, port, stat_tol=3, verbose=True):
    validate_trace(game, "game", require_ages=True)
    validate_trace(port, "port")
    if type(stat_tol) not in (int, float) or not math.isfinite(stat_tol) or stat_tol < 0:
        raise ValueError("stat tolerance must be finite and nonnegative")
    ga_all = {a["age"]: a for a in game["ages"]}
    pa_all = {a["age"]: a for a in port["ages"]}
    # the game trace is a slice of one life (drive rounds overlap); compare only the ages it covers
    window = sorted(ga_all)
    ga = {a: ga_all[a] for a in window if a in pa_all}
    pa = {a: pa_all[a] for a in window if a in pa_all}
    report = {"ages_game": len(ga_all), "ages_port": len(pa_all),
              "window": [window[0], window[-1]] if window else [],
              "comparison_mode": "diagnostic", "parity_certified": False,
              "first_divergence": None, "detail": []}

    missing_ages = sorted(set(ga_all) - set(pa_all))
    if missing_ages:
        report["first_divergence"] = {"stream": "age-coverage", "age": missing_ages[0],
                                      "what": "port has no age %s (game has %d ages, port %d)" %
                                              (missing_ages[0], len(ga_all), len(pa_all))}
        return report

    stat_offset = None
    g_prev = p_prev = None
    for age in sorted(ga):
        g, p = ga[age], pa[age]
        gl, pl = keyset(g["logs"]), keyset(p["logs"])
        only_g, only_p = gl - pl, pl - gl
        near = [(x, max((jac(x, y), y) for y in only_p)) for x in only_g] if only_p else []
        near = [n for n in near if n[1][0] >= 0.6]
        if only_g or only_p:
            report["detail"].append({"age": age, "stream": "logs", "game_only": len(only_g),
                                     "port_only": len(only_p), "near": len(near),
                                     "first_game_only": (sorted(only_g)[0][:120] if only_g else None),
                                     "first_port_only": (sorted(only_p)[0][:120] if only_p else None)})
            if report["first_divergence"] is None or report["first_divergence"]["stream"] in ("scenarios", "stats"):
                report["first_divergence"] = {
                    "stream": "logs", "age": age,
                    "what": "normalized log sets differ: game-only %d, port-only %d" % (len(only_g), len(only_p)),
                    "port_only": sorted(only_p)[:2]}
        gc, pc = len(g["scenarios"]), len(p["scenarios"])
        if gc != pc:
            report["detail"].append({"age": age, "stream": "scenarios", "game": gc, "port": pc,
                                     "game_only": max(0, gc - pc), "port_only": max(0, pc - gc)})
            if report["first_divergence"] is None or report["first_divergence"]["stream"] == "stats":
                report["first_divergence"] = {"stream": "scenarios", "age": age,
                                              "what": "scenario count differs: game %d, port %d" % (gc, pc)}
        gs, ps = g["stats"], p["stats"]
        if gs and ps:
            if stat_offset is None:
                stat_offset = {k: ps.get(k, 0) - gs.get(k, 0) for k in STAT_KEYS}
            # The two RNG streams cannot be aligned, so from the second age on the
            # comparable quantity is the year-over-year change, not the level.
            gstep = pstep = None
            if g_prev and p_prev:
                gstep = {k: gs.get(k, 0) - g_prev.get(k, gs.get(k, 0)) for k in STAT_KEYS}
                pstep = {k: ps.get(k, 0) - p_prev.get(k, ps.get(k, 0)) for k in STAT_KEYS}
                worst = max(((k, abs(pstep[k] - gstep[k])) for k in STAT_KEYS), key=lambda kv: kv[1])
                game_shown, port_shown = gstep, pstep
            else:
                worst = max(((k, abs((ps.get(k, 0) - stat_offset[k]) - gs.get(k, 0))) for k in STAT_KEYS),
                            key=lambda kv: kv[1])
                game_shown = gs
                port_shown = {k: ps.get(k, 0) - stat_offset[k] for k in STAT_KEYS}
            report["detail"].append({"age": age, "stream": "stats", "worst": worst[0],
                                     "delta": worst[1], "game": game_shown,
                                     "port_shifted": port_shown,
                                     "mode": "step" if gstep else "level"})
            g_prev, p_prev = gs, ps
            if worst[1] > stat_tol and report["first_divergence"] is None:
                report["first_divergence"] = {"stream": "stats", "age": age,
                                              "what": "%s differs by %d (tolerance %d)" %
                                                      (worst[0], worst[1], stat_tol)}
    report["stat_offset"] = stat_offset
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game")
    parser.add_argument("port")
    parser.add_argument("--stat-tol", type=int, default=3)
    parser.add_argument("-v", action="store_true")
    args = parser.parse_args()
    if args.stat_tol < 0:
        parser.error("--stat-tol must be nonnegative")
    try:
        with open(args.game) as g, open(args.port) as p:
            game, port = json.load(g), json.load(p)
        rep = compare(game, port, args.stat_tol)
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as error:
        parser.error(str(error))
    print(json.dumps({k: v for k, v in rep.items() if k != "detail"}, indent=None if args.v else 1))
    if args.v:
        for d in rep["detail"]:
            print(json.dumps(d))
    sys.exit(1 if rep["first_divergence"] is not None else 0)
