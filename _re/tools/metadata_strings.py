#!/usr/bin/env python3
"""List, grep and dump the il2cpp string literals inside global-metadata.dat.

This build (BitLife, com.candywriter.bitlife, magic 0xFAB11BAF, version 39) does
not use the published il2cpp header field order, so the layout below was derived
from the file itself; METADATA_STRINGS.en.md records the derivation and the
verification.  All fields are little-endian uint32:

  0x00  magic                   0xFAB11BAF
  0x04  version                 39
  0x08  literal index file off  380       (start of the offset array)
  0x0C  literal index bytes     284892    (4 * 71223)
  0x10  literal count           71223
  0x14  literal data file off   285272    (start of the text blob)
  0x18  literal data bytes      1892233
  0x1C  literal count - 1       71222

The index is 'count' non-decreasing uint32 offsets into the data blob.  Literal i
occupies data[off[i]:off[i+1]] (off[count] := data bytes), i.e. literals are
stored back to back with no length prefix and no NUL terminator, and the blob is
kept in byte order because the table is the sorted literal set il2cpp
binary-searches -- so index order is sorted-text order, not source order.  A
literal's file offset is data_off + off[i].

find_table() re-derives the table from the raw bytes instead of trusting the
header: it validates the header tuple and, if that fails, scans the first 4 KiB
(every 4-byte alignment) for a self-consistent (off, bytes, count, data_off,
data_bytes) tuple -- bytes == 4*count, off + bytes == data_off, offsets sorted,
offs[-1] == data_bytes, blob decodes as UTF-8.  A build that matches neither
exits with an error instead of printing garbage.

Commands: info | list | grep | dump.
"""
import argparse
import bisect
import re
import struct
import sys

DEFAULT_METADATA = ('/Users/Adam/Documents/bitlife/_re/assets/bin/Data/Managed/'
                    'Metadata/global-metadata.dat')
MAGIC = 0xFAB11BAF
# header field name -> file offset; the first five are also the tuple a scan looks for
FIELDS = (('literal index offset', 0x08), ('literal index bytes', 0x0C),
          ('literal count', 0x10), ('literal data offset', 0x14),
          ('literal data bytes', 0x18), ('literal count - 1', 0x1C))
def u32(data, off):
    return struct.unpack_from('<I', data, off)[0]


def consistent_table(data, index_off, index_bytes, count, data_off, data_bytes):
    """Return the offset array if these numbers describe a literal table, else None."""
    if count < 2 or index_bytes != 4 * count or index_off + index_bytes != data_off:
        return None
    if data_off + data_bytes > len(data):
        return None
    offs = struct.unpack_from('<%dI' % count, data, index_off)
    if offs[0] != 0 or offs[-1] != data_bytes:
        return None
    if any(offs[i] > offs[i + 1] for i in range(count - 1)):
        return None
    try:
        data[data_off:data_off + data_bytes].decode('utf-8')
    except UnicodeDecodeError:
        return None
    return list(offs)


def find_table(data):
    """Locate the literal offset array and data blob; returns (index_off, offs, data_off, data_bytes)."""
    hdr = [u32(data, off) for _, off in FIELDS]
    if u32(data, 0) == MAGIC:
        offs = consistent_table(data, hdr[0], hdr[1], hdr[2], hdr[3], hdr[4])
        if offs:
            return hdr[0], offs, hdr[3], hdr[4]
        why = 'header literal fields at 0x08..0x18 are not self-consistent'
    else:
        why = 'magic 0x%08X is not 0x%08X' % (u32(data, 0), MAGIC)
    for off in range(0, min(0x1000, len(data) - 20), 4):
        vals = [u32(data, off + 4 * k) for k in range(5)]
        offs = consistent_table(data, *vals)
        if offs:
            return vals[0], offs, vals[3], vals[4]
    sys.exit('%s: %s, and no self-consistent literal table found in the first 4 KiB'
             % (DEFAULT_METADATA, why))


class Literals(object):
    def __init__(self, path):
        self.path = path
        with open(path, 'rb') as fh:
            self.data = fh.read()
        self.index_off, self.offs, self.data_off, self.data_bytes = find_table(self.data)
        self.count = len(self.offs)

    def end(self, i):
        return self.offs[i + 1] if i + 1 < self.count else self.data_bytes

    def file_offset(self, i):
        return self.data_off + self.offs[i]

    def raw(self, i):
        return self.data[self.data_off + self.offs[i]:self.data_off + self.end(i)]

    def text(self, i):
        return self.raw(i).decode('utf-8', 'replace')

    def index_of(self, data_off):
        """Index of the literal containing a blob-relative offset."""
        return bisect.bisect_right(self.offs, data_off) - 1


