"""Capture stream sizes must reach real RPC payloads, never its timeout argument.
Executes only three AST-selected assignments; transport stays entirely in memory.
"""
import ast
import importlib.util
import io
import json
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
expected = {"logs": ("logs", 3000, 50), "scen": ("scenarios", 1000, 20),
            "ticks": ("ticks", 500, 10)}
driver = ast.parse((ROOT / "drive10.py").read_text())
nodes = [node for node in ast.walk(driver)
         if isinstance(node, ast.Assign) and len(node.targets) == 1
         and isinstance(node.targets[0], ast.Name) and node.targets[0].id in expected]
nodes.sort(key=lambda node: node.lineno)
assert [node.targets[0].id for node in nodes] == list(expected)
assignments = compile(ast.Module(body=nodes, type_ignores=[]), str(ROOT / "drive10.py"), "exec")
spec = importlib.util.spec_from_file_location("capture_rpc", ROOT / "blhrpc.py")
rpc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rpc)

for baseline in ((51, 21, 11), (49, 19, 9)):
    buffers = {op: [{"stream": op, "ordinal": i} for i in range(count + 2)]
               for (op, _, _), count in zip(expected.values(), baseline)}
    requests = []
    pending = []

    def memory_open(path, mode="r"):
        if mode == "w":
            assert str(path).startswith("/tmp/blh_req.")
            stream = io.StringIO()
            pending.append(stream)
            return stream
        assert str(path).startswith("/tmp/blh_res.")
        request = json.loads(pending[-1].getvalue())
        requests.append(request)
        op = request["op"]
        default = next(item[2] for item in expected.values() if item[0] == op)
        return io.StringIO(json.dumps(buffers[op][-request.get("n", default):]))

    namespace = {"blhrpc": rpc, "c0": baseline}
    with patch.object(rpc, "open", memory_open, create=True), \
         patch.object(rpc.os.path, "exists", return_value=True), \
         patch.object(rpc.os, "remove") as remove:
        exec(assignments, namespace)
    assert requests == [{"op": op, "n": count} for op, count, _ in expected.values()], requests
    assert remove.call_count == 3
    for variable, (op, _, _) in expected.items():
        assert namespace[variable] == buffers[op][-2:], (baseline, variable, namespace[variable])

print("ok: real RPC receives explicit stream counts; beyond/at default-tail boundaries retain new observations")
