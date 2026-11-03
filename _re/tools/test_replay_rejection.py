"""A rejected forced scenario/row must fail, never silently simulate an unrelated year."""
import json
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'port/build/blsim'
for push in ({'scenario':'ScenarioDefinitelyNotImplemented','choice':0},
             {'table':'a16_nonexistent-parity-fixture-enc','row':1},
             {'table':'a16_adoption-scenarios-enc','row':999999},
             {'table':'a16_adoption-scenarios-enc','row':9223372036854775807}):
    spec=dict(seed=7,country='United States',gender='male',ages=1,push=[push])
    p=subprocess.run([str(BIN),'--script','/dev/stdin'],cwd=ROOT,
                     input=json.dumps(spec),capture_output=True,text=True)
    assert p.returncode!=0, (push,p.returncode,p.stdout,p.stderr)
    assert not p.stdout.strip(), 'failed replay must not emit a successful trace'
    assert 'push[0]' in p.stderr, p.stderr
# Existing explicit-empty push means clear the override, not reject the run.
for push in ([],[None],[{'scenario':'ScenarioWitchDoctor','choice':0}]):
    spec=dict(seed=7,country='United States',gender='male',ages=1,push=push)
    p=subprocess.run([str(BIN),'--script','/dev/stdin'],cwd=ROOT,
                     input=json.dumps(spec),capture_output=True,text=True)
    assert p.returncode==0, p.stderr
    result=json.loads(p.stdout)
    assert result['events'], result
    if push and isinstance(push[0],dict) and push[0].get('scenario'):
        assert result['events'][0]['title']=='Witch Doctor', result
# A failed second request must not flush the first event as a successful trace.
spec=dict(seed=7,country='United States',gender='male',ages=2,
          push=[{'scenario':'ScenarioWitchDoctor','choice':0},{'scenario':'ScenarioDefinitelyNotImplemented'}])
p=subprocess.run([str(BIN),'--script','/dev/stdin'],cwd=ROOT,
                 input=json.dumps(spec),capture_output=True,text=True)
assert p.returncode!=0 and not p.stdout.strip() and 'push[1]' in p.stderr, (p.stdout,p.stderr)
# Verify the validator accepts the last logical row, not only row1.
import csv
with (ROOT/'gamedata_plain/a16_adoption-scenarios-enc').open(newline='') as stream:
    last=len(list(csv.reader(stream)))-1
spec=dict(seed=7,country='United States',gender='male',ages=1,
          push=[{'table':'a16_adoption-scenarios-enc','row':last}])
p=subprocess.run([str(BIN),'--script','/dev/stdin'],cwd=ROOT,
                 input=json.dumps(spec),capture_output=True,text=True)
assert p.returncode==0, p.stderr
events=json.loads(p.stdout)['events']
assert events[0]['table']=='a16_adoption-scenarios-enc' and events[0]['row']==last, events
print('ok: rejected class/table/row fail closed; extreme/last row and second-request abort; null and known scenario retained')
