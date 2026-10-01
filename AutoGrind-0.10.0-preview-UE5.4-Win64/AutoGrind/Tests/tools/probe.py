"""Lists the triangles of one exported mesh near a point: python probe.py <agt> <mesh-part> x y z radius"""
import sys, numpy as np
sys.path.insert(0, __import__('os').path.dirname(__file__))
from agt import read_agt

def closest_on_triangle(p, a, b, c):
    # Ericson, Real-Time Collision Detection 5.1.5
    ab, ac, ap = b - a, c - a, p - a
    d1, d2 = ab @ ap, ac @ ap
    if d1 <= 0 and d2 <= 0: return a
    bp = p - b; d3, d4 = ab @ bp, ac @ bp
    if d3 >= 0 and d4 <= d3: return b
    vc = d1 * d4 - d3 * d2
    if vc <= 0 and d1 >= 0 and d3 <= 0: return a + ab * (d1 / (d1 - d3))
    cp = p - c; d5, d6 = ab @ cp, ac @ cp
    if d6 >= 0 and d5 <= d6: return c
    vb = d5 * d2 - d1 * d6
    if vb <= 0 and d2 >= 0 and d6 <= 0: return a + ac * (d2 / (d2 - d6))
    va = d3 * d6 - d5 * d4
    if va <= 0 and d4 - d3 >= 0 and d5 - d6 >= 0: return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)))
    den = 1 / (va + vb + vc); return a + ab * (vb * den) + ac * (vc * den)

if __name__ == "__main__":
    agt, part = sys.argv[1], sys.argv[2]
    p = np.array(list(map(float, sys.argv[3:6]))); r = float(sys.argv[6])
    for name, v, t in read_agt(agt):
        if part not in name: continue
        v = v.astype(float)
        for i, tri in enumerate(t):
            a, b, c = v[tri]
            q = closest_on_triangle(p, a, b, c)
            d = np.linalg.norm(q - p)
            if d <= r:
                n = np.cross(b - a, c - a); n /= np.linalg.norm(n) or 1
                print(f"{name.split('/')[2]:24} tri {i:5} d={d:6.1f} n=({n[0]:+.2f},{n[1]:+.2f},{n[2]:+.2f}) "
                      f"v=({a[0]:.0f},{a[1]:.0f},{a[2]:.1f}) ({b[0]:.0f},{b[1]:.0f},{b[2]:.1f}) ({c[0]:.0f},{c[1]:.0f},{c[2]:.1f})")
