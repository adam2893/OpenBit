"""Annotated disassembly of libil2cpp.so, using the symbol and literal caches.

  python3 tools/disasm.py 0x73f3daf658 90
  python3 tools/disasm.py 0x73f3daf658 90 --lits     # resolve slot contents too
  python3 tools/disasm.py --method ScenarioLoverBreakUp::RefreshScenario

Every `bl` is named from tools/symbols.json (140k managed methods, dumped from the
running game by tools/symbols_dump.py), every branch target is labelled with the
name of the method it lands in, and every `adrp`+`ldr` pair is resolved to a slot
address -- with its literal text when tools/lit_cells.json has already seen that
slot load at run time.

Requires capstone (python3 -m pip install capstone); the rest of tools/ does not.
"""
import json
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SO = os.path.join(ROOT, "lib", "arm64-v8a", "libil2cpp.so")
SYMS = os.path.join(ROOT, "tools", "symbols.json")
LITS = os.path.join(ROOT, "tools", "lit_cells.json")
DUMP = os.path.join(ROOT, "il2cpp_dump.txt")
STATIC_BASE = 0x73F049A000
FILE_DELTA = 0x73F049E000     # static address -> file offset


def load(path, default=None):
    try:
        with open(path) as fh:
            return json.load(fh)
    except Exception:
        return default if default is not None else {}


def symbols():
    return load(SYMS)


def method_index():
    """Static address -> 'Scenario*::Method' for the scenario classes, which the
    symbols table has too, but this index also covers methods the walker skipped."""
    idx = {}
    with open(DUMP, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = re.search(r"method @0x([0-9a-f]+) (\S+ (\S+)\(\))", line)
            if m:
                idx.setdefault(int(m.group(1), 16), m.group(2))
    return idx


def find_method_start(syms, addr):
    """The nearest named method at or below addr, as (addr, name)."""
    best = None
    for key, name in syms.items():
        a = int(key, 16) + STATIC_BASE
        if a <= addr and (best is None or a > best[0]):
            best = (a, name)
    return best


def main(argv):
    syms = symbols()
    lits = load(LITS)
    if argv[:1] == ["--method"]:
        want = argv[1]
        for key, name in syms.items():
            if name == want:
                print("0x%x" % (int(key, 16) + STATIC_BASE))
                return 0
        print("not found")
        return 1
    if not argv:
        print(__doc__)
        return 0
    addr = int(argv[0], 0)
    count = int(argv[1]) if len(argv) > 1 else 40
    want_lits = "--lits" in argv

    import capstone
    with open(SO, "rb") as fh:
        fh.seek(addr - FILE_DELTA)
        blob = fh.read(count * 4)
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_LITTLE_ENDIAN)
    adrp = {}
    for insn in md.disasm(blob, addr):
        text = "%x  %-8s %s" % (insn.address, insn.mnemonic, insn.op_str)
        if insn.mnemonic == "adrp":
            m = re.match(r"(x\d+), #(0x[0-9a-f]+)", insn.op_str)
            if m:
                adrp[m.group(1)] = int(m.group(2), 16)
        elif insn.mnemonic in ("ldr", "ldrb", "str", "strb"):
            # The immediate is required: "[x20]" with no offset is a dereference of
            # a pointer the code already holds, not a load from a slot.
            m = re.match(r"(x\d+|w\d+), \[(x\d+), #(0x[0-9a-f]+)\]", insn.op_str)
            if m and m.group(2) in adrp:
                cell = adrp[m.group(2)] + int(m.group(3), 16)
                text += "   ; slot 0x%x" % (cell - STATIC_BASE)
                if want_lits:
                    got = lits.get(hex(cell - STATIC_BASE))
                    if got:
                        text += " = %r" % got
        elif insn.mnemonic in ("bl", "b", "b.eq", "b.ne", "b.lt", "b.gt", "b.hi",
                               "b.ls", "b.ge", "b.le", "cbz", "cbnz", "tbz", "tbnz"):
            m = re.search(r"#(0x[0-9a-f]+)", insn.op_str)
            if m:
                target = int(m.group(1), 16)
                # Exact hits only. The managed table has 140k entries but the
                # runtime helpers the codegen emits (class init, throw stubs) are
                # not among them, and a nearest-below guess would put a wrong name
                # on every one of them.
                name = syms.get(hex(target - STATIC_BASE))
                if name is None and "--near" in argv:
                    hit = find_method_start(syms, target)
                    if hit and target - hit[0] < 0x400:
                        name = "~" + hit[1]
                if name:
                    text += "   ; " + name
        print(text)
    return 0


def mnemonic_is_call(mnemonic):
    return mnemonic == "bl"


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
