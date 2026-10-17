function log(s){ send(String(s)); }
var ready=false, nf=null, IMG=null;
var st={ life:null, hero:null, ticks:[], missed:[], logs:[], scenarios:[], ageTicks:[], seed:null, rng:[], rngSeeds:{}, pushDumb:[], prod2:[], rngSamples:[], decks:[], draws:[], collDraws:[], drawTrue:0, drawFalse:0, deckMath:[], picks:[], prod:[], prodMap:{}, anchors:[], births:[], attSets:[], wraps:[], persons:[], mon:[], bags:[], picksAll:[], queues:[], statW:[] };
function E(n){ return Module.findExportByName('libil2cpp.so', n); }
function N(n,r,a){ var p=E(n); if(p===null){ st.missed.push(n); throw new Error('no export '+n); } return new NativeFunction(p,r,a); }
function toJS(sp){ if(!sp || sp.isNull()) return null; var l=sp.add(0x10).readU32(); if(l>4000) return '<len '+l+'>'; return sp.add(0x14).readUtf16String(l); }
function fromJS(s){ return nf.il2cpp_string_new(Memory.allocUtf8String(s)); }
function findClass(ns,name){
  var cnt=Memory.alloc(4); var asms=nf.il2cpp_domain_get_assemblies(nf.il2cpp_domain_get(),cnt); var n=cnt.readU32();
  for(var i=0;i<n;i++){ var img=nf.il2cpp_assembly_get_image(asms.add(i*Process.pointerSize).readPointer()); if(img.isNull()) continue;
    if(IMG && !img.equals(IMG)) continue;
    var cc=nf.il2cpp_image_get_class_count(img);
    for(var c=0;c<cc;c++){ var k=nf.il2cpp_image_get_class(img,c); if(k.isNull()) continue;
      if(nf.il2cpp_class_get_name(k).readUtf8String()===name && nf.il2cpp_class_get_namespace(k).readUtf8String()===ns) return k; } }
  return null;
}
function tname(t){ var p=nf.il2cpp_type_get_name(t); return p.isNull()?'?':p.readUtf8String(); }
function fieldsOf(k){
  var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0));
  var out=[], fi, g=0;
  while(!(fi=nf.il2cpp_class_get_fields(k,it)).isNull() && g++<3000){
    if(nf.il2cpp_field_get_flags(fi)&0x10) continue;
    out.push({ name:nf.il2cpp_field_get_name(fi).readUtf8String(), type:tname(nf.il2cpp_field_get_type(fi)), off:nf.il2cpp_field_get_offset(fi) });
  }
  return out;
}
function fieldsWithParents(k){
  var chain=[], cur=k, g=0;
  while(cur && !cur.isNull() && g++<12){ chain.push(cur); cur=nf.il2cpp_class_get_parent(cur); }
  var out=[];
  for(var i=chain.length-1;i>=0;i--) out=out.concat(fieldsOf(chain[i]));
  return out;
}
function methodPtr(k,name,argc){
  var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0));
  var m, g=0;
  while(!(m=nf.il2cpp_class_get_methods(k,it)).isNull() && g++<4000){
    if(nf.il2cpp_method_get_name(m).readUtf8String()!==name) continue;
    if(argc!==null && nf.il2cpp_method_get_param_count(m)!==argc) continue;
    var p=m.readPointer();
    if(!p.isNull() && p.compare(ptr('0x1000'))>0) return p;
  }
  return null;
}
function chainProbe(){
  var out={};
  try{
    var bias=null;
    try{ if(st.setupPtr) bias=st.setupPtr.sub(0x2a74048); }catch(e0){}
    if(!bias){ try{ bias=Process.findModuleByName('libil2cpp.so').base.add(0x4000); }catch(e1){} }
    out.bias=bias?bias.toString():null;
    var p0=bias.add(0x5c2c250);
    var v=p0.readPointer(); var ov=objOf(v); out.p0=v.toString(); out.p0c=ov?ov.cls:null;
    var a=v.readPointer(); var oa=objOf(a); out.a=a.toString(); out.ac=oa?oa.cls:null;
    var b=a.add(0xb8).readPointer(); var ob=objOf(b); out.b=b.toString(); out.bc=ob?ob.cls:null;
    var c=b.readPointer(); var oc=objOf(c); out.c=c.toString(); out.cc=oc?oc.cls:null;
    var d=c.add(0x48).readPointer(); var od=objOf(d); out.d=d.toString(); out.dc=od?od.cls:null;
    out.w14c=d.add(0x14c).readS32(); out.w150=d.add(0x150).readS32();
    out.floatHeur=d.add(0x14c).readFloat();
  }catch(eC){ out.err=String(eC); }
  return out;
}

function objOf(p){
  if(!p || p.isNull() || p.compare(ptr('0x1000'))<0) return null;
  var k=nf.il2cpp_object_get_class(p);
  return { cls:nf.il2cpp_class_get_name(k).readUtf8String(), isEnum:nf.il2cpp_class_is_enum(k), k:k };
}
function scalar(type,addr){
  switch(type){
    case 'System.Single': return addr.readFloat();
    case 'System.Double': return addr.readDouble();
    case 'System.Int32': case 'System.UInt32': return addr.readS32();
    case 'System.Int64': case 'System.UInt64': return addr.readS64().toString();
    case 'System.Boolean': return addr.readU8()!==0;
    case 'System.String': return toJS(addr.readPointer());
    default: return undefined;
  }
}
function collCount(p,k){
  try{ if(p.isNull()) return null;
    var cn=nf.il2cpp_class_get_name(k).readUtf8String();
    var want = cn.indexOf('Dictionary')>=0 ? '_count' : '_size';
    var fs=fieldsOf(k); for(var i=0;i<fs.length;i++) if(fs[i].name===want) return p.add(fs[i].off).readS32();
    return null;
  }catch(e){ return null; }
}
function readObj(p, only){
  var o=objOf(p); if(!o) return null;
  var res={ _class:o.cls }, fs=fieldsWithParents(o.k);
  for(var i=0;i<fs.length;i++){
    var f=fs[i];
    if(res[f.name]!==undefined) continue;
    if(only && only.length){ var hit=false; for(var j=0;j<only.length;j++) if(f.name.indexOf(only[j])>=0){ hit=true; break; } if(!hit) continue; }
    var addr=p.add(f.off), v=scalar(f.type,addr);
    if(v!==undefined){ res[f.name]=v; continue; }
    var q=addr.readPointer();
    if(q.isNull() || q.compare(ptr('0x1000'))<0){ res[f.name]=null; continue; }
    var oi=objOf(q);
    if(oi && oi.isEnum){ res[f.name]=addr.readS32(); continue; }
    if(f.type.indexOf('System.Collections')>=0){ res[f.name]={ _cls:(oi?oi.cls:'?'), _count: collCount(q, oi?oi.k:nf.il2cpp_object_get_class(q)) }; continue; }
    res[f.name]={ _ptr:q.toString(), _cls:(oi?oi.cls:'?') };
  }
  return res;
}
var HITS={};
var UERR=[];

function findMethod(k,name,argc){
  var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0));
  var m, g=0;
  while(!(m=nf.il2cpp_class_get_methods(k,it)).isNull() && g++<6000){
    if(nf.il2cpp_method_get_name(m).readUtf8String()!==name) continue;
    if(argc!==null && nf.il2cpp_method_get_param_count(m)!==argc) continue;
    return m;
  }
  return null;
}
function invoke0(mi, self, argc, argPtrs){
  var args = ptr(0);
  if(argc>0){ args=Memory.alloc(Process.pointerSize*argc); for(var i=0;i<argc;i++) args.add(i*Process.pointerSize).writePointer(argPtrs[i]); }
  var exc=Memory.alloc(Process.pointerSize); exc.writePointer(ptr(0));
  var r = nf.il2cpp_runtime_invoke(mi, self, args, exc);
  var e = exc.readPointer();
  return { r:r, exc: e.isNull()? null : e.toString() };
}
function arrItems(arr){
  // Il2CppArray: vector data starts at 0x20 on arm64 (obj 0x10, bounds 0x8, max_length 0x4 + pad)
  var len = arr.add(0x18).readU32();
  var out=[]; for(var i=0;i<len;i++){ var p=arr.add(0x20+i*Process.pointerSize).readPointer(); if(!p.isNull()) out.push(p); }
  return out;
}
function findClassAny(ns,name){
  var cnt=Memory.alloc(4); var asms=nf.il2cpp_domain_get_assemblies(nf.il2cpp_domain_get(),cnt); var n=cnt.readU32();
  for(var i=0;i<n;i++){ var img=nf.il2cpp_assembly_get_image(asms.add(i*Process.pointerSize).readPointer()); if(img.isNull()) continue;
    var cc=nf.il2cpp_image_get_class_count(img);
    for(var c=0;c<cc;c++){ var k=nf.il2cpp_image_get_class(img,c); if(k.isNull()) continue;
      if(nf.il2cpp_class_get_name(k).readUtf8String()===name && nf.il2cpp_class_get_namespace(k).readUtf8String()===ns) return k; } }
  return null;
}
function findMethodHier(k,name,argc){
  var cur=k, d=0;
  while(cur && !cur.isNull() && d++<10){
    var m=findMethod(cur,name,argc);
    if(m) return m;
    cur=nf.il2cpp_class_get_parent(cur);
  }
  return null;
}
function className(o){ try{ var k=nf.il2cpp_object_get_class(o); return nf.il2cpp_class_get_namespace(k).readUtf8String()+'.'+nf.il2cpp_class_get_name(k).readUtf8String(); }catch(e){ return '?'; } }
function uiButtons(){
  var ok=findClassAny('UnityEngine','Object'), bK=findClassAny('UnityEngine.UI','Button'), tK=findClassAny('UnityEngine.UI','Text');
  if(!ok||!bK) return {error:'missing classes'};
  var fmi=findMethod(ok,'FindObjectsOfType',1);
  if(!fmi) return {error:'no FindObjectsOfType'};
  var ty=nf.il2cpp_type_get_object(nf.il2cpp_class_get_type(bK));
  var res=invoke0(fmi, ptr(0), 1, [ty]);
  if(res.exc) return {error:'invoke exc '+res.exc};
  if(res.r.isNull()) return {error:'null result'};
  var objs=arrItems(res.r);
  var out=[];
  var gm=findMethodHier(bK,'get_gameObject',0), tr=findMethodHier(bK,'get_transform',0);
  var gact=null, tpos=null, tget=null, ttext=null;
  var goK=findClassAny('UnityEngine','GameObject'), trK=findClassAny('UnityEngine','Transform'), coK=findClassAny('UnityEngine','Component');
  if(goK) gact=findMethodHier(goK,'get_activeInHierarchy',0);
  if(trK) tpos=findMethodHier(trK,'get_position',0);
  if(tK&&coK){ tget=findMethodHier(coK,'GetComponent',1); ttext=findMethodHier(tK,'get_text',0); }
  var tTy=tK? nf.il2cpp_type_get_object(nf.il2cpp_class_get_type(tK)) : null;
  for(var i=0;i<objs.length && out.length<60;i++){
    var b=objs[i];
    try{
      var rec={ ptr:b.toString() };
      if(gm&&gact){ var g=invoke0(gm,b,0,[]); if(!g.exc && !g.r.isNull()){ var a=invoke0(gact,g.r,0,[]); rec.active = (!a.exc) && a.r.readU8()!==0; } }
      if(tr&&tpos){ var t=invoke0(tr,b,0,[]); if(!t.exc && !t.r.isNull()){ var p=invoke0(tpos,t.r,0,[]); if(!p.exc && !p.r.isNull()){ rec.x=p.r.readFloat(); rec.y=p.r.add(4).readFloat(); rec.z=p.r.add(8).readFloat(); } } }
      if(tget&&ttext&&tTy){ var co=invoke0(tget,b,1,[tTy]); if(!co.exc && !co.r.isNull()){ var s=invoke0(ttext,co.r,0,[]); if(!s.exc) rec.text=toJS(s.r); } }
      if(rec.active!==false) out.push(rec);
    }catch(e){ }
  }
  return { n:objs.length, hasGM:!!gm, hasTR:!!tr, hasGACT:!!gact, hasTPOS:!!tpos, hasTGET:!!tget, hasTTEXT:!!ttext, classes:objs.slice(0,6).map(className), buttons:out };
}

