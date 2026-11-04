"""415: actual helper cursor algebra offline; no Frida/device execution."""
import ast
import json
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/"blh.js").read_text()
assert "function captureAppend(" in source,"capture append ownership missing"
helpers=source[source.index("var captureState="):source.index("function E(n)")]
checks=r'''
const assert=require("node:assert/strict");
for(const [name,cap] of Object.entries({logs:3000,scenarios:1000,ticks:400})){
 let baseline=captureRead(null);assert.equal(baseline.v,1);assert.equal(baseline.streams[name].next,0);
 for(let i=0;i<cap;i++)captureAppend(name,{text:"same",t:1});
 let filled=captureRead(baseline);assert.equal(filled.streams[name].records.length,cap);assert.equal(filled.streams[name].dropped,0);
 let cursor=captureRead(null);captureAppend(name,{text:"new",t:1});
 let moved=captureRead(cursor);assert.equal(moved.streams[name].next,cap+1);assert.equal(moved.streams[name].records.length,1);assert.equal(moved.streams[name].start,cap);assert.equal(moved.streams[name].records[0].text,"new");
 assert.deepEqual(captureRead(cursor),moved); // reads/retry never consume
 let overflow=captureRead(baseline);assert.equal(overflow.streams[name].dropped,1);assert.equal(overflow.streams[name].records.length,cap);
 let future=JSON.parse(JSON.stringify(cursor));future.streams[name].next=cap+2;assert.equal(captureRead(future).error,"invalid-cursor");
 captureClear();assert.equal(captureRead(cursor).error,"epoch-mismatch");assert.equal(captureRead(null).generation,cursor.generation+1);
 assert.equal(st[name].length,0);
}
for(const bad of [{},false,[],{v:2}, {v:1,session:"fixture",generation:3,streams:{}}])assert.equal(captureRead(bad).error,"invalid-cursor");
let old=captureRead(null);captureState.session="reloaded";assert.equal(captureRead(old).error,"epoch-mismatch");
captureState.next.logs=Number.MAX_SAFE_INTEGER;assert.throws(()=>captureAppend("logs",{}),/exhausted/);
captureState.generation=Number.MAX_SAFE_INTEGER;assert.throws(()=>captureClear(),/exhausted/);
captureState.session=null;assert.equal(captureRead(null).error,"unsupported-session");
console.log("ok: caps,append IDs,retry,exact overflow,clear/reload and malformed/future cursors");
'''
subprocess.run(["node","-e",'var CAPTURE_SESSION="fixture";var st={logs:[],scenarios:[],ticks:[],ageTicks:[]};\n'+helpers+checks],check=True)
# Four real producer sites, not a test-only stream implementation.
assert source.count("captureAppend('logs',")==2
assert source.count("captureAppend('scenarios',")==1
assert source.count("captureAppend('ticks',")==1
assert "capture:function(cursor){ return captureRead(cursor); }" in source
assert "clear:function(){ return captureClear(); }" in source
# Execute actual create_script expressions only, never import live attachment scripts.
for name in ("blh_hold.py","blh_attach.py"):
    tree=ast.parse((ROOT/name).read_text())
    calls=[n for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and n.func.attr=="create_script"]
    assert len(calls)==2
    import io,uuid
    class Session:
        def create_script(self,text):return text
    for call in calls:
        code=compile(ast.Expression(call),str(ROOT/name),"eval")
        env=dict(s=Session(),json=json,uuid=uuid,open=lambda path:io.StringIO("// fixture"))
        first=eval(code,env);second=eval(code,env)
        assert first!=second and first.startswith("var CAPTURE_SESSION = ")
        assert first.endswith("\n// fixture")
print("ok: real producers,legacy clear and unique sessions at initial/reload attachment sites")
