"""Ground-truth grind lines from an exported Rollout map JSON, in UE world space."""
import json, math, sys

def rot_matrix(pitch, yaw, roll):
    # FRotationMatrix (UE 5.4 RotationMatrix.h): rows are the X, Y, Z axes.
    sp, cp = math.sin(math.radians(pitch)), math.cos(math.radians(pitch))
    sy, cy = math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    sr, cr = math.sin(math.radians(roll)), math.cos(math.radians(roll))
    return [
        [cp * cy, cp * sy, sp],
        [sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp],
        [-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp],
    ]

def apply(m, v):
    # UE transforms row vectors: v' = v * M
    return [v[0] * m[0][i] + v[1] * m[1][i] + v[2] * m[2][i] for i in range(3)]

def vec(d, default=0.0):
    return [d.get("X", default), d.get("Y", default), d.get("Z", default)] if d else [default] * 3

def load(path):
    objs = json.load(open(path, encoding="utf-8"))
    by_outer = {}
    for o in objs:
        outer = (o.get("Outer") or {}).get("ObjectName", "")
        by_outer.setdefault(outer, []).append(o)
    lines = []
    for a in (o for o in objs if o.get("Type") == "GrindActor_C"):
        name = a["Name"]
        comps = [c for k, v in by_outer.items() if k.endswith("." + name + "'") for c in v]
        root = next(c for c in comps if c["Name"] == "DefaultSceneRoot")["Properties"]
        spline = next(c for c in comps if c["Type"] == "SplineComponent")["Properties"]
        loc = vec(root.get("RelativeLocation"))
        r = root.get("RelativeRotation") or {}
        m = rot_matrix(r.get("Pitch", 0), r.get("Yaw", 0), r.get("Roll", 0))
        scale = vec(root.get("RelativeScale3D"), 1.0)
        sloc = vec(spline.get("RelativeLocation"))
        pts = []
        for p in spline["SplineCurves"]["Position"]["Points"]:
            local = [(sloc[i] + p["OutVal"][k]) * scale[i] for i, k in enumerate("XYZ")]
            w = apply(m, local)
            pts.append([w[i] + loc[i] for i in range(3)])
        modes = {p.get("InterpMode", "").split("::")[-1] for p in spline["SplineCurves"]["Position"]["Points"]}
        kind = "rail" if "NewEnumerator0" in str(a["Properties"].get("GrindType")) else "stone"
        lines.append({"name": name, "kind": kind, "points": pts, "modes": sorted(modes),
                      "has_scale": "RelativeScale3D" in root, "spline_rot": "RelativeRotation" in spline})
    return lines

if __name__ == "__main__":
    lines = load(sys.argv[1])
    json.dump(lines, open(sys.argv[2], "w"), indent=1)
    for l in lines:
        p = l["points"]
        length = sum(math.dist(p[i], p[i + 1]) for i in range(len(p) - 1))
        print(f'{l["name"]:18} {l["kind"]:5} pts={len(p):2} len={length:7.0f} z={min(q[2] for q in p):6.0f}..{max(q[2] for q in p):6.0f} '
              f'start=({p[0][0]:.0f},{p[0][1]:.0f}) {l["modes"]} scale={l["has_scale"]} srot={l["spline_rot"]}')
