"""The OnlyFans channel (REPORT.en.md 225).

The port had no channel, and a16_social-media-channels-onlyfans-tip-responses-enc is
the only card the channel owns: seven rows, each 'Send <him/her> a ... video'. A tip
arrives, the hero picks the thank-you, and '<him/her>' follows the fan's gender.
"""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "port" / "build" / "blsim"


def run(seed):
    spec = {"seed": seed, "country": "United States", "gender": "female", "ages": 40}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(spec, fh)
        path = fh.name
    proc = subprocess.run([str(PORT), "--script", path], text=True,
                          capture_output=True, cwd=str(ROOT / "port"))
    Path(path).unlink()
    assert proc.returncode == 0, proc.stderr[-300:]
    return json.loads(proc.stdout)


starts = 0
tips = 0
for seed in range(1, 41):
    started = False
    for event in run(seed)["events"]:
        table = event.get("table") or ""
        if table == "a16_social-media-channels-enc":
            starts += 1
            assert event["choice"] in ("Start a channel", "Not for me"), (seed, event["choice"])
            if event["choice"] == "Start a channel":
                started = True
        elif table == "a16_social-media-channels-onlyfans-tip-responses-enc":
            tips += 1
            choice = event["choice"]
            assert choice.startswith("Send "), (seed, choice)
            assert "<" not in choice and "<" not in event["body"], (seed, choice)
            assert " her " in choice or " him " in choice, (seed, choice)
            assert started, (seed, "a tip without a channel")
            assert not event.get("residue"), (seed, event.get("residue"))

assert starts >= 3, starts
assert tips >= 2, tips
print("starts:", starts, "tips:", tips)
print("ok")