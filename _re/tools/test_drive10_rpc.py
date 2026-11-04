"""416: cursor capture uses actual RPC encoder; no device-side driver imports."""
import ast
import copy
import importlib.util
import io
import json
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
driver=ast.parse((ROOT/"drive10.py").read_text())
functions=[n for n in driver.body if isinstance(n,ast.FunctionDef) and n.name in ("capture_cursor","capture_fetch","capture_record")]
assert len(functions)==3,"cursor capture functions missing"
spec=importlib.util.spec_from_file_location("capture_rpc",ROOT/"blhrpc.py")
rpc=importlib.util.module_from_spec(spec);spec.loader.exec_module(rpc)
namespace={"blhrpc":rpc}
exec(compile(ast.Module(body=functions,type_ignores=[]),str(ROOT/"drive10.py"),"exec"),namespace)
fetch=namespace["capture_fetch"];cursor_of=namespace["capture_cursor"]
def response(next=3000,first=0):
    return dict(v=1,session="session",generation=0,streams={name:dict(first=first if name=="logs" else 0,next=next if name=="logs" else 0) for name in ("logs","scenarios","ticks")})
baseline=response()
def delta(base,next=3001,first=1):
    result=response(next,first)
    for name,row in result["streams"].items():
        start=max(row["first"],base["streams"][name]["next"])
        row.update(start=start,dropped=max(0,row["first"]-base["streams"][name]["next"]),records=[dict(text="same",t=1) for i in range(row["next"]-start)])
    return result
requests=[];pending=[]
def exchange(payload,base=None):
    def memory_open(path,mode="r"):
        if mode=="w":
            assert str(path).startswith("/tmp/blh_req.");s=io.StringIO();pending.append(s);return s
        request=json.loads(pending[-1].getvalue());requests.append(request)
        assert request==dict(op="x_capture",args=[base]),request
        return io.StringIO(json.dumps(payload))
    with patch.object(rpc,"open",memory_open,create=True),patch.object(rpc.os.path,"exists",return_value=True),patch.object(rpc.os,"remove"):
        return fetch(base)
assert exchange(baseline)==baseline
moved=delta(baseline);assert exchange(moved,baseline)==moved
assert exchange(moved,baseline)==moved # retry keeps original cursor
assert cursor_of(moved)["streams"]["logs"]==dict(next=3001)
assert "records" not in json.dumps(cursor_of(moved))
overflow=delta(response(0),3001,1)
assert exchange(overflow,response(0))["streams"]["logs"]["dropped"]==1
invalid=[None,[],dict(error="timeout"),dict(error="unknown op"),dict(error="epoch-mismatch")]
for field,value in (("v",2),("session","reloaded"),("generation",1)):
    bad=copy.deepcopy(moved);bad[field]=value;invalid.append(bad)
for field,value in (("next",True),("next",2999),("first",-1),("start",3001),("dropped",1),("records",[])):
    bad=copy.deepcopy(moved);bad["streams"]["logs"][field]=value;invalid.append(bad)
for payload in invalid:
    try:exchange(payload,baseline)
    except ValueError:pass
    else:raise AssertionError(("invalid cursor response accepted",payload))
assert baseline==response(),"poll/fetch mutated accepted cursor"
# Run the actual top-level driver tail only with every action/transport in memory.
from types import SimpleNamespace
start=next(i for i,n in enumerate(driver.body) if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=="T0" for t in n.targets))
loop=compile(ast.Module(body=driver.body[start:],type_ignores=[]),str(ROOT/"drive10.py"),"exec")
def run_loop(payloads, action_error=None):
    writes=[];actions=[];sent=[];queue=iter(payloads)
    def transport(op,**kw):
        if op=="hero":return dict(age=46)
        assert op=="x_capture";sent.append(kw["args"][0]);return next(queue)
    def output(path,mode):
        assert mode=="w";stream=io.StringIO();writes.append(stream);return stream
    def action(db):
        actions.append("step")
        if action_error is not None:raise action_error
        return "step"
    ns=dict(namespace,blhrpc=SimpleNamespace(rpc=transport),N=1,time=SimpleNamespace(time=lambda:0,sleep=lambda _:None),
            step=action,recover=lambda i,db:actions.append("recover") or "recover",
            LAST=[None],DTRIES=[0],age_of=lambda h:h["age"],json=json,sys=__import__("sys"),open=output,print=lambda *a,**kw:None)
    # Rebind actual functions to this isolated driver's namespace.
    exec(compile(ast.Module(body=functions,type_ignores=[]),"driver-functions","exec"),ns)
    code=0
    try:exec(loop,ns)
    except SystemExit as error:code=error.code
    assert writes,"partial trace not saved"
    return code,json.loads(writes[-1].getvalue()),actions,sent
code,saved,actions,sent=run_loop([baseline,dict(error="timeout")])
assert code==1 and len(actions)==1 and saved["capture_complete"] is False
assert saved["rounds"][-1]["capture_error"]
# Poll and final fetch both use the same cursor; later final appends are retained.
final=delta(baseline,3002,2)
code,saved,actions,sent=run_loop([baseline,moved,final])
assert code==0 and len(actions)==1 and saved["capture_complete"] is True
assert sent[1]==sent[2] and sent[1]["streams"]["logs"]["next"]==3000
assert len(saved["rounds"][0]["logs"])==2
code,saved,actions,sent=run_loop([baseline,moved,dict(error="timeout")])
assert code==1 and len(saved["rounds"][-1]["logs"])==1 and saved["capture_complete"] is False
code,saved,actions,sent=run_loop([dict(error="unsupported-session")])
assert code==1 and not actions and saved["capture_complete"] is False
code,saved,actions,sent=run_loop([response(0),overflow,overflow])
assert code==1 and saved["capture_complete"] is False
assert saved["rounds"][0]["capture"]["streams"]["logs"]["dropped"]==1
code,saved,actions,sent=run_loop([baseline],KeyboardInterrupt("interrupted"))
assert code==1 and saved["capture_complete"] is False
print("ok: actual cursor RPC,driver polling/final-fetch,overflow and failure persistence")
