import json, os
P="/Users/Adam/Documents/bitlife/_re/gamedata_plain"
for f in ["genesis_master_tags_reference_2","genesis_object_subtypes","a16_dumb-scenarios-career-enc","genesis_movie_director","a16_hero-filler-enc","genesis_lookup_emoji_visuals"]:
    p=os.path.join(P,f)
    if not os.path.exists(p): print(f,"MISSING"); continue
    with open(p,'rb') as fh:
        head=fh.readline().decode('utf-8','ignore')[:400].rstrip()
        l2=fh.readline().decode('utf-8','ignore')[:300].rstrip()
    print("==",f,"\n   H:",head,"\n   R:",l2)
j=json.load(open('/Users/Adam/Documents/bitlife/_re/gamedata_json/female0-age4.json'))
ks=[k for k in j.keys()]
print("\n== female0-age4 save keys (%d) ==" % len(ks))
print(", ".join(k.replace('<','').replace('>k__BackingField','') for k in ks[:90]))

