"""Source-linked parity census. References are discovery hints, NEVER completion evidence."""
import argparse
import csv
import hashlib
import io
import json
import re
import zipfile
from collections import defaultdict
from pathlib import Path


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def build(root, apk=None):
    root = Path(root)
    dump = root / 'il2cpp_dump.txt'
    classes = []
    for number, line in enumerate(dump.read_text().splitlines(), 1):
        if line.startswith('class '):
            name, _, base = line[6:].partition(' : ')
            classes.append(dict(id='native:L'+str(number), name=name, base=base,
                                line=number, methods=0, status='unverified'))
        elif line.startswith('    method ') and classes:
            classes[-1]['methods'] += 1
    descendants = {'Scenario', 'ScenarioBase'}
    while True:
        added = {x['name'] for x in classes if x['base'] in descendants} - descendants
        if not added:
            break
        descendants.update(added)
    sources = sorted((root/'port').rglob('*.cpp')) + sorted((root/'port').rglob('*.h'))
    sources = [p for p in sources if 'build' not in p.relative_to(root).parts]
    tests = sorted((root/'tools').glob('test_*.py'))
    refs = defaultdict(list)
    test_refs = defaultdict(list)
    limits = []
    markers = re.compile(r'TODO|FIXME|unimplement|unsupported|shortcut|ponytail:|stub|placeholder|not yet|still missing|remaining|ceiling|out.of.scope|simplif|approxim|not (?:ported|implemented|supported|recovered)|does not|doesn.t|not parity|partial|pending|incomplete|defer|not modelled|not modeled|unmeasured|unverified|blocked|invented', re.I)
    token = re.compile(r'a16_[A-Za-z0-9_-]+|genesis_[A-Za-z0-9_-]+|[A-Za-z_][A-Za-z0-9_]*')
    for paths, index in ((sources, refs), (tests, test_refs)):
        for path in paths:
            for number, line in enumerate(path.read_text().splitlines(), 1):
                ref = str(path.relative_to(root))+'#L'+str(number)
                for word in set(token.findall(line)):
                    index[word].append(ref)
                if paths is sources and markers.search(line):
                    limits.append(dict(source=ref, text=line.strip(), status='review_required'))
    report = root/'REPORT.en.md'
    section = ''
    if report.exists():
        for number, line in enumerate(report.read_text().splitlines(), 1):
            if line.startswith('## '):
                section = line
            if markers.search(line):
                limits.append(dict(source='REPORT.en.md#L'+str(number), section=section,
                                   text=line.strip(), status='review_required'))
    scenarios = []
    for item in classes:
        if item['name'] in descendants and item['name'] not in {'Scenario','ScenarioBase'}:
            scenarios.append(dict(item, port_text_refs=refs[item['name']],
                                  test_text_refs=test_refs[item['name']]))
    content = []
    for path in sorted((root/'gamedata_plain').glob('*')):
        if not path.is_file():
            continue
        item = dict(name=path.name, source=str(path.relative_to(root)), sha256=digest(path),
                    bytes=path.stat().st_size, status='unverified', port_text_refs=refs[path.name])
        try:
            text = path.read_text(encoding='utf-8-sig')
        except UnicodeError:
            item['format']='binary'
        else:
            item['format']='text'
            if path.name.startswith('a16_'):
                reader=csv.reader(io.StringIO(text, newline=''), strict=True)
                rows=[]
                previous=0
                try:
                    for row in reader:
                        rows.append(dict(id=len(rows), line=previous+1, end_line=reader.line_num))
                        if len(rows)==1:
                            item['headers']=row
                        previous=reader.line_num
                    item.update(format='csv', data_rows=max(0,len(rows)-1), rows=rows[1:])
                except csv.Error as error:
                    item.update(format='unparsed', error=str(error))
        content.append(item)
    products=[]
    game=root/'port/src/game.cpp'
    if game.exists():
        inside=False
        for number,line in enumerate(game.read_text().splitlines(),1):
            if 'const Entry kEntitlements[]' in line:
                inside=True
            elif inside and line.strip()=='};':
                break
            elif inside and line.lstrip().startswith('{'):
                values=re.findall(r'"([^"]*)"|\b(nullptr)\b',line)
                fields=[value or None for value,null in values]
                if len(fields)!=3:
                    raise ValueError('unparsed entitlement at line '+str(number))
                products.append(dict(grant=fields[0], purchase=fields[1], iap_field=fields[2],
                                     source='port/src/game.cpp#L'+str(number), status='unverified'))
    entries=[]
    if apk is not None:
        with zipfile.ZipFile(apk) as archive:
            entries=[dict(id='apk:'+str(i), name=x.filename, bytes=x.file_size, crc32=x.CRC, status='unverified')
                     for i,x in enumerate(archive.infolist(),1)]
    return dict(schema=1, policy='No parity certified: text references and passing samples are not behavioral proof.',
                inputs=dict(metadata_sha256=digest(dump), apk_sha256=digest(Path(apk)) if apk else None,
                            source_sha256={str(p.relative_to(root)):digest(p)
                                           for p in sources+tests+([report] if report.exists() else [])}),
                summary=dict(native_declarations=len(classes), scenario_candidates=len(scenarios),
                             content_files=len(content), entitlement_entries=len(products),
                             test_files=len(tests), limit_candidates=len(limits), apk_entries=len(entries),
                             verified_parity_items=0),
                native_classes=classes, scenarios=scenarios, content=content,
                entitlement_products=products, limit_candidates=limits, apk_entries=entries,
                unknowns=['Unqualified duplicate type names need namespace resolution.',
                          'Scenario ancestry is a candidate set, not executable scenario coverage.',
                          'Dynamic table aliases and source comments require manual mapping.',
                          'Historical report limits may already be superseded: review, do not auto-close.',
                          'Nested Unity assets, runtime products, server content and retail differences remain uninventoried.',
                          'Every content row, native branch, UI input and state transition requires differential proof.'])


