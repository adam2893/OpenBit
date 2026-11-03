"""Run every regression script against frozen inputs; keep logs and machine-readable evidence.
Usage: python3 tools/run_suite.py --output /tmp/qdir/suite403
Requires an existing build; never builds, deletes output, or overwrites an earlier run.
"""
import argparse
import hashlib
import json
import os
import signal
import subprocess
import sys
from pathlib import Path


def snapshot(root):
    paths=set()
    paths.update(p for pattern in ('*.py','*.js') for p in root.glob(pattern) if p.is_file())
    for folder,pattern in (('port/src','*.cpp'),('port/include','*.h'),('tools','*.py'),
                           ('gamedata_plain','*'),('gamedata_json','*.json')):
        paths.update(p for p in (root/folder).rglob(pattern) if p.is_file() and '__pycache__' not in p.parts)
    for name in ('port/build/blsim','port/build/libblport.a','il2cpp_dump.txt'):
        if (root/name).is_file():
            paths.add(root/name)
    result={}
    for path in sorted(paths):
        with path.open('rb') as stream:
            result[str(path.relative_to(root))]=hashlib.file_digest(stream,'sha256').hexdigest()
    return result


def execute(script,root,log):
    # Each test owns a separate process group, including compilers it starts.
    process=subprocess.Popen([sys.executable,str(script)],cwd=root,stdout=log,
                             stderr=subprocess.STDOUT,start_new_session=True)
    try:
        return process.wait()
    finally:
        # Kill once: TERM then KILL races orphan reaping on macOS (EPERM).
        try:
            os.killpg(process.pid,signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.wait()


def run(root,output):
    if os.name!='posix':
        raise ValueError('runner requires POSIX process groups; Windows target integration is separate')
    scripts=sorted((root/'tools').glob('test_*.py'))
    if not scripts:
        raise ValueError('no regression scripts found')
    for name in ('port/build/blsim','port/build/libblport.a'):
        if not (root/name).is_file():
            raise ValueError('missing build input: '+name)
    if output.exists():
        raise FileExistsError('refusing to overwrite previous acceptance run: '+str(output))
    before=snapshot(root)
    output.mkdir(parents=True)
    (output/"inputs_before.json").write_text(json.dumps(before,indent=1)+chr(10))
    results=[]
    interrupted=True
    try:
        for script in scripts:
            print('RUN '+script.name,flush=True)
            with (output/(script.stem+'.log')).open('w') as log:
                code=execute(script,root,log)
            results.append(dict(script=str(script.relative_to(root)),exit_code=code))
            print(('PASS ' if code==0 else 'FAIL ')+script.name,flush=True)
        interrupted=False
    finally:
        snapshot_error=None
        try:
            after=snapshot(root)
        except OSError as error:
            after={}
            snapshot_error=str(error)
        changed=sorted(p for p in before.keys()|after.keys() if before.get(p)!=after.get(p))
        summary=dict(planned=len(scripts),completed=len(results),
                     passed=sum(r['exit_code']==0 for r in results),
                     failed=sum(r['exit_code']!=0 for r in results),interrupted=interrupted,
                     integrity_ok=not changed and snapshot_error is None,changed_inputs=changed,
                     snapshot_error=snapshot_error,integrity_scope="endpoint-sha256",
                     inputs_before=before,inputs_after=after,results=results)
        (output/'summary.json').write_text(json.dumps(summary,indent=1)+chr(10))
    print('PASS %d FAIL %d INTEGRITY %s DONE 1' %
          (summary['passed'],summary['failed'],summary['integrity_ok']),flush=True)
    return 2 if changed or snapshot_error else (1 if summary['failed'] else 0)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    def interrupt(number,frame):
        raise SystemExit(128+number)
    signal.signal(signal.SIGTERM,interrupt)
    signal.signal(signal.SIGINT,interrupt)
    try:
        sys.exit(run(args.root.resolve(),args.output.resolve()))
    except (OSError,ValueError) as error:
        print('suite: '+str(error),file=sys.stderr)
        sys.exit(2)
