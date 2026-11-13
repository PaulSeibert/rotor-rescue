"""64x64 periodic blue-noise threshold map (void-and-cluster, Ulichney) in [0,1). Tileable: 64 divides 320."""
import os, numpy as np
N = 64
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bn64.npy")

def _kernel(sigma=1.5):
    y, x = np.mgrid[0:N, 0:N]
    dx = np.minimum(x, N - x); dy = np.minimum(y, N - y)
    g = np.exp(-(dx * dx + dy * dy) / (2 * sigma * sigma))
    return np.fft.fft2(g)

def _energy(b, K):
    return np.real(np.fft.ifft2(np.fft.fft2(b.astype(float)) * K))

def make(seed=7):
    rng = np.random.default_rng(seed)
    K = _kernel()
    b = np.zeros((N, N), bool)
    idx = rng.choice(N * N, N * N // 10, replace=False)
    b.flat[idx] = True
    while True:   # relax the initial pattern
        e = _energy(b, K)
        c = np.where(b, e, -1).argmax(); b.flat[c] = False
        e = _energy(b, K)
        v = np.where(~b, e, 1e9).argmin()
        b.flat[v] = True
        if v == c: break
    rank = np.zeros((N, N), int)
    p = b.copy(); n = p.sum()
    q = p.copy()
    for r in range(n - 1, -1, -1):          # phase 1: remove the tightest cluster
        e = _energy(q, K); c = np.where(q, e, -1).argmax(); q.flat[c] = False; rank.flat[c] = r
    q = p.copy()
    for r in range(n, N * N):               # phase 2: fill the largest void
        e = _energy(q, K); v = np.where(~q, e, 1e9).argmin(); q.flat[v] = True; rank.flat[v] = r
    return (rank + 0.5) / (N * N)

def get():
    if os.path.exists(CACHE):
        return np.load(CACHE)
    t = make(); np.save(CACHE, t); return t

if __name__ == "__main__":
    t = get(); print(t.shape, t.min(), t.max())