function snapshot(tag){
  var h=st.hero; if(!h) return null;
  var s=readObj(h, ['Name','Alive','Gender','Age','Sexuality','PlaceOfResidenceString','Att_','BankBalance']);
  return { tag:tag, t:Date.now(), p:h.toString(), s:s };
}
function install(){
  var lifeK=findClass('','Life'), heroK=findClass('','SimHero');
  var out=[];
  function ctorPtr(k){ var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0)); var m,g=0;
    while(!(m=nf.il2cpp_class_get_methods(k,it)).isNull() && g++<6000){ var nm=nf.il2cpp_method_get_name(m).readUtf8String(); if(nm==='.ctor'){ var p=m.readPointer(); if(!p.isNull() && p.compare(ptr('0x1000'))>0) return p; } }
    return null; }
  if(lifeK){ var lc=ctorPtr(lifeK); if(lc){ Interceptor.attach(lc,{ onEnter:function(a){ HITS.lifeCtor=(HITS.lifeCtor||0)+1; st.life=a[0]; } }); out.push('Life..ctor@'+lc); }
    var lp=methodPtr(lifeK,'LogPresentation',1);
    if(lp){ Interceptor.attach(lp,{ onEnter:function(a){ HITS.log=(HITS.log||0)+1; try{ st.life=a[0]; var s=toJS(a[1]); if(s!==null){ var ag=null; try{ if(st.hero) ag=st.hero.add(0x20).readS32(); }catch(e0){} st.logs.push({t:Date.now(), text:s, age:ag, n:st.logs.length}); if(st.logs.length>3000) st.logs.shift(); } }catch(e1){} } }); out.push('Life.LogPresentation@'+lp); }
    var lsp=methodPtr(lifeK,'LogScenarioPresentation',1);
    if(lsp){ Interceptor.attach(lsp,{ onEnter:function(a){ HITS.scen=(HITS.scen||0)+1; try{ st.life=a[0]; var sc=a[1]; var d=sc.isNull()?null:readObj(sc,['Title','Body','ExteriorTitle','HasDismissButton','_class','_Choice','MiniGame']);
      if(d){ d._ptr=sc.toString(); d.t=Date.now();
        // Scenario.ChoiceArray (ArrayList at +0x108) holds ScenarioChoice objects whose
        // <Title>k__BackingField (string at +0x10) is the button label the player sees.
        try{ var ca=sc.add(0x108).readPointer(); if(!ca.isNull()){ var nc=ca.add(0x18).readS32(); var itc=ca.add(0x10).readPointer(); var arr=[];
          for(var ci=0;ci<Math.min(nc,12);ci++){ var el=itc.add(0x20+ci*Process.pointerSize).readPointer(); if(el.isNull()) continue; arr.push(toJS(el.add(0x10).readPointer())); } d._choices=arr; } }catch(eCh){}
        try{ d._push = sc.add(0x131).readU8()? true : false; }catch(eP){}
        st.scenarios.push(d); if(st.scenarios.length>1000) st.scenarios.shift(); } }catch(e2){} } }); out.push('Life.LogScenarioPresentation@'+lsp); }
    var llk=findClass('','LifeLog');
    var ll=llk? methodPtr(llk,'Log',1) : null;
    if(ll){ Interceptor.attach(ll,{ onEnter:function(a){ HITS.log=(HITS.log||0)+1; try{ var s2=toJS(a[1]); var ag2=null; try{ ag2=a[0].add(0x1c).readS32(); }catch(e0){} if(s2!==null){ st.logs.push({t:Date.now(), text:s2, age:ag2, src:'LifeLog.Log'}); if(st.logs.length>3000) st.logs.shift(); } }catch(e1){} } }); out.push('LifeLog.Log@'+ll); }
    var ia2=methodPtr(lifeK,'IncrementAge',0);
    if(ia2){ Interceptor.attach(ia2,{ onEnter:function(a){ HITS.lifeAge=(HITS.lifeAge||0)+1; st.life=a[0]; st.ageTicks.push({t:Date.now(), phase:'enter', hero:st.hero?st.hero.toString():null}); if(st.ageTicks.length>1000) st.ageTicks.shift(); } }); out.push('Life.IncrementAge@'+ia2); } }
  if(lifeK){
    [['GetRandomPushScenario',0],['GetAgeSpecificPushScenarioArray',0],['ValidateAndSetScenario',3],['StringOfScenariosAtCurrentAge',0]].forEach(function(sp){
      try{
        var p=methodPtr(lifeK, sp[0], sp[1]); if(!p) return;
        Interceptor.attach(p,{ onEnter:function(a){
            this.tag=sp[0]; this.args=[];
            for(var i=0;i<sp[1];i++){ try{ this.args.push(argInfo(a[i+1])); }catch(eA){ this.args.push('?'); } }
            try{ this.age = st.hero? st.hero.add(0x20).readS32() : null; }catch(eB){ this.age=null; }
            this.al=null; this.sz0=null; this.bp=null;
            if(sp[0]==='ValidateAndSetScenario'){ try{ this.al=a[3]; this.bp=a[3].toString(); if(!this.al.isNull()) this.sz0=this.al.add(0x18).readS32(); }catch(eG){} }
          }, onLeave:function(ret){
            try{
              var cn=null, nv=null;
              try{ if(!ret.isNull()){ var ob=nf.il2cpp_object_get_class(ret); if(!ob.isNull()) cn=nf.il2cpp_class_get_name(ob).readUtf8String(); } }catch(eC){}
              try{ nv = ret.toInt32() & 1; }catch(eD){}
              var rec={m:this.tag, age:this.age, args:this.args, cls:cn, n:nv, p:ret.toString(), t:Date.now()}; if(this.bp) rec.bp=this.bp;
              if(this.al && this.sz0!==null){
                try{ var sz1=this.al.add(0x18).readS32(); rec.sz0=this.sz0; rec.sz1=sz1;
                  if(sz1>this.sz0 && sz1>0){
                    var items=this.al.add(0x10).readPointer();
                    var el=items.add(0x20+(sz1-1)*Process.pointerSize).readPointer();
                    if(!el.isNull()){
                      try{ var ob=nf.il2cpp_object_get_class(el); if(!ob.isNull()) rec.lastCls=nf.il2cpp_class_get_name(ob).readUtf8String(); }catch(eH){}
                      try{ rec.lastStr=el.add(0x14).readUtf16String(); }catch(eI){}
                      try{ var tk=nf.il2cpp_class_from_system_type(el); if(!tk.isNull()) rec.lastType=nf.il2cpp_class_get_name(tk).readUtf8String(); }catch(eJ){}
                    }
                  }
                }catch(eK){}
              }
              st.prod2.push(rec);
              if(st.prod2.length>800) st.prod2.shift();
              HITS['p_'+this.tag]=(HITS['p_'+this.tag]||0)+1;
            }catch(eE){}
          }});
        out.push('Life.'+sp[0]+'@'+p);
      }catch(eF){}
    });
  }
  if(lifeK){
    // The age-specific push bag and the pick out of it. GetAgeSpecificPushScenarioArray returns
    // List<Scenario>; each element is an instance of its own Scenario subclass, so the class name
    // is the scenario identity. GetRandomPushScenario returns the one the game actually chose.
    try{
      var gas=methodPtr(lifeK,'GetAgeSpecificPushScenarioArray',0);
      if(gas){ Interceptor.attach(gas,{ onEnter:function(a){ this.age = st.hero? st.hero.add(0x20).readS32() : null; },
        onLeave:function(ret){
          try{ st.bagHits=(st.bagHits||0)+1; var rec={ age:this.age, sz:null, cls:[], t:Date.now() };
            if(!ret.isNull()){ var sz=ret.add(0x18).readS32(); rec.sz=sz;
              var items=ret.add(0x10).readPointer(); var n2=Math.min(sz,300);
              for(var i=0;i<n2;i++){ try{ var el=items.add(0x20+i*Process.pointerSize).readPointer(); if(el.isNull()) continue;
                var ob=nf.il2cpp_object_get_class(el); rec.cls.push(nf.il2cpp_class_get_name(ob).readUtf8String()); }catch(e0){} } }
            st.bags.push(rec); if(st.bags.length>400) st.bags.shift(); }catch(e1){ st.bagErr=String(e1); }
        }}); out.push('Life.GetAgeSpecificPushScenarioArray@'+gas); }
    }catch(e2){ out.push('bag hook failed '+e2); }
    try{
      var grp=methodPtr(lifeK,'GetRandomPushScenario',0);
      if(grp){ Interceptor.attach(grp,{ onEnter:function(a){ this.age = st.hero? st.hero.add(0x20).readS32() : null; },
        onLeave:function(ret){
          try{ st.pickHits=(st.pickHits||0)+1; var cn=null; if(!ret.isNull()){ var ob2=nf.il2cpp_object_get_class(ret); cn=nf.il2cpp_class_get_name(ob2).readUtf8String(); }
            var last=st.bags.length? st.bags[st.bags.length-1] : null;
            st.picksAll.push({ age:this.age, cls:cn, bag: last? last.cls.slice() : null, t:Date.now() });
            if(st.picksAll.length>400) st.picksAll.shift(); }catch(e3){ st.pickErr=String(e3); }
        }}); out.push('Life.GetRandomPushScenario@'+grp); }
    }catch(e4){ out.push('pick hook failed '+e4); }
    try{
      // The age queue: the game asks "is this scenario type in the current queue?" for every
      // candidate type each age. The accepted set is the per-age scenario pool; the returned
      // StringOfScenariosAtCurrentAge text is the same queue as the game prints it.
      var isq=methodPtr(lifeK,'IsScenarioIncludedInCurrentQueue',1);
      if(isq){ Interceptor.attach(isq,{ onEnter:function(a){
          this.age = st.hero? st.hero.add(0x20).readS32() : null;
          this.tn=null; try{ var k=nf.il2cpp_class_from_system_type(a[1]); if(!k.isNull()) this.tn=nf.il2cpp_class_get_name(k).readUtf8String(); }catch(eT){}
        }, onLeave:function(rv){
          try{ var v=rv.toInt32()&1;
            st.qchecks=st.qchecks||[]; st.qchecks.push({age:this.age, t:this.tn, v:v, t2:Date.now()});
            if(st.qchecks.length>4000) st.qchecks.shift(); }catch(eQ){ st.qErr=String(eQ); }
        }}); out.push('Life.IsScenarioIncludedInCurrentQueue@'+isq); }
    }catch(e5){ out.push('queue hook failed '+e5); }
    try{
      // The per-age scenario queue the game builds inside IncrementAge and then drains while
      // presenting: Life.IncrementAgeScenarioQueueArray (List<Scenario>, field 0x18). Reading it
      // after IncrementAge returns gives the exact candidate bag for that age.
      var qh=function(listPtr){
        var rec={sz:null, cls:[]};
        try{ if(listPtr && !listPtr.isNull()){ rec.sz=listPtr.add(0x18).readS32();
          var items=listPtr.add(0x10).readPointer(); var n2=Math.min(rec.sz,400);
          for(var i=0;i<n2;i++){ try{ var el=items.add(0x20+i*Process.pointerSize).readPointer(); if(el.isNull()) continue;
            var ob=nf.il2cpp_object_get_class(el); rec.cls.push(nf.il2cpp_class_get_name(ob).readUtf8String()); }catch(e00){} } } }catch(e01){}
        return rec;
      };
      var inc=methodPtr(lifeK,'IncrementAge',0);
      if(inc){ Interceptor.attach(inc,{ onEnter:function(a){
          this.self=a[0]; this.age=null;
          try{ this.age=a[0].add(0x20).readS32(); }catch(e02){}
          try{ this.before=qh(a[0].add(0x18).readPointer()); }catch(e03){ this.before={sz:null,cls:[]}; }
        }, onLeave:function(ret){
          try{ st.queueHits=(st.queueHits||0)+1;
            var after={sz:null,cls:[]}; try{ after=qh(this.self.add(0x18).readPointer()); }catch(e04){}
            st.queues=st.queues||[]; st.queues.push({age:this.age, before:this.before, after:after, t:Date.now()});
            if(st.queues.length>200) st.queues.shift(); }catch(e05){ st.queueErr=String(e05); }
        }}); out.push('Life.IncrementAge@'+inc); }
    }catch(e7){ out.push('queue-array hook failed '+e7); }
    try{
      var ssc=methodPtr(lifeK,'StringOfScenariosAtCurrentAge',0);
      if(ssc){ Interceptor.attach(ssc,{ onEnter:function(a){ this.age = st.hero? st.hero.add(0x20).readS32() : null; },
        onLeave:function(ret){
          try{ st.qtexts=st.qtexts||[]; st.qtexts.push({age:this.age, text: toJS(ret), t:Date.now()});
            if(st.qtexts.length>400) st.qtexts.shift(); }catch(eS){ st.qErr2=String(eS); }
        }}); out.push('Life.StringOfScenariosAtCurrentAge@'+ssc); }
    }catch(e6){ out.push('qtext hook failed '+e6); }
  }
  try{
    var iak=findClass('Game','UIIAPManager');
    var iam=iak? methodPtr(iak,'AppStoreConnectionErrorAlert',0):null;
    if(iam){ Interceptor.replace(iam, new NativeCallback(function(){ HITS.gpAlert=(HITS.gpAlert||0)+1; }, 'void', []));
      out.push('UIIAPManager.AppStoreConnectionErrorAlert suppressed@'+iam); }
    else out.push('UIIAPManager.AppStoreConnectionErrorAlert not found');
  }catch(eIA){ out.push('IAP suppress failed '+eIA); }
  if(lifeK){ var gh=methodPtr(lifeK,'get_Hero',0);
    if(gh){ Interceptor.attach(gh,{ onEnter:function(a){ HITS.getHero=(HITS.getHero||0)+1; st.life=a[0]; } }); out.push('Life.get_Hero@'+gh); } }
  if(heroK){
    // Every new life runs through here once, before any age-up: full hero snapshot
    // on entry and exit plus the RNG window, which is where the newborn stats get set.
    var seh=methodPtr(heroK,'SetupEverythingElseForNewHero',0);
    if(seh){ st.setupPtr=seh; }
    // Branch probe: the setup method reads an int from [x8+0x14c] and compares it with 1.
    // Hooking the instruction right after that load exposes both the object (x8) and value (x9).
    try{
      var bpAddr=seh.add(0x2a74368-0x2a74048);
      st.branch=[];
      if(1) throw 'branch probe disabled (crashed game)';
      Interceptor.attach(bpAddr,{ onEnter:function(){ try{
        var x8=this.context.x8, x9=this.context.x9;
        var rec={t:Date.now(), obj:x8.toString(), val:x9.toInt32(), cls:null, chain:[]};
        try{ var o=objOf(x8); rec.cls=o?o.cls:null; }catch(e0){}
        try{ var props=fieldsWithParents(objOf(x8).k); for(var pi=0; pi<props.length; pi++){ var pf=props[pi]; if(pf.off>=0x140 && pf.off<=0x160){ var v=null; try{ v=scalar(pf.type, x8.add(pf.off)); }catch(ev){} rec.chain.push({name:pf.name, off:pf.off, type:pf.type, v:v}); } } }catch(e1){}
        if(st.branch.length<50) st.branch.push(rec);
      }catch(e2){} } });
      out.push('branch@'+bpAddr);
    }catch(e3){ out.push('branch-hook-fail '+e3); }
    if(seh){ try{ Interceptor.attach(seh,{ onEnter:function(a){ HITS.setupHero=(HITS.setupHero||0)+1; st.hero=a[0];
        this.t0=Date.now(); this.ptr=a[0].toString(); this.before=readObj(a[0],null); this.ri0=st.rngSamples.length;
        try{ this.chain=chainProbe(); }catch(eCh){}
    // --- pinpoint the 4 primary-stat writers inside setup via page write-monitor ---
    try{
      var oo=objOf(a[0]);
      if(oo){
        var fs2=fieldsWithParents(oo.k); var offs={};
        for(var fi=0; fi<fs2.length; fi++){ var fn=fs2[fi].name; if(fn.indexOf('<Att_')===0){ offs[fn.substring(5).replace('>k__BackingField','')]=fs2[fi].off; } }
        st.attOff=offs;
        var want=['happiness','health','intelligence','appearance'];
        var mn=0x7fffffff, mx=-1;
        for(var wi=0; wi<want.length; wi++){ var wo=offs[want[wi]]; if(wo===undefined) continue; if(wo<mn) mn=wo; if(wo>mx) mx=wo; }
        if(mx>0){
          var pg=4096; var hero=a[0];
          var start=hero.add(mn).and(ptr('0xfffffffffffff000'));
          var size=Math.ceil((hero.add(mx+4).sub(start).toInt32())/pg)*pg;
          this.monBase=start; this.monSize=size; this.monHero=hero; this.monWant=want; this.monOff=offs;
          st.mon=[];
          var arm=function(){
            try{ MemoryAccessMonitor.monitor({ base:start, size:size, accesses:'w', onAccess:function(d){
              try{ var tgt=d.address.sub(hero).toInt32();
                for(var i2=0;i2<want.length;i2++){ if(tgt===offs[want[i2]]){ st.mon.push({f:want[i2], at:d.address.toString(), from:(d.from?d.from.toString():null), to:(d.to?d.to.toString():null), t:Date.now()}); break; } }
              }catch(e1){}
              try{ MemoryAccessMonitor.disable(); }catch(e2){}
              try{ if(st.mon.length<300) arm(); }catch(e3){}
            } }); }catch(e4){ st.mon.push({err:String(e4)}); }
          };
          arm();
        }
      }
    }catch(eMon){ st.mon=[{err:String(eMon)}]; } },
      onLeave:function(){ try{ try{ MemoryAccessMonitor.disable(); }catch(eD){} st.births.push({t:Date.now(), t0:this.t0, hero:this.ptr, before:this.before, after:readObj(st.hero,null), attOff:st.attOff, mon:st.mon, chain:this.chain, rng:st.rngSamples.slice(this.ri0).filter(function(r){ return r.m==='Next/2'; })});
        if(st.births.length>50) st.births.shift(); }catch(e){} } }); out.push('SimHero.SetupEverythingElseForNewHero@'+seh); }catch(e){ out.push('setupHero hook failed '+e); } }
    var ia=methodPtr(heroK,'IncrementAge',0);
    if(ia){ Interceptor.attach(ia,{ onEnter:function(a){ HITS.heroAge=(HITS.heroAge||0)+1; st.hero=a[0]; this.hero=a[0]; st.yearStart=Date.now(); }, onLeave:function(){ var s=snapshot('afterIncrementAge'); if(s){ s.yearStart=st.yearStart; st.ticks.push(s); if(st.ticks.length>400) st.ticks.shift(); } } }); out.push('SimHero.IncrementAge@'+ia); }
    // Health drifts every year in the game and the dumb-scenario tables carry no health column, so
    // name the writers instead of guessing: hook every Att_* setter and Impact* wrapper on SimPerson.
    (function(){
      try{
        var k=findClass('','SimPerson'); if(!k||k.isNull()){ out.push('SimPerson class missing (stat hooks)'); return; }
        st.attOff={};
        var att=['happiness','health','intelligence','appearance','discipline','karma'];
        var push=function(f,v,ret){ if(st.statW.length<4000) st.statW.push({f:f, v:v, ret:ret, t:Date.now()}); };
        for(var ai=0; ai<att.length; ai++){ (function(nm){
          var sp=methodPtr(k,'set_Att_'+nm,0);
          if(sp && !sp.isNull()) Interceptor.attach(sp,{ onEnter:function(){ try{ push('set_'+nm, null, this.returnAddress.toString()); }catch(e){} } });
          var ip=methodPtr(k,'Impact'+(nm.charAt(0).toUpperCase()+nm.slice(1)),0);
          if(ip && !ip.isNull()) Interceptor.attach(ip,{ onEnter:function(a){ try{ push('impact_'+nm, a[1].toInt32(), this.returnAddress.toString()); }catch(e){} } });
        })(att[ai]); }
        out.push('stat-writer hooks: '+att.join(','));
      }catch(eSW){ out.push('stat hooks failed '+eSW); }
    })();
    var ctor=null; var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0)); var m,g=0;
    while(!(m=nf.il2cpp_class_get_methods(heroK,it)).isNull() && g++<4000){ if(nf.il2cpp_method_get_name(m).readUtf8String()==='.ctor'){ ctor=m.readPointer(); break; } }
    if(ctor && !ctor.isNull()){ var cc=function(){ try{ Interceptor.attach(ctor,{ onEnter:function(a){ HITS.heroCtor=(HITS.heroCtor||0)+1; st.hero=a[1]; } }); out.push('SimHero..ctor@'+ctor); }catch(e){} }; cc(); }
  }
  // --- every SimPerson constructed: name/age/Att_* snapshot, for newborn-vs-parent comparison ---
  (function(){
    var k=findClass('','SimPerson'); if(!k || k.isNull()){ out.push('SimPerson class missing'); return; }
    var names=[]; var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0)); var m,g=0;
    while(!(m=nf.il2cpp_class_get_methods(k,it)).isNull() && g++<6000){
      var nm=nf.il2cpp_method_get_name(m).readUtf8String(); if(nm!=='.ctor') continue;
      names.push({nm:nm, p:m.readPointer(), argc:nf.il2cpp_method_get_param_count(m)}); }
    for(var i=0;i<names.length;i++){
      var p=names[i].p; if(p.isNull() || p.compare(ptr('0x1000'))<=0) continue;
      try{ (function(p2,idx){ Interceptor.attach(p2,{ onEnter:function(a){ this.self=a[0]; this.ri0=st.rngSamples.length; }, onLeave:function(){ try{
        var o=readObj(this.self, null);
        if(st.persons.length<600) st.persons.push({t:Date.now(), argc:names[idx].argc, self:this.self.toString(), nd:st.rngSamples.length-this.ri0, draws:st.rngSamples.slice(this.ri0, this.ri0+90).map(function(r){return [r.m, r.mx, r.v];}), cls:o._class, name:o['<Name>k__BackingField'], age:o['<Age>k__BackingField'],
          att:{ happiness:o['<Att_happiness>k__BackingField'], health:o['<Att_health>k__BackingField'], intelligence:o['<Att_intelligence>k__BackingField'], appearance:o['<Att_appearance>k__BackingField'], discipline:o['<Att_discipline>k__BackingField'], karma:o['<Att_karma>k__BackingField'] }});
      }catch(e){} } }); })(p,i); out.push('SimPerson..ctor/'+names[i].argc+'@'+p);
      }catch(e){ out.push('person hook failed '+e); }
    }
  })();
  // --- RNG wrappers: record the caller (return address) plus min/max so an attribute
  //     draw can be attributed to the function that asked for it. ---
  (function(){
    var ks=[['GameReference',findClass('','GameReference')],['DataHub',findClass('','DataHub')]];
    for(var i=0;i<ks.length;i++){
      var nmk=ks[i][0], k=ks[i][1]; if(!k || k.isNull()){ out.push('wrapper class missing '+nmk); continue; }
      var mp=methodPtr(k,'GetRandomNumber',2); if(!mp){ out.push('wrapper method missing '+nmk); continue; }
      try{ Interceptor.attach(mp,{ onEnter:function(a){ this.mn=a[1].toInt32(); this.mx=a[2].toInt32();
          try{ this.ra=this.returnAddress.toString(); }catch(e){ this.ra=null; } },
        onLeave:function(rv){ var v=null; try{ v=rv.toInt32(); }catch(e){}
          var bt=null; try{ bt=Thread.backtrace(this.context, BacktraceType.fast).slice(0,8).map(function(x){ return x.toString(); }); }catch(eB){}
          if(st.wraps.length<20000) st.wraps.push({k:nmk, mn:this.mn, mx:this.mx, v:v, ra:this.ra, bt:bt, t:Date.now()}); } });
        out.push(nmk+'.GetRandomNumber/2@'+mp); }catch(e){ out.push('wrapper hook failed '+nmk+' '+e); }
    }
  })();
  // --- attribute setter taps: every Att_* write with the RNG call index just before it,
  //     so a draw can be tied to the attribute it produced. ---
  (function(){
    var targets=[findClass('','SimPerson'), heroK];
    var n=0;
    for(var ti=0; ti<targets.length; ti++){
      var k=targets[ti]; if(!k || k.isNull()) continue;
      var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0)); var m,g=0;
      while(!(m=nf.il2cpp_class_get_methods(k,it)).isNull() && g++<6000){
        var nm=nf.il2cpp_method_get_name(m).readUtf8String();
        if(nm.indexOf('set_Att_')!==0) continue;
        var p=m.readPointer(); if(p.isNull() || p.compare(ptr('0x1000'))<=0) continue;
        try{
          (function(nm2){ Interceptor.attach(p,{ onEnter:function(a){ try{
            st.attSets.push({f:nm2, v:a[1].toInt32(), n:st.rngSamples.length, t:Date.now(), self:a[0].toString()});
            if(st.attSets.length>8000) st.attSets.shift(); }catch(e){} } }); })(nm);
          out.push('attr '+nm+'@'+p); n++;
        }catch(e){ out.push('attr hook failed '+nm+' '+e); }
      }
    }
  })();
  // --- RNG + RandomDeck evidence (Phase 1: replicate the game's own selection math) ---
  var rndK=findClassAny('System','Random');
  if(rndK){
    var rc=methodPtr(rndK,'.ctor',1);
    if(rc){ Interceptor.attach(rc,{ onEnter:function(a){ HITS.rngCtor=(HITS.rngCtor||0)+1;
      try{ var v=a[1].toInt32(); if(st.seed===null) st.seed=v;
        st.rngSeeds[a[0].toString()]=v;
        if(st.rng.length<300) st.rng.push({t:Date.now(), tag:'System.Random..ctor', seed:v, self:a[0].toString()}); }catch(e){} } });
      out.push('System.Random..ctor(I)@'+rc); }
    // Draws: one record per Next()/Next(int)/Next(int,int) call, keyed by the
    // instance pointer so a sequence can be replayed from its captured seed.
    var seq={};
    var hookNext=function(nm, argc){
      var mp=methodPtr(rndK,nm,argc); if(!mp) return;
      try{ Interceptor.attach(mp,{ onEnter:function(a){ this.self=a[0].toString();
        try{ if(argc>=2){ this.mn=a[1].toInt32(); this.mx=a[2].toInt32(); } }catch(eM){} }, onLeave:function(rv){
        var v=null; try{ v=rv.toInt32(); }catch(e){}
        var k=this.self; seq[k]=(seq[k]||0)+1;
        var ra=null; try{ ra=this.returnAddress.toString(); }catch(eR){}
        if(st.rngSamples.length<60000) st.rngSamples.push({self:k, m:nm+'/'+argc, seq:seq[k], v:v, mn:this.mn, mx:this.mx, ra:ra, seed:(st.rngSeeds[k]===undefined?null:st.rngSeeds[k]), t:Date.now()});
      } }); out.push('System.Random.'+nm+'/'+argc+'@'+mp); }catch(e){}
    };
    hookNext('Next',0); hookNext('Next',1); hookNext('Next',2);
    // NextDouble()/Sample() are the float-returning draws: on arm64 the double comes
    // back in d0, which Interceptor.onLeave exposes through this.context.
    var hookDbl=function(nm, argc){
      var mp=methodPtr(rndK,nm,argc); if(!mp) return;
      try{ Interceptor.attach(mp,{ onEnter:function(a){ this.self=a[0].toString(); }, onLeave:function(rv){
        var d=null; try{ d=this.context.d0.toFloat(); }catch(e){}
        var k=this.self; seq[k]=(seq[k]||0)+1;
        if(st.rngSamples.length<60000) st.rngSamples.push({self:k, m:nm+'/'+argc, seq:seq[k], v:null, d:d, seed:(st.rngSeeds[k]===undefined?null:st.rngSeeds[k]), t:Date.now()});
      } }); out.push('System.Random.'+nm+'/'+argc+'@'+mp); }catch(e){}
    };
    hookDbl('NextDouble',0); hookDbl('Sample',0);
    var r0=methodPtr(rndK,'.ctor',0);
    if(r0){ Interceptor.attach(r0,{ onEnter:function(a){ HITS.rngCtor0=(HITS.rngCtor0||0)+1;
      try{ if(st.rng.length<300) st.rng.push({t:Date.now(), tag:'System.Random..ctor()', seed:null, self:a[0].toString()}); }catch(e){} } });
      out.push('System.Random..ctor@'+r0); }
  }
  var deckK=findClass('','RandomDeck');
  if(deckK){
    var readDeck=function(self){ var r={deckSize:null, successCount:null, index:null, cards:null};
      try{ r.deckSize=self.add(0x1c).readS32(); r.successCount=self.add(0x20).readS32(); r.index=self.add(0x18).readS32(); }catch(e){}
      try{ var lst=self.add(0x10).readPointer(); if(!lst.isNull()){ var arr=lst.add(0x10).readPointer(); var n=lst.add(0x18).readS32();
        r.cards=''; if(!arr.isNull() && n>0 && n<4096){ var s=[]; for(var i=0;i<n;i++) s.push(arr.add(i).readU8()?1:0); r.cards=s.join(''); } } }catch(e){}
      return r; };
    var pushDeck=function(rec){ if(st.decks.length>1500) st.decks.shift(); st.decks.push(rec); };
    var dc=methodPtr(deckK,'.ctor',2);
    if(dc){ Interceptor.attach(dc,{ onEnter:function(a){ HITS.deckCtor=(HITS.deckCtor||0)+1;
      try{ st.deckSample=a[0]; if(st.decks.length<400) st.deckSamples=(st.deckSamples||[]).concat([a[0].toString()]);
        pushDeck({t:Date.now(), ev:'ctor', self:a[0].toString(), deckSize:a[1].toInt32(), successCount:a[2].toInt32(), cards:null}); }catch(e){} } });
      out.push('RandomDeck..ctor@'+dc); }
    var dsh=methodPtr(deckK,'ShuffleAll',0);
    if(dsh){ Interceptor.attach(dsh,{ onEnter:function(a){ this.self=a[0]; }, onLeave:function(rv){ HITS.deckShuffle=(HITS.deckShuffle||0)+1;
      try{ var d=readDeck(this.self); pushDeck({t:Date.now(), ev:'shuffle', self:this.self.toString(), deckSize:d.deckSize, successCount:d.successCount, index:d.index, cards:d.cards}); }catch(e){} } });
      out.push('RandomDeck.ShuffleAll@'+dsh); }
    var dfill=methodPtr(deckK,'FillAndShuffle',1);
    if(dfill){ Interceptor.attach(dfill,{ onEnter:function(a){ this.self=a[0]; this.arg=a[1].toInt32(); }, onLeave:function(rv){ HITS.deckFill=(HITS.deckFill||0)+1;
      try{ var d=readDeck(this.self); pushDeck({t:Date.now(), ev:'fill', self:this.self.toString(), arg:this.arg, deckSize:d.deckSize, successCount:d.successCount, index:d.index, cards:d.cards}); }catch(e){} } });
      out.push('RandomDeck.FillAndShuffle@'+dfill); }
    var ddraw=methodPtr(deckK,'Draw',0);
    if(ddraw){ Interceptor.attach(ddraw,{ onEnter:function(a){ this.self=a[0]; try{ this.i=a[0].add(0x18).readS32(); }catch(e){ this.i=null; } }, onLeave:function(rv){ HITS.deckDraw=(HITS.deckDraw||0)+1;
      try{ var v=(rv.toInt32()&1); st.drawTrue+=v; st.drawFalse+=(1-v);
        if(st.draws.length<800) st.draws.push({i:this.i, v:v, size:this.self?this.self.add(0x1c).readS32():null}); }catch(e){} } });
      out.push('RandomDeck.Draw@'+ddraw); }
    var dset=methodPtr(deckK,'SetPercent',1);
    if(dset){ Interceptor.attach(dset,{ onEnter:function(a){ HITS.deckSetPct=(HITS.deckSetPct||0)+1;
      try{ pushDeck({t:Date.now(), ev:'setPercent', self:a[0].toString(), p:a[1].readFloat()}); }catch(e){} } });
      out.push('RandomDeck.SetPercent@'+dset); }
  }
  var dcolK=findClass('','RandomDeckCollection');
  if(dcolK){
    var dd=methodPtr(dcolK,'Draw',2);
    if(dd){ Interceptor.attach(dd,{ onEnter:function(a){ HITS.deckCollDraw=(HITS.deckCollDraw||0)+1; this.id=a[1].toInt32(); this.p=a[2].readFloat(); },
      onLeave:function(rv){ try{ var v=(rv.toInt32()&1); if(st.collDraws.length<800) st.collDraws.push({id:this.id, p:this.p, v:v}); }catch(e){} } });
      out.push('RandomDeckCollection.Draw@'+dd); }
  }
  // --- UnityEngine.Random call census (Phase 1: who actually rolls the dice) ---
  var uerK=findClassAny('UnityEngine','Random');
  if(uerK){
    if(!st.uer) st.uer=[];
    var hookUeAll=function(nm){
      var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0)); var m, g=0;
      while(!(m=nf.il2cpp_class_get_methods(uerK,it)).isNull() && g++<4000){
        if(nf.il2cpp_method_get_name(m).readUtf8String()!==nm) continue;
        var pc=nf.il2cpp_method_get_param_count(m); var mp=m.readPointer();
        if(mp.isNull()||mp.compare(ptr('0x1000'))<=0) continue;
        (function(mp,pc,nm){ Interceptor.attach(mp,{ onEnter:function(a){
            HITS['uer_'+nm+'_'+pc]=(HITS['uer_'+nm+'_'+pc]||0)+1;
            this.pc=pc; this.v=[];
            for(var i=1;i<=pc && i<5;i++){ try{ this.v.push(a[i].toInt32()); }catch(e){ this.v.push(null); } } },
          onLeave:function(rv){ try{ if(st.uer.length<20000) st.uer.push({tag:nm, pc:this.pc, v:this.v, ri:rv.toInt32(), t:Date.now()}); }catch(e){} } }); })(mp,pc,nm);
        out.push('UnityEngine.Random.'+nm+'/'+pc+'@'+mp);
      }
    };
    hookUeAll('Range'); hookUeAll('RandomRangeInt'); hookUeAll('get_value');
  }
  // --- Phase 1: which slot of the bag the picker reads (index semantics) ---
  try{
    var alK=findClassAny('System.Collections','ArrayList');
    if(alK){
      if(!st.alget) st.alget=[];
      var aIt=Memory.alloc(Process.pointerSize); aIt.writePointer(ptr(0)); var am, ag=0;
      while(!(am=nf.il2cpp_class_get_methods(alK,aIt)).isNull() && ag++<4000){
        var anm=nf.il2cpp_method_get_name(am).readUtf8String();
        if(anm!=='get_Item' && anm!=='IndexOf' && anm!=='ToArray' && anm!=='CopyTo') continue;
        var apc=nf.il2cpp_method_get_param_count(am); var amp=am.readPointer();
        if(amp.isNull()||amp.compare(ptr('0x1000'))<=0) continue;
        (function(amp,apc,anm){ Interceptor.attach(amp,{ onEnter:function(a){
            try{ var idx=-1; try{ if(apc>=1) idx=a[1].toInt32(); }catch(eI2){}
              var sz=0; try{ sz=a[0].add(0x18).readS32(); }catch(eS2){}
              if(st.alget.length<20000) st.alget.push({m:anm, i:idx, sz:sz, t:Date.now()}); }catch(eA2){} } }); })(amp,apc,anm);
        out.push('ArrayList.'+anm+'/'+apc+'@'+amp);
      }
    }
  }catch(eAL){ out.push('ArrayList probe failed '+eAL); }
  // --- Phase 1: scenario picker observation (which set feeds each year) ---
  try{
    var getRet = N('il2cpp_method_get_return_type','pointer',['pointer']);
    var HEAP = ptr('0x1000000000');
    var argInfo = function(v){
      try{
        if(v.compare(HEAP)>0){ var kk=nf.il2cpp_object_get_class(v); if(!kk.isNull()){ var cn=nf.il2cpp_class_get_name(kk).readUtf8String();
            if(cn==='String') return toJS(v); return '<'+cn+'>'; } return v.toString(); }
        return v.toInt32();
      }catch(e){ return '?'; }
    };
    var hookPicks = function(k, filter, tag){
      if(!k){ log('picks '+tag+': class not found'); return; }
      var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0)); var m,g=0,n=0;
      while(!(m=nf.il2cpp_class_get_methods(k,it)).isNull() && g++<4000){
        var nm=nf.il2cpp_method_get_name(m).readUtf8String();
        if(!filter(nm)) continue;
        var p=m.readPointer(); if(p.isNull() || p.compare(ptr('0x1000'))<=0) continue;
        var rn='?'; try{ rn=tname(getRet(m)); }catch(e){}
        var pc=nf.il2cpp_method_get_param_count(m);
        if(n>=300) break;
        (function(p,nm,rn,pc,tag){ try{ Interceptor.attach(p,{
          onEnter:function(a){ HITS.picks=(HITS.picks||0)+1;
            try{ var args=[]; for(var i=1;i<=pc && i<5;i++) args.push(argInfo(a[i]));
              var ag=null; try{ if(st.hero && !st.hero.isNull()) ag=st.hero.add(0x20).readS32(); }catch(e0){}
              this.rec={tag:tag, m:nm, cls:rn, args:args, age:ag, t:Date.now()}; }catch(e){} },
          onLeave:function(rv){ try{ var r=this.rec; if(!r) return;
              r.ret=rv.isNull()?null:rv.toString();
              if(!rv.isNull() && rv.compare(HEAP)>0){
                var oc=nf.il2cpp_object_get_class(rv);
                if(!oc.isNull()){ r.retCls=nf.il2cpp_class_get_name(oc).readUtf8String();
                  if(st.picks.length<600) r.row=readObj(rv,['Index','Title','Body','Name','Cost','Commonness Score']); } }
              st.picks.push(r); if(st.picks.length>1500) st.picks.shift(); }catch(e){} } }); }catch(e){} })(p,nm,rn,pc,tag);
        n++;
      }
      log('picks '+tag+' hooked='+n);
    };
    try{
      var dk=findClass('','DumbScenarioInterface');
      var dmp=dk?methodPtr(dk,'GetARandomPushDumbScenario',0):null;
      if(!dmp){ log('pushdumb probe: method missing'); } else {
        try{ st.modBase=Process.findModuleByName('libil2cpp.so').base.toString(); }catch(eB){}
        Interceptor.attach(dmp,{
          onEnter:function(){ this.frames=[]; try{
              var bt=Thread.backtrace(this.context, Backtracer.ACCURATE);
              for(var i=0;i<bt.length && i<10;i++) this.frames.push(bt[i].toString());
            }catch(eT){} },
          onLeave:function(rv){ try{
              if(rv.isNull()) return;
              // DumbScenario.Data is the DataDumbScenario row object (dump
              // field 0x170); the Scenario base Title/Body (0x38/0x48) are
              // still empty at this point, so read the row instead:
              // <DumbScenarioType> 0x10, <Index> 0x14, <ExteriorTitle> 0x18,
              // <Title> 0x20, <Body> 0x28, <Cta> 0x30.
              var rec={t:Date.now(), frames:this.frames, cls:null, index:null, type:null, title:null, body:null, ext:null, cta:null};
              try{ rec.cls=nf.il2cpp_class_get_name(nf.il2cpp_object_get_class(rv)).readUtf8String(); }catch(eC){}
              var d=null;
              try{ d=rv.add(0x170).readPointer(); }catch(eD){}
              if(d && !d.isNull()){
                try{ rec.index=d.add(0x14).readS32(); }catch(e1){}
                try{ rec.type=d.add(0x10).readS32(); }catch(e2){}
                try{ rec.title=toJS(d.add(0x20).readPointer()); }catch(e3){}
                try{ rec.body=toJS(d.add(0x28).readPointer()); }catch(e4){}
                try{ rec.ext=toJS(d.add(0x18).readPointer()); }catch(e5){}
                try{ rec.cta=toJS(d.add(0x30).readPointer()); }catch(e6){}
              }
              if(st.pushDumb.length<400) st.pushDumb.push(rec);
            }catch(e){ HITS.pushDumbErr=String(e); } } });
        log('pushdumb probe attached @'+dmp);
      }
    }catch(eD){ log('pushdumb probe failed: '+eD); }
    hookPicks(findClass('','DumbScenarioInterface'), function(nm){ return nm.indexOf('Get')===0; }, 'dumb');
    hookPicks(findClass('','DataHub'), function(nm){ return nm.indexOf('GetRandom')===0; }, 'datahub');
    try{
      var base0=Module.findBaseAddress('libil2cpp.so');
      st.modInfo=(function(){ try{ var m=Process.findModuleByName('libil2cpp.so'); return {base:m.base.toString(), size:m.size}; }catch(e){ return null; } })();
      var scK=findClass('','Scenario');
      st.prodMap={}; st.prod=[]; st.anchors=[]; st.prodHooked=0;
      if(scK){
        var itc=Memory.alloc(Process.pointerSize); itc.writePointer(ptr(0)); var mc, gc=0;
        while(!(mc=nf.il2cpp_class_get_methods(scK,itc)).isNull() && gc++<4000){
          if(nf.il2cpp_method_get_name(mc).readUtf8String()!=='.ctor') continue;
          var cp=mc.readPointer(); if(cp.isNull() || cp.compare(ptr('0x1000'))<=0) continue;
          st.prodHooked++;
          Interceptor.attach(cp,{ onEnter:function(a){
            try{
              HITS.prod=(HITS.prod||0)+1;
              var cls='?';
              try{ var kk=nf.il2cpp_object_get_class(a[0]); cls=nf.il2cpp_class_get_name(kk).readUtf8String(); if(cls && cls!=='Scenario') hookClassCtors(kk,cls); }catch(e0){}
              var fr=[];
              var lim=(st.modInfo&&st.modInfo.size)?base0.add(st.modInfo.size):null;
              var keep=function(f){ if(f.compare(base0)<=0) return false; if(lim && f.compare(lim)>=0) return false; return true; };
              try{ var bt=(st.armBt? Thread.backtrace(this.context, Backtracer.ACCURATE) : []);
                   for(var i=0;i<bt.length && i<8;i++){ if(keep(bt[i])) fr.push(bt[i].sub(base0).toString()); } }catch(e1){
                try{ var bt2=Thread.backtrace(this.context, Backtracer.FUZZY);
                     for(var j=0;j<bt2.length && j<8;j++){ if(keep(bt2[j])) fr.push(bt2[j].sub(base0).toString()); } }catch(e2){} }
              var key=cls+'|'+fr.slice(0,3).join(',');
              if(st.prodMap[key]){ st.prodMap[key].n++; }
              else if(st.prod.length<600){ var rec={cls:cls,frames:fr,n:1}; st.prodMap[key]=rec; st.prod.push(rec); }
            }catch(e){}
          }});
        }
      }
    var hookClassCtors=function(kk,cls){
      try{
        st.ctorSeen=st.ctorSeen||{}; st.ctorRecs=st.ctorRecs||[];
        if(st.ctorSeen[cls] || Object.keys(st.ctorSeen).length>=60) return;
        st.ctorSeen[cls]=1;
        var itx=Memory.alloc(Process.pointerSize); itx.writePointer(ptr(0)); var mx, gx=0;
        while(!(mx=nf.il2cpp_class_get_methods(kk,itx)).isNull() && gx++<4000){
          if(nf.il2cpp_method_get_name(mx).readUtf8String()!=='.ctor') continue;
          var px=mx.readPointer(); if(px.isNull() || px.compare(ptr('0x1000'))<=0) continue;
          var pc=nf.il2cpp_method_get_param_count(mx);
          (function(px,pc,cls){ try{ Interceptor.attach(px,{ onEnter:function(a){
            try{ var args=[]; for(var i=1;i<=pc && i<=6;i++) args.push(argInfo(a[i]));
              var fr=[]; var lim2=(st.modInfo&&st.modInfo.size)?base0.add(st.modInfo.size):null;
              try{ var bt=(st.armBt? Thread.backtrace(this.context, Backtracer.ACCURATE) : []);
                for(var i2=0;i2<bt.length && i2<9;i2++){ var f=bt[i2]; if(f.compare(base0)>0 && (!lim2 || f.compare(lim2)<0)) fr.push(f.sub(base0).toString()); } }catch(e3){}
              st.ctorRecs.push({cls:cls, args:args, frames:fr, t:Date.now()});
              if(st.ctorRecs.length>400) st.ctorRecs.shift();
            }catch(e){}
          }}); }catch(e){} })(px,pc,cls);
        }
      }catch(e){}
    };
    try{
      var ri=(st.armInvoke? E('il2cpp_runtime_invoke') : null);
      if(ri){
        Interceptor.attach(ri,{ onEnter:function(a){
          try{
            var mi=a[0]; if(mi.isNull()) return;
            var nm2=nf.il2cpp_method_get_name(mi).readUtf8String();
            if(nm2!=='.ctor') return;
            var kk2=nf.il2cpp_method_get_class(mi); if(kk2.isNull()) return;
            var cn2=nf.il2cpp_class_get_name(kk2).readUtf8String();
            if(cn2.indexOf('Scenario')<0) return;
            var pc2=nf.il2cpp_method_get_param_count(mi);
            var args2=[];
            try{ var parr=a[2];
              if(!parr.isNull() && pc2>0){ var pb=parr.readPointer();
                for(var i3=0;i3<pc2 && i3<6;i3++){ try{ var pv=pb.add(i3*Process.pointerSize).readPointer(); args2.push(argInfo(pv)); }catch(e4){ args2.push('?'); } } }
            }catch(e5){}
            var fr2=[]; var lim3=(st.modInfo&&st.modInfo.size)?base0.add(st.modInfo.size):null;
            try{ var bt3=(st.armBt? Thread.backtrace(this.context, Backtracer.ACCURATE) : []);
              for(var j3=0;j3<bt3.length && j3<9;j3++){ var f3=bt3[j3]; if(f3.compare(base0)>0 && (!lim3 || f3.compare(lim3)<0)) fr2.push(f3.sub(base0).toString()); } }catch(e6){}
            st.ctorRecs=st.ctorRecs||[];
            st.ctorRecs.push({cls:cn2, args:args2, frames:fr2, src:'invoke', t:Date.now()});
            if(st.ctorRecs.length>400) st.ctorRecs.shift();
            HITS.invokeCtor=(HITS.invokeCtor||0)+1;
          }catch(e7){}
        }});
        log('[inv] runtime_invoke .ctor capture armed @'+ri.sub(base0));
      } else log('[inv] no il2cpp_runtime_invoke export');
    }catch(e){ log('[inv] install err '+e); }
      var specs=[['Life','IncrementAge',0],['SimHero','IncrementAge',0],['Scenario','.ctor',0],['LifeLog','Log',1],['RandomDeck','Draw',0]];
      for(var ai=0; ai<specs.length; ai++){ try{
        var ak=findClass('',specs[ai][0]); if(!ak) continue;
        var am=findMethod(ak,specs[ai][1],specs[ai][2]); if(!am) continue;
        var ap=am.readPointer(); if(ap.isNull()) continue;
        st.anchors.push({cls:specs[ai][0],name:specs[ai][1],argc:specs[ai][2],off:ap.sub(base0).toString()});
      }catch(e){} }
      log('[prod] Scenario ctors hooked='+st.prodHooked+' anchors='+st.anchors.length);
    }catch(e){ log('prod install err '+e); }
  }catch(e){ log('picks install err '+e); }
  return out;
}
function setup(){
  if(ready) return true;
  if(!Module.findBaseAddress('libil2cpp.so')) return false;
  try{
    nf={
      il2cpp_domain_get: N('il2cpp_domain_get','pointer',[]),
      il2cpp_thread_attach: N('il2cpp_thread_attach','pointer',['pointer']),
      il2cpp_domain_get_assemblies: N('il2cpp_domain_get_assemblies','pointer',['pointer','pointer']),
      il2cpp_assembly_get_image: N('il2cpp_assembly_get_image','pointer',['pointer']),
      il2cpp_image_get_class_count: N('il2cpp_image_get_class_count','uint32',['pointer']),
      il2cpp_image_get_class: N('il2cpp_image_get_class','pointer',['pointer','uint32']),
      il2cpp_class_get_name: N('il2cpp_class_get_name','pointer',['pointer']),
      il2cpp_class_get_namespace: N('il2cpp_class_get_namespace','pointer',['pointer']),
      il2cpp_method_get_class: N('il2cpp_method_get_class','pointer',['pointer']),
      il2cpp_image_get_name: N('il2cpp_image_get_name','pointer',['pointer']),
      il2cpp_class_get_parent: N('il2cpp_class_get_parent','pointer',['pointer']),
      il2cpp_class_get_fields: N('il2cpp_class_get_fields','pointer',['pointer','pointer']),
      il2cpp_object_new: N('il2cpp_object_new','pointer',['pointer']),
      il2cpp_class_get_method_from_name: N('il2cpp_class_get_method_from_name','pointer',['pointer','pointer','int']),
      il2cpp_class_get_methods: N('il2cpp_class_get_methods','pointer',['pointer','pointer']),
      il2cpp_method_get_name: N('il2cpp_method_get_name','pointer',['pointer']),
      il2cpp_method_get_param_count: N('il2cpp_method_get_param_count','uint32',['pointer']),
      il2cpp_method_get_param: N('il2cpp_method_get_param','pointer',['pointer','uint32']),
      il2cpp_field_get_name: N('il2cpp_field_get_name','pointer',['pointer']),
      il2cpp_field_get_type: N('il2cpp_field_get_type','pointer',['pointer']),
      il2cpp_field_get_flags: N('il2cpp_field_get_flags','uint32',['pointer']),
      il2cpp_field_get_offset: N('il2cpp_field_get_offset','uint32',['pointer']),
      il2cpp_type_get_name: N('il2cpp_type_get_name','pointer',['pointer']),
      il2cpp_object_get_class: N('il2cpp_object_get_class','pointer',['pointer']),
      il2cpp_class_is_enum: N('il2cpp_class_is_enum','bool',['pointer']),
      il2cpp_runtime_invoke: N('il2cpp_runtime_invoke','pointer',['pointer','pointer','pointer','pointer']),
      il2cpp_string_new: N('il2cpp_string_new','pointer',['pointer']),
      il2cpp_class_get_type: N('il2cpp_class_get_type','pointer',['pointer']),
      il2cpp_type_get_object: N('il2cpp_type_get_object','pointer',['pointer'])
    };
    var d=nf.il2cpp_domain_get();
    if(d.isNull() || d.compare(ptr('0x10000'))<0) return false;
    nf.il2cpp_thread_attach(d);
    var cnt=Memory.alloc(4); var asms=nf.il2cpp_domain_get_assemblies(d,cnt); var n=cnt.readU32();
    for(var i=0;i<n;i++){ var im=nf.il2cpp_assembly_get_image(asms.add(i*Process.pointerSize).readPointer()); if(im.isNull()) continue;
      var nm=nf.il2cpp_image_get_name(im).readUtf8String(); if(nm && nm.indexOf('Assembly-CSharp')>=0){ IMG=im; break; } }
    if(!IMG) return false;
    var cls=findClass('','EncryptionManager');
    var M={};
    if(cls){ var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0));
      while(true){ var mi=nf.il2cpp_class_get_methods(cls,it); if(mi.isNull()) break; M[nf.il2cpp_method_get_name(mi).readUtf8String()]=mi; } }
    function call0(nm){ var e=Memory.alloc(Process.pointerSize); e.writePointer(ptr(0)); return nf.il2cpp_runtime_invoke(M[nm], ptr(0), ptr(0), e); }
    function call2(nm,a,b){ var buf=Memory.alloc(16); buf.writePointer(fromJS(a)); buf.add(8).writePointer(fromJS(b));
      var e=Memory.alloc(Process.pointerSize); e.writePointer(ptr(0)); return nf.il2cpp_runtime_invoke(M[nm], ptr(0), buf, e); }
    var key=function(){ try { return toJS(call0('DefaultCipherKey')); } catch(e){ return null; } };

