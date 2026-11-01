"""324: the casino gambler compliment card (ScenarioCasinoAgeUpGamblerComplimentGame)."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"

TABLE = "registry:ScenarioCasinoAgeUpGamblerComplimentGame"
TIERS = {"Beginner", "Amateur", "Novice", "Regular", "High Roller"}
BODY = re.compile(r"^Your casino's (?P<tier>[^,]+), (?P<name>.+), is complimenting "
                  r"your casino for having (?P<poss>his|her) favorite game, "
                  r"(?P<game>.+)\.$")


def run(seed=7, ages=45, cash=3000000, choices=None, casino=True):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": cash}
    if casino:
        spec.update({"casino": True, "casinoBank": 1000000000,
                     "only": "casino-compliment"})
    if choices is not None:
        spec["choices"] = choices
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


def compliment(doc):
    return [e for e in doc["events"] if e.get("title") == "Compliment"]


def at(doc, index):
    events = doc["events"]
    return events[index] if index < len(events) else None


PREFIX = {0: "Thank ", 1: "Give ", 2: "Ignore "}


def forced(seed, want, ages=45):
    # choices[i] answers the card of iteration i, and in "only" mode one iteration
    # is one year of the hero's life, so the padding that reaches a card is its own
    # age.
    for pad in range(1, ages + 1):
        doc = run(seed=seed, ages=ages, choices=[-1] * pad + [want])
        hits = [e for e in compliment(doc)
                if e["age"] == pad and e["choice"].startswith(PREFIX[want])]
        if hits:
            return doc, hits[0]
    raise AssertionError("compliment card not reachable by padding")


def cell_hits(cell, text):
    # The cells carry <he/she>, <his/her> and <a/b/c> tokens; the card resolves
    # them, so each token is a wildcard and the literal text must match verbatim.
    parts = re.split(r"<[^>]+>", cell)
    pattern = r"\S+".join(re.escape(part) for part in parts)
    return re.fullmatch(pattern, text) is not None


def main():
    doc = run()
    found = compliment(doc)
    assert found, "the compliment card never fired on a solvent casino"
    card = found[0]
    paras = card["body"].split("\n\n")
    assert len(paras) == 2, card["body"]
    m = BODY.match(paras[0])
    assert m, card["body"]
    assert m.group("tier") in TIERS, m.group("tier")
    assert m.group("game"), card["body"]
    poss = m.group("poss")
    options = ["Thank " + ("her" if poss == "her" else "him"),
               "Give " + ("her" if poss == "her" else "him") + " some free games",
               "Ignore " + ("her" if poss == "her" else "him")]
    assert card["choice"] in options, card["choice"]

    # The mood table's own line follows the sentence, capitalised.
    rows = [r["String"] for r in
            csv.DictReader(open(DATA / "a16_casino-gambler-game-room-compliment-enc",
                                newline=""))]
    assert any(cell_hits(r, paras[1]) for r in rows), paras[1]
    assert paras[1][:1].isupper(), paras[1]

    # The refresh's default journal line, kept by every branch.
    name = m.group("name")
    said = "I thanked my casino's gambler, " + name + ", for " + poss + \
           " compliment about my casino."
    assert any(said in line for line in card["logLines"]), card["logLines"]
    assert card["deltas"] == {"karma": 0, "discipline": 0, "happiness": 0,
                              "health": 0, "smarts": 0, "looks": 0, "fame": 0}, \
        card["deltas"]

    # The three buttons each keep a journal line of their own.
    _, give = forced(7, 1)
    assert give["choice"].startswith("Give "), give["choice"]
    assert give["choice"].endswith(" some free games"), give["choice"]
    line = [l for l in give["logLines"] if "some free games after" in l]
    assert line, give["logLines"]
    assert re.match(r"^I gave my casino's gambler, .+, some free games after "
                    r"(he|she) complimented my casino\.$", line[0].split("\n")[-1]), line[0]

    _, ignore = forced(7, 2)
    assert ignore["choice"].startswith("Ignore "), ignore["choice"]
    line = [l for l in ignore["logLines"] if "I ignored my casino's gambler" in l]
    assert line, ignore["logLines"]
    assert re.match(r"^I ignored my casino's gambler, .+, after (he|she) "
                    r"complimented my casino\.$", line[0].split("\n")[-1]), line[0]

    _, thank = forced(7, 0)
    assert thank["choice"].startswith("Thank "), thank["choice"]
    assert not [l for l in thank["logLines"] if "I ignored my casino" in l], \
        thank["logLines"]

    # Life::YearsSinceScenario > 4: two cards in one life are at least five years
    # apart, and a life without a casino never sees one.
    ages = [e["age"] for e in compliment(doc)]
    for before, after in zip(ages, ages[1:]):
        assert after - before > 4, (before, after)
    plain = run(casino=False, ages=60)
    assert not compliment(plain)
    print("card:", card["age"], "|", paras[0][:80])
    print("tiers seen:", sorted({BODY.match(e["body"].split("\n\n")[0]).group("tier")
                                 for e in compliment(doc)}))
    print("ok")


main()
