import json, sys
sys.path.insert(0, '/Users/Adam/Documents/bitlife/_re')
import blhrpc

d = blhrpc.rpc('rand', n=6000)
rows = d['rows']
seeds = d['seeds']
by = {}
for r in rows:
    if r['m'] != 'Next/0' or r['v'] is None or r['seed'] is None:
        continue
    by.setdefault(str(r['seed']), []).append((r['seq'], r['v']))
out = {k: sorted(v) for k, v in by.items()}
json.dump(out, open('/Users/Adam/Documents/bitlife/_re/rng_vectors.json', 'w'), indent=1)
print('instances', len(out), 'samples', sum(len(v) for v in out.values()))

MX = 2**31 - 1
class DN:
    def __init__(self, seed):
        sub = MX if seed == -2**31 else abs(seed)
        mj = 161803398 - sub
        self.a = [0] * 56
        self.a[55] = mj
        mk = 1
        for i in range(1, 55):
            ii = (21 * i) % 55
            self.a[ii] = mk
            mk = mj - mk
            if mk < 0: mk += MX
            mj = self.a[ii]
        for k in range(1, 5):
            for i in range(1, 56):
                self.a[i] -= self.a[1 + (i + 30) % 55]
                if self.a[i] < 0: self.a[i] += MX
        self.i1, self.i2 = 0, 21
    def sample(self):
        i1, i2 = self.i1, self.i2
        i1 += 1
        if i1 >= 56: i1 = 1
        i2 += 1
        if i2 >= 56: i2 = 1
        r = self.a[i1] - self.a[i2]
        if r == MX: r -= 1
        if r < 0: r += MX
        self.a[i1] = r
        self.i1, self.i2 = i1, i2
        return r

ok = bad = 0
for seedstr, pairs in sorted(out.items(), key=lambda kv: -len(kv[1])):
    if len(pairs) < 3: continue
    rng = DN(int(seedstr))
    want = dict(pairs)
    lo, hi = pairs[0][0], pairs[-1][0]
    if hi - lo > 40000: hi = lo + 40000
    hits = 0
    for n in range(1, hi + 1):
        v = rng.sample()
        if n in want:
            if want[n] == v: hits += 1
            else: bad += 1
    ok += hits
    print('seed %-12s pairs=%-4d replayed=%d matched=%d' % (seedstr, len(pairs), hi, hits))
print('MATCHED', ok, 'MISMATCHED', bad)