var RB={hooked:false,err:null,rec:[]};
function rbBias(){
  var k=findClass('','BracketParser');
  var gp=k? findMethodHier(k,'GetParsedString',1) : null;
  if(!gp) return null;
  return gp.sub(ptr('0x73f337a04c'));
}
function rbHookAbs(bias, dumpAddr, name, kind){
  var fn=ptr(dumpAddr).add(bias);
  Interceptor.attach(fn,{ onEnter:function(a){
      var r={fn:name, k:kind, t:Date.now()};
      try{
        if(kind==='pick0'){ r.pool=toJS(a[0]); r.sep=toJS(a[1]); }
        else if(kind==='getparsed'){ r.in=toJS(a[0]); }
        else { r.br=toJS(a[1]); if(kind==='randbr'){ r.idx=a[2].toInt32(); } }
      }catch(e){ r.err=String(e); }
      this.r=r; if(kind==='randbr'){ try{ this.dc=a[3]; }catch(e2){} }
    },
    onLeave:function(ret){
      try{
        var r=this.r;
        if(r.k==='randbr' && this.dc){ try{ r.out=toJS(this.dc.add(8).readPointer()); }catch(e3){} }
        if(r.k==='getparsed'){ r.ret=toJS(ret); try{ if(!(String(r.in||'').indexOf('<rb')>=0 || String(r.ret||'').indexOf('rb')>=0)) return; }catch(e4){} }
        if(RB.rec.length<6000) RB.rec.push(r);
      }catch(e5){}
    }});
  return fn.toString();
}
function rbInstall(){
  try{
    RB={hooked:false,err:null,rec:[]};
    var bias=rbBias();
    if(!bias) { RB.err='no BracketParser.GetParsedString'; return {err:RB.err}; }
    var addrs={ReplaceBrackets:'0x73f337c0dc',ApplyBracketReplacements:'0x73f337c24c',ReplaceAndCacheRandomBracket:'0x73f337c85c',pick:'0x73f4264a80',GetParsedString:'0x73f337a04c'};
    var out={}, kinds={ReplaceBrackets:'br',ApplyBracketReplacements:'br',ReplaceAndCacheRandomBracket:'randbr',pick:'pick0',GetParsedString:'getparsed'};
    for(var nm in addrs){ out[nm]=rbHookAbs(bias, addrs[nm], nm, kinds[nm]); }
    RB.hooked=true; RB.fns=out; RB.bias=bias.toString();
    return {ok:true, bias:RB.bias, fns:out};
  }catch(e){ RB.err=String(e); return {err:RB.err}; }
}
function rbDump(n){ return {hooked:RB.hooked, err:RB.err, bias:RB.bias, n:RB.rec.length, rows:RB.rec.slice(-(n||120))}; }


