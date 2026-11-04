"""Demo stat arithmetic and played/scripted annual illness health stay consistent."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "port/build/blsim"
spec = dict(seed=7, country="United States", gender="male", ages=3)
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "life.json"
    path.write_text(json.dumps(spec))
    scripted = subprocess.run([str(BIN), "--script", str(path)], cwd=ROOT,
                              capture_output=True, text=True)
    assert scripted.returncode == 0, scripted.stderr
    life = json.loads(scripted.stdout)
    assert life["final"]["age"] == spec["ages"] and life["final"]["alive"], life["final"]
    assert any(event["ailmentCost"] > 0 for event in life["events"]), "fixture must exercise annual illness damage"
    played = subprocess.run([str(BIN), "--script", str(path), "--play"], cwd=ROOT,
                            input=chr(10)*30, capture_output=True, text=True)
    assert played.returncode == 0, played.stderr
    health = re.findall(r"health ([0-9]+)", played.stdout)
    ages = re.findall(r"lived to ([0-9]+)", played.stdout)
    assert health and ages, played.stdout
    assert int(health[-1]) == life["final"]["stats"]["health"], (health[-1], life["final"], played.stdout)
    assert int(ages[-1]) == life["final"]["age"], (ages, life["final"])
    assert ("*** death at " not in played.stdout) == life["final"]["alive"]
# Compile the actual demo application block, not a rewritten copy of its arithmetic.
demo = (ROOT / "port/src/main.cpp").read_text()
start = demo.index("    bl::Hero& hh = life.hero;")
stop = demo.index('    std::cout << "  age "', start)
block = demo[start:stop]
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "demo_stats.cpp"
    binary = Path(directory) / "demo_stats"
    source.write_text('#include "bl/sim.h"\n#include <cassert>\nvoid apply(bl::Life& life, const bl::AgeUp& ev){\n' + block + r'''}
int main() {
  const int cases[][4] = {{95,0,13,82},{5,2,10,0},{95,20,10,100},
                         {95,10,13,92},{82,0,0,82},{82,7,0,89}};
  for (const auto& c : cases) {
    bl::Life life; life.hero.health=c[0];
    bl::AgeUp event; event.health=c[1]; event.ailmentCost=c[2];
    apply(life,event);
    assert(life.hero.health==c[3]);
  }
}
''')
    subprocess.run(["c++","-std=c++17","-I"+str(ROOT/"port/include"),str(source),
                    str(ROOT/"port/build/libblport.a"),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print("ok: played/script health agree; actual demo block applies illness cost before lower/upper clamping")
