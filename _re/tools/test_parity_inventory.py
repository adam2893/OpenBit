"""Parity census must preserve duplicates, unknowns and real CSV records, never infer completion."""
import importlib.util
import hashlib
import json
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("parity_inventory", ROOT / "tools/parity_inventory.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory)
    (root/"port/src").mkdir(parents=True)
    (root/"gamedata_plain").mkdir()
    (root/"tools").mkdir()
    (root/"il2cpp_dump.txt").write_text("class Scenario : Object"+chr(10)+"class Base : Scenario"+chr(10)+"class Child : Base"+chr(10)+"class Child : Base"+chr(10)+"class Other : Object"+chr(10))
    (root/"port/src/sim.cpp").write_text('// Child and a16_demo-enc mentioned only; not implementation proof'+chr(10)+'// ponytail: unknown outcome')
    (root/"REPORT.en.md").write_text('## 1. Old notes'+chr(10)+'shortcut: historical or current; requires review'+chr(10))
    (root/"gamedata_plain/a16_demo-enc").write_text('Name,Body'+chr(10)+'x,"first'+chr(10)+'second"'+chr(10)+'y,last'+chr(10))
    (root/"gamedata_plain/raw").write_bytes(bytes([255,254]))
    (root/'gamedata_plain/a16_broken-enc').write_text('A,B'+chr(10)+'x,"unfinished')
    (root/'port/src/game.cpp').write_text('const Entry kEntitlements[] = {'+chr(10)+'{"ExamplePurchased", nullptr, "NonConsume_Example"},'+chr(10)+'};')
    (root/"tools/test_demo.py").write_text('# Child reference only')
    apk=root/"target.apk"
    with zipfile.ZipFile(apk,'w') as z:
        z.writestr('assets/test.bin',b'abc')
    data=module.build(root,apk)
    assert len(data['native_classes'])==5
    assert len(data['scenarios'])==3, data['scenarios']
    assert sum(x['name']=='Child' for x in data['scenarios'])==2
    assert all(x['status']=='unverified' for x in data['scenarios'])
    assert data['summary']['verified_parity_items']==0
    table=next(x for x in data['content'] if x['name']=='a16_demo-enc')
    assert table['data_rows']==2 and table['port_text_refs'], table
    assert table['status']=='unverified'
    assert next(x for x in data['content'] if x['name']=='raw')['format']=='binary'
    assert len(data['limit_candidates'])==2
    assert next(x for x in data['content'] if x['name']=='a16_broken-enc')['format']=='unparsed'
    assert data['entitlement_products'][0]['purchase'] is None
    assert data['entitlement_products'][0]['iap_field']=='NonConsume_Example'
    assert data['inputs']['metadata_sha256']==hashlib.sha256((root/'il2cpp_dump.txt').read_bytes()).hexdigest()
    assert 'port/src/game.cpp' in data['inputs']['source_sha256']
    assert data['apk_entries'][0]['name']=='assets/test.bin'
    assert module.build(root,apk)==data, 'inventory must be deterministic'
    bad=root/'bad.apk'; bad.write_bytes(b'not a zip')
    try:
        module.build(root,bad)
    except zipfile.BadZipFile:
        pass
    else:
        raise AssertionError('bad APK must not produce an apparently complete inventory')
    json.dumps(data)
    pages=module.checklists(data)
    assert pages['SCENARIOS_TODO.md'].count('- [ ]')==3
    assert pages['NATIVE_TYPES_TODO.md'].count('- [ ]')==5
    progress=root/'progress'; progress.mkdir()
    saved=progress/'SCENARIOS_TODO.md'; saved.write_text('KEEP COMPLETED CHECKBOXES')
    output=root/'inventory.json'
    p=subprocess.run([sys.executable,str(ROOT/'tools/parity_inventory.py'),'--root',str(root),
                      '--output',str(output),'--checklist-dir',str(progress)],capture_output=True,text=True)
    assert p.returncode!=0 and saved.read_text()=='KEEP COMPLETED CHECKBOXES'
    assert not output.exists(), 'refuse existing progress before writing any output'
print('ok: duplicate native types, inherited scenarios, multiline CSV, unknown binary, comments not proof, deterministic inventory')
