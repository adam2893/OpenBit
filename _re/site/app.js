const $ = (s, root=document) => root.querySelector(s);
const esc = s => String(s ?? "").replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
let DATA, tab="all", shown=60, query="", kind="all", selected=null;

const queueSeeds = [
  {title:"Complete AddYear string appender growth contract",lane:"UI / LIFE LOG",address:"0x2270ce4 → 0x2270d80",why:"Latest round recovered the appender body and exact UTF-16 write behavior. Heap growth/adjustment and allocation contract still gate composing a buildable source unit.",gate:"GROWTH + ALLOCATION",evidence:["tagged_utf16_append","tagged_buffer_prepare"],terms:["appender","growth","capacity","allocation"],rank:1},
  {title:"Recover adjustment and converter dependencies",lane:"UI / LIFE LOG",address:"0x2270b24 · 0x2274064",why:"The active checkpoint names both dependencies next. Their contracts determine whether the recovered string path can be safely integrated.",gate:"DEPENDENCY CLOSURE",evidence:["add_year_dependency_recovery_index"],terms:["adjustment","converter","2270b24","2274064"],rank:2},
  {title:"Close AddYear exception-site and cleanup coverage",lane:"EXCEPTION HANDLING",address:"LSDA / landing pads",why:"Named paths and partial cleanup effects exist, but complete exception-site coverage and ownership obligations remain open.",gate:"UNWIND + OWNERSHIP",evidence:["add_year_exceptions","exception_chain"],terms:["LSDA","exception","cleanup","ownership"],rank:3},
  {title:"Compose the actual AddYear source path",lane:"UI / LIFE LOG",address:"0x2ffaa44 · 3,080 bytes",why:"The native body and selected dependencies have bounded records. Keep whole-source acceptance gated by ABI, aliasing, allocation, and cleanup obligations.",gate:"SOURCE INTEGRATION",evidence:["add_year","add_year_dependency_recovery_index"],terms:["AddYear","integration","ABI","aliasing"],rank:4},
  {title:"Establish representative method denominator",lane:"WHOLE PROGRAM",address:"metadata / registration",why:"The target manifest explicitly leaves method/type layout incomplete. Until the denominator and exclusions are justified, global observed-block coverage is undefined.",gate:"DENOMINATOR",evidence:["target_manifest"],terms:["denominator","method","metadata","registration"],rank:5}
];

const laneSeeds = [
  {id:"add_year",title:"AddYear & UI life log",kind:"CONTROL FLOW",obs:"Native body, header construction, selected callees, layout accessors",work:"Source notes + bounded verifiers",state:"Active · integration gated",tone:"amber",search:"add year lifelog ui year log"},
  {id:"tagged_utf16_builder",title:"Tagged text buffers",kind:"STRING / MEMORY",obs:"UTF-8 scanner, decoder, UTF-16 emitter, append path",work:"Finite models + native byte pins",state:"Growth contract open",tone:"amber",search:"tagged utf16 buffer append decoder"},
  {id:"cfi_program_dispatch",title:"CFI / unwind runtime",kind:"EXCEPTIONS",obs:"LSDA, CFI, register rules, stack walker units",work:"Scoped recovery notes + verifiers",state:"Consumer & runtime open",tone:"amber",search:"cfi unwind exception landing pad"},
  {id:"generic_argument",title:"Generic containers & descriptors",kind:"RUNTIME SUPPORT",obs:"Lookup, relocation, resize, publication, cleanup chain",work:"Named dependency units + finite checks",state:"Semantics remain gated",tone:"amber",search:"generic argument descriptor cache"},
  {id:"metadata_record_decoder",title:"Metadata & method identity",kind:"IDENTITY / MAPPING",obs:"Pinned metadata records and conditional identity links",work:"Target manifest + evidence records",state:"No full method denominator",tone:"gray",search:"metadata method identity registration"},
  {id:"cxa_",title:"C++ exception support",kind:"NATIVE ABI",obs:"Throw, allocation, storage, cleanup, diagnostics",work:"Native body units + bounded verifiers",state:"Ownership & bindings open",tone:"amber",search:"cxa exception throw allocation"}
];