var RB2={hooked:false,err:null,rec:[],info:null};
function rbInstall2(){
  try{
    RB2={hooked:false,err:null,rec:[],info:null};
    var k=findClass('','BracketParser');
    if(!k) return {err:'no BracketParser class'};
    var info={cls:k.toString(), fns:{}};
    function hookOne(mname, argc, kind){
      var mi=findMethodHier(k,mname,argc);
      if(!mi) { info.fns[mname]='miss'; return; }
      info.fns[mname]=mi.toString();
      try{
        Interceptor.attach(mi,{ onEnter:function(a){
            var r={fn:mname,k:kind,t:Date.now()};
            try{ r.in=toJS(a[0]); }catch(e){}
            this.r=r;
          }, onLeave:function(ret){
            try{ var r=this.r; r.out=toJS(ret);
              var s1=String(r.in||''), s2=String(r.out||'');
              if(s1.indexOf('<rb')>=0 || (s2.length>0 && s1.length>0 && s1!==s2 && s2.length<4000)){
                if(RB2.rec.length<5000) RB2.rec.push(r);
              }
            }catch(e){}
          }});
      }catch(e){ info.fns[mname]='attachfail:'+String(e); }
    }
    hookOne('GetParsedString',1,'gps');
    hookOne('GetParsedUniversalStringAndIsBody',2,'gpu');
    hookOne('GetParsedBodyString',1,'gpb');
    hookOne('PrepareParser',0,'prep');
    RB2.hooked=true; RB2.info=info;
    return {ok:true, info:info};
  }catch(e){ RB2.err=String(e); return {err:RB2.err}; }
}
function rbDump2(n){ return {hooked:RB2.hooked, err:RB2.err, info:RB2.info, n:RB2.rec.length, rows:RB2.rec.slice(-(n||100))}; }



function dictRows(d,max){
  try{
    var k=nf.il2cpp_object_get_class(d); var fs=fieldsOf(k); var off={};
    for(var i=0;i<fs.length;i++) off[fs[i].name]=fs[i].off;
    var count=d.add(off['_count']).readS32();
    var entries=d.add(off['_entries']).readPointer();
    var rows=[];
    for(var j=0;j<count && j<(max||400);j++){
      var ep=entries.add(0x20+j*0x18);
      var kp=ep.add(8).readPointer();
      var vp=ep.add(0x10);
      var sv=null; try{ sv=toJS(vp.readPointer()); }catch(e){ sv=null; }
      rows.push([toJS(kp), sv, vp.readS32(), vp.readPointer().toString()]);
    }
    return {count:count, rows:rows, offs:off};
  }catch(e){ return {err:String(e)}; }
}
function statFields(clsName){
  var k=findClass('',clsName); if(!k) return {err:'no class '+clsName};
  var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0)); var fi,out=[],g=0;
  var buf=Memory.alloc(0x10);
  while(!(fi=nf.il2cpp_class_get_fields(k,it)).isNull() && g++<3000){
    var flags=nf.il2cpp_field_get_flags(fi);
    if(!(flags & 0x10)) continue;
    var nm=nf.il2cpp_field_get_name(fi).readUtf8String();
    var tp=tname(nf.il2cpp_field_get_type(fi));
    var foff=nf.il2cpp_field_get_offset(fi);
    var o={name:nm,type:tp,addr:fi.toString(),foff:foff};
    try{
      var v=null;
      try{ var blk=nf.il2cpp_class_get_static_field_data(k); if(blk && !blk.isNull()) v=blk.add(foff).readPointer(); }catch(eSB){ o.sbErr=String(eSB); }
      if(v===null||v===undefined) v=ptr(o.addr).readPointer();
      o.ptr=v.toString();
      if(!v.isNull()){ o.cls=className(v);
        if(tp.indexOf('Dictionary')>=0) o.dict=dictRows(v,400);
        else if(tp.indexOf('List')>=0){ var fs=fieldsOf(nf.il2cpp_object_get_class(v)); var mo={}; for(var q=0;q<fs.length;q++) mo[fs[q].name]=fs[q].off;
          var items=v.add(mo['_items']).readPointer(); var cnt=v.add(mo['_size']).readS32(); var arr=[];
          for(var w=0;w<cnt && w<200;w++) arr.push(toJS(items.add(0x20+w*Process.pointerSize).readPointer()));
          o.list={count:cnt,rows:arr}; }
      }
    }catch(e){ o.err=String(e); }
    out.push(o);
  }
  return out;
}

