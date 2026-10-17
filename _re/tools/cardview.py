"""A card-shaped view of a disassembly: only the instructions that are the card.

A scenario handler is 500-1200 instructions, and most of them are framework
codegen: List<T>.Add closures, LINQ enumerators, string.Format wrappers, the
il2cpp runtime helpers. The card itself is a few dozen lines: the game methods it
calls, the immediate arguments those calls take, and the fields it writes on
itself. This tool prints exactly that, collapsing the rest, so a decode starts
from the card and not from the compiler.

  python3 tools/cardview.py 0x73f3dee5c4 200
  python3 tools/cardview.py --method ScenarioExLoverPaternityTestCourtOrder::RefreshScenario 200
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SO = os.path.join(ROOT, "lib", "arm64-v8a", "libil2cpp.so")
SYMS = os.path.join(ROOT, "tools", "symbols.json")
LITS = os.path.join(ROOT, "tools", "lit_cells.json")
STATIC_BASE = 0x73F049A000
FILE_DELTA = 0x73F049E000

# A call into one of these is codegen, not the card.
NOISE = ("System.", "Amazon.", "UnityEngine.", "Newtonsoft.", "<>c", "<", "Microsoft.",
         "Mono.", "Firebase.", "Google.", "Meta.", "Apollo16")
# Fields a card writes on itself: Title, Body, choices, the two flags, narrative.
SELF_FIELDS = ("#0x38]", "#0x48]", "#0x110]", "#0x132]", "#0xb8]", "#0x108]",
               "#0x170]", "#0x178]", "#0x180]")

def load(path):
    try:
        with open(path) as fh:
            return json.load(fh)
    except Exception:
        return {}

def main(argv):
    syms = load(SYMS); lits = load(LITS)
    if argv[:1] == ["--method"]:
        want = argv[1]
        for k, v in syms.items():
            if v == want:
                addr = int(k, 16) + STATIC_BASE
                break
        else:
            print("not found"); return 1
        count = int(argv[2]) if len(argv) > 2 else 200
    else:
        addr = int(argv[0], 0)
        count = int(argv[1]) if len(argv) > 1 else 200
    import capstone
    with open(SO, "rb") as fh:
        fh.seek(addr - FILE_DELTA)
        blob = fh.read(count * 4)
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_LITTLE_ENDIAN)
    adrp = {}
    pending_imm = None
    skipped = 0
    for insn in md.disasm(blob, addr):
        text = insn.mnemonic + " " + insn.op_str
        keep = False
        note = ""
        if insn.mnemonic == "adrp":
            m = re.match(r"(x\d+), #(0x[0-9a-f]+)", insn.op_str)
            if m:
                adrp[m.group(1)] = int(m.group(2), 16)
        elif insn.mnemonic in ("ldr", "ldrb") and "#0x" in insn.op_str:
            m = re.match(r"(x\d+|w\d+), \[(x\d+), #(0x[0-9a-f]+)\]", insn.op_str)
            if m and m.group(2) in adrp:
                cell = adrp[m.group(2)] + int(m.group(3), 16) - STATIC_BASE
                got = lits.get(hex(cell))
                note = "  slot %s%s" % (hex(cell), (" = " + repr(got)) if got else "")
                keep = bool(got)
        elif insn.mnemonic == "bl":
            m = re.search(r"#(0x[0-9a-f]+)", insn.op_str)
            name = syms.get(hex(int(m.group(1), 16) - STATIC_BASE)) if m else None
            if name and not any(name.startswith(n) or n in name for n in NOISE):
                note = "  ; " + name
                keep = True
        elif insn.mnemonic in ("mov", "fmov") and re.search(r"w1, #", insn.op_str):
            keep = True
            pending_imm = insn.op_str
        elif insn.mnemonic == "str" and any(f in insn.op_str for f in SELF_FIELDS):
            keep = True
        if keep:
            if skipped:
                print("        ... %d" % skipped)
                skipped = 0
            print("%x  %-10s %-28s%s" % (insn.address, insn.mnemonic, insn.op_str, note))
        else:
            skipped += 1
    if skipped:
        print("        ... %d" % skipped)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))