"""Scores AutoGrind's lines against a retail map's own GrindActors.

    python score.py <truth.json> <found.json> <map.agt> <plot-dir>

Recall: how much of the hand-placed grind length the detector found. Precision: how much of what it
found lies on a hand-placed line. Retail designers did not line every edge that could grind, so
precision is a lower bound: read the plots before calling an unmatched line a mistake.
"""
import json, math, os, sys
import numpy as np

TOLERANCE = 15.0  # cm, 3D
STEP = 10.0

def samples(points):
    out = []
    for a, b in zip(points, points[1:]):
        a, b = np.array(a, float), np.array(b, float)
        n = max(1, int(math.ceil(np.linalg.norm(b - a) / STEP)))
        out += [a + (b - a) * (i / n) for i in range(n)]
    out.append(np.array(points[-1], float))
    return np.array(out)

def segments(lines):
    A, B, owner = [], [], []
    for k, l in enumerate(lines):
        for a, b in zip(l["points"], l["points"][1:]):
            A.append(a); B.append(b); owner.append(k)
    return np.array(A, float).reshape(-1, 3), np.array(B, float).reshape(-1, 3), np.array(owner)

def nearest(S, A, B):
    """Distance from each sample to its nearest segment, and that segment's index."""
    if len(A) == 0:
        return np.full(len(S), np.inf), np.zeros(len(S), int)
    AB = B - A
    L = (AB * AB).sum(1)
    L[L == 0] = 1
    best_d = np.full(len(S), np.inf)
    best_i = np.zeros(len(S), int)
    for start in range(0, len(A), 2048):
        a, ab, l = A[start:start + 2048], AB[start:start + 2048], L[start:start + 2048]
        t = np.clip(((S[:, None, :] - a[None]) * ab[None]).sum(-1) / l[None], 0, 1)
        d = np.linalg.norm(S[:, None, :] - (a[None] + t[..., None] * ab[None]), axis=-1)
        i = d.argmin(1)
        dm = d[np.arange(len(S)), i]
        better = dm < best_d
        best_d[better] = dm[better]
        best_i[better] = i[better] + start
    return best_d, best_i

def main(truth_path, found_path, agt_path, plot_dir):
    truth = json.load(open(truth_path))
    found = json.load(open(found_path))
    FA, FB, Fowner = segments(found)
    TA, TB, _ = segments(truth)

    print(f"{'truth line':18} {'kind':5} {'length':>7} {'found':>6}  kind found")
    total = covered = 0.0
    kind_right = kind_checked = 0
    missed = []
    for l in truth:
        S = samples(l["points"])
        d, i = nearest(S, FA, FB)
        hit = d <= TOLERANCE
        length = sum(math.dist(a, b) for a, b in zip(l["points"], l["points"][1:]))
        share = hit.mean()
        total += length
        covered += length * share
        kinds = {found[Fowner[j]]["kind"] for j in i[hit]} if hit.any() else set()
        if share >= 0.5:
            kind_checked += 1
            kind_right += l["kind"] in kinds and len(kinds) == 1
        else:
            missed.append(l["name"])
        print(f"{l['name']:18} {l['kind']:5} {length:7.0f} {share:6.0%}  {','.join(sorted(kinds)) or '-'}")

    found_total = found_on_truth = 0.0
    for l in found:
        S = samples(l["points"])
        d, _ = nearest(S, TA, TB)
        found_total += l["length"]
        found_on_truth += l["length"] * (d <= TOLERANCE).mean()
    print()
    print(f"recall    {covered / total:6.1%} of {total / 100:.0f} m of hand-placed grind")
    print(f"precision {found_on_truth / max(found_total, 1):6.1%} of {found_total / 100:.0f} m found ({len(found)} lines)")
    print(f"kind      {kind_right}/{kind_checked} matched lines have the retail GrindType")
    print(f"missed    {', '.join(missed) or 'none'}")

    if plot_dir:
        plot(truth, found, agt_path, plot_dir)

def plot(truth, found, agt_path, plot_dir):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    sys.path.insert(0, os.path.dirname(__file__))
    from agt import read_agt
    os.makedirs(plot_dir, exist_ok=True)
    meshes = {n: (v, t) for n, v, t in read_agt(agt_path)}
    for name in sorted({l["mesh"] for l in found}):
        v, t = meshes[name]
        lo, hi = v.min(0) - 100, v.max(0) + 100
        fig, ax = plt.subplots(figsize=(12, 12 * max(0.3, (hi[1] - lo[1]) / max(hi[0] - lo[0], 1))))
        edges = np.unique(np.sort(np.concatenate([t[:, [0, 1]], t[:, [1, 2]], t[:, [2, 0]]]), 1), axis=0)
        for a, b in edges:
            ax.plot(v[[a, b], 0], v[[a, b], 1], color="#cccccc", lw=0.3, zorder=1)
        for l in truth:
            P = np.array(l["points"])
            if ((P[:, :2] >= lo[:2]) & (P[:, :2] <= hi[:2])).all(1).any():
                ax.plot(P[:, 0], P[:, 1], color="#2ca02c", lw=6, alpha=0.45, zorder=2)
        for l in found:
            if l["mesh"] != name:
                continue
            P = np.array(l["points"])
            ax.plot(P[:, 0], P[:, 1], color="#d62728" if l["kind"] == "rail" else "#1f77b4", lw=1.4, zorder=3)
        ax.set_aspect("equal")
        ax.set_title(f"{name}  (green: retail GrindActors, blue: found stone, red: found rail)")
        fig.savefig(os.path.join(plot_dir, name.strip("/").replace("/", "_").replace("[", "_").replace("]", "") + ".png"), dpi=90, bbox_inches="tight")
        plt.close(fig)

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else "")