function noteBy(term){return DATA.notes.find(n=>n.id.includes(term));}
function init(){
  const v=DATA.verifier;
  const bytes=DATA.byteInventory;
  $("#target-id").textContent=DATA.target.id.toUpperCase();
  $("#generated-label").textContent="· ROUND "+v.round;
  $("#checkpoint-summary").textContent=DATA.current.checkpoint;
  $("#coverage-caveat").textContent=DATA.claims.coverage;
  $("#entity-count").textContent=DATA.counts.entities.toLocaleString();
  $("#function-count").textContent=DATA.counts.functions.toLocaleString();
  $("#byte-count").textContent=bytes.sizedFunctionBytes.toLocaleString();
  $("#byte-note").textContent=bytes.sizedFunctions+" of "+DATA.counts.functions+" function records have a size";
  $("#byte-coverage").textContent="Summed spans; not unique byte coverage";
  $("#check-count").innerHTML=`${v.checks}<small> / snapshot</small>`;
  $("#source-count").textContent=DATA.counts.sourceNotes;
  $("#open-count").textContent=DATA.counts.openItems.toLocaleString();
  $("#observe-copy").textContent=DATA.byteInventory.nativeBlocks+" named native blocks + "+DATA.byteInventory.landingPads+" landing pads; "+DATA.traces.length+" trace files indexed.";
  $("#lift-copy").textContent=DATA.cycles.lift;
  $("#build-copy").textContent=DATA.byteInventory.portUnits+" C++ units; scenario coverage belongs to the separate port track.";
  $("#verify-copy").textContent=DATA.cycles.verify;
  $("#diff-status").textContent=DATA.differential.status;
  $("#port-coverage").textContent=DATA.portScenarioCoverage.recoverable+" / "+DATA.portScenarioCoverage.captured+" scenarios · "+DATA.portScenarioCoverage.percent+"%";
  $("#run-line").textContent=`${v.checks} bounded checks · exit ${v.exitCode}`;
  $("#evidence-total").textContent=(DATA.counts.entities+DATA.notes.length).toLocaleString()+" RECORDS";
  $("#footer-source").textContent=`Source: decomp/ · snapshot date 13 Nov 2026 · round ${v.round}`;
  renderQueue(); renderLanes(); renderSearch(); renderTraces();
  $("#tab-all").textContent=(DATA.entities.length+DATA.notes.length).toLocaleString();
  $("#tab-functions").textContent=DATA.counts.functions;
  $("#tab-notes").textContent=DATA.notes.filter(n=>n.category!=="Native C++ port unit").length.toLocaleString();
  $("#tab-pocs").textContent=DATA.byteInventory.portUnits;
}

function renderTraces(){
 const root=$("#trace-tape");
 root.innerHTML=DATA.traces.map(t=>'<button class="trace-chip" data-trace="'+esc(t.name)+'"><span class="trace-signal"></span><span><b>'+esc(t.name)+'</b><small>'+(t.bytes/1024).toFixed(1)+' KB · JSON observation</small></span><span class="trace-arrow">↗</span></button>').join("")||'<div class="empty-state">No trace files were included in this snapshot.</div>';
 root.querySelectorAll("[data-trace]").forEach(el=>el.onclick=()=>{const n=DATA.notes.find(x=>x.id==="trace-"+el.dataset.trace.replace(/\.json$/,""));if(n)openDetail({...n,type:"note",kind:"trace"});});
}

function rankQueueFromBacklog(){
 const backlog=(DATA.openItems||[]).join("\n").toLowerCase();
 const current=DATA.current.checkpoint.toLowerCase();
 for(const q of queueSeeds){
   q.score=q.terms.reduce((sum,term)=>sum+Math.max(0,backlog.split(term.toLowerCase()).length-1),0);
   q.score+=q.terms.reduce((sum,term)=>sum+(current.includes(term.toLowerCase())?3:0),0);
 }
 queueSeeds.sort((a,b)=>b.score-a.score||a.rank-b.rank);
}

