"""251: the ex-lover's paternity test -- asked, taken, and answered.

The played-life trace at 43 has all four of the chain's lines:
'My ex-fling, Ariana, asked me to take a paternity test for her 2-year old son,
Cleveland. / I agreed to take a paternity test.', 'The paternity test for my
ex-fling, Ariana's, 2-year old son came back positive!', and 'I became the legal
father of Cleveland Jenner.'
"""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"

ASK = re.compile(r"^My (ex-girlfriend|ex-boyfriend|ex-wife|ex-husband), (.+?), asked me to take "
                 r"a paternity test for her (\d+)-year old (son|daughter), (.+?)\.")
RESULT = re.compile(r"^The paternity test for my (ex-girlfriend|ex-boyfriend|ex-wife|ex-husband), "
                    r"(.+?)'s, (\d+)-year old (son|daughter) came back (positive|negative)!$")
LEGAL = re.compile(r"^I became the legal father of (.+?) (.+?)\.$")


def run(seed, ages=60):
    spec = {"seed": seed, "country": "United States", "gender": "male", "ages": ages,
            "relationship": True, "ex": True, "children": 1}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


cards = 0
asked = 0
results = 0
positive = 0
for seed in range(1, 41):
    pending = None  # (ex, kid age, son/daughter) from the ask
    for event in run(seed)["events"]:
        lines = event.get("logLines") or []
        if event.get("title") == "Paternity Test":
            cards += 1
            assert event["choice"] in ("I agree to take a paternity test", "Refuse the test"), event
            if event["choice"] != "I agree to take a paternity test":
                continue
            asked += 1
            # The ask and the agreement are one log string with a line break:
            # the trace keeps them apart only in its own rendering.
            deep = [l for l in lines if ASK.match(l.split(chr(10))[0])]
            assert deep, (seed, lines)
            m = ASK.match(deep[0].split(chr(10))[0])
            pending = m.groups()
            assert "I agreed to take a paternity test." in deep[0], (seed, deep[0])
            continue
        got = [l for l in lines if RESULT.match(l)]
        if got:
            results += 1
            m = RESULT.match(got[0])
            ex_word, ex_name, age, kind, verdict = m.groups()
            assert pending is not None, (seed, "a result without an ask", got[0])
            want = pending
            assert (ex_word, ex_name, age, kind) == (want[0], want[1], want[2], want[3]), (seed, got[0], want)
            if verdict == "positive":
                positive += 1
                legal = [l for l in lines if LEGAL.match(l)]
                assert legal, (seed, "a positive result without the legal-father line", lines)
            else:
                assert not [l for l in lines if LEGAL.match(l)], (seed, "negative but legal father", lines)
            pending = None

assert cards >= 10, cards
assert asked >= 5, asked
assert results >= 4, results
assert positive >= 1, positive
print("cards:", cards, "agreed:", asked, "results:", results, "positive:", positive)
print("ok")
