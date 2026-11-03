"""P0.3: missing ages and divergence must never become a successful verifier exit."""
import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('trace_diff',ROOT/'tools/diff.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def age(n,logs=None):
    return dict(age=n,logs=[] if logs is None else logs,scenarios=[],
                stats=dict(happiness=50,health=50,smarts=50,looks=50,karma=50,discipline=50))

game=dict(ages=[age(1),age(2)])
port=dict(ages=[age(1)])
rep=module.compare(game,port)
assert rep['first_divergence']['stream']=='age-coverage',rep
assert rep['first_divergence']['age']==2,rep
assert (rep['ages_game'],rep['ages_port'])==(2,1),rep
assert module.compare(game,dict(ages=[]))['first_divergence']['age']==1
assert module.compare(game,game)['first_divergence'] is None
# Diagnostic comparison is scoped to the captured game window.
assert module.compare(game,dict(ages=[age(0),age(1),age(2),age(3)]))['first_divergence'] is None
# Scenario cardinality is an advertised stream even when logs/stats match.
g=age(1); p=age(1)
g['scenarios']=[dict(title='Card',body='Body',cls='ScenarioFixture')]
assert module.compare(dict(ages=[g]),dict(ages=[p]))['first_divergence']['stream']=='scenarios'
assert module.compare(dict(ages=[p]),dict(ages=[g]))['first_divergence']['stream']=='scenarios'
# Extra port logs are a divergence too, not only absent game logs.
assert module.compare(dict(ages=[age(1)]),dict(ages=[age(1,['extra event'])]))['first_divergence']['stream']=='logs'
# Advertised stream priority holds across ages: logs outrank an earlier stat deviation.
gs=[age(1),age(2),age(3,['required event'])]
ps=[age(1),age(2),age(3)]
ps[1]['stats']['health']=20
assert module.compare(dict(ages=gs),dict(ages=ps))['first_divergence']['stream']=='logs'
# Structurally incomplete, duplicate or empty observations cannot pass by omission.
for trace in (dict(ages=[]),dict(ages=[age(1),age(1)]),dict(ages=[dict(age=1,logs=[],scenarios=[])])):
    try:
        module.compare(trace,dict(ages=[age(1)]))
    except ValueError:
        pass
    else:
        raise AssertionError(('incomplete reference trace accepted',trace))
assert rep["comparison_mode"] == "diagnostic" and rep["parity_certified"] is False
for field, value in (("age", True), ("age", -1), ("logs", [None]),
                     ("scenarios", ["not a scenario"]), ("stats", {})):
    invalid = age(1)
    invalid[field] = value
    for left, right in ((invalid, age(1)), (age(1), invalid)):
        try:
            module.compare(dict(ages=[left]), dict(ages=[right]))
        except ValueError:
            pass
        else:
            raise AssertionError(("malformed observation accepted", field, value))
for value in (True, float("nan"), float("inf"), "50"):
    invalid = age(1)
    invalid["stats"]["health"] = value
    try:
        module.compare(dict(ages=[age(1)]), dict(ages=[invalid]))
    except ValueError:
        pass
    else:
        raise AssertionError(("invalid stat accepted", value))
with tempfile.TemporaryDirectory() as directory:
    g=Path(directory)/'game.json'; p=Path(directory)/'port.json'
    g.write_text(json.dumps(game)); p.write_text(json.dumps(port))
    command=[sys.executable,str(ROOT/'tools/diff.py')]
    result=subprocess.run(command+[str(g),str(p)],capture_output=True,text=True)
    assert result.returncode==1,(result.returncode,result.stdout,result.stderr)
    assert json.loads(result.stdout)['first_divergence']['stream']=='age-coverage'
    p.write_text(json.dumps(game))
    result=subprocess.run(command+['--stat-tol','0',str(g),str(p)],capture_output=True,text=True)
    assert result.returncode==0,(result.returncode,result.stderr)
    assert json.loads(result.stdout)['first_divergence'] is None
    # A log divergence also exits1; do not special-case only missing ages.
    p.write_text(json.dumps(dict(ages=[age(1,['a different event']),age(2)])))
    g.write_text(json.dumps(dict(ages=[age(1,['the required event']),age(2)])))
    result=subprocess.run(command+[str(g),str(p)],capture_output=True,text=True)
    assert result.returncode==1,(result.returncode,result.stdout,result.stderr)
    # Verbose mode is consumed as JSON-lines; neither summary nor detail may be truncated.
    result=subprocess.run(command+[str(g),str(p),'-v'],capture_output=True,text=True)
    records=[json.loads(line) for line in result.stdout.splitlines()]
    assert records[0]['first_divergence']['stream']=='logs'
    assert any(x.get('stream')=='stats' for x in records)
    # Invalid invocations/data fail as input errors without a success report.
    for options in (['--stat-tol','-1',str(g),str(p)], [str(g),str(p)+'.missing']):
        result=subprocess.run(command+options,capture_output=True,text=True)
        assert result.returncode==2 and not result.stdout.strip(),result
print('ok: missing-age domain, matching traces, divergence exit1, invalid input exit2, option ordering')