function renderQueue(){
  const list=$("#queue-list");
  list.innerHTML=queueSeeds.map((q,i)=>`<button class="queue-item ${selected===i?"selected":""}" data-queue="${i}"><span class="queue-rank">0${i+1}</span><span class="queue-main"><span class="queue-lane">${esc(q.lane)} <i>·</i> ${esc(q.address)}</span><b>${esc(q.title)}</b><small>${esc(q.why)}</small></span><span class="queue-gate">${esc(q.gate)}</span><span class="queue-arrow">↗</span></button>`).join("");
  list.querySelectorAll("[data-queue]").forEach(el=>el.onclick=()=>{selected=+el.dataset.queue;renderQueue();const q=queueSeeds[selected];query=q.evidence[0];$("#search-input").value=query;tab="all";renderSearch();document.querySelector("#evidence").scrollIntoView({behavior:"smooth",block:"start"});});
}
function renderLanes(){
 const tb=$("#lane-rows");
 tb.innerHTML=laneSeeds.map(l=>{const notes=DATA.notes.filter(n=>(l.search+" "+l.title).toLowerCase().split(/\s+/).some(w=>w.length>4&&(n.title+" "+n.id).toLowerCase().includes(w))).length;const entities=DATA.entities.filter(e=>(l.search+" "+l.title).toLowerCase().split(/\s+/).some(w=>w.length>4&&(e.label+" "+e.address).toLowerCase().includes(w))).length;return `<tr data-lane="${l.id}"><td><button class="lane-title" data-lane-open="${l.id}">${esc(l.title)}<small>${l.kind}</small></button></td><td>${esc(l.obs)}</td><td><span class="lane-count">${notes} notes</span><span class="lane-count">${entities} entities</span></td><td><span class="state-chip ${l.tone}"><i></i>${esc(l.state)}</span></td><td><button class="row-arrow" data-lane-open="${l.id}" aria-label="Open ${esc(l.title)}">↗</button></td></tr>`}).join("");
 tb.querySelectorAll("[data-lane-open]").forEach(el=>el.onclick=()=>{const lane=laneSeeds.find(x=>x.id===el.dataset.laneOpen);query=lane.search.split(" ").slice(0,2).join(" ");$("#search-input").value=query;tab="all";renderSearch();document.querySelector("#evidence").scrollIntoView({behavior:"smooth"});});
}

