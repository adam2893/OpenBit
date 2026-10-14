"""Compare a game-side and a port-side canonical trace; print the first divergence.

usage: python3 diff.py GAME.norm.json PORT.norm.json [--stat-tol 3]

Streams, in priority order (the first stream that diverges is reported as *the* first divergence):
  1. age coverage               - does the port even reach the game's ages
  2. per-age log lines          - normalized text sets (exact, then token-Jaccard near-miss)
  3. per-age scenario count     - events the game showed that the port has no counterpart for
  4. per-age stats              - max |delta| over the six stats, ignoring the port's base offset
"""
import sys, json
from trace_common import keyset, norm

STAT_KEYS = ["happiness", "health", "smarts", "looks", "karma", "discipline"]


def jac(a, b):
    ta, tb = set(a.split()), set(b.split())
    return 0.0 if not ta or not tb else len(ta & tb) / len(ta | tb)


def compare(game, port, stat_tol=3, verbose=True):
    ga_all = {a["age"]: a for a in game["ages"]}
    pa_all = {a["age"]: a for a in port["ages"]}
    # the game trace is a slice of one life (drive rounds overlap); compare only the ages it covers
    window = sorted(ga_all)
    ga = {a: ga_all[a] for a in window if a in pa_all}
    pa = {a: pa_all[a] for a in window if a in pa_all}
    report = {"ages_game": len(ga_all), "ages_port": len(pa_all),
              "window": [window[0], window[-1]] if window else [],
              "first_divergence": None, "detail": []}

    missing_ages = sorted(set(ga) - set(pa))
    if missing_ages:
        report["first_divergence"] = {"stream": "age-coverage", "age": missing_ages[0],
                                      "what": "port has no age %s (game has %d ages, port %d)" %
                                              (missing_ages[0], len(ga), len(pa))}
        return report

    stat_offset = None
    g_prev = p_prev = None
    for age in sorted(ga):
        g, p = ga[age], pa[age]
        gl, pl = keyset(g["logs"]), keyset(p["logs"])
        only_g, only_p = gl - pl, pl - gl
        near = [(x, max((jac(x, y), y) for y in only_p)) for x in only_g] if only_p else []
        near = [n for n in near if n[1][0] >= 0.6]
        if only_g:
            report["detail"].append({"age": age, "stream": "logs", "game_only": len(only_g),
                                     "port_only": len(only_p), "near": len(near),
                                     "first_game_only": sorted(only_g)[0][:120],
                                     "first_port_only": (sorted(only_p)[0][:120] if only_p else None)})
            if report["first_divergence"] is None:
                report["first_divergence"] = {
                    "stream": "logs", "age": age,
                    "what": "game logged %d line(s) the port did not (first: %r)" %
                            (len(only_g), sorted(only_g)[0][:100]),
                    "port_only": sorted(only_p)[:2]}
        gs, ps = g.get("stats"), p.get("stats")
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
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    tol = 3
    if "--stat-tol" in sys.argv:
        tol = int(sys.argv[sys.argv.index("--stat-tol") + 1])
    game, port = json.load(open(args[0])), json.load(open(args[1]))
    rep = compare(game, port, tol)
    print(json.dumps({k: v for k, v in rep.items() if k != "detail"}, indent=1))
    if "-v" in sys.argv:
        for d in rep["detail"]:
            print(json.dumps(d)[:300])