function ranges(){
  var out={big:[], il2:[], mods:[]};
  try{
    Process.enumerateRanges({protection:'r--',coalesce:true}).forEach(function(r){
      if(r.size>4*1024*1024) out.big.push({base:r.base.toString(),size:r.size,prot:r.protection,file:r.file?r.file.path:null});
    });
  }catch(e){ out.err=String(e); }
  try{
    Process.enumerateModules().forEach(function(m){ if(m.name.indexOf('il2cpp')>=0||m.name.indexOf('unity')>=0||m.name.indexOf('main')>=0) out.mods.push({name:m.name,base:m.base.toString(),size:m.size,path:m.path}); });
  }catch(e){ out.merr=String(e); }
  return out;
}

function rbBytes(a,n){ try{ var b=ptr(a).readByteArray(n||16); return {addr:ptr(a).toString(), hex:Array.prototype.map.call(new Uint8Array(b),function(x){return ('0'+x.toString(16)).slice(-2);}).join(' '), range:(function(){var r=Process.findRangeByAddress(ptr(a)); return r?{base:r.base.toString(),size:r.size,prot:r.protection}:null;})()}; }catch(e){ return {addr:String(a), err:String(e)}; } }
function rbProbe(){
  var out={};
  var sp=findClass('','SimPerson'); var spg=findMethodHier(sp,'set_Att_health',1);
  var bp=findClass('','BracketParser');
  var gps=findMethodHier(bp,'GetParsedString',1);
  var gpu=findMethodHier(bp,'GetParsedUniversalStringAndIsBody',2);
  var out2=[];
  ['0x73f30e0508','0x73f337a04c','0x73f337a054','0x73f337c0dc','0x73f337c85c','0x73f4264a80','0x73f49a5f80'].forEach(function(a){
    var t=ptr(a);
    if(spg && a==='0x73f30e0508') t=spg;
    if(gps && a==='0x73f337a04c') t=gps;
    if(gpu && a==='0x73f337a054') t=gpu;
    out2.push(rbBytes(t,16));
  });
  out.resolved=out2;
  if(spg){ var bias=spg.sub(ptr('0x73f30e0508')); out.bias=bias.toString();
    out.pred=[]; ['0x73f337a04c','0x73f337c0dc','0x73f4264a80'].forEach(function(a){ out.pred.push(rbBytes(ptr(a).add(bias),16)); });
  }
  try{
    var rs=[];
    Process.enumerateRanges({protection:'r-x',coalesce:false}).forEach(function(r){ if(r.size>2*1024*1024) rs.push({base:r.base.toString(),size:r.size,file:r.file?r.file.path:null}); });
    out.execBig=rs;
  }catch(e){ out.execErr=String(e); }
  return out;
}

function codeOf(k,name,argc){ var m=findMethodHier(k,name,argc); if(!m) return null; try{ return m.readPointer(); }catch(e){ return null; } }
function rbAddr(){ var out={};
  var sp=findClass('','SimPerson'); var bp=findClass('','BracketParser');
  function add(lbl,k,n,a,da){ var c=codeOf(k,n,a); out[lbl]={code:c?c.toString():null, dump:da, delta:c?c.sub(ptr(da)).toString():null}; }
  add('set_Att_health',sp,'set_Att_health',1,'0x73f30e0508');
  add('GetParsedString',bp,'GetParsedString',1,'0x73f337a04c');
  add('GetParsedUniversalStringAndIsBody',bp,'GetParsedUniversalStringAndIsBody',2,'0x73f337a054');
  add('ReplaceBrackets',bp,'<GetParsedUniversalStringAndIsBody>g__ReplaceBrackets|159_0',3,'0x73f337c0dc');
  add('cctor',bp,'.cctor',0,'0x73f337d5e8');
  add('PrepareParser',bp,'PrepareParser',0,'0x73f3379640');
  return out;
}
var RBC={ insts:[], hooks:[], err:null, n:0 };

var HOOKS=[];
function biasOf(){ var sp=findClass('','SimPerson'); var m=findMethodHier(sp,'set_Att_health',1);
  if(!m) return null; return m.readPointer().sub(ptr('0x73f30e0508')); }

function hookDicts(addrStr,maxRows,argc){
  try{
    var b=biasOf(); if(!b) return {err:'no bias'};
    var t=ptr(addrStr).add(b);
    var rec=[];
    var h={addr:addrStr, real:t.toString(), tag:addrStr, rec:rec};
    var n=(argc===undefined||argc===null)?4:argc;
    Interceptor.attach(t,{onEnter:function(args){
      try{
        var o={};
        for(var i=0;i<n;i++){
          try{ o['cls'+i]=className(args[i]); }catch(e){}
          try{ var c=o['cls'+i]||''; if(c.indexOf('Dictionary')<0){ o['s'+i]=toJS(args[i]); continue; }
            var d=dictRows(args[i], maxRows||160);
            o['d'+i]={count:d.count, rows:(d.rows||[]).slice(0, maxRows||160)};
          }catch(e){ o['e'+i]=String(e); }
        }
        rec.push(o); if(rec.length>60) rec.shift();
      }catch(e){ rec.push({err:String(e)}); }
    }});
    HOOKS.push(h); return {ok:true, real:t.toString()};
  }catch(e){ return {err:String(e)}; }
}
function hookAt(addrStr,tag){
  try{
    var b=biasOf(); if(!b) return {err:'no bias'};
    var t=ptr(addrStr).add(b);
    var rec=[];
    var h={addr:addrStr, real:t.toString(), tag:tag||addrStr, rec:rec};
    Interceptor.attach(t,{onEnter:function(args){
      try{
        var o={a0:null,a1:null,a2:null};
        try{ o.a0=toJS(args[0]); }catch(e){ o.a0e=String(e); }
        try{ o.a0cls=className(args[0]); }catch(e){}
        try{ o.a1=toJS(args[1]); }catch(e){}
        try{ o.a1cls=className(args[1]); }catch(e){}
        try{ o.a2=args[2].toInt32(); }catch(e){}
        rec.push(o); if(rec.length>1500) rec.shift();
        if(o.a0cls && o.a0cls.indexOf('BracketParser')>=0) RBC.insts.push(args[0]);
      }catch(e){ rec.push({err:String(e)}); }
    }, onLeave:function(ret){
      try{ if(rec.length){ try{ rec[rec.length-1].ret=toJS(ret); }catch(e){ rec[rec.length-1].retErr=String(e); } } }catch(e){}
      try{ if(ret && !ret.isNull()){ var c=className(ret); if(c && c.indexOf('BracketParser')>=0){ RBC.insts.push(ret); if(RBC.insts.length>80) RBC.insts.shift(); } } }catch(e){}
    }});
    HOOKS.push(h);
    return {ok:true, real:t.toString(), bias:b.toString(), tag:h.tag};
  }catch(e){ return {err:String(e)}; }
}
function hookDump(){ return HOOKS.map(function(h){ return {tag:h.tag, addr:h.addr, real:h.real, n:h.rec.length, rows:h.rec.slice(-40)}; }); }
function rbCapInstall(){
  try{
    var bp=findClass('','BracketParser');
    [[0,'.ctor'],[1,'.ctor'],[3,'.ctor'],[4,'.ctor']].forEach(function(pair){
      try{
        var m=findMethodHier(bp,pair[1],pair[0]); if(!m) return;
        var code=m.readPointer();
        Interceptor.attach(code,{ onEnter:function(args){ try{ RBC.insts.push(args[0]); if(RBC.insts.length>60) RBC.insts.shift(); }catch(e){} } });
        RBC.hooks.push(pair[1]+'/'+pair[0]+'@'+code);
      }catch(e){ RBC.err=String(e); }
    });
    return {ok:true, hooks:RBC.hooks, err:RBC.err};
  }catch(e){ return {err:String(e)}; }
}


function fieldOff(o,name){ try{ var fs=fieldsOf(nf.il2cpp_object_get_class(o)); for(var i=0;i<fs.length;i++) if(fs[i].name===name) return fs[i].off; }catch(e){} return null; }
function listInsts(max){
  var out=[];
  for(var i=0;i<RBC.insts.length && out.length<(max||60);i++){
    var p=RBC.insts[i]; if(!p||!p.isNull||p.isNull()) continue;
    if(p.compare(ptr('0x10000000000'))<0) continue;
    var o={i:i, ptr:p.toString()};
    try{ o.cls=className(p); }catch(e){ o.clsErr=String(e); }
    try{ var off=fieldOff(p,'SelectedRandomBracketList'); o.rblOff=off;
      if(off!==null){ var L=p.add(off).readPointer(); o.rblPtr=L.toString(); o.rblCount=listCount(L);
        var li=listItems(L,16); o.rbl=li.rows; } }catch(e){ o.rblErr=String(e); }
    try{ var ob=fieldOff(p,'<BodyBracketDictionary>k__BackingField')===null?fieldOff(p,'BodyBracketDictionary'):fieldOff(p,'<BodyBracketDictionary>k__BackingField');
      if(ob!==null){ var B=p.add(ob).readPointer(); o.bodyPtr=B.toString(); o.bodyCount=dictCount(B); } }catch(e){ o.bodyErr=String(e); }
    try{ var on=fieldOff(p,'<NarrativeBracketDictionary>k__BackingField')===null?fieldOff(p,'NarrativeBracketDictionary'):fieldOff(p,'<NarrativeBracketDictionary>k__BackingField');
      if(on!==null){ var N=p.add(on).readPointer(); o.narrPtr=N.toString(); o.narrCount=dictCount(N); } }catch(e){ o.narrErr=String(e); }
    out.push(o);
  }
  return {total:RBC.insts.length, rows:out};
}
function pfields(idx){
  try{
    var p=RBC.insts[idx]; if(!p||p.isNull()) return {err:'no inst', n:RBC.insts.length};
    var fs=fieldsOf(nf.il2cpp_object_get_class(p));
    var out=[];
    for(var i=0;i<fs.length;i++){
      var f=fs[i]; var o={n:f.name, t:f.type, off:f.off};
      try{ var v=p.add(f.off).readPointer(); if(!v.isNull()){ o.ptr=v.toString(); o.cls=className(v);
        if(o.cls && o.cls.indexOf('Dictionary')>=0) o.count=dictCount(v);
        else if(o.cls && o.cls.indexOf('List')>=0) o.count=listCount(v);
      } }catch(e){ o.err=String(e); }
      out.push(o);
    }
    return {n:RBC.insts.length, ptr:p.toString(), fields:out};
  }catch(e){ return {err:String(e)}; }
}
function dictCount(d){ try{ var fs=fieldsOf(nf.il2cpp_object_get_class(d)); for(var i=0;i<fs.length;i++) if(fs[i].name==='_count') return d.add(fs[i].off).readS32(); }catch(e){} return null; }
function listCount(d){ try{ var fs=fieldsOf(nf.il2cpp_object_get_class(d)); for(var i=0;i<fs.length;i++) if(fs[i].name==='_size') return d.add(fs[i].off).readS32(); }catch(e){} return null; }
function listItems(d,max){ try{ var fs=fieldsOf(nf.il2cpp_object_get_class(d)); var mo={}; for(var i=0;i<fs.length;i++) mo[fs[i].name]=fs[i].off;
  var items=d.add(mo['_items']).readPointer(), cnt=d.add(mo['_size']).readS32(), out=[];
  for(var j=0;j<cnt&&j<(max||30);j++){ var it=items.add(0x20+j*Process.pointerSize).readPointer(); out.push({ptr:it.toString(), cls:it.isNull()?null:className(it), count:it.isNull()?null:dictCount(it)}); }
  return {count:cnt, rows:out}; }catch(e){ return {err:String(e)}; } }
function poolDump(idx, listIdx, max){
  try{
    var p=RBC.insts[idx]; if(!p||p.isNull()) return {err:'no inst'};
    var fs=fieldsOf(nf.il2cpp_object_get_class(p)); var f=null;
    for(var i=0;i<fs.length;i++) if(fs[i].name==='SelectedRandomBracketList'){ f=fs[i]; break; }
    if(!f) return {err:'no field'};
    var L=p.add(f.off).readPointer(); if(L.isNull()) return {err:'list null'};
    var li=listItems(L,40);
    var target=null;
    if(listIdx!==null && listIdx!==undefined){ target=(li.rows[listIdx]||{}).ptr; }
    if(!target) return {n:RBC.insts.length, count:li.count, rows:li.rows.slice(0,20)};
    var D=ptr(target); var r=dictRows(D, max||200);
    return {count:li.count, idx:listIdx, dictCount:r.count, rows:r.rows};
  }catch(e){ return {err:String(e)}; }
}
function rbCapDump(n){
  var out=[];
  var seen=0;
  for(var i=RBC.insts.length-1;i>=0 && out.length<(n||20);i--){
    var p=RBC.insts[i]; if(!p||p.isNull()) continue;
    try{
      var o=objOf(p); if(!o) continue;
      var fs=fieldsOf(o.k); var off={}; for(var j=0;j<fs.length;j++) off[fs[j].name]=fs[j].off;
      var rec={ptr:p.toString(), cls:className(p)};
      var body=p.add(off['<BodyBracketDictionary>k__BackingField']).readPointer();
      var narr=p.add(off['<NarrativeBracketDictionary>k__BackingField']).readPointer();
      if(!body.isNull()) rec.body=dictRows(body,60);
      if(!narr.isNull()) rec.narr=dictRows(narr,60);
      var rbl=p.add(off['SelectedRandomBracketList']).readPointer();
      if(!rbl.isNull()){
        var rfs=fieldsOf(nf.il2cpp_object_get_class(rbl)); var roff={};
        for(var q=0;q<rfs.length;q++) roff[rfs[q].name]=rfs[q].off;
        var items=rbl.add(roff['_items']).readPointer(); var cnt=rbl.add(roff['_size']).readS32();
        rec.rblCount=cnt; rec.rbl=[];
        var lim=Math.min(cnt,24);
        for(var w=0;w<lim;w++){
          var d=items.add(0x20+w*Process.pointerSize).readPointer();
          if(d.isNull()){ rec.rbl.push(null); continue; }
          var dr=dictRows(d,40); dr.idx=w; rec.rbl.push(dr);
        }
      }
      out.push(rec);
    }catch(e){ out.push({err:String(e), ptr:p.toString()}); }
  }
  return {n:RBC.insts.length, rows:out};
}

function rdict(addrStr){
  try{ var v=ptr(addrStr).readPointer(); var o={addr:ptr(addrStr).toString(), ptr:v.toString(), cls:(v.isNull()?null:className(v))};
    if(!v.isNull() && o.cls && o.cls.indexOf('Dictionary')>=0) o.dict=dictRows(v,400);
    else if(!v.isNull() && o.cls && o.cls.indexOf('List')>=0){ var fs=fieldsOf(nf.il2cpp_object_get_class(v)); var mo={}; for(var q=0;q<fs.length;q++) mo[fs[q].name]=fs[q].off;
      var items=v.add(mo['_items']).readPointer(); var cnt=v.add(mo['_size']).readS32(); var arr=[];
      for(var w=0;w<cnt && w<400;w++){ var it=items.add(0x20+w*Process.pointerSize).readPointer(); arr.push({s:toJS(it), cls:it.isNull()?null:className(it)}); }
      o.list={count:cnt, rows:arr}; }
    return o;
  }catch(e){ return {addr:String(addrStr), err:String(e)}; }
}

function fieldObj(clsName, fieldName){
  try{
    var k=findClass('',clsName); if(!k) return {err:'no class'};
    var fi=nf.il2cpp_class_get_field_from_name(k, Memory.allocUtf8String(fieldName));
    if(!fi || fi.isNull()) return {err:'no field '+fieldName};
    var fn2=new NativeFunction(E('il2cpp_field_get_value_object'),'pointer',['pointer','pointer']);
    var o=fn2(fi, ptr(0));
    return {ptr:o.toString(), cls: o.isNull()?null:className(o), off:nf.il2cpp_field_get_offset(fi)};
  }catch(e){ return {err:String(e)}; }
}
function sd(){
  var out={};
  ['RB_Brackets','FormattingTags','MaleFemalePronounBracketsForGenderStrings','HeroPronounBracketsForPointOfViewStrings'].forEach(function(nm){
    var r=fieldObj('BracketParser',nm);
    try{
      if(r.ptr && r.cls){
        var p=ptr(r.ptr);
        if(r.cls.indexOf('Dictionary')>=0) r.dict=dictRows(p,600);
        else if(r.cls.indexOf('List')>=0){
          var fs=fieldsOf(nf.il2cpp_object_get_class(p)); var mo={}; for(var i=0;i<fs.length;i++) mo[fs[i].name]=fs[i].off;
          var items=p.add(mo['_items']).readPointer(), cnt=p.add(mo['_size']).readS32(); var arr=[];
          for(var i2=0;i2<cnt&&i2<400;i2++) arr.push(toJS(items.add(0x20+i2*Process.pointerSize).readPointer()));
          r.list={count:cnt, rows:arr};
        }
      }
    }catch(e){ r.readErr=String(e); }
    out[nm]=r;
  });
  return out;
}

