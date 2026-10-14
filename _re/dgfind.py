"""Find dump methods/fields matching a regex, with enclosing class + nearest class header."""
import re, sys
PATH = '/Users/Adam/Documents/bitlife/_re/il2cpp_dump.txt'
pat = re.compile(sys.argv[1], re.I)
kind = sys.argv[2] if len(sys.argv) > 2 else 'both'
lines = open(PATH, encoding='utf-8', errors='replace').read().split('\n')
cls = '?'
for i, l in enumerate(lines):
    if l.startswith('class '):
        cls = l[6:].split(' : ')[0].strip()
    if kind in ('both', 'method') and l.startswith('    method') and pat.search(l):
        print('M', i + 1, cls, '|', l.strip()[:120])
    if kind in ('both', 'field') and l.startswith('    field') and pat.search(l):
        print('F', i + 1, cls, '|', l.strip()[:120])
