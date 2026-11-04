"""414: strict observed text never loses order, multiplicity or raw spelling."""
import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path
import diff
import game_trace
import port_trace

ROOT = Path(__file__).resolve().parents[1]
MARK = "source-array-order-v1"
def trace(lines):
    return dict(log_provenance=MARK, ages=[dict(age=1,logs=lines,raw_logs=lines,scenarios=[],stats={k:50 for k in diff.STAT_KEYS})])
def strict(g,p):
    return diff.compare(g,p,strict_logs=True)

# Actual producer boundary first: existing diagnostic de-dup must not erase evidence.
raw=[dict(age=1,round=1,logs=["same","same",""],scenarios=[],hero={"Att_"+k:50 for k in ("happiness","health","intelligence","appearance","karma","discipline")})]
g=game_trace.normalize(raw)
assert g.get("log_provenance")==MARK,g
assert g["ages"][0]["raw_logs"]==["same","same",""]
assert g["ages"][0]["logs"]==["same"]
for left,right,index in ((["a","b"],["b","a"],0),(["a","a"],["a"],1),(["$10"],["$90"],0),(["Alice"],["Bob"],0),([""],[],0),(["A\nB"],["A B"],0),(["é"],["e"],0)):
    rep=strict(trace(left),trace(right))
    d=rep["first_divergence"]
    assert d["stream"]=="logs" and d["index"]==index,rep
    assert (d["game_count"],d["port_count"])==(len(left),len(right))
    assert d["game_text"]==(left[index] if index<len(left) else None)
    assert d["port_text"]==(right[index] if index<len(right) else None)
    assert rep["comparison_mode"]=="strict-logs" and rep["parity_certified"] is False
for lines in ([],[""],["a","a"],["A\nB"]):
    assert strict(trace(lines),trace(lines))["first_divergence"] is None
assert diff.compare(trace(["$10"]),trace(["$90"]))["first_divergence"] is None
assert diff.compare(trace(["a","b"]),trace(["b","a"]))["first_divergence"] is None
for mutate in (lambda t:t.update(capture_complete=False),lambda t:t.pop("log_provenance"),lambda t:t.update(log_provenance=True),lambda t:t["ages"][0].pop("raw_logs"),lambda t:t["ages"][0].update(raw_logs=[None])):
    invalid=trace([]);mutate(invalid)
    for a,b in ((invalid,trace([])),(trace([]),invalid)):
        try:strict(a,b)
        except ValueError:pass
        else:raise AssertionError(("invalid strict input accepted",a,b))
try:diff.compare(trace([]),trace([]),strict_logs=1)
except ValueError:pass
else:raise AssertionError("nonboolean strict mode accepted")
# Logs retain stream priority across ages, while missing reference ages win first.
g=trace(["same"]);p=trace(["same"])
g["ages"].append(dict(g["ages"][0],age=2,logs=["a"],raw_logs=["a"]))
p["ages"].append(dict(p["ages"][0],age=2,logs=["b"],raw_logs=["b"]))
p["ages"][0]["scenarios"]=[{}]
assert strict(g,p)["first_divergence"]["stream"]=="logs"
p["ages"].pop()
assert strict(g,p)["first_divergence"]["stream"]=="age-coverage"
incomplete=game_trace.normalize(dict(rounds=raw,capture_complete=False,capture_error="overflow"))
assert incomplete.get("capture_complete") is False and "log_provenance" not in incomplete
legacy=trace([]);legacy.pop("log_provenance");legacy["source"]="game"
assert "log_provenance" not in game_trace.normalize(legacy)
for rounds in (raw+raw,[dict(raw[0],logs=None)],[dict(raw[0],logs=[{}])]):
    assert "log_provenance" not in game_trace.normalize(rounds)
out=dict(birth=[],hero=dict(stats={k:50 for k in diff.STAT_KEYS}),events=[dict(age=1,deltas={},logLines=["same","same",""])])
p=port_trace.normalize(out)
assert p["log_provenance"]==MARK and p["ages"][0]["raw_logs"]==["same","same",""]
missing_birth=copy.deepcopy(out);missing_birth.pop("birth")
assert "log_provenance" not in port_trace.normalize(missing_birth)
for value in (None,[None]):
    bad=copy.deepcopy(out);bad["events"][0]["logLines"]=value
    assert "log_provenance" not in port_trace.normalize(bad)
with tempfile.TemporaryDirectory() as directory:
    gp=Path(directory)/"g.json";pp=Path(directory)/"p.json"
    for a,b,code in ((trace(["a","a"]),trace(["a"]),1),(trace([]),trace([]),0),(legacy,trace([]),2)):
        gp.write_text(json.dumps(a));pp.write_text(json.dumps(b))
        proc=subprocess.run([sys.executable,str(ROOT/"tools/diff.py"),str(gp),"--strict-logs",str(pp),"-v"],capture_output=True,text=True)
        assert proc.returncode==code,(proc.returncode,proc.stdout,proc.stderr)
        if code==2:assert not proc.stdout
        else:assert json.loads(proc.stdout.splitlines()[0])["parity_certified"] is False
print("ok: strict raw ordered occurrences, provenance rejection, producers and CLI exits")
