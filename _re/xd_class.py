import json, glob, os, re
ROOT = '/Users/Adam/Documents/bitlife/_re/gamedata_plain'
files = [os.path.basename(f) for f in glob.glob(ROOT + '/*')]

def camel_to_kebab(cls):
    s = re.sub(r'^Scenario', '', cls)
    s = re.sub(r'(?<!^)(?=[A-Z])', '-', s)
    return s.lower()

cases = ['ScenarioFriendMeet', 'ScenarioSchoolEnroll', 'ScenarioStepparentCruelty', 'ScenarioPetDiscipline',
         'ScenarioCustomAttack', 'ScenarioLoveHookup', 'ScenarioCrimePickpocket', 'ScenarioRetentionFreelance',
         'ScenarioPushSpendTime', 'ScenarioPetDeath', 'ScenarioAttackInbound', 'ScenarioEncounterProstitute',
         'ScenarioPodcastDumb', 'ScenarioRetentionCrime', 'ScenarioRetentionPodcast', 'ScenarioScriptEventSibling']
for c in cases:
    k = camel_to_kebab(c)
    m = [f for f in files if k in f]
    print('%-28s -> %-34s %s' % (c, k, m[:3] if m else 'NO TABLE'))
