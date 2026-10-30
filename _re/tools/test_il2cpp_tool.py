"""299: the il2cpp runtime toolkit -- the assumptions it cannot check at run time.

The toolkit lives in tools/il2cpp_scenarios.py and talks to a live emulator, so it is
not part of the offline suite. What IS checkable offline is what it depends on: the
exported il2cpp C API it calls, the absence of il2cpp_method_get_pointer (the reason a
method address is read from MethodInfo+0x00), and the metadata version the callbacks
were written against.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SO = ROOT / "lib" / "arm64-v8a" / "libil2cpp.so"
META = ROOT / "assets" / "bin" / "Data" / "Managed" / "Metadata" / "global-metadata.dat"
TOOL = ROOT / "tools" / "il2cpp_scenarios.py"

NEEDED = [
    b"il2cpp_domain_get",
    b"il2cpp_domain_get_assemblies",
    b"il2cpp_assembly_get_image",
    b"il2cpp_image_get_class_count",
    b"il2cpp_image_get_class",
    b"il2cpp_class_get_name",
    b"il2cpp_class_get_namespace",
    b"il2cpp_class_get_methods",
    b"il2cpp_method_get_name",
]

assert SO.is_file(), SO
assert META.is_file(), META
assert TOOL.is_file(), TOOL

blob = SO.read_bytes()
for name in NEEDED:
    assert name + b"\x00" in blob, name
# The one export il2cpp does not publish; the toolkit therefore reads +0x00.
assert b"il2cpp_method_get_pointer\x00" not in blob

meta = META.read_bytes()[:8]
assert meta[0:4] == bytes.fromhex("af1bb1fa"), meta
assert int.from_bytes(meta[4:8], "little") == 39, meta[4:8]

src = TOOL.read_text()
for name in NEEDED:
    assert name.decode() in src, name
assert "'RefreshScenario'" in src and "'ChoiceSelectedAtIndex'" in src
print("exports:", len(NEEDED), "metadata version:", int.from_bytes(meta[4:8], "little"))
print("ok")
