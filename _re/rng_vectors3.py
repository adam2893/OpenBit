import json
d = json.load(open('/Users/Adam/Documents/bitlife/_re/rng_vectors.json'))
MX = 2**31 - 1
class DN:
    def __init__(self, seed):
        sub = MX if seed == -2**31 else abs(seed)
        mj = 161803398 - sub
        self.a=[0]*56; self.a[55]=mj
        mk = 1
        for i in range(1,55):
            ii=(21*i)%55; self.a[ii]=mk; mk=mj-mk
            if mk<0: mk+=MX
            mj=self.a[ii]
        for k in range(1,5):
            for i in range(1,56):
                self.a[i]-=self.a[1+(i+30)%55]
                if self.a[i]<0: self.a[i]+=MX
        self.i1,self.i2=0,21
    def sample(self):
        i1,i2=self.i1+1,self.i2+1
        if i1>=56: i1=1
        if i2>=56: i2=1
        r=self.a[i1]-self.a[i2]
        if r==MX: r-=1
        if r<0: r+=MX
        self.a[i1]=r; self.i1,self.i2=i1,i2
        return r
for seed, pl in sorted(d.items(), key=lambda kv:-len(kv[1])):
    pairs=[tuple(p) for p in pl]
    if len(pairs)<20: continue
    rng=DN(int(seed)); stream=[rng.sample() for _ in range(6000)]
    head=pairs[:30]
    best=(0,None)
    for base in range(0, 3000):
        m=0
        for sq,v in head:
            ix = base + (sq - head[0][0])
            if ix < len(stream) and stream[ix]==v: m+=1
        if m>best[0]: best=(m,base)
    m,base=best
    allm=0
    for sq,v in pairs:
        ix = base + (sq - head[0][0])
        if ix < len(stream) and stream[ix]==v: allm+=1
    print('seed', seed, 'pairs', len(pairs), 'base', base, 'head', m, 'all', allm)
