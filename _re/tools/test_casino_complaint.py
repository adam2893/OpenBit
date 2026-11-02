"""322: the casino gambler complaint card (ScenarioCasinoAgeUpGamblerComplaintGame)."""
import csv
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
DATA = ROOT / "gamedata_plain"

TABLE = "registry:ScenarioCasinoAgeUpGamblerComplaintGame"
TIERS = ("casual gambler", "high roller")  # REPORT 358: the type words, not the rank ladder
BODY = re.compile(r"^Your casino's (?P<tier>\w+(?: \w+)?), (?P<name>.+?), is complaining "
                  r"that your casino doesn't have (?:his|her) favorite game, (?P<game>.+)\.$")


def run(seed=7, ages=40, choices=None, cash=3000000, bank=1000000000):
    spec = {"seed": seed, "country": "United States", "gender": "male",
            "ages": ages, "cash": cash, "casino": True, "only": "casino-complaint",
            "casinoBank": bank}
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


def first_line(event):
    return (event.get("body") or "").split("\n\n")[0]


def cards(doc):
    # A ban appends the banned-gambler snippet and re-points the event at that table, so
    # the card is recognised by the first paragraph's recovered shape.
    return [e for e in doc["events"] if BODY.match(first_line(e))]


def forced(seed, want, ages=40):
    """The harness answers one card per year; pad until the poker of the tick lands."""
    for pad in range(0, 80):
        for idx in range(3):
            doc = run(seed=seed, ages=ages, choices=[-1] * pad + [idx])
            found = cards(doc)
            if found and found[0]["choice"].startswith(want):
                return doc, found[0]
    raise AssertionError("the card never took the forced index")


def main():
    doc = run()
    found = cards(doc)
    assert found, "the complaint card never fired on a solvent casino"
    card = found[0]
    m = BODY.match(first_line(card))
    assert m, card["body"]
    assert m.group("tier") in TIERS, m.group("tier")
    assert m.group("game"), card["body"]
    assert card["choice"].startswith(("Ban ", "Apologize", "Ignore ")), card["choice"]

    # The three buttons are the native labels; the two short ones carry the gambler's
    # name, which is why the literals end in a space (#12770, #26366).
    ban_doc, ban = forced(7, "Ban ")
    name = BODY.match(first_line(ban)).group("name")
    assert ban["body"].startswith("Your casino's "), ban["body"]
    # The mood table's own line always follows the sentence; a ban adds the
    # banned-gambler snippet after it.
    paras = ban["body"].split("\n\n")
    assert len(paras) in (2, 3), ban["body"]
    mood = [r["String"] for r in
            csv.DictReader(open(DATA / "a16_casino-casino-game-room-complaint-enc",
                                newline=""))]
    # The cells carry <he/she> and <a/b/c> tokens; the card resolves them, so each
    # token becomes a wildcard and the rest of the cell must match verbatim.
    def cell_hits(cell, text):
        parts = re.split(r"<[^>]+>", cell)
        pattern = r".+?".join(re.escape(part) for part in parts)
        return re.fullmatch(pattern, text) is not None
    assert any(cell_hits(r, paras[1]) for r in mood), paras[1]
    assert paras[1][:1].isupper(), paras[1]
    if len(paras) == 3:
        snippet = paras[2]
        assert re.match(r"^(She|He) .+\.$", snippet), snippet
        table = list(csv.DictReader(open(DATA / "a16_casino-banned-gambler-snippet-enc",
                                          newline="")))
        stems = {re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", r["Snippet"]).strip())
                 .lower()[:16] for r in table}
        assert any(stem in snippet.lower() for stem in stems), snippet
    assert ban["deltas"] == {"karma": 0, "discipline": 0, "happiness": 0, "health": 0,
                             "smarts": 0, "looks": 0, "fame": 0}, ban["deltas"]

    ap_doc, ap = forced(7, "Apologize")
    assert ap["choice"] == "Apologize for the inconvenience", ap["choice"]
    assert BODY.match(first_line(ap)), ap["body"]

    ig_doc, ig = forced(7, "Ignore ")
    assert ig["choice"].startswith("Ignore "), ig["choice"]
    assert BODY.match(first_line(ig)), ig["body"]

    # A life without a casino never sees the card.
    plain = {"seed": 7, "country": "United States", "gender": "male", "ages": 40,
             "cash": 3000000}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(plain, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    assert not cards(json.loads(proc.stdout)), "a hero without a casino got a complaint"

    # A closed casino cannot be complained about: the bankrupt card leaves the hero with
    # no floor, so the complaint family has nothing to gate on (320, 321).
    closed = run(bank=-1000000000)
    first = [e for e in closed["events"] if e.get("title") == "Bankrupt"]
    assert first, "the bankruptcy card did not fire on a drowning casino"
    assert not [e for e in cards(closed) if e["age"] > first[0]["age"]], \
        "a closed casino still drew a complaint"

    print("card:", ig["age"], "|", ig["body"][:90])
    print("tiers seen:", sorted({BODY.match(first_line(c)).group("tier") for c in cards(run(ages=70))}))
    print("ok")


main()
