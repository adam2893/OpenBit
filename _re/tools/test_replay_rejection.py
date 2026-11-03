"""A rejected forced scenario/row must fail, never silently simulate an unrelated year."""
import json
import subprocess
import sys
import tempfile
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
# Registry replay choices must not silently fall back to another branch.
for scenario, count in (("ScenarioOfferFood",2), ("ScenarioChildhoodVaccination",3),
                        ("ScenarioDiseaseContract",4), ("ScenarioCasinoBet",1),
                        ("ScenarioWitchDoctor",1), ("ScenarioFriendNew",0),
                        ("ScenarioScriptEventParentsBreakUp",0),
                        ("ScenarioScriptEventParentMarriesStepparent",0)):
    for choice in (-2,count,2147483647):
        spec=dict(seed=7,country='United States',gender='male',ages=1,
                  push=[dict(scenario=scenario,choice=choice)])
        p=subprocess.run([str(BIN),'--script','/dev/stdin'],cwd=ROOT,
                         input=json.dumps(spec),capture_output=True,text=True)
        assert p.returncode!=0 and not p.stdout.strip() and 'push[0]' in p.stderr, (scenario,choice,p.stdout,p.stderr)
    for choice in ((-1,) if count==0 else (-1,0,count-1)):
        spec['push'][0]['choice']=choice
        p=subprocess.run([str(BIN),'--script','/dev/stdin'],cwd=ROOT,
                         input=json.dumps(spec),capture_output=True,text=True)
        assert p.returncode==0,(scenario,choice,p.stderr)
        events=json.loads(p.stdout)['events']
        assert events,(scenario,choice)
        if scenario=='ScenarioOfferFood' and choice>=0:
            assert events[0]['choice']==('Join them','No thanks')[choice],events[0]
spec=dict(seed=7,country='United States',gender='male',ages=1,
          push=[dict(scenario='ScenarioOfferFood')])
default=subprocess.run([str(BIN),'--script','/dev/stdin'],cwd=ROOT,
                       input=json.dumps(spec),capture_output=True,text=True)
spec['push'][0]['choice']=-1
sentinel=subprocess.run([str(BIN),'--script','/dev/stdin'],cwd=ROOT,
                        input=json.dumps(spec),capture_output=True,text=True)
assert default.returncode==sentinel.returncode==0 and default.stdout==sentinel.stdout
for choice in (True,False,None,'1',0.5,1.0,4294967296,9223372036854775808):
    spec=dict(seed=7,country='United States',gender='male',ages=1,
              push=[dict(scenario='ScenarioOfferFood',choice=choice)])
    p=subprocess.run([str(BIN),'--script','/dev/stdin'],cwd=ROOT,
                     input=json.dumps(spec),capture_output=True,text=True)
    assert p.returncode!=0 and not p.stdout.strip() and 'push[0]' in p.stderr,(choice,p.stdout,p.stderr)
# The real producer must not mistake auto-scene text variants for button indices.
automatic = {
    'ScenarioFriendNew': 'I am now friends with Sam.',
    'ScenarioScriptEventParentsBreakUp': 'My parents have divorced.',
    'ScenarioScriptEventParentMarriesStepparent': 'My mother has married my new stepfather, Sam.',
    'ScenarioScriptEventSiblingBorn': 'My mother and my father had a baby girl named Sam, my new sister.',
    'ScenarioScriptEventDiseaseContract': 'My mother is no longer suffering from flu.',
    'ScenarioPetDeath': 'My cat, Sam, died while sleeping peacefully.',
}
with tempfile.TemporaryDirectory() as directory:
    trace=Path(directory)/'trace.json'; recovered=Path(directory)/'push.json'
    rows=[dict(age=i,logs=[line],scenarios=[dict(cls=cls,title='',body='')])
          for i,(cls,line) in enumerate(automatic.items(),1)]
    rows.append(dict(age=len(rows)+1,logs=['I started eating rice.'],
                     scenarios=[dict(cls='ScenarioOfferFood',title='',body='')]))
    trace.write_text(json.dumps(dict(ages=rows)))
    p=subprocess.run([sys.executable,str(ROOT/'tools/row_recover.py'),str(trace),
                      '--out',str(recovered),'--re',str(ROOT)],capture_output=True,text=True)
    assert p.returncode==0,(p.stdout,p.stderr)
    entries=json.loads(recovered.read_text())['push']
    assert entries[:-1]==[dict(scenario=cls) for cls in automatic],entries
    assert entries[-1]==dict(scenario='ScenarioOfferFood',choice=0),entries
    for entry in entries:
        spec=dict(seed=7,country='United States',gender='male',ages=1,push=[entry])
        p=subprocess.run([str(BIN),'--script','/dev/stdin'],cwd=ROOT,
                         input=json.dumps(spec),capture_output=True,text=True)
        assert p.returncode==0 and json.loads(p.stdout)['events'],(entry,p.stdout,p.stderr)
print('ok: rejected replay choices fail closed; valid/malformed boundaries, lifecycle abort and producer auto-scenes preserved')
