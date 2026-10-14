import json, sys
sys.path.insert(0, '/Users/Adam/Documents/bitlife/_re')

d = json.load(open('/Users/Adam/Documents/bitlife/_re/rng_vectors.json'))
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
        i1, i2 = self.i1 + 1, self.i2 + 1
        if i1 >= 56: i1 = 1
        if i2 >= 56: i2 = 1
        r = self.a[i1] - self.a[i2]
        if r == MX: r -= 1
        if r < 0: r += MX
        self.a[i1] = r
        self.i1, self.i2 = i1, i2
        return r

for seedstr, pairs in sorted(d.items(), key=lambda kv: -len(kv[1])):
    pairs = [tuple(p) for p in pairs]
    if len(pairs) < 20: continue
    rng = DN(int(seedstr))
    stream = [rng.sample() for _ in range(200000)]
    first_seq, first_val = pairs[0]
    try:
        n0 = stream.index(first_val)
    except ValueError:
        print('seed %-12s first value %d never appears in 200k samples' % (seedstr, first_val))
        continue
    bad = [p for p in pairs if n0 + (p[0] - first_seq) >= len(stream) or stream[n0 + (p[0] - first_seq)] != p[1]]
    print('seed %-12s pairs=%-4d offset=%-7d mismatched=%d' % (seedstr, len(pairs), n0 - first_seq, len(bad)))
    if bad[:3]: print('   first mismatches', bad[:3])
