"""World-space triangles for every mesh in an exported Rollout map USD, written for the AutoGrind scorer.

Output (little endian): b'AGT1', uint32 objects; per object: uint32 name bytes, name utf-8,
uint32 vertices, float32 xyz * vertices, uint32 triangles, uint32 abc * triangles.
"""
import struct, sys
from pxr import Usd, UsdGeom, Gf

FLIP_Y = True  # set from the axis check against GrindActor_C_98

def to_ue(p):
    return (p[0], -p[1] if FLIP_Y else p[1], p[2])

def triangles(mesh):
    counts = mesh.GetFaceVertexCountsAttr().Get() or []
    idx = mesh.GetFaceVertexIndicesAttr().Get() or []
    tris, at = [], 0
    for c in counts:
        for k in range(1, c - 1):
            tris.append((idx[at], idx[at + k], idx[at + k + 1]))
        at += c
    return tris

def emit(out, name, points, xf, tris):
    verts = [to_ue(xf.Transform(Gf.Vec3d(*p))) for p in points]
    if FLIP_Y:  # mirroring reverses winding; keep outward normals
        tris = [(a, c, b) for a, b, c in tris]
    out.append((name, verts, tris))

def collect(stage_path):
    stage = Usd.Stage.Open(stage_path)
    cache = UsdGeom.XformCache()
    out = []
    it = iter(Usd.PrimRange(stage.GetPseudoRoot()))
    for prim in it:
        if prim.IsA(UsdGeom.PointInstancer):
            inst = UsdGeom.PointInstancer(prim)
            protos = inst.GetPrototypesRel().GetTargets()
            xforms = inst.ComputeInstanceTransformsAtTime(Usd.TimeCode.Default(), Usd.TimeCode.Default())
            ids = inst.GetProtoIndicesAttr().Get() or []
            parent = cache.GetLocalToWorldTransform(prim)
            for n, (pi, m) in enumerate(zip(ids, xforms)):
                proto = stage.GetPrimAtPath(protos[pi])
                for sub in Usd.PrimRange(proto):
                    if sub.IsA(UsdGeom.Mesh):
                        mesh = UsdGeom.Mesh(sub)
                        local = cache.GetLocalToWorldTransform(sub) * cache.GetLocalToWorldTransform(proto).GetInverse()
                        emit(out, f"{prim.GetPath()}[{n}]", mesh.GetPointsAttr().Get(), local * Gf.Matrix4d(m) * parent, triangles(mesh))
            it.PruneChildren()
        elif prim.IsA(UsdGeom.Mesh):
            mesh = UsdGeom.Mesh(prim)
            emit(out, str(prim.GetPath()), mesh.GetPointsAttr().Get() or [], cache.GetLocalToWorldTransform(prim), triangles(mesh))
    return out

def write(out, path):
    with open(path, "wb") as f:
        f.write(b"AGT1" + struct.pack("<I", len(out)))
        for name, verts, tris in out:
            b = name.encode()
            f.write(struct.pack("<I", len(b)) + b + struct.pack("<I", len(verts)))
            f.write(b"".join(struct.pack("<3f", *v) for v in verts))
            f.write(struct.pack("<I", len(tris)))
            f.write(b"".join(struct.pack("<3I", *t) for t in tris))

if __name__ == "__main__":
    if len(sys.argv) > 3:
        FLIP_Y = sys.argv[3] == "flip"
    out = collect(sys.argv[1])
    write(out, sys.argv[2])
    for name, verts, tris in out:
        lo = [min(v[i] for v in verts) for i in range(3)] if verts else [0] * 3
        hi = [max(v[i] for v in verts) for i in range(3)] if verts else [0] * 3
        print(f"{name:70} v={len(verts):6} t={len(tris):6} min=({lo[0]:.0f},{lo[1]:.0f},{lo[2]:.0f}) max=({hi[0]:.0f},{hi[1]:.0f},{hi[2]:.0f})")
