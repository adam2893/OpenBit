"""A relative refuses a money request (REPORT.en.md 202).

a16_money-request-rejections-enc, 34 rows. The row flags which relatives it can be
about (Parent/Sibling/Lover/Child), which is how <person> is answered: the flag picks
the relation, the relation picks an actor the port already has, and the actor's gender
sets the pronoun pair so <him/her> agrees.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"
KIN = ("my mother", "my father", "my sister", "my brother", "my girlfriend", "my boyfriend")


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": 60,
            "relationship": True}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
for seed in range(1, 31):
    for event in run(seed)["events"]:
        if event.get("table") != "a16_money-request-rejections-enc":
            continue
        cards += 1
        body = event["body"]
        journal = (event.get("logLines") or [""])[-1]
        # No token survives, no empty name slot, and a relative is named.
        for text in (body, journal):
            assert "<" not in text and ">" not in text, (seed, text)
            assert ", ," not in text and " ," not in text, (seed, text)
        assert any(k in body.lower() or k in journal.lower() for k in KIN), (seed, body)
        # A relative's pronoun agrees: "my mother said he" is the failure this catches.
        if "mother" in body.lower() or "girlfriend" in body.lower() or "sister" in body.lower():
            assert " said he " not in body.lower(), (seed, body)

assert cards >= 5, cards
print("cards:", cards)
print("ok")