function normalizedRecords(){
 const es=DATA.entities.map(e=>({...e,type:"entity",search:[e.label,e.address,e.kind,e.confidence,e.acceptance,e.authority,e.id].join(" ").toLowerCase()}));
 const ns=DATA.notes.map(n=>({id:n.id,label:n.title,kind:n.category||"source_note",category:n.category,address:"",path:n.path,text:n.text,type:n.category==="Native C++ port unit"?"poc":"note",search:[n.title,n.id,n.path,n.category,n.text].join(" ").toLowerCase()}));
 return [...es,...ns];
}
function stateFor(e){const x=(e.acceptance||"").toLowerCase();if(x.includes("runtime")||x.includes("blocked")||x.includes("unverified")||x.includes("open"))return ["OPEN GATE","amber"]; if(x.includes("static")||x.includes("bounded")||x.includes("native evidence"))return ["BOUNDED","green"];return ["RECORDED","gray"]}
function visibleRecords(){
 let all=normalizedRecords();
 if(tab==="functions")all=all.filter(e=>e.kind==="function"||e.kind==="function_candidate");
 if(tab==="notes")all=all.filter(e=>e.type==="note");
 if(tab==="pocs")all=all.filter(e=>e.type==="poc");
 if(kind==="function")all=all.filter(e=>e.kind==="function"||e.kind==="function_candidate");
 if(kind==="native_role")all=all.filter(e=>e.kind==="native_role");
 if(kind==="note")all=all.filter(e=>e.type==="note");
 if(kind==="block")all=all.filter(e=>["native_block","exception_landing_pad"].includes(e.kind));
 if(query)all=all.filter(e=>e.search.includes(query.toLowerCase()));
 return all;
}
function renderSearch(){
 const records=visibleRecords(), part=records.slice(0,shown), root=$("#results");
 root.innerHTML=part.length?part.map((e,i)=>{
   const [state,tone]=stateFor(e);const isNote=e.type==="note"||e.type==="poc";const path=e.path||e.raw?.source||"";const address=e.address||e.raw?.linked_va||"";
   const meta=isNote?`${e.category||"NOTE"} · ${e.path}`:e.kind+(address?" · "+address:"");
   const detail= e.acceptance||e.confidence||e.text?.slice(0,150)||"Repository artifact; open source for full context.";
   return `<button class="evidence-row" data-result="${i}"><span class="result-kind ${isNote?"note-kind":""}">${isNote?"NOTE":esc((e.kind||"FILE").replaceAll("_"," ").toUpperCase())}</span><span class="result-main"><b>${esc(e.label)}</b><small>${esc(meta)}</small></span><span class="result-status ${tone}">${esc(state)}</span><span class="result-open">↗</span></button>`
 }).join(""):`<div class="empty-state"><span>⌕</span><b>${tab==="pocs"?"No reconstruction PoCs are registered in this snapshot.":"No matching evidence found."}</b><p>Try a function name, a linked address such as <code>0x2ffaa44</code>, or an artifact path.</p></div>`;
 root.querySelectorAll("[data-result]").forEach((el,i)=>el.onclick=()=>openDetail(part[i]));
 $("#result-range").textContent=records.length?`SHOWING ${Math.min(shown,records.length)} OF ${records.length.toLocaleString()} MATCHES`:`0 MATCHES`;
 $("#load-more").hidden=shown>=records.length;
}
function openDetail(e){
 const d=$("#detail-dialog"), content=$("#dialog-content"), raw=e.raw||{};let path=e.path||raw.source||"";let address=e.address||raw.linked_va||"";
 let body=(e.type==="note"||e.type==="poc")?`<pre class="note-body">${esc(e.text||"")}</pre>`:`<div class="detail-grid"><div><label>TYPE</label><b>${esc(e.kind||"Artifact")}</b></div><div><label>ADDRESS</label><b class="mono">${esc(address||"Not recorded")}</b></div><div><label>CONFIDENCE</label><b>${esc(e.confidence||"Not stated")}</b></div><div><label>ACCEPTANCE</label><b>${esc(e.acceptance||"Not stated")}</b></div><div class="wide"><label>NAME AUTHORITY</label><p>${esc(e.authority||"Not specified in this record")}</p></div><div class="wide"><label>EVIDENCE BOUNDARY</label><p>${esc(raw.original_name_hint?"Original name is retained as a hint; do not promote without identity evidence.":"Use the linked source record to interpret the scope and limits.")}</p></div></div>`;
 content.innerHTML=`<div class="dialog-eyebrow">REPOSITORY RECORD</div><h2>${esc(e.label)}</h2><div class="dialog-meta">${esc(path||e.kind||"")}</div>${body}<div class="dialog-actions"><button class="outline-button" onclick="document.querySelector('#detail-dialog').close()">Close</button></div>`;
 d.showModal();
}
function openNamedNote(id){const n=DATA.notes.find(x=>x.id===id);if(n)openDetail({...n,type:"note"});}

