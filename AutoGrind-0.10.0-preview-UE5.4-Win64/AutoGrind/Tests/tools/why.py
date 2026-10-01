"""Why was nothing found here? python why.py <found.json> x y z radius -> lines and rejected edges near the point."""
import json, sys, numpy as np
found, p, r = sys.argv[1], np.array(list(map(float, sys.argv[2:5]))), float(sys.argv[5])
def dist(a, b):
    a, b = np.array(a), np.array(b); ab = b - a; t = np.clip((p - a) @ ab / max(ab @ ab, 1e-9), 0, 1)
    return np.linalg.norm(p - (a + ab * t))
for l in json.load(open(found)):
    d = min(dist(a, b) for a, b in zip(l["points"], l["points"][1:]))
    if d <= r: print(f"LINE {l['kind']:5} d={d:5.1f} len={l['length']:.0f} drop={l['drop']:.0f} {l['mesh'].split('/')[2]}")
for e in json.load(open(found + ".rejected.json")):
    d = dist(e["a"], e["b"])
    if d <= r: print(f"REJ  d={d:5.1f} drop={e['drop']:8.1f} {e['reason']}  {[round(c) for c in e['a']]} -> {[round(c) for c in e['b']]}")
