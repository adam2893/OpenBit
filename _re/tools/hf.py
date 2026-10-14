import json, glob, os
def atts(d):
    o={}
    for k,v in (d or {}).items():
        if 'Att_' in k: o[k.split('Att_')[1].split('>')[0]]=v
    return o
for f in sorted(glob.glob('/tmp/birth*.json')):
    try: B=json.load(open(f))
    except Exception as e: print(os.path.basename(f),'skip',e); continue
    if isinstance(B, dict): B=[B] if 'error' not in B else []
    for b in B:
        if not isinstance(b, dict) or 'before' not in b: continue
        bf=atts(b['before']); af=atts(b['after'])
        print(os.path.basename(f), 'homo %s fert %s fame %s | hap %s->%s hp %s->%s int %s->%s app %s->%s' % (bf.get('homo'), bf.get('fertility'), bf.get('fame'), bf.get('happiness'), af.get('happiness'), bf.get('health'), af.get('health'), bf.get('intelligence'), af.get('intelligence'), bf.get('appearance'), af.get('appearance')))