function staticVal(clsName, fieldName){
  var out={cls:clsName, field:fieldName};
  try{
    var k=findClass('',clsName); if(!k){ out.err='no class'; return out; }
    var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0)); var fi, g=0, found=null, off=null;
    while(!(fi=nf.il2cpp_class_get_fields(k,it)).isNull() && g++<3000){
      if(!(nf.il2cpp_field_get_flags(fi)&0x10)) continue;
      if(nf.il2cpp_field_get_name(fi).readUtf8String()===fieldName){ found=fi; off=nf.il2cpp_field_get_offset(fi); break; }
    }
    if(!found){ out.err='no field'; return out; }
    out.off=off;
    try{ out.blk=(nf.il2cpp_class_get_static_field_data? nf.il2cpp_class_get_static_field_data(k).toString():'no export'); }catch(e){ out.blkErr=String(e); }
    function tryRead(addr,lbl){
      try{ var v=addr.readPointer(); out[lbl]={addr:addr.toString(), ptr:v.toString(), cls:v.isNull()?null:className(v)}; }catch(e){ out[lbl]={addr:addr.toString(), err:String(e)}; }
    }
    if(out.blk && out.blk!=='no export') tryRead(ptr(out.blk).add(off), 'viaBlk');
    if(off) tryRead(ptr(off), 'viaOff');
    try{ tryRead(ptr(off).readPointer(), 'derefOff'); }catch(e){}
    // dictionary dump if any interpretation looks like a Dictionary
    ['viaBlk','viaOff','derefOff'].forEach(function(lbl){
      var r=out[lbl]; if(!r||!r.cls) return;
      try{ if(r.cls.indexOf('Dictionary')>=0){ var p=ptr(r.ptr); r.dict=dictRows(p,600); }
        else if(r.cls.indexOf('List')>=0){ var pp=ptr(r.ptr); var fs=fieldsOf(nf.il2cpp_object_get_class(pp)); var mo={}; for(var i=0;i<fs.length;i++) mo[fs[i].name]=fs[i].off;
          var items=pp.add(mo['_items']).readPointer(), cnt=pp.add(mo['_size']).readS32(), arr=[];
          for(var i2=0;i2<cnt&&i2<400;i2++) arr.push(toJS(items.add(0x20+i2*Process.pointerSize).readPointer()));
          r.list={count:cnt, rows:arr}; }
      }catch(e){ r.dumpErr=String(e); }
    });
  }catch(e){ out.err=String(e); }
  return out;
}
function sv(){ var o={}; ['RB_Brackets','FormattingTags','MaleFemalePronounBracketsForGenderStrings','HeroPronounBracketsForPointOfViewStrings'].forEach(function(n){ o[n]=staticVal('BracketParser',n); }); return o; }
function rbStat(){ var r={}; try{ r.BracketParser=statFields('BracketParser'); }catch(e){ r.err=String(e); } return r; }

function rbCal(){
  var out={};
  try{
    var m=Process.findModuleByName('libil2cpp.so');
    out.modBase=m.base.toString(); out.modSize=m.size;
    function p2(mname,ns,argc){ var k=findClass(ns||'',mname.split('.')[0]);
      var cls=findClass(ns||'',mname.split('.')[0]);
      var kk=findClass(ns||'', mname.split('.')[0]);
      return null; }
    var sp=findClass('','SimPerson');
    var spg=findMethodHier(sp,'set_Att_health',1);
    var bp=findClass('','BracketParser');
    var gps=findMethodHier(bp,'GetParsedString',1);
    var gpu=findMethodHier(bp,'GetParsedUniversalStringAndIsBody',2);
    function info(lbl,ptrv,dumpAddr){
      var o={ptr:ptrv?ptrv.toString():null};
      if(ptrv){ var r=Process.findRangeByAddress(ptrv); o.range=r? {base:r.base.toString(),size:r.size,prot:r.protection}:null;
        if(dumpAddr) o.delta=ptrv.sub(ptr('0x'+dumpAddr)).toString(); }
      out[lbl]=o;
    }
    info('set_Att_health',spg,'73f30e0508');
    info('GetParsedString',gps,'73f337a04c');
    info('GetParsedUniversalStringAndIsBody',gpu,'73f337a054');
    if(spg){ var bias=spg.sub(ptr('0x73f30e0508'));
      out.bias=bias.toString();
      ['0x73f337c0dc','0x73f337c24c','0x73f337c85c','0x73f4264a80','0x73f337a04c'].forEach(function(a){
        var t=ptr(a).add(bias); var r=Process.findRangeByAddress(t);
        out['pred'+a]= {addr:t.toString(), prot:r?r.protection:null};
      });
      // sanity: does the predicted GetParsedString equal the il2cpp pointer?
      try{ out.predEqGps = ptr('0x73f337a04c').add(bias).equals(gps); }catch(e){ out.predEqGps='err:'+e; }
    }
  }catch(e){ out.err=String(e); }
  return out;
}

    var hooks=install();

var META_BASE = 0x73f049a000;
function modBase(){ var b=biasOf(); if(!b) return null; return b.add(META_BASE); }
function hexoff(v){ var n=(typeof v==='number')? v : parseInt(String(v),0); return '0x'+(n>>>0).toString(16); }
function readMetaSlots(offs){
  var mb=modBase(); if(!mb) return {err:'no bias'};
  var out=[];
  for(var i=0;i<offs.length;i++){
    var off=(typeof offs[i]==='number')? offs[i] : parseInt(String(offs[i]),0);
    var rec={off:'0x'+(off>>>0).toString(16)};
    try{
      var p=mb.add(off);
      var v=p.readPointer();
      rec.val=v.toString();
      if(!v.isNull()) rec.s=toJS(v);
    }catch(e){ rec.err=String(e); }
    out.push(rec);
  }
  return {base:mb.toString(), n:out.length, out:out};
}
// readMetaSlots2: same as readMetaSlots but follows the second indirection
// (code does 'ldr x9,[page+off]; ldr x0,[x9]'), so the string lives at *(slot value).
function readMetaSlots2(offs){
  var mb=modBase(); if(!mb) return {err:'no bias'};
  var out=[];
  for(var i=0;i<offs.length;i++){
    var off=(typeof offs[i]==='number')? offs[i] : parseInt(String(offs[i]),0);
    var rec={off:'0x'+(off>>>0).toString(16)};
    try{
      var p=mb.add(off);
      var v=p.readPointer();
      rec.val=v.toString();
      if(!v.isNull()){
        var s2=v.readPointer();
        rec.val2=s2.toString();
        if(!s2.isNull()) rec.s=toJS(s2);
      }
    }catch(e){ rec.err=String(e); }
    out.push(rec);
  }
  return {base:mb.toString(), n:out.length, out:out};
}
// probeCard: build a scenario object, run its RefreshScenario on the calling thread,
// and read back every String field plus the ScenarioChoice list, so card text and
// button labels come from the game itself instead of a static guess.
function probeCard(clsName, friendPtr){
  var out={cls:clsName};
  try{
    var k=findClass('',clsName); if(!k){ out.err='no class '+clsName; return out; }
    var obj=nf.il2cpp_object_new(k); out.obj=obj.toString();
    var fs=fieldsWithParents(k);
    out.fields={}; fs.forEach(function(f){ out.fields[f.name]=f.off; });
    var foff=null; for(var i=0;i<fs.length;i++) if(fs[i].name==='_friend') foff=fs[i].off;
    if(friendPtr && foff!==null) obj.add(foff).writePointer(friendPtr);
    var ctor1=findMethod(k,'.ctor',1), ctor0=findMethod(k,'.ctor',0), cexc=null;
    out.ctor1=ctor1? ctor1.toString():null; out.ctor0=ctor0? ctor0.toString():null;
    try{
      if(friendPtr && ctor1){ var ab=Memory.alloc(Process.pointerSize); ab.writePointer(friendPtr);
        var cc=invoke0(ctor1,obj,1,[ab]); cexc=cc.exc; out.ctorUsed='ctor(SimPerson)'; }
      else if(ctor0){ var c0=invoke0(ctor0,obj,0,[]); cexc=c0.exc; out.ctorUsed='ctor()'; }
      else { out.err='no ctor'; return out; }
    }catch(eC){ out.stage='ctor'; out.err=String(eC); return out; }
    out.ctorExc=cexc;
    if(friendPtr && foff!==null){ obj.add(foff).writePointer(friendPtr); out.wroteAfterCtor=true; }
    try{ out.friendAfter = foff===null? 'no field' : obj.add(foff).readPointer().toString(); }catch(eF){}
    try{ var gf=findMethod(k,'GetFriend',0);
      if(gf){ var gr=invoke0(gf,obj,0,[]); out.getFriend = gr.exc? ('exc '+gr.exc) : gr.r.toString(); } }catch(eG){ out.getFriendErr=String(eG); }
    var rf=findMethod(k,'RefreshScenario',0); out.rf=rf? rf.toString():null;
    out.miPtr = rf? rf.readPointer().toString() : null;
    if(rf){ try{ var r1=invoke0(rf,obj,0,[]); out.refreshExc=r1.exc; }
      catch(eR){ out.refreshErr=String(eR); } }
    out.stage='strings';
    out.strings={};
    for(var i2=0;i2<fs.length;i2++){ var f2=fs[i2];
      if(f2.type!=='System.String') continue;
      try{ out.strings[f2.name]=toJS(obj.add(f2.off).readPointer()); }catch(e){ out.strings[f2.name]='err '+e; } }
    var coff=null; for(var i3=0;i3<fs.length;i3++) if(fs[i3].name==='_choices') coff=fs[i3].off;
    out.choiceOff=coff;
    if(coff!==null){
      var a=obj.add(coff).readPointer();
      out.arrCls=a.isNull()?null:className(a);
      if(!a.isNull()){
        var sk=findClass('','ScenarioChoice'); var sfs=sk?fieldsOf(sk):[];
        out.choiceFields=sfs.map(function(f){ return f.name+':'+f.type+'@0x'+f.off.toString(16); });
        var n=a.add(0x18).readU32(); out.choiceCount=n; out.choices=[];
        for(var i4=0;i4<n && i4<12;i4++){
          var cp=a.add(0x20+i4*Process.pointerSize).readPointer(); var one={ptr:cp.toString()};
          for(var i5=0;i5<sfs.length;i5++){ var sf=sfs[i5];
            if(sf.type!=='System.String') continue;
            try{ one[sf.name]=toJS(cp.add(sf.off).readPointer()); }catch(e){ one[sf.name]='err'; } }
          out.choices.push(one);
        }
      }
    }
    return out;
  }catch(e){ out.err=String(e); return out; }
}
function invokeAt(dumpAddr, arg){
  var b=biasOf(); if(!b) return {err:'no bias'};
  var p=b.add(ptr(dumpAddr));
  var f;
  try{ f=new NativeFunction(p,'pointer',['pointer']); }catch(e){ return {err:'nfn '+String(e)}; }
  try{
    var r=f(ptr(arg? String(arg):'0'));
    var s=null; try{ if(r && !r.isNull()) s=toJS(r); }catch(e2){}
    return {ok:true, ret:r? r.toString():null, s:s};
  }catch(e){ return {err:'call '+String(e)}; }
}

