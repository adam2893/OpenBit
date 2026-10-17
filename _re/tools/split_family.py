"""Move one family out of src/sim.cpp into its own translation unit.

The split is mechanical: each function is cut verbatim (brace matched) and pasted
into src/fam/<name>.cpp behind the same includes. What is *not* mechanical is
linkage -- a helper that was file-local to sim.cpp has to be exported first, and
the linker is the authority on which ones. So the tool does the cut, and the build
names the rest.

  python3 tools/split_family.py model Sim::modelYear Sim::modelCard
  --dry-run      show what would move and how big it is
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIM = os.path.join(ROOT, 'port', 'src', 'sim.cpp')
FAM = os.path.join(ROOT, 'port', 'src', 'fam')


def take(lines, marker):
    """Cut the definition that starts with marker; return it as a list of lines."""
    for i, line in enumerate(lines):
        if line.startswith(marker):
            break
    else:
        raise SystemExit('not found: ' + marker)
    depth = 0
    j = i
    while True:
        depth += lines[j].count('{') - lines[j].count('}')
        if depth == 0 and '{' in chr(10).join(lines[i:j + 1]):
            break
        j += 1
        if j >= len(lines):
            raise SystemExit('unbalanced braces after ' + marker)
    body = lines[i:j + 1]
    del lines[i:j + 1]
    return body


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    family = argv[0]
    markers = argv[1:]
    dry = '--dry-run' in markers
    markers = [m for m in markers if not m.startswith('--')]
    lines = open(SIM, encoding='utf-8').read().split(chr(10))
    moved = []
    for marker in markers:
        body = take(lines, marker)
        moved.append((marker, body))
        print('%-34s %4d lines' % (marker, len(body)))
    if dry:
        return 0
    open(SIM, 'w', encoding='utf-8').write(chr(10).join(lines))
    path = os.path.join(FAM, family + '.cpp')
    header = [
        '// src/fam/%s.cpp -- moved out of src/sim.cpp (REPORT.en.md 131).' % family,
        '//',
        '// Cut verbatim by tools/split_family.py; the only edits are the includes.',
        '#include "bl/sim.h"',
        '#include "bl/table.h"',
        '#include "bl/text.h"',
        '',
        '#include <string>',
        '#include <vector>',
        '',
        'namespace bl {',
        '',
    ]
    out = header
    for _, body in moved:
        out += body
        out += ['']
    out += ['}  // namespace bl', '']
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write(chr(10).join(out))
    print('wrote %s, sim.cpp now %d lines' % (path, len(lines)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))