"""Extract the il2cpp string-literal table from global-metadata.dat.

Layout (header, all int32 little-endian):
  [0] sanity  [4] version
  [8] stringLiteralOffset  [12] stringLiteralCount
  [16] stringLiteralDataOffset  [20] stringLiteralDataCount
Each literal entry is 8 bytes: uint32 length, uint32 dataIndex.
String data is UTF-8 at stringLiteralDataOffset + dataIndex.
"""
import struct, json, sys

PATH = '/Users/Adam/Documents/bitlife/_re/assets/bin/Data/Managed/Metadata/global-metadata.dat'
OUT = '/Users/Adam/Documents/bitlife/_re/metadata_strings.json'

data = open(PATH, 'rb').read()
sanity, version = struct.unpack_from('<Ii', data, 0)
sl_off, sl_cnt = struct.unpack_from('<ii', data, 8)
sld_off, sld_cnt = struct.unpack_from('<ii', data, 16)
print('sanity=0x%x version=%d stringLiteralOff=%d count=%d dataOff=%d dataCount=%d' % (
    sanity, version, sl_off, sl_cnt, sld_off, sld_cnt))
assert 0 < sl_cnt < 4000000, 'stringLiteralCount looks wrong'
lit = []
for i in range(sl_cnt):
    ln, di = struct.unpack_from('<II', data, sl_off + 8 * i)
    if ln > 4096:
        continue
    raw = data[sld_off + di: sld_off + di + ln]
    s = raw.decode('utf-8', 'replace').rstrip('\x00')
    if s:
        lit.append(s)
print('literals', len(lit))
print('sample:', json.dumps(lit[:6]))
hits = [s for s in lit if 'wants to become your friend' in s]
print('probe "wants to become your friend":', hits[:2])
hits2 = [s for s in lit if 'started primary school' in s or 'primary school' in s]
print('probe "primary school":', hits2[:5])
json.dump(lit, open(OUT, 'w'))