def cmd_info(lit, args):
    print('metadata      %s (%d bytes)' % (lit.path, len(lit.data)))
    print('magic         0x%08X   version %d' % (u32(lit.data, 0), u32(lit.data, 4)))
    for name, off in FIELDS:
        print('  0x%02X %-22s %d' % (off, name, u32(lit.data, off)))
    print('located index %d .. %d (%d entries), data %d .. %d'
          % (lit.index_off, lit.index_off + 4 * lit.count, lit.count,
             lit.data_off, lit.data_off + lit.data_bytes))
    nonempty = sum(1 for i in range(lit.count) if lit.end(i) > lit.offs[i])
    bad = [i for i in range(lit.count - 1)
           if lit.end(i) > lit.offs[i] and lit.end(i + 1) > lit.offs[i + 1]
           and lit.raw(i) > lit.raw(i + 1)]
    print('literals      %d non-empty of %d entries (the last entry is the empty sentinel)'
          % (nonempty, lit.count))
    print('utf-8         blob decodes clean')
    print('sorted        %s' % ('yes' if not bad else 'no, %d violation(s) at %s'
                               % (len(bad), bad[:8])))
    return 0


def cmd_list(lit, args):
    start = max(0, args.start)
    stop = lit.count if args.limit is None else min(lit.count, start + args.limit)
    for i in range(start, stop):
        print('%d\t#%d\t%s' % (lit.file_offset(i), i,
                               lit.text(i) if args.raw else repr(lit.text(i))))
    return 0


def cmd_grep(lit, args):
    if args.ignore_case and not args.regex:
        sys.exit('grep: --ignore-case only applies with --regex')
    if args.regex:
        rx = re.compile(args.pattern, re.IGNORECASE if args.ignore_case else 0)
        search = lambda text: rx.search(text)
    else:
        search = lambda text: text.find(args.pattern) if args.pattern in text else None
    hits = 0
    for i in range(lit.count):
        text = lit.text(i)
        spans = [m.start() for m in rx.finditer(text)] if args.regex else []
        if not args.regex:
            at = text.find(args.pattern)
            while at >= 0:
                spans.append(at)
                at = text.find(args.pattern, at + 1)
        for at in spans:
            delta = len(text[:at].encode('utf-8'))
            hits += 1
            if args.limit is not None and hits > args.limit:
                break
            print('%d\t#%d\t+%d\t%s' % (lit.file_offset(i), i, delta,
                                        text if args.raw else repr(text)))
    if args.count:
        print(hits)
    return 0 if hits else 1


def cmd_dump(lit, args):
    if args.index is not None:
        i = args.index
        what = 'index %d' % i
    elif args.file_offset is not None:
        if not (lit.data_off <= args.file_offset < lit.data_off + lit.data_bytes):
            sys.exit('dump: file offset %d is outside the literal data blob [%d, %d)'
                     % (args.file_offset, lit.data_off, lit.data_off + lit.data_bytes))
        i = lit.index_of(args.file_offset - lit.data_off)
        what = 'file offset %d' % args.file_offset
    else:
        if not (0 <= args.data_offset < lit.data_bytes):
            sys.exit('dump: blob offset %d is outside [0, %d)' % (args.data_offset, lit.data_bytes))
        i = lit.index_of(args.data_offset)
        what = 'blob offset %d' % args.data_offset
    text = lit.text(i)
    print('literal      #%d  (%s)' % (i, what))
    print('file offset  %d .. %d' % (lit.file_offset(i), lit.data_off + lit.end(i)))
    print('blob offset  %d .. %d' % (lit.offs[i], lit.end(i)))
    print('bytes        %d' % len(lit.raw(i)))
    print('text         %s' % (text if args.raw else repr(text)))
    return 0


def main(argv=None):
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument('--metadata', default=DEFAULT_METADATA, help='path to global-metadata.dat')
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0], parents=[common])
    sub = ap.add_subparsers(dest='cmd', required=True)

    sub.add_parser('info', parents=[common], help='print the located layout and validation facts')

    p = sub.add_parser('list', parents=[common], help='list literals in index (sorted-text) order')
    p.add_argument('--start', type=int, default=0, help='first literal index')
    p.add_argument('--limit', type=int, default=None, help='how many literals to print')
    p.add_argument('--raw', action='store_true', help='print text instead of repr()')

    p = sub.add_parser('grep', parents=[common], help='search literals; one line per hit: file offset, index, byte delta, literal')
    p.add_argument('pattern', help='substring, or a regular expression with --regex')
    p.add_argument('--regex', action='store_true', help='treat pattern as a regular expression')
    p.add_argument('--ignore-case', action='store_true', help='case-insensitive (with --regex)')
    p.add_argument('--limit', type=int, default=None, help='stop after this many hits')
    p.add_argument('--count', action='store_true', help='also print the hit count')
    p.add_argument('--raw', action='store_true', help='print text instead of repr()')

    p = sub.add_parser('dump', parents=[common], help='print the literal at one offset')
    p.add_argument('--index', type=int, default=None, help='literal index (sorted-text order)')
    p.add_argument('--file-offset', type=int, default=None, help='absolute file offset inside the data blob')
    p.add_argument('--data-offset', type=int, default=None, help='blob-relative offset')
    p.add_argument('--raw', action='store_true', help='print text instead of repr()')

    args = ap.parse_args(argv)
    lit = Literals(args.metadata)
    return {'info': cmd_info, 'list': cmd_list, 'grep': cmd_grep, 'dump': cmd_dump}[args.cmd](lit, args)


if __name__ == '__main__':
    sys.exit(main())