def checklists(data):
    result={}
    def page(name, title, note, lines):
        result[name]='# '+title+chr(10)+chr(10)+note+chr(10)+chr(10)+chr(10).join(lines)+chr(10)
    page('SCENARIOS_TODO.md','Native scenario contract backlog',
         'Every inherited scenario candidate remains unchecked until native behavior, entry path, all branches, state/recovery, inputs and differential tests are mapped. Text references are hints only. Duplicate declarations remain distinct.',
         ['- [ ] '+x['id']+' '+x['name']+' : '+x['base']+' — '+str(x['methods'])+' methods; '+str(len(x['port_text_refs']))+' port-text hints; [source](../il2cpp_dump.txt#L'+str(x['line'])+')' for x in data['scenarios']])
    page('NATIVE_TYPES_TODO.md','Full native declaration scope triage',
         'Classify every declaration as game behavior, presentation/platform integration, shipped-dead content, dependency or unresolved. Dependencies need boundary compatibility, not a handwritten reimplementation. No declaration is silently excluded.',
         ['- [ ] '+x['id']+' '+x['name']+' : '+x['base']+' — [source](../il2cpp_dump.txt#L'+str(x['line'])+')' for x in data['native_classes']])
    page('CONTENT_TODO.md','Every decoded content file and logical row',
         'For each file classify schema and pack(s), map consumers/eligibility/selection/actions/impacts/narratives, test every logical row and branch, record dead/alias evidence. Exact source mention is not coverage. The JSON index retains physical line ranges for every CSV row.',
         ['- [ ] ['+x['name']+'](../'+x['source']+') — '+x['format']+'; data rows '+str(x.get('data_rows','unknown'))+'; port-text hints '+str(len(x['port_text_refs'])) for x in data['content']])
    page('PRODUCTS_TODO.md','Every entitlement grant, item, pack and bundle',
         'Each entry needs original SKU/variants, bundle membership, benefit effects, gameplay/menu gates, persistence, sandbox purchase/restore/cancel/pending/refund/revoke tests. Catalog metadata and free mod grants are not IAP implementation.',
         ['- [ ] IAP-'+str(i)+' '+x['grant']+'; purchase='+str(x['purchase'])+'; field='+str(x['iap_field'])+'; [source](../'+x['source']+')' for i,x in enumerate(data['entitlement_products'],1)])
    page('LIMITS_TODO.md','Documented ceiling candidates requiring reconciliation',
         'These are candidates, NOT a count of current unresolved tasks. Reconcile numeric section chronology and source changes; mark current/historical-fixed/shipped-dead/intentional-exclusion/unknown with evidence. Never resurrect a superseded limit or silently drop an unknown.',
         ['- [ ] GAP-'+str(i)+' [source](../'+x['source']+') '+x.get('section','')+' — '+x['text'] for i,x in enumerate(data['limit_candidates'],1)])
    page('RESOURCES_TODO.md','Every packaged APK entry',
         'Classify each entry: gameplay data, native/runtime dependency, art/audio/font/UI/localization, platform integration, mod/repack-only or unknown. Unpack nested Unity assets separately; ZIP entry enumeration is not internal resource parity. Keep malicious/repack telemetry out of the port.',
         ['- [ ] '+x['id']+' '+x['name']+' — '+str(x['bytes'])+' bytes' for x in data['apk_entries']])
    return result


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--apk',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--checklist-dir',type=Path,help='create initial checklists; refuse to overwrite tracked progress')
    args=parser.parse_args()
    data=build(args.root,args.apk)
    pages=checklists(data) if args.checklist_dir else {}
    for name in pages:
        if (args.checklist_dir/name).exists():
            raise FileExistsError('Refusing to overwrite checklist progress: '+str(args.checklist_dir/name))
    args.output.write_text(json.dumps(data,indent=1,ensure_ascii=False)+chr(10))
    if pages:
        args.checklist_dir.mkdir(parents=True,exist_ok=True)
        for name,text in pages.items():
            (args.checklist_dir/name).write_text(text)
    print(json.dumps(data['summary'],sort_keys=True))