$("#search-input").addEventListener("input",e=>{query=e.target.value.trim();shown=60;renderSearch()});
$("#kind-filter").addEventListener("change",e=>{kind=e.target.value;shown=60;renderSearch()});
document.querySelectorAll(".result-tab").forEach(b=>b.addEventListener("click",()=>{document.querySelectorAll(".result-tab").forEach(x=>x.classList.remove("active"));b.classList.add("active");tab=b.dataset.tab;shown=60;renderSearch()}));
$("#load-more").addEventListener("click",()=>{shown+=60;renderSearch()});
$("#rank-queue").addEventListener("click",()=>{rankQueueFromBacklog();renderQueue()});
$("#show-traces").addEventListener("click",()=>{$(".result-tab[data-tab='all']").click();$("#search-input").value="trace";query="trace";renderSearch();$("#evidence").scrollIntoView({behavior:"smooth"})});
$("#open-verifier").addEventListener("click",()=>openNamedNote("record-full_verifier_round256"));
$("#open-protocol").addEventListener("click",()=>openNamedNote("record-README"));
$("#open-current").addEventListener("click",()=>openNamedNote("record-CURRENT_TASK"));
const localCommands={
 verify:["python3 decomp/verify_all.py","From the research workspace root · complete bounded decomp verifier cohort"],
 build:["cmake -S port -B /tmp/bitlife-port-build -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/bitlife-port-build -j4","Configure and build the separate native C++17 port · no interpreter"],
 diff:["cmake -S port -B port/build -DCMAKE_BUILD_TYPE=Release && bash tools/bench.sh trace_game10.json","Build the port, replay one captured trace, then report the first divergence"]
};
document.querySelectorAll(".command-tab").forEach(b=>b.addEventListener("click",()=>{document.querySelectorAll(".command-tab").forEach(x=>x.classList.remove("active"));b.classList.add("active");const [cmd,hint]=localCommands[b.dataset.command];$("#command-text").textContent=cmd;$("#command-hint").textContent=hint;$("#copy-command").textContent="COPY"}));
$("#copy-command").addEventListener("click",async()=>{try{await navigator.clipboard.writeText($("#command-text").textContent);$("#copy-command").textContent="COPIED";setTimeout(()=>$("#copy-command").textContent="COPY",1300)}catch{$("#copy-command").textContent="SELECT COMMAND"}});
$("#help-button").addEventListener("click",()=>{const d=$("#detail-dialog");$("#dialog-content").innerHTML=`<div class="dialog-eyebrow">HOW TO READ THIS CONSOLE</div><h2>Evidence has a scope.</h2><p class="help-copy">Entity counts come from the naming ledger. Check counts come from the persisted round snapshot. Neither is an exhaustive method denominator. A bounded verifier can establish byte pins or finite behavior under stated assumptions; it does not by itself establish target execution, whole-source correctness, C++ integration, or runtime equivalence.</p><p class="help-copy">GitHub Pages serves a generated, static research view. Links to execution remain local to the repository and instrumented environment.</p>`;d.showModal()});
document.addEventListener("keydown",e=>{if(e.key==="/"&&!e.metaKey&&!e.ctrlKey&&document.activeElement.tagName!=="INPUT"){e.preventDefault();$("#search-input").focus()}});
async function loadSnapshot(){
 try{const r=await fetch("./data.json");if(r.ok)return await r.json()}catch{}
 const mr=await fetch("./snapshot-parts/manifest.json");if(!mr.ok)throw new Error("Neither data.json nor the compressed snapshot is available.");
 const manifest=await mr.json(),chunks=await Promise.all(Array.from({length:manifest.parts},(_,i)=>fetch("./snapshot-parts/part-"+String(i).padStart(2,"0")+".txt").then(r=>{if(!r.ok)throw new Error("Snapshot chunk "+i+" is missing.");return r.text()})));
 const binary=atob(chunks.join("").trim()),bytes=new Uint8Array(binary.length);for(let i=0;i<binary.length;i++)bytes[i]=binary.charCodeAt(i);
 if(!("DecompressionStream" in window))throw new Error("This browser cannot open the compressed snapshot; use a current browser.");
 const stream=new Blob([bytes]).stream().pipeThrough(new DecompressionStream("gzip"));return await new Response(stream).json();
}
loadSnapshot().then(d=>{DATA=d;init()}).catch(err=>{$("#checkpoint-summary").textContent="Snapshot data is unavailable. Rebuild the Pages dataset from the repository.";$("#results").innerHTML=`<div class="empty-state"><b>Could not load repository snapshot</b><p>${esc(err.message)}</p></div>`});
