"""Durable acceptance never passes zero scripts, failed scripts or changed frozen inputs."""
import importlib.util
import json
from unittest.mock import patch
import os
import socket
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
RUNNER=ROOT/'tools/run_suite.py'
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory)/'root'
    (root/'tools').mkdir(parents=True)
    (root/'port/build').mkdir(parents=True)
    (root/'port/build/blsim').write_bytes(b'fixture binary')
    (root/'port/build/libblport.a').write_bytes(b'fixture library')
    command=[sys.executable,str(RUNNER),'--root',str(root)]
    output=Path(directory)/'zero'
    p=subprocess.run(command+['--output',str(output)],capture_output=True,text=True)
    assert p.returncode==2 and not output.exists(),(p.stdout,p.stderr)
    (root/'tools/test_pass.py').write_text("print('pass context')")
    (root/'tools/test_fail.py').write_text("print('failure context'); raise SystemExit(3)")
    output=Path(directory)/'failure'
    p=subprocess.run(command+['--output',str(output)],capture_output=True,text=True)
    assert p.returncode==1,(p.stdout,p.stderr)
    summary=json.loads((output/'summary.json').read_text())
    assert summary['passed']==1 and summary['failed']==1 and summary['integrity_ok']
    assert summary['completed']==2 and not summary['interrupted']
    assert 'failure context' in (output/'test_fail.log').read_text()
    saved=(output/'summary.json').read_bytes()
    p=subprocess.run(command+['--output',str(output)],capture_output=True,text=True)
    assert p.returncode==2 and (output/'summary.json').read_bytes()==saved
    # A script that exits0 after mutating itself must still make acceptance fail.
    (root/'tools/test_fail.py').write_text("print('now passes')")
    (root/'tools/test_mutation.py').write_text("from pathlib import Path; p=Path(__file__); p.write_text(p.read_text()+'#changed')")
    output=Path(directory)/'mutated'
    p=subprocess.run(command+['--output',str(output)],capture_output=True,text=True)
    assert p.returncode==2,(p.stdout,p.stderr)
    summary=json.loads((output/'summary.json').read_text())
    assert summary['passed']==3 and not summary['integrity_ok']
    assert 'tools/test_mutation.py' in summary['changed_inputs']
    # Verify success and binary immutability are measured, not inferred from stdout.
    (root/'tools/test_mutation.py').write_text("print('stable')")
    output=Path(directory)/'success'
    p=subprocess.run(command+['--output',str(output)],capture_output=True,text=True)
    assert p.returncode==0,(p.stdout,p.stderr)
    summary=json.loads((output/'summary.json').read_text())
    assert summary['passed']==3 and summary['failed']==0 and summary['integrity_ok']
    assert 'port/build/blsim' in summary['inputs_before']
    assert 'port/build/libblport.a' in summary['inputs_before']
    # Interrupt only after the owned child announces readiness over a bounded local socket.
    marker=Path(directory)/'ready.sock'
    (root/'tools/test_signal.py').write_text("import os,socket,signal; s=socket.socket(socket.AF_UNIX); s.connect(os.environ['SUITE_SIGNAL_SOCKET']); s.sendall(str(os.getpid()).encode()); s.close(); signal.pause()")
    with socket.socket(socket.AF_UNIX) as server:
        server.settimeout(5)
        server.bind(str(marker)); server.listen()
        env=dict(os.environ,SUITE_SIGNAL_SOCKET=str(marker))
        output=Path(directory)/'interrupted'
        process=subprocess.Popen(command+['--output',str(output)],env=env,
                                 stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            connection,_=server.accept()
            with connection:
                child=int(connection.recv(100).decode())
            assert (output/"inputs_before.json").is_file(), "initial hash evidence missing during execution"
            process.terminate()
            stdout,stderr=process.communicate(timeout=10)
            assert process.returncode==143,(stdout,stderr)
        finally:
            if process.poll() is None:
                process.terminate(); process.communicate(timeout=10)
    summary=json.loads((output/'summary.json').read_text())
    assert summary['interrupted'] and summary['completed']==3 and summary['planned']==4
    try:
        os.kill(child,0)
    except ProcessLookupError:
        pass
    else:
        raise AssertionError('interrupted child was not released')
    # Preserve completed results even if the final filesystem snapshot fails.
    (root/"tools/test_signal.py").write_text("print('stable')")
    spec=importlib.util.spec_from_file_location("suite_runner",RUNNER)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    output=Path(directory)/"snapshot_error"
    before=module.snapshot(root)
    with patch.object(module,"snapshot",side_effect=[before,OSError("snapshot fixture failure")]):
        assert module.run(root,output)==2
    summary=json.loads((output/"summary.json").read_text())
    assert summary["completed"]==4 and not summary["integrity_ok"]
    assert summary["snapshot_error"]=="snapshot fixture failure"
    assert json.loads((output/"inputs_before.json").read_text())==before
    # A normally exiting test must not leave its owned descendants running.
    (root/'tools/test_signal.py').write_text("print('stable')")
    child_code = "import os,socket,signal; s=socket.socket(socket.AF_UNIX); s.connect(os.environ['SUITE_SIGNAL_SOCKET']); s.sendall(str(os.getpid()).encode()); print('ready',flush=True); signal.pause()"
    (root/'tools/test_descendant.py').write_text(
        "import subprocess,sys; p=subprocess.Popen([sys.executable,'-c',"+repr(child_code)+"],stdout=subprocess.PIPE); assert p.stdout.readline().strip()==b'ready'")
    marker=Path(directory)/'descendant.sock'
    with socket.socket(socket.AF_UNIX) as server:
        server.settimeout(5)
        server.bind(str(marker)); server.listen()
        env=dict(os.environ,SUITE_SIGNAL_SOCKET=str(marker))
        process=subprocess.Popen(command+['--output',str(Path(directory)/'descendant')],
                                 env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        child=None
        try:
            connection,_=server.accept()
            with connection:
                connection.settimeout(5)
                child=int(connection.recv(100).decode())
                stdout,stderr=process.communicate(timeout=10)
                assert process.returncode==0,(stdout,stderr)
                assert connection.recv(1)==b'', 'normal-exit descendant remains alive'
        finally:
            if process.poll() is None:
                process.terminate(); process.communicate(timeout=10)
            if child is not None:
                try:
                    os.kill(child,9)
                except ProcessLookupError:
                    pass
print('ok: zero/failure/mutation/success/interruption exits, starting hashes, preserved logs, normal/interrupted descendant cleanup')