function invokeN(dumpAddr, n, arg){
  var b=biasOf(); if(!b) return {err:'no bias'};
  var p=b.add(ptr(dumpAddr));
  var f;
  try{ f=new NativeFunction(p,'pointer',['pointer']); }catch(e){ return {err:'nfn '+String(e)}; }
  var a=ptr(arg? String(arg):'0');
  var tally={}, errors=0, sample=null, order=[];
  n=n||200;
  for(var i=0;i<n;i++){
    try{
      var r=f(a);
      var s=null; try{ if(r && !r.isNull()) s=toJS(r); }catch(e2){}
      if(s===null){ errors++; continue; }
      if(sample===null) sample=s;
      if(!tally[s]){ tally[s]=0; order.push(s); }
      tally[s]++;
    }catch(e){ errors++; }
  }
  return {addr:dumpAddr, n:n, errors:errors, distinct:order.length, first:sample,
          rows:order.map(function(k){ return [k, tally[k]]; })};
}
// §103: walk the Sim singleton chain to the hero without needing the ctor hooks
// (a life restored from a save never runs SimHero..ctor, so st.hero stays null).
function objName(p){ try{ if(!p || p.isNull()) return null; var k=nf.il2cpp_object_get_class(p); if(k.isNull()) return null; return nf.il2cpp_class_get_name(k).readUtf8String(); }catch(e){ return '!'+String(e); } }
function simChain(){
  var out={};
  try{
    var mb=modBase(); if(!mb) return {err:'no bias'};
    var a=mb.add(0x5c30250).readPointer(); out.cell=a.toString();
    var b=a.isNull()? ptr(0) : a.readPointer();        out.a_deref=b.toString(); out.a_cls=objName(b);
    var c=b.isNull()? ptr(0) : b.add(0xb8).readPointer(); out.b_plusB8=c.toString(); out.b_cls=objName(b);
    var d=c.isNull()? ptr(0) : c.readPointer();        out.c_deref=d.toString(); out.c_cls=objName(c);
    var hero=d.isNull()? ptr(0) : d.add(0x28).readPointer(); out.hero=hero.toString(); out.hero_cls=objName(hero);
    var alt=b.isNull()? ptr(0) : b.add(0xb8).readPointer();
    var alt2=alt.isNull()? ptr(0) : alt.add(0x28).readPointer(); out.alt_hero=alt2.toString(); out.alt_cls=objName(alt2);
    out.d_cls=objName(d);
    return out;
  }catch(e){ out.err=String(e); return out; }
}
// §103: call the party row picker and read the returned row's own cells.
function partyPicker(a,b,c,d,e,f){
  var mb=modBase(); if(!mb) return {err:'no bias'};
  try{
    var fn2=new NativeFunction(mb.add(ptr('0x73f2ccb1e4')),'pointer',['int','int','int','int','int','int','pointer']);
    var r=fn2(a,b,c,d,e,f,ptr(0));
    var out={row:r.toString(), isNull:r.isNull()};
    if(!r.isNull()){
      var k=findClass('','DataFriendPartyScenario');
      if(k){ var fs=fieldsOf(k); out.fields={};
        for(var i=0;i<fs.length;i++){ var f2=fs[i], o2=f2.off, ty=f2.type;
          try{
            if(ty==='System.String') out.fields[f2.name]=toJS(r.add(o2).readPointer());
            else if(ty==='System.Int32') out.fields[f2.name]=r.add(o2).readS32();
            else if(ty==='System.Boolean') out.fields[f2.name]=r.add(o2).readU8()!==0;
            else out.fields[f2.name]=ty;
          }catch(e3){ out.fields[f2.name]='!err'; }
        }
      } else out.err2='no DataFriendPartyScenario class';
    }
    return out;
  }catch(e){ return {err:String(e)}; }
}
// §106: build a real SimPerson with the game's own factory, so a card that needs a
// friend can be constructed on a fresh life (which otherwise has no friends at all).
function genPerson(gender, minAge, maxAge){
  var out={};
  try{
    var mb=modBase(); if(!mb) return {err:'no bias'};
    var ch=simChain(); out.hero=ch.hero; out.hero_cls=ch.hero_cls;
    var hero=ptr(ch.hero);
    var fk=findClass('',ch.hero_cls||'SimHero'), place=null;
    if(fk){ var fs=fieldsWithParents(fk);
      for(var i=0;i<fs.length;i++){ if(fs[i].name==='_PlaceOfResidence'||fs[i].name==='PlaceOfResidence'){
        place=hero.add(fs[i].off).readPointer(); out.placeField=fs[i].name; break; } } }
    out.place=place? place.toString():null;
    if(!place||place.isNull()) return {err:'no place'};
    var fn=new NativeFunction(mb.add(ptr('0x73f3817c34').sub(META_BASE)),'pointer',['int','pointer','int','int','int']);
    var p=fn(gender|0, place, minAge|0, maxAge|0, 0);
    out.person=p.toString(); out.person_cls=objName(p);
    var pk=findClass('','SimPerson');
    if(pk){ var ps=fieldsWithParents(pk), o={};
      for(var j=0;j<ps.length;j++){ var f=ps[j];
        try{ if(f.type==='System.String') o[f.name]=toJS(p.add(f.off).readPointer());
             else if(f.type==='System.Int32') o[f.name]=p.add(f.off).readS32();
             else if(f.type==='System.Boolean') o[f.name]=p.add(f.off).readU8()!==0;
             else o[f.name]=f.type; }catch(e2){ o[f.name]='!err'; } }
      out.personFields=o; }
    return out;
  }catch(e){ out.err=String(e); return out; }
}
function friendCard(cls, gender, minAge, maxAge, friendStr){
  var g=null;
  try{ g = friendStr? {person:String(friendStr)} : genPerson(gender,minAge,maxAge); }catch(e){ return {genErr:String(e)}; }
  if(!g||g.err||!g.person) return {gen:g};
  var res=null; try{ res=probeCard(cls, ptr(g.person)); }catch(e){ res={err:String(e)}; }
  return {gen:g, card:res};
}
function rumorOutCard(friendStr, retal){
  var out={cls:'ScenarioFriendRumorOutbound', retal:retal?1:0};
  try{
    var b=biasOf(); if(!b){ out.err='no bias'; return out; }
    var mb=b.add(META_BASE);
    var k=findClass('','ScenarioFriendRumorOutbound'); if(!k){ out.err='no class'; return out; }
    var fs=fieldsWithParents(k); out.fields={}; fs.forEach(function(f){ out.fields[f.name]=f.off; });
    var obj=nf.il2cpp_object_new(k); out.obj=obj.toString();
    var ctor2=findMethod(k,'.ctor',2); out.ctor2=ctor2? ctor2.toString():null;
    if(!ctor2){ out.err='no 2-arg ctor'; return out; }
    var fn=new NativeFunction(mb.add(ptr('0x73f3c52d58').sub(META_BASE)),'void',['pointer','pointer','int']);
    try{ fn(obj, ptr(String(friendStr)), retal?1:0); }catch(eC){ out.ctorErr=String(eC); }
    try{ out.narrativeAfterCtor=toJS(obj.add(0x180).readPointer()); }catch(e){}
    var rf=findMethod(k,'RefreshScenario',0); out.rf=rf? rf.toString():null;
    if(rf){
      try{ var r1=invoke0(rf,obj,0,[]); out.refreshExc=r1.exc; }
      catch(eR){ out.refreshErr=String(eR); }
    }
    out.strings={};
    for(var i2=0;i2<fs.length;i2++){ var f2=fs[i2];
      if(f2.type!=='System.String') continue;
      try{ out.strings[f2.name]=toJS(obj.add(f2.off).readPointer()); }catch(e){ out.strings[f2.name]='err '+e; } }
    return out;
  }catch(e){ out.err=String(e); return out; }
}
function friendSpendProbeX(){ return null; }
function rumorRowProbe(a,b,c,d){
  var out={args:[a,b,c,d]};
  try{
    var bo=biasOf(); if(!bo){ out.err='no bias'; return out; }
    var mb=bo.add(META_BASE);
    var f=new NativeFunction(mb.add(ptr('0x73f2ccace4').sub(META_BASE)),'pointer',['int','int','int','int']);
    var r=f(a|0,b|0,c|0,d|0);
    if(r.isNull()){ out.row=null; return out; }
    out.row=r.toString();
    try{ out.OutboundScenario=toJS(r.add(0x10).readPointer()); }catch(e){ out.e1=String(e); }
    try{ out.InboundScenario=toJS(r.add(0x18).readPointer()); }catch(e){ out.e2=String(e); }
    try{ out.InboundLogVersion=toJS(r.add(0x20).readPointer()); }catch(e){ out.e3=String(e); }
    out.SenderMinAge=r.add(0x28).readS32(); out.SenderMaxAge=r.add(0x2c).readS32(); out.SenderGender=r.add(0x30).readS32();
    out.ReceiverMinAge=r.add(0x34).readS32(); out.ReceiverMaxAge=r.add(0x38).readS32(); out.ReceiverGender=r.add(0x3c).readS32();
    return out;
  }catch(e){ out.err=String(e); return out; }
}
// §113: ScenarioAttackInbound driven end to end. The card has nine ctors and a
// 1240+1150 line pair of methods, so instead of reading every branch we build the
// card with its real (SimPerson,DataWeapon) ctor and press each button, then read
// the impact calls, stat slots, log lines and child card the game itself produced.
function attStat(p){
  try{ var o=readObj(p,null); var r={};
    for(var k in o){ if(k.indexOf('<Att_')===0){ r[k.replace('<Att_','').replace('>k__BackingField','')]=o[k]; } }
    return r;
  }catch(e){ return {err:String(e)}; }
}
function attFields(obj,k){
  var out={strings:{},choices:[]};
  var fs=fieldsWithParents(k);
  for(var i=0;i<fs.length;i++){ var f=fs[i];
    if(f.type!=='System.String') continue;
    try{ out.strings[f.name]=toJS(obj.add(f.off).readPointer()); }catch(e){ out.strings[f.name]='err'; } }
  var coff=null; for(var j=0;j<fs.length;j++) if(fs[j].name==='_choices') coff=fs[j].off;
  if(coff!==null){
    var a=obj.add(coff).readPointer();
    if(!a.isNull()){
      var sk=findClass('','ScenarioChoice'); var sfs=sk?fieldsOf(sk):[];
      var n=a.add(0x18).readU32(); out.choiceCount=n;
      for(var i2=0;i2<n && i2<12;i2++){
        var cp=a.add(0x20+i2*Process.pointerSize).readPointer(); var one={};
        for(var i3=0;i3<sfs.length;i3++){ var sf=sfs[i3];
          if(sf.type!=='System.String') continue;
          try{ one[sf.name]=toJS(cp.add(sf.off).readPointer()); }catch(e3){} }
        out.choices.push(one);
      }
    }
  }
  return out;
}
function attBuild(person, wp, k){
  var b=biasOf(); if(!b) return {err:'no bias'};
  var mb=b.add(META_BASE);
  var obj=nf.il2cpp_object_new(k);
  var out={obj:obj.toString()};
  try{
    var mi=null, g=0, m=null;
    var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0));
    while(!(m=nf.il2cpp_class_get_methods(k,it)).isNull() && g++<4000){
      if(nf.il2cpp_method_get_name(m).readUtf8String()!=='.ctor') continue;
      if(nf.il2cpp_method_get_param_count(m)!==2) continue;
      var t0=tname(nf.il2cpp_method_get_param(m,0)), t1=tname(nf.il2cpp_method_get_param(m,1));
      out.ctors=(out.ctors||[]).concat([t0+'|'+t1]);
      if(t0==='SimPerson' && t1==='DataWeapon'){ mi=m; break; }
    }
    if(!mi){ out.ctorErr='no (SimPerson,DataWeapon) ctor'; return out; }
    out.ctorMI=mi.toString();
    var c1=Memory.alloc(8); c1.writePointer(person);
    var c2=Memory.alloc(8); c2.writePointer(wp);
    var rc=invoke0(mi, obj, 2, [c1,c2]);
    out.ctorExc=rc.exc;
    if(rc.exc) return out;
  }catch(eC){ out.ctorErr=String(eC); return out; }
  var rf=findMethod(k,'RefreshScenario',0);
  out.rf=rf? rf.toString():null;
  if(rf){ try{ var r1=invoke0(rf,obj,0,[]); out.refreshExc=r1.exc; }catch(eR){ out.refreshErr=String(eR); } }
  return out;
}
function attWeaponFields(w){
  try{
    var k=findClass('','DataWeapon'); if(!k) return null;
    var fs=fieldsWithParents(k), o={};
    for(var i=0;i<fs.length;i++){ var f=fs[i];
      try{
        if(f.type==='System.String') o[f.name]=toJS(w.add(f.off).readPointer());
        else if(f.type==='System.Int32') o[f.name]=w.add(f.off).readS32();
        else if(f.type==='System.Boolean') o[f.name]=w.add(f.off).readU8()!==0;
      }catch(e2){}
    }
    return o;
  }catch(e){ return {err:String(e)}; }
}
function attProbe(mode, friendStr, weaponStr){
  var out={mode:String(mode||'refresh')};
  try{
    var b=biasOf(); if(!b){ out.err='no bias'; return out; }
    var mb=b.add(META_BASE);
    var ch=simChain();
    var hero = st.hero? st.hero : (ch.hero? ptr(ch.hero) : null);
    out.hero=hero? hero.toString():null; out.hero_cls=objName(hero);
    if(!hero){ out.err='no hero'; return out; }
    out.heroBefore=attStat(hero);
    var person=null;
    if(friendStr){ person=ptr(String(friendStr)); out.personSource='given'; }
    else { var g=genPerson(0,20,45); out.gen=(g&&g.err)?('err '+g.err):'ok';
           if(g&&g.person){ person=ptr(g.person); out.personSource='generated'; } }
    out.person=person? person.toString():null; out.person_cls=objName(person);
    if(!person){ out.err='no person'; return out; }
    var wp=null;
    if(weaponStr && weaponStr!=='0'){ wp=ptr(String(weaponStr)); out.weaponSource='given'; }
    else {
      try{
        var dk=findClass('','DataHub');
        var wm=dk? findMethod(dk,'GetRandomAssaultWeaponAndIsInPrison',1) : null;
        if(wm){ var ab0=Memory.alloc(8); ab0.writeS32(0);
          var wr=invoke0(wm, ptr(0), 1, [ab0]); out.weaponInvokeExc=wr.exc; wp=wr.r; out.weaponSource='invoke'; }
      }catch(eW){ out.weaponErr=String(eW); }
      if(!wp || wp.isNull()){
        try{ var wf=new NativeFunction(mb.add(ptr('0x73f2ca1ac0').sub(META_BASE)),'pointer',['int']); wp=wf(0); out.weaponSource='native'; }
        catch(eW2){ out.weaponErr2=String(eW2); }
      }
    }
    out.weapon=wp? wp.toString():null;
    if(wp && !wp.isNull()) out.weaponFields=attWeaponFields(wp);
    if(!wp || wp.isNull()){ out.err='no weapon'; return out; }
    var k=findClass('','ScenarioAttackInbound'); if(!k){ out.err='no class'; return out; }
    var bb=attBuild(person, wp, k);
    if(bb.err||bb.ctorErr){ for(var kk in bb) out[kk]=bb[kk]; return out; }
    out.ctorOk=true;
    var fr=attFields(bb.obj? ptr(bb.obj): null, k);
    out.strings=fr.strings; out.choiceCount=fr.choiceCount; out.choices=fr.choices;
    out.beforeChild=null;
    try{ out.childClass=objName(ptr(bb.obj).add(0x128).readPointer()); }catch(e){}
    if(out.mode!=='press'){ return out; }
    var cm=findMethod(k,'ChoiceSelectedAtIndex',1);
    out.cm=cm? cm.toString():null;
    out.presses=[];
    var n=fr.choiceCount||0;
    for(var ci=0; ci<n && ci<6; ci++){
      var b2=attBuild(person, wp, k);
      if(b2.ctorErr){ out.presses.push({i:ci,ctorErr:b2.ctorErr}); continue; }
      var o2=ptr(b2.obj);
      var lbl=null; try{ var lk=findClass('','ScenarioChoice'); }catch(e){}
      var pre=attFields(o2,k);
      var before=attStat(hero);
      var sw=(st.statW||[]).length, lg=(st.logs||[]).length;
      var exc=null, perr=null;
      try{ var ab=Memory.alloc(8); ab.writeS32(ci); var rr=invoke0(cm,o2,1,[ab]); exc=rr.exc; }
      catch(eP){ perr=String(eP); }
      var rec={i:ci, label:(pre.choices[ci]&&pre.choices[ci]._Choice)?pre.choices[ci]._Choice:null,
               exc:exc, pressErr:perr, after:attStat(hero)};
      var tt={}; for(var q in before){ if(before[q]!==rec.after[q]) tt[q]=[before[q],rec.after[q]]; }
      rec.diff=tt;
      rec.impacts=(st.statW||[]).slice(sw).map(function(w){ var r={f:w.f,v:w.v};
        try{ r.dump='0x'+ptr(w.ret).sub(biasOf()).toString(16); }catch(e){}
        return r; });
      rec.logs=(st.logs||[]).slice(lg).map(function(w){ return w.text; });
      try{ rec.child=objName(o2.add(0x128).readPointer()); }catch(e){}
      try{ rec.titleAfter=toJS(o2.add(0x38).readPointer()); }catch(e){}
      out.presses.push(rec);
    }
    out.heroAfter=attStat(hero);
    return out;
  }catch(e){ out.err=String(e); return out; }
}
    rpc.exports={
      rumorrow:function(){ var a=Array.prototype.slice.call(arguments); return rumorRowProbe(a[0]|0,a[1]|0,a[2]|0,a[3]|0); },
      rumorscan:function(){
        var bo=biasOf(); if(!bo) return {err:'no bias'};
        var mb=bo.add(META_BASE);
        var f=new NativeFunction(mb.add(ptr('0x73f2ccace4').sub(META_BASE)),'pointer',['int','int','int','int']);
        var nulls=[], total=0, seen={};
        for(var sa=0;sa<=20;sa+=5){ for(var sg=0;sg<=2;sg++){ for(var ra=0;ra<=20;ra+=5){ for(var rg=0;rg<=2;rg++){
          total++;
          var r=null; try{ r=f(sa,sg,ra,rg); }catch(e){ nulls.push([sa,sg,ra,rg,'ERR '+e]); continue; }
          if(r.isNull()) nulls.push([sa,sg,ra,rg]);
          else { var o=toJS(r.add(0x10).readPointer()); if(o) seen[o]=1; }
        }}}}
        return {total:total, nullCount:nulls.length, nulls:nulls.slice(0,40), distinctOutbound:Object.keys(seen).length};
      },
      ping:function(){ return 'pong'; },
      // card(cls): build one scenario object and run its own RefreshScenario, with
      // no person minted. ctor() cards need none, and genPerson cannot be used on
      // a hero that has no place chain yet.
      card:function(cls){ try{ return probeCard(String(cls), null); }catch(e){ return {err:String(e)}; } },
      attprobe:function(a,c,d){
        var s=(c===undefined||c===null||c==='')? null : String(c);
        var w=(d===undefined||d===null||d==='')? null : String(d);
        try{ Java.scheduleOnMainThread(function(){ try{ st.att=attProbe(String(a||'refresh'), s, w); }
          catch(e){ st.att={err:String(e)}; } });
          return {ok:true, queued:String(a||'refresh')}; }
        catch(e){ return {error:String(e)}; } },
      attres:function(){ return st.att||null; },
      ctortest:function(){
        try{
          var k=findClass('','ScenarioAttackInbound'); if(!k) return {err:'no class'};
          var mi=null, g=0, m=null;
          var it=Memory.alloc(Process.pointerSize); it.writePointer(ptr(0));
          while(!(m=nf.il2cpp_class_get_methods(k,it)).isNull() && g++<4000){
            if(nf.il2cpp_method_get_name(m).readUtf8String()!=='.ctor') continue;
            if(nf.il2cpp_method_get_param_count(m)!==2) continue;
            var t0=tname(nf.il2cpp_method_get_param(m,0)), t1=tname(nf.il2cpp_method_get_param(m,1));
            if(t0==='SimPerson' && t1==='DataWeapon'){ mi=m; break; }
          }
          if(!mi) return {err:'no ctor'};
          var mp=mi.readPointer();
          var hits=[]; st.ctorHits=hits;
          Interceptor.attach(mp,{ onEnter:function(a){ try{ hits.push({self:a[0].toString(), p1:a[1].toString(), p2:a[2].toString(), lr:this.returnAddress.toString()}); }catch(e){} } });
          var obj=nf.il2cpp_object_new(k);
          var c1=Memory.alloc(8); c1.writePointer(ptr(0));
          var c2=Memory.alloc(8); c2.writePointer(ptr(0));
          var r={obj:obj.toString(), mi:mi.toString(), mp:mp.toString()};
          try{ r.empty=invoke0(mi,obj,2,[c1,c2]); }catch(e){ r.emptyErr=String(e); }
          return r;
        }catch(e){ return {err:String(e)}; } },
      ctorhits:function(){ return st.ctorHits||[]; },
      dhub:function(name,argc,arg){
        try{
          var k=findClass('','DataHub'); if(!k) return {err:'no DataHub'};
          var m=findMethod(k,String(name),argc|0); if(!m) return {err:'no method '+String(name)};
          var ab=Memory.alloc(8); ab.writeS32((arg===undefined||arg===null)?0:(arg|0));
          var r=invoke0(m, ptr(0), argc|0, (argc|0)>0? [ab] : []);
          var s=null; try{ if(r.r && !r.r.isNull()) s=toJS(r.r); }catch(e){}
          return {cls:'DataHub', name:String(name), exc:r.exc, ret:r.r? r.r.toString():null, s:s,
                  nRet:(r.r&&!r.r.isNull())? objName(r.r):null};
        }catch(e){ return {err:String(e)}; } },
      genp:function(){ var a=Array.prototype.slice.call(arguments); return genPerson(a[0]|0,a[1]|0,a[2]|0); },
      fc:function(cls,gen,mn,mx){
        try{ Java.scheduleOnMainThread(function(){ try{ st.fc=friendCard(cls, gen|0, mn|0, mx|0, null); }
          catch(e){ st.fc={err:String(e)}; } });
          return {ok:true, queued:cls, gender:gen|0, min:mn|0, max:mx|0}; }
        catch(e){ return {error:String(e)}; } },
      fcptr:function(cls,fstr){ var s=String(fstr);
        try{ Java.scheduleOnMainThread(function(){ try{ st.fc=friendCard(cls, 0, 0, 0, s); }
          catch(e){ st.fc={err:String(e)}; } }); return {ok:true, queued:cls, friend:s}; }
        catch(e){ return {error:String(e)}; } },
      fcres:function(){ return st.fc||null; },
      rumorout:function(fstr,retal){ var s=String(fstr);
        try{ Java.scheduleOnMainThread(function(){ try{ st.fc=rumorOutCard(s, retal|0); }
          catch(e){ st.fc={err:String(e)}; } }); return {ok:true, queued:'ScenarioFriendRumorOutbound', friend:s, retal:retal|0}; }
        catch(e){ return {error:String(e)}; } },
      heroptrs:function(){ return simChain(); },
      partypick:function(){ var a=Array.prototype.slice.call(arguments); return partyPicker(a[0]|0,a[1]|0,a[2]|0,a[3]|0,a[4]|0,a[5]|0); },
      probegeneric:function(cls, fstr){
        var fp=null; try{ fp=fstr? ptr(String(fstr)) : (st.hero|| (function(){ var c=simChain(); return c.hero? ptr(c.hero):null; })()); }catch(e){}
        try{ Java.scheduleOnMainThread(function(){ try{ st.prank=probeCard(cls, fp); }catch(e){ st.prank={err:String(e)}; } }); return {ok:true, queued:cls, friend: fp? fp.toString():null}; }
        catch(e){ return {error:String(e)}; } },
      rbi:function(){ return rbInstall(); },
      rbdump:function(n){ return rbDump(n); },
      rbi2:function(){ return rbInstall2(); },
      rbdump2:function(n){ return rbDump2(n); },
      rbc:function(){ return rbCal(); },
      rbs:function(){ return rbStat(); },
      rng:function(){ return ranges(); },
      rbp:function(){ return rbProbe(); },
      rba:function(){ return rbAddr(); },
      rbci:function(){ return rbCapInstall(); },
      rbcd:function(n){ return rbCapDump(n); },
      rd:function(a){ return rdict(a); },
      hk:function(a,t){ return hookAt(a,t); },
      hkd:function(){ return hookDump(); },
      sd:function(){ return sd(); },
      sv:function(){ return sv(); },
      pf:function(i){ return pfields(i); },
      pl:function(m){ return listInsts(m); },
      hd:function(a,m,c){ return hookDicts(a,m,c); },
      pd:function(i,j,m){ return poolDump(i,j,m); },
      slots:function(){ return readMetaSlots(Array.prototype.slice.call(arguments)); },
      slots2:function(){ return readMetaSlots2(Array.prototype.slice.call(arguments)); },
      prank:function(which, friendStr){
        var cls = (which==='out')? 'ScenarioFriendPrankOutbound' : 'ScenarioFriendPrankInbound';
        var fp=null; try{ fp = friendStr? ptr(String(friendStr)) : st.hero; }catch(e){}
        try{ Java.scheduleOnMainThread(function(){ try{ st.prank=probeCard(cls, fp); }
          catch(e){ st.prank={err:String(e)}; } }); return {ok:true, queued:cls, friend: fp?fp.toString():null}; }
        catch(e){ return {error:String(e)}; } },
      prankres:function(){ return st.prank||null; },
      circ:function(x){ return invokeAt('0x73f2ce2d08', x); },
      sample:function(off,n,a){ return invokeN(off, n, a); },
      key:key,
      hooks:function(){ return hooks; },
      missed:function(){ return st.missed; },
      pointers:function(){ return { life: st.life?st.life.toString():null, hero: st.hero?st.hero.toString():null, ticks: st.ticks.length }; },
      hero:function(){ return st.hero? readObj(st.hero,null) : {error:'no hero ptr'}; },
      life:function(){ return st.life? readObj(st.life,['Hero','World','Past','Death','Father','Mother','Finances','LifeLog','MetaHeroID','MetaLifeType','Occupation','Fame','Royal','ChallengeTracker','ScenarioLogArray','_ChildArray','_SiblingArray']) : {error:'no life ptr'}; },
      sub:function(which,field){ var p = which==='life'?st.life:st.hero; if(!p) return {error:'no ptr'};
        var o=objOf(p); if(!o) return {error:'bad ptr'};
        var fs=fieldsOf(o.k); var f=null; for(var i=0;i<fs.length;i++) if(fs[i].name===field||fs[i].name.indexOf(field)>=0){ f=fs[i]; break; }
        if(!f) return {error:'no field '+field}; var q=p.add(f.off).readPointer(); if(q.isNull()) return {error:'null'};
        return { field:f.name, type:f.type, value: readObj(q,null), _ptr:q.toString() }; },
      cls:function(ns,name){ var k=findClass(ns,name); if(!k) return {error:'not found'};
        return { name:name, fields:fieldsOf(k).map(function(f){ return [f.name,f.type,f.off]; }) }; },
      ticks:function(n){ return st.ticks.slice(-(n||10)); },
      pushdumb:function(n){ return {base:(st.modBase||null), rows:(st.pushDumb||[]).slice(-(n||20))}; },
    births:function(n){ return (st.births||[]).slice(-(n||5)); },
      chain:function(){ return chainProbe(); },
      branch:function(n){ return (st.branch||[]).slice(-(n||10)); },
      queues:function(n){ try{ return {queueHits:st.queueHits||0, queueErr:st.queueErr||null, rows:(st.queues||[]).slice(-(n||20))}; }catch(e){ return {error:String(e)}; } },
      attsets:function(n){ return (st.attSets||[]).slice(-(n||200)); },
      persons:function(n){ return (st.persons||[]).slice(-(n||100)); },
      family: function(){
        var out2={};
        try{
          var l=readObj(st.life, ['Father','Mother']);
          out2.life_cls=l? l._class : null;
          ['Father','Mother'].forEach(function(k){
            var f=l? l[k] : null;
            if(!f || !f._ptr){ out2[k]=null; return; }
            var o=readObj(ptr(f._ptr));
            out2[k]={ _class:o._class, att:{ happiness:o['<Att_happiness>k__BackingField'], health:o['<Att_health>k__BackingField'], intelligence:o['<Att_intelligence>k__BackingField'], appearance:o['<Att_appearance>k__BackingField'], discipline:o['<Att_discipline>k__BackingField'], karma:o['<Att_karma>k__BackingField'] }, age:o['<Age>k__BackingField'], name:(o['<Name>k__BackingField']&&o['<Name>k__BackingField']._cls)? null : o['<Name>k__BackingField'] };
          });
        }catch(e){ out2.err=String(e); }
        return out2;
      },
      wraps:function(n){ return (st.wraps||[]).slice(-(n||500)); },
      picks:function(n,tag){ var a=tag? st.picks.filter(function(r){return r.tag===tag;}) : st.picks; return a.slice(-(n||40)); },
      prod2:function(n){ return (st.prod2||[]).slice(-(n||40)); },
      bags:function(n){ return { bagHits:st.bagHits||0, bagErr:st.bagErr||null, pickHits:st.pickHits||0, pickErr:st.pickErr||null, qErr:st.qErr||null, qErr2:st.qErr2||null, rows:(st.bags||[]).slice(-(n||10)) }; },
      qchecks:function(n){ return { n:(st.qchecks||[]).length, rows:(st.qchecks||[]).slice(-(n||500)) }; },
      qtexts:function(n){ return (st.qtexts||[]).slice(-(n||50)); },
      picksall:function(n){ return (st.picksAll||[]).slice(-(n||50)); },
  prod:function(n){ return {hooked:st.prodHooked||0, mod:st.modInfo||null, anchors:st.anchors||[], rows:(st.prod||[]).slice(-(n||60)), ctors:(st.ctorRecs||[]).slice(-(n||60)), seen:Object.keys(st.ctorSeen||{}).length}; },
      hits:function(){ return HITS; },
      buttons:function(){ try{ return uiButtons(); }catch(e){ return {error:String(e)}; } },
      sceninfo:function(){ try{
        var out=[]; var k=findClass('','Scenario');
        var cmi=findMethodHier(k,'ChoiceSelectedAtIndex',1), dmi=findMethodHier(k,'DismissSelected',0);
        for(var i=0;i<st.scenarios.length;i++){ var d=st.scenarios[i]; var rec={ptr:d._ptr, title:d.Title, dismiss:!!d.HasDismissButton, choices:0};
          try{ var sp=ptr(d._ptr); var arr=sp.add(0x108).readPointer(); if(!arr.isNull()){ rec.choices=arr.add(0x18).readU32(); } }catch(e){}
          out.push(rec); }
        return { n:out.length, hasChoiceMI:!!cmi, hasDismissMI:!!dmi, list:out.slice(-12) };
      }catch(e){ return {error:String(e)}; } },
      resolve:function(ptrStr, idx){
        try{
          var k=findClass('','Scenario'); var self=ptr(ptrStr);
          var arr=self.add(0x108).readPointer(); var n=(arr && !arr.isNull())? arr.add(0x18).readU32() : 0;
          var mi = n>0 ? findMethodHier(k,'ChoiceSelectedAtIndex',1) : findMethodHier(k,'DismissSelected',0);
          if(!mi) return {error:'no method'};
          var args=null;
          if(n>0){ var b=Memory.alloc(4); b.writeS32(idx<0?0:(idx>=n?0:idx)); args=[b]; }
          Java.scheduleOnMainThread(function(){ try{ var rr=invoke0(mi, self, args?1:0, args||[]); log('resolve exc='+rr.exc+' n='+n+' idx='+idx); }catch(e){ log('resolve err '+e); } });
          return {ok:true, choices:n, mi:mi.toString()};
        }catch(e){ return {error:String(e)}; } },

      age:function(){ try{
        if(!st.life) return {error:'no life instance - tap Age once first'};
        var k=findClass('','Life'); var mi=findMethod(k,'IncrementAge',0);
        if(!mi) return {error:'no Life.IncrementAge'};
        Java.scheduleOnMainThread(function(){ try{ var rr=invoke0(mi, st.life, 0, []); log('age invoke exc='+rr.exc); }catch(e){ log('age err '+e); } });
        return {ok:true, mi:mi.toString(), life:st.life.toString()};
      }catch(e){ return {error:String(e)}; } },
      probe:function(){ try{
          var ek=findClass('','EncryptionManager'); if(!ek) return {error:'no EncryptionManager'};
          var p=methodPtr(ek,'DefaultCipherKey',0); if(!p) return {error:'no method ptr'};
          if(!HITS.probeInstalled){ Interceptor.attach(p,{ onEnter:function(){ HITS.probe=(HITS.probe||0)+1; } }); HITS.probeInstalled=1; }
          for(var i=0;i<3;i++){ try{ call0('DefaultCipherKey'); }catch(e){} }
          var lk=findClass('','Life');
          var lk2=null; try{ lk2=lk? nf.il2cpp_class_get_name(lk).readUtf8String():null; }catch(e){}
          var m=lk? methodPtr(lk,'IncrementAge',0):null;
          var mp=lk? methodPtr(lk,'LogPresentation',1):null;
          return { probeAddr:p.toString(), hits:HITS, lifeName:lk2, lifeIncAge:m?m.toString():null, lifeLogPres:mp?mp.toString():null, lifeClass:lk?lk.toString():null };
        }catch(e){ return {error:String(e)}; } },
      diag:function(){ return { life: st.life?st.life.toString():null, hero: st.hero?st.hero.toString():null, hits:HITS, n:{logs:st.logs.length,scen:st.scenarios.length,ticks:st.ticks.length,ageTicks:st.ageTicks.length} }; },
      logs:function(n){ return st.logs.slice(-(n||50)); },
      scenarios:function(n){ return st.scenarios.slice(-(n||20)); },
      ageticks:function(n){ return st.ageTicks.slice(-(n||20)); },
      statw:function(n){ return st.statW.slice(-(n||500)); },
      clear:function(){ st.logs=[]; st.scenarios=[]; st.ticks=[]; st.ageTicks=[]; return true; },
      dec:function(b64){ try{ var r=call2('DecodeAndDecryptString',b64,key()); var v=toJS(r); return (v&&v.length>0)?{ok:true,v:v}:{ok:false,empty:true}; }catch(e){ return {ok:false,err:String(e)}; } },
      seed:function(){ return { seed: st.seed, rng: st.rng.slice(-40) }; },
      rand:function(n){ return { seeds:st.rngSeeds, total:st.rngSamples.length, rows:st.rngSamples.slice(-(n||600)) }; },
      decks:function(n){ return st.decks.slice(-(n||40)); },
      alget: function(n){ return { n:(st.alget||[]).length, rows:(st.alget||[]).slice(-(n||200)) }; },
      uer: function(n){ return { counts:HITS, n:(st.uer||[]).length, rows:(st.uer||[]).slice(-(n||300)), err:UERR }; },
  deckstats:function(){ return { trueCount: st.drawTrue, falseCount: st.drawFalse, drawHits: HITS.deckDraw||0, collHits: HITS.deckCollDraw||0, draws: st.draws.slice(-300), coll: st.collDraws.slice(-300) }; },
      deckmath:function(ps){
        try{
          var k=findClass('','RandomDeck'); if(!k) return {error:'no RandomDeck'};
          var cp=methodPtr(k,'ComputeDeckParameters',1), gcdm=methodPtr(k,'GCD',2), setp=methodPtr(k,'SetPercent',1), dset=methodPtr(k,'SetPercent',1);
          if(!cp) return {error:'missing ComputeDeckParameters'};
          if(!st.deckSample) return {error:'no live RandomDeck instance captured yet (play an age first)'};
          var obj=st.deckSample;
          st.deckMath=[]; st.deckMathMeta={cp:cp.toString(), gcd:gcdm?gcdm.toString():null, setp:setp?setp.toString():null, sample:obj.toString()};
          Java.scheduleOnMainThread(function(){ try{
            for(var j=0;j<ps.length;j++){ var pb=Memory.alloc(4); pb.writeFloat(ps[j]);
              var r2=invoke0(cp, obj, 1, [pb]);
              var raw=null; try{ raw=hexdump(r2.r, {length:24, ansi:false}); }catch(e){}
              var asInts=null; try{ asInts=[r2.r.readS32(), r2.r.add(4).readS32(), r2.r.add(8).readS32(), r2.r.add(12).readS32(), r2.r.add(0x10).readS32(), r2.r.add(0x14).readS32()]; }catch(e){}
              st.deckMath.push({p:ps[j], ints:asInts, exc:r2.exc, raw:r2.r?r2.r.toString():null, dump:raw});
              if(setp){ try{ invoke0(setp, obj, 1, [pb]); }catch(e){} }
              var after=null; try{ after={deckSizeBytes:obj.add(0x1c).readS32(), successCountBytes:obj.add(0x20).readS32()}; }catch(e){}
              if(st.deckMath.length) st.deckMath[st.deckMath.length-1].after=after;
            }
          }catch(e){ log('deckmath err '+e); } });
          return {ok:true, queued:ps.length, mi:cp.toString(), sample:obj.toString()};
        }catch(e){ return {error:String(e)}; } },
      deckmathres:function(){ return { meta: st.deckMathMeta||null, rows: st.deckMath }; },
      deckobj:function(deckSize, successCount){
        try{
          var k=findClass('','RandomDeck'); if(!k) return {error:'no RandomDeck'};
          var ctor=methodPtr(k,'.ctor',2), draw=methodPtr(k,'Draw',0), setp=methodPtr(k,'SetPercent',1), gcdm=methodPtr(k,'GCD',2);
          var out={ctor:null, draws:[], deckSize:null, successCount:null, gcd:[]};
          Java.scheduleOnMainThread(function(){ try{
            var obj=nf.il2cpp_object_new(k);
            var i1=Memory.alloc(4), i2=Memory.alloc(4); i1.writeS32(deckSize); i2.writeS32(successCount);
            invoke0(ctor, obj, 2, [i1, i2]);
            out.ctor={deckSize:obj.add(0x1c).readS32(), successCount:obj.add(0x20).readS32(), index:obj.add(0x18).readS32()};
            if(gcdm){ for(var g=0; g<4; g++){ var a1=Memory.alloc(4), a2=Memory.alloc(4); a1.writeS32([12,18,100,7][g]); a2.writeS32([18,12,75,3][g]);
              var rr=invoke0(gcdm, ptr(0), 2, [a1,a2]); try{ out.gcd.push([ [12,18,100,7][g], [18,12,75,3][g], rr.r?rr.r.readS32():null ]); }catch(e){ out.gcd.push([g,'err',String(e)]); } } }
            if(draw){ for(var i=0;i<Math.min(deckSize,64);i++){ var dr=invoke0(draw, obj, 0, []); out.draws.push(dr.r?(dr.r.toInt32()&1):null); } }
          }catch(e){ log('deckobj err '+e); } });
          out.args={deckSize:deckSize, successCount:successCount};
          return out;
        }catch(e){ return {error:String(e)}; } },
    };
    ready=true;
    log('[i] ready key='+key()+' hooks='+hooks.join(',')+' missed='+st.missed.join('|'));
    return true;
  }catch(e){ log('[setup] '+e+' | stack '+(e.stack||'')+' | missed '+st.missed.join(',')); return false; }
}
Java.perform(function(){
  var GATE_PKG='com.adb.apkinstaller';
  var done=false;
  // Framework-level gate stub: answer 9ADB as installed, with a FULLY populated
  // PackageInfo (a bare stub gives null sourceDir and crashes DexLoadReporter).
  try{
    var PM=Java.use('android.app.ApplicationPackageManager');
    var PI=Java.use('android.content.pm.PackageInfo');
    var busyFw=false;
    PM.getPackageInfo.overloads.forEach(function(o){
      o.implementation=function(){
        if(!busyFw && arguments[0]===GATE_PKG && arguments.length>1){
          busyFw=true;
          try{
            var real=this.getPackageInfo('com.candywriter.bitlife', arguments[1]);
            var pi=PI.$new();
            pi.packageName.value=GATE_PKG;
            pi.versionName.value=real.versionName.value;
            pi.versionCode.value=real.versionCode.value;
            if(real.applicationInfo.value) pi.applicationInfo.value=real.applicationInfo.value;
            log('[gate] stub PackageInfo for '+GATE_PKG);
            return pi;
          }catch(e){ log('[gate] stub failed '+e); }
          finally{ busyFw=false; }
        }
        if(arguments.length===1) return this.getPackageInfo(arguments[0]);
        if(arguments.length===2) return this.getPackageInfo(arguments[0], arguments[1]);
        return this.getPackageInfo(arguments[0], arguments[1], arguments[2]);
      };
    });
    log('[gate] framework getPackageInfo stubbed ('+PM.getPackageInfo.overloads.length+')');

  // Auto-dismiss the Google Play connection error dialog: it pins the UI on
  // every age-up and blocks the driver (Phase 1 trace collection).
  try{
    var AD = Java.use('android.app.AlertDialog');
    AD.show.overload().implementation = function(){
      var ret = this.show();
      try{
        var msg = '';
        try{ var tv = this.findViewById(0x102000b); if(tv) msg = tv.getText().toString(); }catch(e0){}
        if(msg && msg.indexOf('Google Play') >= 0){
          HITS.killedGP = (HITS.killedGP||0) + 1;
          var self = this;
          setTimeout(function(){ try{ Java.scheduleOnMainThread(function(){ try{ self.dismiss(); }catch(e1){} }); }catch(e2){} }, 400);
        }
      }catch(e3){}
      return ret;
    };
    log('[gp] AlertDialog.show hooked -> auto-dismiss Google Play errors');
  }catch(e){ log('[gp] hook failed '+e); }
  }catch(e){ log('[gate] framework stub failed '+e); }
  function hookLoaded(loader, M){
    try{
      Java.classFactory.loader=loader;
      if(!M) M=loader.loadClass('com.android.support.Main');
      if(!M || !M.isPackageInstalled) throw new Error('no isPackageInstalled on '+M);
      var ov=M.isPackageInstalled.overloads;
      for(var j=0;j<ov.length;j++) ov[j].implementation=function(){ return true; };
      if(M.ShowRequiredAppDialog){ var sd=M.ShowRequiredAppDialog.overloads; for(var k=0;k<sd.length;k++) sd[k].implementation=function(){ log('[gate] dialog suppressed'); }; }
      log('[gate] mod class hooked via '+loader);
      done=true;
      return true;
    }catch(e){ log('[gate] hookLoaded failed '+e); return false; }
  }
  // NOTE: hooking java.lang.ClassLoader.loadClass crashes ART (SIGSEGV in
  // FindVirtualMethodForVirtualOrInterface via CallObjectMethod from class-init paths);
  // poll class loaders instead.
  function hookGate(){
    if(done) return true;
    try{
      var loaders=Java.enumerateClassLoadersSync();
      for(var i=0;i<loaders.length;i++){
        try{ var M=loaders[i].loadClass('com.android.support.Main');
          if(hookLoaded(loaders[i], M)) return true; }catch(e){}
      }
    }catch(e){}
    return false;
  }
  setInterval(hookGate,150);
  var t=setInterval(function(){ try{ if(setup()) clearInterval(t); }catch(e){ log('[tick] '+e); } }, 200);
});
